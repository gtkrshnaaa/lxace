#pragma once

#include <string>
#include <map>
#include <vector>

namespace lxace::net {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Options,
    Unknown
};

[[nodiscard]] inline HttpMethod string_to_http_method(const std::string& str) {
    if (str == "GET") return HttpMethod::Get;
    if (str == "POST") return HttpMethod::Post;
    if (str == "PUT") return HttpMethod::Put;
    if (str == "DELETE") return HttpMethod::Delete;
    if (str == "OPTIONS") return HttpMethod::Options;
    return HttpMethod::Unknown;
}

[[nodiscard]] inline std::string http_method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get: return "GET";
        case HttpMethod::Post: return "POST";
        case HttpMethod::Put: return "PUT";
        case HttpMethod::Delete: return "DELETE";
        case HttpMethod::Options: return "OPTIONS";
        case HttpMethod::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

struct HttpRequest {
    HttpMethod method{HttpMethod::Unknown};
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;
    std::string body;

    [[nodiscard]] std::string get_header(const std::string& key) const {
        for (const auto& [k, v] : headers) {
            // Case-insensitive lookup
            if (k.size() == key.size()) {
                bool match = true;
                for (size_t i = 0; i < k.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(k[i])) !=
                        std::tolower(static_cast<unsigned char>(key[i]))) {
                        match = false;
                        break;
                    }
                }
                if (match) return v;
            }
        }
        return "";
    }
};

struct HttpResponse {
    int status_code{200};
    std::string status_text{"OK"};
    std::map<std::string, std::string> headers;
    std::string body;

    static HttpResponse json(int status, const std::string& json_payload) {
        HttpResponse res;
        res.status_code = status;
        res.status_text = (status == 200) ? "OK" : (status == 404 ? "Not Found" : "Error");
        res.headers["Content-Type"] = "application/json";
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        res.body = json_payload;
        return res;
    }

    static HttpResponse ok(const std::string& body, const std::string& content_type = "text/plain") {
        HttpResponse res;
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = content_type;
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.body = body;
        return res;
    }

    [[nodiscard]] std::string to_http_string() const {
        std::string out = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n";
        auto h = headers;
        if (h.find("Content-Length") == h.end() && !body.empty()) {
            h["Content-Length"] = std::to_string(body.size());
        }
        if (h.find("Connection") == h.end()) {
            h["Connection"] = "close";
        }
        for (const auto& [k, v] : h) {
            out += k + ": " + v + "\r\n";
        }
        out += "\r\n";
        out += body;
        return out;
    }
};

} // namespace lxace::net
