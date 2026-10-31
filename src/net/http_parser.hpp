#pragma once

#include "net/http_types.hpp"
#include <string_view>
#include <optional>

namespace lxace::net {

class HttpParser {
public:
    static std::optional<HttpRequest> parse(std::string_view raw_request);

private:
    static void parse_url(std::string_view url, HttpRequest& req);
};

} // namespace lxace::net
