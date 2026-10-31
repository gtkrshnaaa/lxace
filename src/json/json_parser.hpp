#pragma once

#include "json/json_value.hpp"
#include <string_view>
#include <optional>

namespace lxace::json {

class JsonParser {
public:
    static std::optional<JsonValue> parse(std::string_view json_str);

private:
    explicit JsonParser(std::string_view input);

    std::optional<JsonValue> parse_value();
    std::optional<JsonValue> parse_null();
    std::optional<JsonValue> parse_bool();
    std::optional<JsonValue> parse_number();
    std::optional<std::string> parse_string_raw();
    std::optional<JsonValue> parse_string();
    std::optional<JsonValue> parse_array();
    std::optional<JsonValue> parse_object();

    void skip_whitespace();
    [[nodiscard]] char peek() const;
    char get();
    [[nodiscard]] bool eof() const;

    std::string_view input_;
    size_t pos_{0};
};

} // namespace lxace::json
