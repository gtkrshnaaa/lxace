#pragma once

#include "net/http_auth.hpp"
#include "net/http_types.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>

namespace lxace::net {

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpRouter {
public:
    HttpRouter() = default;

    void add_route(HttpMethod method, const std::string& path, RouteHandler handler);
    [[nodiscard]] HttpResponse route(const HttpRequest& req) const;

    void set_auth_policy(AuthPolicy policy) { auth_policy_ = std::move(policy); }
    [[nodiscard]] const AuthPolicy& auth_policy() const { return auth_policy_; }

    // Routes that may be served without a bearer token, for liveness probing.
    void add_public_route(const std::string& path) { public_paths_.insert(path); }

private:
    [[nodiscard]] bool is_public(const std::string& path) const;

    std::map<std::pair<HttpMethod, std::string>, RouteHandler> exact_routes_;
    std::set<std::string> public_paths_;
    AuthPolicy auth_policy_;
};

} // namespace lxace::net
