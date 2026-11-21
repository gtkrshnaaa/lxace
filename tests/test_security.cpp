#include "test_runner.hpp"
#include "net/http_auth.hpp"
#include "net/http_parser.hpp"
#include "net/http_router.hpp"
#include "net/request_reader.hpp"

using namespace lxace::net;

namespace {

HttpRequest make_request(HttpMethod method, const std::string& path) {
    HttpRequest req;
    req.method = method;
    req.path = path;
    return req;
}

} // namespace

TEST_CASE(auth_rejects_missing_bearer_token) {
    AuthPolicy policy;
    policy.require_token = true;
    policy.bearer_token = "s3cret-token";

    HttpRequest req = make_request(HttpMethod::Post, "/api/exec");

    int status = 0;
    ASSERT_FALSE(HttpAuth::authorize(req, policy, status));
    ASSERT_EQ(status, 401);
}

TEST_CASE(auth_accepts_matching_bearer_token) {
    AuthPolicy policy;
    policy.require_token = true;
    policy.bearer_token = "s3cret-token";

    HttpRequest req = make_request(HttpMethod::Post, "/api/exec");
    req.headers["authorization"] = "Bearer s3cret-token";

    int status = 0;
    ASSERT_TRUE(HttpAuth::authorize(req, policy, status));
    ASSERT_EQ(status, 200);
}

TEST_CASE(auth_rejects_wrong_bearer_token) {
    AuthPolicy policy;
    policy.require_token = true;
    policy.bearer_token = "s3cret-token";

    HttpRequest req = make_request(HttpMethod::Post, "/api/exec");
    req.headers["authorization"] = "Bearer wrong-token";

    int status = 0;
    ASSERT_FALSE(HttpAuth::authorize(req, policy, status));
    ASSERT_EQ(status, 401);
}

TEST_CASE(auth_constant_time_compare_is_length_safe) {
    ASSERT_TRUE(HttpAuth::constant_time_equals("abc123", "abc123"));
    ASSERT_FALSE(HttpAuth::constant_time_equals("abc123", "abc124"));
    ASSERT_FALSE(HttpAuth::constant_time_equals("abc", "abcd"));
    ASSERT_FALSE(HttpAuth::constant_time_equals("", "a"));
}

TEST_CASE(cors_allows_only_listed_or_loopback_origins) {
    AuthPolicy policy;
    policy.extra_origins = {"https://trusted.example"};

    ASSERT_TRUE(policy.allows_origin("https://trusted.example"));
    ASSERT_TRUE(policy.allows_origin("http://localhost:3000"));
    // A wildcard or attacker origin must never be reflected back.
    ASSERT_FALSE(policy.allows_origin("https://evil.example"));
    ASSERT_FALSE(policy.allows_origin("*"));
    ASSERT_FALSE(policy.allows_origin(""));
}

TEST_CASE(router_preflight_accepts_listed_origin_without_wildcard) {
    HttpRouter router;
    AuthPolicy policy;
    policy.extra_origins = {"https://trusted.example"};
    router.set_auth_policy(policy);

    HttpRequest preflight = make_request(HttpMethod::Options, "/api/exec");
    preflight.headers["Origin"] = "https://trusted.example";
    auto res = router.route(preflight);

    ASSERT_EQ(res.status_code, 204);
    ASSERT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://trusted.example");
}

TEST_CASE(http_parser_percent_decodes_path_and_query) {
    std::string raw =
        "GET /api/fs/list?path=src%2Fnet&q=a%20b HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    auto req = HttpParser::parse(raw);
    ASSERT_TRUE(req.has_value());
    ASSERT_EQ(req->path, "/api/fs/list");
    ASSERT_EQ(req->query_params["path"], "src/net");
    ASSERT_EQ(req->query_params["q"], "a b");
}

TEST_CASE(http_response_always_emits_content_length) {
    HttpResponse res;
    res.status_code = 204;
    res.status_text = HttpResponse::reason_phrase(204);
    const std::string wire = res.to_http_string();

    ASSERT_TRUE(wire.find("Content-Length: 0") != std::string::npos);
    ASSERT_TRUE(wire.find("204 No Content") != std::string::npos);
}

TEST_CASE(http_reason_phrases_are_specific) {
    ASSERT_EQ(HttpResponse::reason_phrase(400), "Bad Request");
    ASSERT_EQ(HttpResponse::reason_phrase(403), "Forbidden");
    ASSERT_EQ(HttpResponse::reason_phrase(401), "Unauthorized");
    ASSERT_EQ(HttpResponse::reason_phrase(500), "Internal Server Error");
}

TEST_CASE(router_requires_auth_before_dispatch) {
    HttpRouter router;
    AuthPolicy policy;
    policy.require_token = true;
    policy.bearer_token = "router-token";
    router.set_auth_policy(policy);

    bool handler_called = false;
    router.add_route(HttpMethod::Post, "/api/exec", [&](const HttpRequest&) {
        handler_called = true;
        return HttpResponse::ok("ran");
    });

    HttpRequest unauth = make_request(HttpMethod::Post, "/api/exec");
    auto rejected = router.route(unauth);
    ASSERT_EQ(rejected.status_code, 401);
    ASSERT_FALSE(handler_called);

    HttpRequest auth = make_request(HttpMethod::Post, "/api/exec");
    auth.headers["authorization"] = "Bearer router-token";
    auto allowed = router.route(auth);
    ASSERT_EQ(allowed.status_code, 200);
    ASSERT_TRUE(handler_called);
}

TEST_CASE(router_preflight_rejects_unknown_origin) {
    HttpRouter router;
    AuthPolicy policy;
    policy.extra_origins = {"https://trusted.example"};
    router.set_auth_policy(policy);

    HttpRequest preflight = make_request(HttpMethod::Options, "/api/exec");
    preflight.headers["origin"] = "https://evil.example";
    auto res = router.route(preflight);

    ASSERT_EQ(res.status_code, 403);
    ASSERT_TRUE(res.headers.find("Access-Control-Allow-Origin") == res.headers.end() ||
                res.headers.at("Access-Control-Allow-Origin") != "*");
}

TEST_CASE(request_reader_finds_headers_case_insensitively) {
    std::string raw =
        "POST /api/exec HTTP/1.1\r\n"
        "CONTENT-LENGTH: 12\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"a\":\"b\"}";

    const size_t header_end = raw.find("\r\n\r\n");
    ASSERT_TRUE(header_end != std::string::npos);

    std::string value;
    ASSERT_TRUE(RequestReader::find_header(raw, header_end, "content-length", value));
    ASSERT_EQ(value, "12");

    std::string ctype;
    ASSERT_TRUE(RequestReader::find_header(raw, header_end, "CONTENT-TYPE", ctype));
    ASSERT_EQ(ctype, "application/json");
}
