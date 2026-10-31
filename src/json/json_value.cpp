#include "json/json_value.hpp"
#include "json/json_writer.hpp"
#include <stdexcept>

namespace lxace::json {

const std::string JsonValue::empty_str_ = "";
const JsonValue JsonValue::null_val_{nullptr};

JsonValue::JsonValue() : data_(std::monostate{}) {}
JsonValue::JsonValue(std::nullptr_t) : data_(std::monostate{}) {}
JsonValue::JsonValue(bool b) : data_(b) {}
JsonValue::JsonValue(int i) : data_(static_cast<double>(i)) {}
JsonValue::JsonValue(int64_t i) : data_(static_cast<double>(i)) {}
JsonValue::JsonValue(double d) : data_(d) {}
JsonValue::JsonValue(const char* s) : data_(std::string(s ? s : "")) {}
JsonValue::JsonValue(std::string s) : data_(std::move(s)) {}
JsonValue::JsonValue(JsonArray arr) : data_(std::move(arr)) {}
JsonValue::JsonValue(JsonObject obj) : data_(std::move(obj)) {}

JsonType JsonValue::type() const {
    switch (data_.index()) {
        case 0: return JsonType::Null;
        case 1: return JsonType::Boolean;
        case 2: return JsonType::Number;
        case 3: return JsonType::String;
        case 4: return JsonType::Array;
        case 5: return JsonType::Object;
        default: return JsonType::Null;
    }
}

bool JsonValue::is_null() const { return std::holds_alternative<std::monostate>(data_); }
bool JsonValue::is_bool() const { return std::holds_alternative<bool>(data_); }
bool JsonValue::is_number() const { return std::holds_alternative<double>(data_); }
bool JsonValue::is_string() const { return std::holds_alternative<std::string>(data_); }
bool JsonValue::is_array() const { return std::holds_alternative<JsonArray>(data_); }
bool JsonValue::is_object() const { return std::holds_alternative<JsonObject>(data_); }

bool JsonValue::as_bool(bool default_val) const {
    if (const auto* val = std::get_if<bool>(&data_)) return *val;
    return default_val;
}

int64_t JsonValue::as_int(int64_t default_val) const {
    if (const auto* val = std::get_if<double>(&data_)) return static_cast<int64_t>(*val);
    return default_val;
}

double JsonValue::as_double(double default_val) const {
    if (const auto* val = std::get_if<double>(&data_)) return *val;
    return default_val;
}

const std::string& JsonValue::as_string(const std::string& default_val) const {
    if (const auto* val = std::get_if<std::string>(&data_)) return *val;
    return default_val;
}

const JsonArray& JsonValue::as_array() const {
    if (const auto* val = std::get_if<JsonArray>(&data_)) return *val;
    static const JsonArray empty_arr;
    return empty_arr;
}

const JsonObject& JsonValue::as_object() const {
    if (const auto* val = std::get_if<JsonObject>(&data_)) return *val;
    static const JsonObject empty_obj;
    return empty_obj;
}

JsonArray& JsonValue::as_array() {
    if (!is_array()) data_ = JsonArray{};
    return std::get<JsonArray>(data_);
}

JsonObject& JsonValue::as_object() {
    if (!is_object()) data_ = JsonObject{};
    return std::get<JsonObject>(data_);
}

bool JsonValue::contains(const std::string& key) const {
    if (!is_object()) return false;
    const auto& obj = as_object();
    return obj.find(key) != obj.end();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!is_object()) return null_val_;
    const auto& obj = as_object();
    auto it = obj.find(key);
    return (it != obj.end()) ? it->second : null_val_;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    return as_object()[key];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (!is_array()) return null_val_;
    const auto& arr = as_array();
    return (index < arr.size()) ? arr[index] : null_val_;
}

JsonValue& JsonValue::operator[](size_t index) {
    auto& arr = as_array();
    if (index >= arr.size()) arr.resize(index + 1);
    return arr[index];
}

std::string JsonValue::serialize(bool pretty) const {
    return JsonWriter::write(*this, pretty);
}

} // namespace lxace::json
