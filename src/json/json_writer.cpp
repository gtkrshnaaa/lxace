#include "json/json_writer.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace lxace::json {

std::string JsonWriter::write(const JsonValue& value, bool pretty) {
    std::string out;
    write_value(value, out, pretty, 0);
    return out;
}

void JsonWriter::write_indent(std::string& out, int indent) {
    out.append(static_cast<size_t>(indent * 2), ' ');
}

void JsonWriter::write_string(const std::string& str, std::string& out) {
    out.push_back('"');
    for (char c : str) {
        switch (c) {
            case '"': out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b"); break;
            case '\f': out.append("\\f"); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out.append(buf);
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
}

void JsonWriter::write_value(const JsonValue& value, std::string& out, bool pretty, int indent) {
    switch (value.type()) {
        case JsonType::Null:
            out.append("null");
            break;
        case JsonType::Boolean:
            out.append(value.as_bool() ? "true" : "false");
            break;
        case JsonType::Number: {
            double d = value.as_double();
            if (std::floor(d) == d && std::abs(d) < 1e15) {
                out.append(std::to_string(static_cast<int64_t>(d)));
            } else {
                std::ostringstream ss;
                ss << std::setprecision(10) << d;
                out.append(ss.str());
            }
            break;
        }
        case JsonType::String:
            write_string(value.as_string(), out);
            break;
        case JsonType::Array: {
            const auto& arr = value.as_array();
            if (arr.empty()) {
                out.append("[]");
                break;
            }
            out.push_back('[');
            if (pretty) out.push_back('\n');
            for (size_t i = 0; i < arr.size(); ++i) {
                if (pretty) write_indent(out, indent + 1);
                write_value(arr[i], out, pretty, indent + 1);
                if (i + 1 < arr.size()) out.push_back(',');
                if (pretty) out.push_back('\n');
            }
            if (pretty) write_indent(out, indent);
            out.push_back(']');
            break;
        }
        case JsonType::Object: {
            const auto& obj = value.as_object();
            if (obj.empty()) {
                out.append("{}");
                break;
            }
            out.push_back('{');
            if (pretty) out.push_back('\n');
            size_t idx = 0;
            for (const auto& [k, v] : obj) {
                if (pretty) write_indent(out, indent + 1);
                write_string(k, out);
                out.append(pretty ? ": " : ":");
                write_value(v, out, pretty, indent + 1);
                if (++idx < obj.size()) out.push_back(',');
                if (pretty) out.push_back('\n');
            }
            if (pretty) write_indent(out, indent);
            out.push_back('}');
            break;
        }
    }
}

} // namespace lxace::json
