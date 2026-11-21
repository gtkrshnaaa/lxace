#include "json/json_parser.hpp"
#include <cctype>
#include <charconv>
#include <cmath>

namespace lxace::json {

namespace {

// Encodes a Unicode scalar value as UTF-8. Lone surrogates are rejected upstream.
void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

JsonParser::JsonParser(std::string_view input) : input_(input) {}

std::optional<JsonValue> JsonParser::parse(std::string_view json_str) {
    JsonParser parser(json_str);
    parser.skip_whitespace();
    if (parser.eof()) return std::nullopt;
    auto val = parser.parse_value();
    if (!val) return std::nullopt;
    parser.skip_whitespace();
    if (!parser.eof()) return std::nullopt; // trailing garbage
    return val;
}

void JsonParser::skip_whitespace() {
    while (!eof()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            get();
        } else {
            break;
        }
    }
}

char JsonParser::peek() const {
    if (eof()) return '\0';
    return input_[pos_];
}

char JsonParser::get() {
    if (eof()) return '\0';
    return input_[pos_++];
}

bool JsonParser::eof() const {
    return pos_ >= input_.size();
}

std::optional<JsonValue> JsonParser::parse_value() {
    skip_whitespace();
    if (eof()) return std::nullopt;

    char c = peek();
    if (c == 'n') return parse_null();
    if (c == 't' || c == 'f') return parse_bool();
    if (c == '"') return parse_string();
    if (c == '[') return parse_array();
    if (c == '{') return parse_object();
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();

    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_null() {
    if (input_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return JsonValue(nullptr);
    }
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_bool() {
    if (input_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return JsonValue(true);
    }
    if (input_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return JsonValue(false);
    }
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_number() {
    size_t start = pos_;
    if (peek() == '-') get();

    while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
        get();
    }
    if (peek() == '.') {
        get();
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        get();
        if (peek() == '+' || peek() == '-') get();
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            get();
        }
    }

    std::string_view num_str = input_.substr(start, pos_ - start);
    double val = 0.0;
    auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), val);
    if (ec == std::errc() && std::isfinite(val)) {
        return JsonValue(val);
    }
    // Non-finite values are not valid JSON and would poison every later serialization.
    return std::nullopt;
}

std::optional<std::string> JsonParser::parse_string_raw() {
    if (get() != '"') return std::nullopt;
    std::string result;
    while (!eof()) {
        char c = get();
        if (c == '"') return result;
        if (c == '\\') {
            if (eof()) return std::nullopt;
            char esc = get();
            switch (esc) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (pos_ + 4 > input_.size()) return std::nullopt;
                    uint32_t code_point = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = input_[pos_++];
                        uint32_t digit = 0;
                        if (h >= '0' && h <= '9') digit = static_cast<uint32_t>(h - '0');
                        else if (h >= 'a' && h <= 'f') digit = static_cast<uint32_t>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') digit = static_cast<uint32_t>(h - 'A' + 10);
                        else return std::nullopt;
                        code_point = (code_point << 4) | digit;
                    }

                    // Surrogate pair: a high surrogate must be followed by \uDC00-\uDFFF.
                    if (code_point >= 0xD800 && code_point <= 0xDBFF) {
                        if (pos_ + 6 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
                            return std::nullopt;
                        }
                        pos_ += 2;
                        uint32_t low = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = input_[pos_++];
                            uint32_t digit = 0;
                            if (h >= '0' && h <= '9') digit = static_cast<uint32_t>(h - '0');
                            else if (h >= 'a' && h <= 'f') digit = static_cast<uint32_t>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') digit = static_cast<uint32_t>(h - 'A' + 10);
                            else return std::nullopt;
                            low = (low << 4) | digit;
                        }
                        if (low < 0xDC00 || low > 0xDFFF) return std::nullopt;
                        code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                    } else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
                        return std::nullopt;
                    }

                    append_utf8(result, code_point);
                    break;
                }
                default: result.push_back(esc); break;
            }
        } else {
            result.push_back(c);
        }
    }
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_string() {
    auto s = parse_string_raw();
    if (!s) return std::nullopt;
    return JsonValue(std::move(*s));
}

std::optional<JsonValue> JsonParser::parse_array() {
    if (get() != '[') return std::nullopt;
    JsonArray arr;
    skip_whitespace();
    if (peek() == ']') {
        get();
        return JsonValue(std::move(arr));
    }

    while (!eof()) {
        auto val = parse_value();
        if (!val) return std::nullopt;
        arr.push_back(std::move(*val));
        skip_whitespace();
        char c = peek();
        if (c == ']') {
            get();
            return JsonValue(std::move(arr));
        }
        if (c == ',') {
            get();
            skip_whitespace();
        } else {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse_object() {
    if (get() != '{') return std::nullopt;
    JsonObject obj;
    skip_whitespace();
    if (peek() == '}') {
        get();
        return JsonValue(std::move(obj));
    }

    while (!eof()) {
        skip_whitespace();
        if (peek() != '"') return std::nullopt;
        auto key = parse_string_raw();
        if (!key) return std::nullopt;

        skip_whitespace();
        if (get() != ':') return std::nullopt;

        auto val = parse_value();
        if (!val) return std::nullopt;
        obj[*key] = std::move(*val);

        skip_whitespace();
        char c = peek();
        if (c == '}') {
            get();
            return JsonValue(std::move(obj));
        }
        if (c == ',') {
            get();
            skip_whitespace();
        } else {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace lxace::json
