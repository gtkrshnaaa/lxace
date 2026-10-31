#include "test_runner.hpp"
#include "net/http_parser.hpp"
#include "net/http_router.hpp"

using namespace lxace::net;

TEST_CASE(http_parse_get_request) {
    std::string raw =
        "GET /api/session?filter=all HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: test\r\n"
        "\r\n";

    auto req = HttpParser::parse(raw);
    ASSERT_TRUE(req.has_value());
    ASSERT_EQ(req->method, HttpMethod::Get);
    ASSERT_EQ(req->path, "/api/session");
    ASSERT_EQ(req->query_string, "filter=all");
    ASSERT_EQ(req->query_params["filter"], "all");
    ASSERT_EQ(req->get_header("Host"), "localhost:8080");
}

TEST_CASE(http_router_dispatch) {
    HttpRouter router;
    bool handler_called = false;

    router.add_route(HttpMethod::Get, "/api/test", [&](const HttpRequest&) {
        handler_called = true;
        return HttpResponse::ok("test-response");
    });

    HttpRequest req;
    req.method = HttpMethod::Get;
    req.path = "/api/test";

    auto res = router.route(req);
    ASSERT_TRUE(handler_called);
    ASSERT_EQ(res.status_code, 200);
    ASSERT_EQ(res.body, "test-response");
}
