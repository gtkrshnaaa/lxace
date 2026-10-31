#pragma once

#include "net/http_types.hpp"
#include <functional>
#include <map>
#include <string>

namespace lxace::net {

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpRouter {
public:
    HttpRouter() = default;

    void add_route(HttpMethod method, const std::string& path, RouteHandler handler);
    [[nodiscard]] HttpResponse route(const HttpRequest& req) const;

private:
    std::map<std::pair<HttpMethod, std::string>, RouteHandler> exact_routes_;
};

} // namespace lxace::net
