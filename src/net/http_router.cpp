#include "net/http_router.hpp"

namespace lxace::net {

void HttpRouter::add_route(HttpMethod method, const std::string& path, RouteHandler handler) {
    exact_routes_[{method, path}] = std::move(handler);
}

HttpResponse HttpRouter::route(const HttpRequest& req) const {
    if (req.method == HttpMethod::Options) {
        HttpResponse res;
        res.status_code = 204;
        res.status_text = "No Content";
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        return res;
    }

    auto it = exact_routes_.find({req.method, req.path});
    if (it != exact_routes_.end() && it->second) {
        return it->second(req);
    }

    // Fallback 404
    HttpResponse not_found;
    not_found.status_code = 404;
    not_found.status_text = "Not Found";
    not_found.headers["Content-Type"] = "application/json";
    not_found.headers["Access-Control-Allow-Origin"] = "*";
    not_found.body = "{\"error\": \"Route not found\"}";
    return not_found;
}

} // namespace lxace::net
