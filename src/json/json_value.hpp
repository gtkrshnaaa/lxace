#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <cstdint>

namespace lxace::json {

class JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    using VariantType = std::variant<
        std::monostate,
        bool,
        double,
        std::string,
        JsonArray,
        JsonObject
    >;

    JsonValue();
    JsonValue(std::nullptr_t);
    JsonValue(bool b);
    JsonValue(int i);
    JsonValue(int64_t i);
    JsonValue(double d);
    JsonValue(const char* s);
    JsonValue(std::string s);
    JsonValue(JsonArray arr);
    JsonValue(JsonObject obj);

    [[nodiscard]] JsonType type() const;
    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_number() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_object() const;

    [[nodiscard]] bool as_bool(bool default_val = false) const;
    [[nodiscard]] int64_t as_int(int64_t default_val = 0) const;
    [[nodiscard]] double as_double(double default_val = 0.0) const;
    [[nodiscard]] const std::string& as_string(const std::string& default_val = empty_str_) const;
    [[nodiscard]] const JsonArray& as_array() const;
    [[nodiscard]] const JsonObject& as_object() const;

    [[nodiscard]] JsonArray& as_array();
    [[nodiscard]] JsonObject& as_object();

    [[nodiscard]] bool contains(const std::string& key) const;
    [[nodiscard]] const JsonValue& operator[](const std::string& key) const;
    [[nodiscard]] JsonValue& operator[](const std::string& key);
    [[nodiscard]] const JsonValue& operator[](size_t index) const;
    [[nodiscard]] JsonValue& operator[](size_t index);

    [[nodiscard]] std::string serialize(bool pretty = false) const;

private:
    VariantType data_;
    static const std::string empty_str_;
    static const JsonValue null_val_;
};

} // namespace lxace::json
