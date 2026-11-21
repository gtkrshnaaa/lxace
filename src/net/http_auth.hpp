#pragma once

#include "net/http_types.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace lxace::net {

struct AuthPolicy {
    bool require_token{false};
    std::string bearer_token;
    std::vector<std::string> extra_origins;

    [[nodiscard]] bool allows_origin(const std::string& origin) const;
    [[nodiscard]] bool token_configured() const { return !bearer_token.empty(); }
};

class HttpAuth {
public:
    // Returns true when the request may proceed. On rejection, out_status carries the
    // HTTP status the caller must return.
    [[nodiscard]] static bool authorize(const HttpRequest& req, const AuthPolicy& policy, int& out_status);

    // Applies the CORS response headers only for origins the policy allows.
    static void apply_cors(const HttpRequest& req, const AuthPolicy& policy, HttpResponse& res);

    [[nodiscard]] static std::string extract_bearer(const HttpRequest& req);
    [[nodiscard]] static bool constant_time_equals(std::string_view a, std::string_view b);

    // Extracts the host portion of an origin header, or an empty string when malformed.
    [[nodiscard]] static std::string origin_host(const std::string& origin);

    // Reads LXACE_API_TOKEN, otherwise generates and persists a 256-bit token at file_path.
    [[nodiscard]] static std::string resolve_token(const std::string& file_path);

    [[nodiscard]] static std::string generate_token();
};

} // namespace lxace::net
