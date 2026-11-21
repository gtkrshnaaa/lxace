#include "net/http_parser.hpp"
#include <sstream>
#include <algorithm>

namespace lxace::net {

namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Percent-decodes a query component. Malformed escapes are left verbatim rather than
// silently dropped, so callers see the same bytes the client sent.
std::string percent_decode(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            const int hi = hex_value(in[i + 1]);
            const int lo = hex_value(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (in[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(in[i]);
    }
    return out;
}

} // namespace

void HttpParser::parse_url(std::string_view url, HttpRequest& req) {
    auto q_pos = url.find('?');
    if (q_pos == std::string_view::npos) {
        req.path = percent_decode(url);
        return;
    }

    req.path = percent_decode(url.substr(0, q_pos));
    req.query_string = std::string(url.substr(q_pos + 1));

    std::string_view remaining = req.query_string;
    while (!remaining.empty()) {
        auto amp_pos = remaining.find('&');
        std::string_view pair = (amp_pos == std::string_view::npos) ? remaining : remaining.substr(0, amp_pos);
        if (amp_pos == std::string_view::npos) {
            remaining = "";
        } else {
            remaining = remaining.substr(amp_pos + 1);
        }

        auto eq_pos = pair.find('=');
        if (eq_pos != std::string_view::npos) {
            std::string key = percent_decode(pair.substr(0, eq_pos));
            std::string val = percent_decode(pair.substr(eq_pos + 1));
            req.query_params[key] = val;
        } else if (!pair.empty()) {
            req.query_params[percent_decode(pair)] = "";
        }
    }
}

std::optional<HttpRequest> HttpParser::parse(std::string_view raw_request) {
    if (raw_request.empty()) return std::nullopt;

    auto header_end = raw_request.find("\r\n\r\n");
    std::string_view header_section;
    std::string_view body_section;

    if (header_end != std::string_view::npos) {
        header_section = raw_request.substr(0, header_end);
        body_section = raw_request.substr(header_end + 4);
    } else {
        auto header_end_alt = raw_request.find("\n\n");
        if (header_end_alt == std::string_view::npos) {
            return std::nullopt;
        }
        header_section = raw_request.substr(0, header_end_alt);
        body_section = raw_request.substr(header_end_alt + 2);
    }

    HttpRequest req;
    req.body = std::string(body_section);

    std::istringstream stream{std::string(header_section)};
    std::string line;
    if (!std::getline(stream, line)) return std::nullopt;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream line_stream(line);
    std::string method_str, url_str, version_str;
    if (!(line_stream >> method_str >> url_str >> version_str)) {
        return std::nullopt;
    }

    req.method = string_to_http_method(method_str);
    parse_url(url_str, req);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        auto colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string val = line.substr(colon_pos + 1);

            // Trim
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t");
                size_t end = s.find_last_not_of(" \t");
                if (start == std::string::npos) s.clear();
                else s = s.substr(start, end - start + 1);
            };
            trim(key);
            trim(val);
            req.headers[key] = val;
        }
    }

    return req;
}

} // namespace lxace::net
