#pragma once

#include "json/json_value.hpp"
#include <string>

namespace lxace::json {

class JsonWriter {
public:
    static std::string write(const JsonValue& value, bool pretty = false);

private:
    static void write_value(const JsonValue& value, std::string& out, bool pretty, int indent);
    static void write_string(const std::string& str, std::string& out);
    static void write_indent(std::string& out, int indent);
};

} // namespace lxace::json
