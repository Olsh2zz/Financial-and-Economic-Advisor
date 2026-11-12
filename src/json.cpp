// Файл: json.cpp
// Назначение: реализация чтения и записи JSON.

#include "xpay/json.hpp"

#include <utility>

namespace xpay {

Json::Json() noexcept = default;
Json::Json(bool value) noexcept : data_(value) {}
Json::Json(int value) noexcept : data_(static_cast<long long>(value)) {}
Json::Json(long long value) noexcept : data_(value) {}
Json::Json(const char* value) : data_(std::string(value)) {}
Json::Json(std::string value) noexcept : data_(std::move(value)) {}
Json::Json(std::string_view value) : data_(std::string(value)) {}
Json::Json(Array value) noexcept : data_(std::move(value)) {}
Json::Json(Object value) noexcept : data_(std::move(value)) {}

Json Json::array() { return Json(Array{}); }
Json Json::object() { return Json(Object{}); }

bool Json::is_null() const noexcept { return std::holds_alternative<std::monostate>(data_); }

std::optional<bool> Json::as_bool() const noexcept {
    const bool* value = std::get_if<bool>(&data_);
    return value != nullptr ? std::optional<bool>(*value) : std::nullopt;
}

std::optional<long long> Json::as_int() const noexcept {
    const long long* value = std::get_if<long long>(&data_);
    return value != nullptr ? std::optional<long long>(*value) : std::nullopt;
}

const std::string* Json::as_string() const noexcept { return std::get_if<std::string>(&data_); }
const Json::Array* Json::as_array() const noexcept { return std::get_if<Array>(&data_); }
const Json::Object* Json::as_object() const noexcept { return std::get_if<Object>(&data_); }

const Json* Json::find(std::string_view key) const noexcept {
    const Object* members = as_object();
    if (members == nullptr) {
        return nullptr;
    }
    for (const Member& member : *members) {
        if (member.key == key) {
            return &member.value;
        }
    }
    return nullptr;
}

Json& Json::set(std::string key, Json value) {
    std::get<Object>(data_).push_back(Member{std::move(key), std::move(value)});
    return *this;
}

Json& Json::push(Json value) {
    std::get<Array>(data_).push_back(std::move(value));
    return *this;
}

namespace {

void write_string(std::string& out, const std::string& text) {
    constexpr char kHex[] = "0123456789abcdef";
    out.push_back('"');
    for (const char symbol : text) {
        const auto code = static_cast<unsigned char>(symbol);
        switch (symbol) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                // Управляющие символы JSON разрешает только в виде \u00XX.
                // Байты UTF-8 (от 0x80) пишутся как есть.
                if (code < 0x20) {
                    out += "\\u00";
                    out.push_back(kHex[code >> 4]);
                    out.push_back(kHex[code & 0x0F]);
                } else {
                    out.push_back(symbol);
                }
        }
    }
    out.push_back('"');
}

void write_break(std::string& out, int indent, int level) {
    if (indent > 0) {
        out.push_back('\n');
        out.append(static_cast<std::size_t>(indent * level), ' ');
    }
}

}  // namespace

void Json::write(std::string& out, int indent, int level) const {
    if (is_null()) {
        out += "null";
    } else if (const bool* flag = std::get_if<bool>(&data_)) {
        out += *flag ? "true" : "false";
    } else if (const long long* number = std::get_if<long long>(&data_)) {
        out += std::to_string(*number);
    } else if (const std::string* text = as_string()) {
        write_string(out, *text);
    } else if (const Array* items = as_array()) {
        out.push_back('[');
        for (std::size_t i = 0; i < items->size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            write_break(out, indent, level + 1);
            (*items)[i].write(out, indent, level + 1);
        }
        if (!items->empty()) {
            write_break(out, indent, level);
        }
        out.push_back(']');
    } else if (const Object* members = as_object()) {
        out.push_back('{');
        for (std::size_t i = 0; i < members->size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            write_break(out, indent, level + 1);
            write_string(out, (*members)[i].key);
            out += indent > 0 ? ": " : ":";
            (*members)[i].value.write(out, indent, level + 1);
        }
        if (!members->empty()) {
            write_break(out, indent, level);
        }
        out.push_back('}');
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    write(out, indent, 0);
    return out;
}

namespace {

// Разборщик рекурсивным спуском. Первая найденная ошибка запоминается
// в error_, после неё все функции сразу возвращают управление.
class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Result<Json> run() {
        Json value = parse_value(0);
        skip_spaces();
        if (error_.empty() && position_ != text_.size()) {
            fail("лишние символы после значения");
        }
        if (!error_.empty()) {
            return Result<Json>::failure("Ошибка JSON в позиции " + std::to_string(position_) + ": " + error_ + ".");
        }
        return Result<Json>::success(std::move(value));
    }

private:
    void fail(const char* message) {
        if (error_.empty()) {
            error_ = message;
        }
    }

    void skip_spaces() {
        while (position_ < text_.size()) {
            const char symbol = text_[position_];
            if (symbol != ' ' && symbol != '\t' && symbol != '\r' && symbol != '\n') {
                break;
            }
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ < text_.size() && text_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    bool consume_word(std::string_view word) {
        if (text_.substr(position_, word.size()) == word) {
            position_ += word.size();
            return true;
        }
        return false;
    }

    Json parse_value(int depth) {
        if (depth > Json::kMaxDepth) {
            fail("слишком глубокая вложенность");
            return {};
        }
        skip_spaces();
        if (position_ >= text_.size()) {
            fail("неожиданный конец текста");
            return {};
        }
        const char symbol = text_[position_];
        if (symbol == '{') {
            return parse_object(depth);
        }
        if (symbol == '[') {
            return parse_array(depth);
        }
        if (symbol == '"') {
            return Json(parse_string());
        }
        if (symbol == '-' || (symbol >= '0' && symbol <= '9')) {
            return parse_number();
        }
        if (consume_word("true")) {
            return Json(true);
        }
        if (consume_word("false")) {
            return Json(false);
        }
        if (consume_word("null")) {
            return {};
        }
        fail("неожиданный символ");
        return {};
    }

    Json parse_object(int depth) {
        Json result = Json::object();
        ++position_;
        skip_spaces();
        if (consume('}')) {
            return result;
        }
        while (error_.empty()) {
            skip_spaces();
            if (position_ >= text_.size() || text_[position_] != '"') {
                fail("ожидалось имя поля в кавычках");
                break;
            }
            std::string key = parse_string();
            skip_spaces();
            if (!consume(':')) {
                fail("ожидалось двоеточие");
                break;
            }
            result.set(std::move(key), parse_value(depth + 1));
            skip_spaces();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                fail("ожидалась запятая или закрывающая скобка");
            }
        }
        return result;
    }

    Json parse_array(int depth) {
        Json result = Json::array();
        ++position_;
        skip_spaces();
        if (consume(']')) {
            return result;
        }
        while (error_.empty()) {
            result.push(parse_value(depth + 1));
            skip_spaces();
            if (consume(']')) {
                break;
            }
            if (!consume(',')) {
                fail("ожидалась запятая или закрывающая скобка");
            }
        }
        return result;
    }

    Json parse_number() {
        // Наибольшее число, к которому ещё можно дописать цифру без переполнения.
        constexpr long long kLimit = 900'000'000'000'000'000;
        const bool negative = consume('-');
        long long value = 0;
        std::size_t digits = 0;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            if (value > kLimit / 10) {
                fail("слишком большое число");
                return {};
            }
            value = value * 10 + (text_[position_] - '0');
            ++position_;
            ++digits;
        }
        if (digits == 0) {
            fail("ожидалась цифра");
            return {};
        }
        if (position_ < text_.size() && (text_[position_] == '.' || text_[position_] == 'e' || text_[position_] == 'E')) {
            fail("дробные числа не поддерживаются");
            return {};
        }
        return Json(negative ? -value : value);
    }

    // Четыре шестнадцатеричные цифры после \u. При ошибке возвращает 0 и ставит error_.
    unsigned parse_hex4() {
        unsigned code = 0;
        for (int i = 0; i < 4; ++i) {
            const char symbol = position_ < text_.size() ? text_[position_] : '\0';
            unsigned digit = 0;
            if (symbol >= '0' && symbol <= '9') {
                digit = static_cast<unsigned>(symbol - '0');
            } else if (symbol >= 'a' && symbol <= 'f') {
                digit = static_cast<unsigned>(symbol - 'a' + 10);
            } else if (symbol >= 'A' && symbol <= 'F') {
                digit = static_cast<unsigned>(symbol - 'A' + 10);
            } else {
                fail("неверная запись \\u");
                return 0;
            }
            code = code * 16 + digit;
            ++position_;
        }
        return code;
    }

    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    void parse_unicode_escape(std::string& out) {
        unsigned code = parse_hex4();
        // Символы за пределами U+FFFF записаны парой: старшая половина
        // U+D800..U+DBFF и сразу за ней младшая U+DC00..U+DFFF.
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (!consume_word("\\u")) {
                fail("неполная суррогатная пара");
                return;
            }
            const unsigned low = parse_hex4();
            if (low < 0xDC00 || low > 0xDFFF) {
                fail("неполная суррогатная пара");
                return;
            }
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        } else if (code >= 0xDC00 && code <= 0xDFFF) {
            fail("неполная суррогатная пара");
            return;
        }
        append_utf8(out, code);
    }

    std::string parse_string() {
        std::string out;
        ++position_;
        while (error_.empty()) {
            if (position_ >= text_.size()) {
                fail("строка не закрыта кавычкой");
                break;
            }
            const char symbol = text_[position_++];
            if (symbol == '"') {
                break;
            }
            if (static_cast<unsigned char>(symbol) < 0x20) {
                fail("управляющий символ внутри строки");
                break;
            }
            if (symbol != '\\') {
                out.push_back(symbol);
                continue;
            }
            const char escape = position_ < text_.size() ? text_[position_++] : '\0';
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': parse_unicode_escape(out); break;
                default: fail("неизвестная последовательность после обратной черты");
            }
        }
        return out;
    }

    std::string_view text_;
    std::size_t position_ = 0;
    std::string error_;
};

}  // namespace

Result<Json> Json::parse(std::string_view text) {
    return Parser(text).run();
}

}  // namespace xpay
