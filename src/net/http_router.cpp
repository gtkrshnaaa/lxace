#include "net/http_router.hpp"

namespace lxace::net {

void HttpRouter::add_route(HttpMethod method, const std::string& path, RouteHandler handler) {
    exact_routes_[{method, path}] = std::move(handler);
}

bool HttpRouter::is_public(const std::string& path) const {
    return public_paths_.find(path) != public_paths_.end();
}

HttpResponse HttpRouter::route(const HttpRequest& req) const {
    if (req.method == HttpMethod::Options) {
        const std::string origin = req.get_header("Origin");

        // Preflight is answered only for origins the policy allows. A wildcard is never
        // returned, and an unlisted origin learns nothing about the allowed methods.
        if (!auth_policy_.allows_origin(origin)) {
            HttpResponse denied = HttpResponse::json(403, "{\"error\": \"Origin not allowed\"}");
            return denied;
        }

        HttpResponse res;
        res.status_code = 204;
        res.status_text = HttpResponse::reason_phrase(204);
        res.headers["Access-Control-Allow-Origin"] = origin;
        res.headers["Vary"] = "Origin";
        res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        res.headers["Access-Control-Max-Age"] = "600";
        return res;
    }

    if (auth_policy_.require_token && !is_public(req.path)) {
        int auth_status = 200;
        if (!HttpAuth::authorize(req, auth_policy_, auth_status)) {
            HttpResponse denied = HttpResponse::json(auth_status, "{\"error\": \"Unauthorized\"}");
            denied.headers["WWW-Authenticate"] = "Bearer realm=\"lxace\"";
            HttpAuth::apply_cors(req, auth_policy_, denied);
            return denied;
        }
    }

    auto it = exact_routes_.find({req.method, req.path});
    if (it != exact_routes_.end() && it->second) {
        HttpResponse res = it->second(req);
        HttpAuth::apply_cors(req, auth_policy_, res);
        return res;
    }

    HttpResponse not_found = HttpResponse::json(404, "{\"error\": \"Route not found\"}");
    HttpAuth::apply_cors(req, auth_policy_, not_found);
    return not_found;
}

} // namespace lxace::net
