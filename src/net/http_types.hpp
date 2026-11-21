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

    [[nodiscard]] static std::string reason_phrase(int status) {
        switch (status) {
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 413: return "Payload Too Large";
            case 415: return "Unsupported Media Type";
            case 422: return "Unprocessable Entity";
            case 429: return "Too Many Requests";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            default: break;
        }
        if (status >= 100 && status < 200) return "Informational";
        if (status >= 200 && status < 300) return "Success";
        if (status >= 300 && status < 400) return "Redirection";
        if (status >= 400 && status < 500) return "Client Error";
        if (status >= 500 && status < 600) return "Server Error";
        return "Unknown";
    }

    static HttpResponse json(int status, const std::string& json_payload) {
        HttpResponse res;
        res.status_code = status;
        res.status_text = reason_phrase(status);
        res.headers["Content-Type"] = "application/json";
        res.body = json_payload;
        return res;
    }

    static HttpResponse ok(const std::string& body, const std::string& content_type = "text/plain") {
        HttpResponse res;
        res.status_code = 200;
        res.status_text = reason_phrase(200);
        res.headers["Content-Type"] = content_type;
        res.body = body;
        return res;
    }

    [[nodiscard]] std::string to_http_string() const {
        std::string out = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n";
        auto h = headers;
        // HTTP/1.1 requires a length on every response so strict clients never block.
        if (h.find("Content-Length") == h.end() && h.find("Transfer-Encoding") == h.end()) {
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
