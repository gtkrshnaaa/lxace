#include "net/http_auth.hpp"
#include <algorithm>
#include <cctype>

namespace lxace::net {

namespace {

std::string to_lower_copy(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool is_loopback_host(const std::string& host) {
    return host == "localhost" || host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

constexpr std::string_view kBearerPrefix = "Bearer ";

} // namespace

bool AuthPolicy::allows_origin(const std::string& origin) const {
    if (origin.empty() || origin == "null") {
        return false;
    }
    const std::string host = HttpAuth::origin_host(origin);
    if (!host.empty() && is_loopback_host(host)) {
        return true;
    }
    return std::find(extra_origins.begin(), extra_origins.end(), origin) != extra_origins.end();
}

std::string HttpAuth::origin_host(const std::string& origin) {
    const auto scheme_pos = origin.find("://");
    std::string rest = (scheme_pos == std::string::npos) ? origin : origin.substr(scheme_pos + 3);

    const auto slash = rest.find('/');
    if (slash != std::string::npos) {
        rest = rest.substr(0, slash);
    }

    const auto at = rest.rfind('@');
    if (at != std::string::npos) {
        rest = rest.substr(at + 1);
    }

    if (!rest.empty() && rest.front() == '[') {
        const auto close = rest.find(']');
        if (close == std::string::npos) {
            return "";
        }
        return to_lower_copy(rest.substr(0, close + 1));
    }

    const auto colon = rest.find(':');
    if (colon != std::string::npos) {
        rest = rest.substr(0, colon);
    }
    return to_lower_copy(rest);
}

std::string HttpAuth::extract_bearer(const HttpRequest& req) {
    const std::string header = req.get_header("Authorization");
    if (header.size() <= kBearerPrefix.size()) {
        return "";
    }
    for (size_t i = 0; i < kBearerPrefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(header[i])) !=
            std::tolower(static_cast<unsigned char>(kBearerPrefix[i]))) {
            return "";
        }
    }
    return header.substr(kBearerPrefix.size());
}

bool HttpAuth::constant_time_equals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff = static_cast<unsigned char>(
            diff | static_cast<unsigned char>(static_cast<unsigned char>(a[i]) ^
                                              static_cast<unsigned char>(b[i])));
    }
    return diff == 0;
}

bool HttpAuth::authorize(const HttpRequest& req, const AuthPolicy& policy, int& out_status) {
    if (!policy.require_token) {
        out_status = 200;
        return true;
    }

    const std::string presented = extract_bearer(req);
    if (presented.empty() || !policy.token_configured() ||
        !constant_time_equals(presented, policy.bearer_token)) {
        out_status = 401;
        return false;
    }

    out_status = 200;
    return true;
}

void HttpAuth::apply_cors(const HttpRequest& req, const AuthPolicy& policy, HttpResponse& res) {
    const std::string origin = req.get_header("Origin");
    if (!policy.allows_origin(origin)) {
        return;
    }
    res.headers["Access-Control-Allow-Origin"] = origin;
    res.headers["Vary"] = "Origin";
    res.headers["Access-Control-Allow-Credentials"] = "true";
}

} // namespace lxace::net
