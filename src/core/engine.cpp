#include "core/engine.hpp"
#include "json/json_value.hpp"
#include "json/json_parser.hpp"
#include "net/http_auth.hpp"
#include "sys/fs.hpp"
#include "sys/process.hpp"
#include "sys/procfs.hpp"
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace lxace::core {

Engine::Engine(std::filesystem::path project_dir, int port)
    : project_dir_(std::move(project_dir)), port_(port) {
    auto config_file = project_dir_ / ".lxace" / "config.json";
    config_ = std::make_shared<ConfigManager>(config_file);
    event_bus_ = std::make_shared<EventBus>();
    ring_buffer_ = std::make_shared<RingBuffer<std::string>>(500);
    tool_registry_ = std::make_shared<tool::ToolRegistry>();
    orchestrator_ = std::make_shared<agent::Orchestrator>(event_bus_, tool_registry_, project_dir_);
    sse_manager_ = std::make_shared<net::SseStreamManager>();
    http_router_ = std::make_shared<net::HttpRouter>();
    http_server_ = std::make_shared<net::HttpServer>(static_cast<uint16_t>(port_));
}

Engine::~Engine() {
    stop_services();
}

bool Engine::initialize() {
    // Listen to events to populate ring buffer and SSE stream
    event_bus_listener_id_ = event_bus_->subscribe([this](const Event& ev) {
        std::string line = ev.format_cli_line();
        ring_buffer_->push(line);
        if (sse_manager_) {
            sse_manager_->broadcast(ev);
        }
    });

    configure_auth();
    register_api_routes();
    http_server_->set_router(http_router_);
    http_server_->set_sse_manager(sse_manager_);

    return true;
}

void Engine::configure_auth() {
    // Remote exposure is opt-in via LXACE_BIND. Loopback stays the default so a
    // freshly built binary is never reachable from the network by accident.
    if (const char* bind = std::getenv("LXACE_BIND"); bind != nullptr && bind[0] != '\0') {
        bind_address_ = bind;
    }

    api_token_ = net::HttpAuth::resolve_token((project_dir_ / ".lxace" / "api_token").string());

    net::AuthPolicy policy;
    const bool remote = bind_address_ != "127.0.0.1" && bind_address_ != "localhost";
    // A non-loopback bind always requires a token. Loopback also requires one unless
    // LXACE_ALLOW_UNAUTHENTICATED=1 is explicitly set for local tooling.
    const bool allow_local_anon = []() {
        const char* v = std::getenv("LXACE_ALLOW_UNAUTHENTICATED");
        return v != nullptr && std::string(v) == "1";
    }();

    policy.require_token = remote || (!allow_local_anon && !api_token_.empty());
    policy.bearer_token = api_token_;

    if (const char* origins = std::getenv("LXACE_ALLOWED_ORIGINS"); origins != nullptr && origins[0] != '\0') {
        std::string list(origins);
        size_t start = 0;
        while (start <= list.size()) {
            const auto comma = list.find(',', start);
            const auto token = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            if (!token.empty()) {
                policy.extra_origins.push_back(token);
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }

    http_server_->set_bind_address(bind_address_);
    http_router_->set_auth_policy(std::move(policy));
}

bool Engine::start_services() {
    if (!http_server_->start()) {
        last_error_ = http_server_->last_error();
        return false;
    }
    return true;
}

void Engine::stop_services() {
    if (event_bus_ && event_bus_listener_id_ > 0) {
        event_bus_->unsubscribe(event_bus_listener_id_);
        event_bus_listener_id_ = 0;
    }
    if (orchestrator_) {
        orchestrator_->stop_all();
    }
    if (http_server_) {
        http_server_->stop();
    }
}

void Engine::submit_prompt(const std::string& prompt) {
    if (orchestrator_) {
        orchestrator_->submit_prompt(prompt);
    }
}

void Engine::stop_all() {
    if (orchestrator_) {
        orchestrator_->stop_all();
    }
}

void Engine::register_api_routes() {
    // GET /api/session
    http_router_->add_route(net::HttpMethod::Get, "/api/session", [this](const net::HttpRequest&) {
        json::JsonObject obj;
        obj["project_dir"] = project_dir_.string();
        obj["model"] = config_->get().model;
        obj["base_url_masked"] = config_->masked_base_url();
        obj["api_key_masked"] = config_->masked_api_key();

        // 50% max resource limits
        uint32_t total_cpus = sys::ProcfsReader::get_cpu_cores();
        uint32_t cpu_50_cap = sys::ProcfsReader::get_50_percent_cpu_cap();
        auto sys_mem = sys::ProcfsReader::get_system_memory();
        uint64_t total_mem = sys_mem ? sys_mem->total_bytes : 0;
        uint64_t mem_50_cap = sys::ProcfsReader::get_50_percent_memory_cap();

        json::JsonObject res_obj;
        res_obj["host_cpu_cores"] = static_cast<int>(total_cpus);
        res_obj["cpu_50_percent_cap"] = static_cast<int>(cpu_50_cap);
        res_obj["host_total_memory_bytes"] = static_cast<double>(total_mem);
        res_obj["memory_50_percent_cap_bytes"] = static_cast<double>(mem_50_cap);
        obj["resource_limits"] = json::JsonValue(res_obj);

        json::JsonArray tasks_arr;
        for (const auto& t : orchestrator_->get_tasks()) {
            json::JsonObject t_obj;
            t_obj["id"] = t.id;
            t_obj["prompt"] = t.prompt;
            t_obj["state"] = agent::task_state_to_string(t.state);
            t_obj["assigned_agent"] = t.assigned_agent;
            tasks_arr.push_back(json::JsonValue(t_obj));
        }
        obj["tasks"] = json::JsonValue(tasks_arr);
        return net::HttpResponse::json(200, json::JsonValue(obj).serialize());
    });

    // POST /api/prompt
    http_router_->add_route(net::HttpMethod::Post, "/api/prompt", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed || !parsed->contains("prompt")) {
            return net::HttpResponse::json(400, "{\"error\": \"Missing prompt field\"}");
        }
        std::string prompt = (*parsed)["prompt"].as_string();
        std::string task_id = orchestrator_->submit_prompt(prompt);
        json::JsonObject res;
        res["task_id"] = task_id;
        res["status"] = "queued";
        return net::HttpResponse::json(200, json::JsonValue(res).serialize());
    });

    // POST /api/stop
    http_router_->add_route(net::HttpMethod::Post, "/api/stop", [this](const net::HttpRequest&) {
        stop_all();
        return net::HttpResponse::json(200, "{\"status\": \"all tasks stopped\"}");
    });

    // GET /api/settings
    http_router_->add_route(net::HttpMethod::Get, "/api/settings", [this](const net::HttpRequest&) {
        auto conf = config_->get();
        json::JsonObject obj;
        // Only the masked form is returned. Echoing the raw value made the mask cosmetic
        // and exposed any credentials embedded in the URL.
        obj["base_url_masked"] = config_->masked_base_url();
        obj["api_key_masked"] = config_->masked_api_key();
        obj["model"] = conf.model;
        obj["max_tokens"] = conf.max_tokens;
        obj["temperature"] = conf.temperature;
        obj["server_port"] = conf.server_port;
        return net::HttpResponse::json(200, json::JsonValue(obj).serialize());
    });

    // POST /api/settings
    http_router_->add_route(net::HttpMethod::Post, "/api/settings", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed) {
            return net::HttpResponse::json(400, "{\"error\": \"Invalid JSON\"}");
        }

        auto conf = config_->get();
        std::vector<std::string> rejected;

        if (parsed->contains("base_url")) conf.base_url = (*parsed)["base_url"].as_string();
        if (parsed->contains("api_key")) conf.api_key = (*parsed)["api_key"].as_string();
        if (parsed->contains("model")) conf.model = (*parsed)["model"].as_string();

        if (parsed->contains("max_tokens")) {
            const double v = (*parsed)["max_tokens"].as_double();
            if (!std::isfinite(v) || v < 1.0 || v > 1000000.0) {
                rejected.push_back("max_tokens must be between 1 and 1000000");
            } else {
                conf.max_tokens = static_cast<int>(v);
            }
        }

        if (parsed->contains("temperature")) {
            const double v = (*parsed)["temperature"].as_double();
            if (!std::isfinite(v) || v < 0.0 || v > 2.0) {
                rejected.push_back("temperature must be between 0 and 2");
            } else {
                conf.temperature = v;
            }
        }

        if (parsed->contains("server_port")) {
            // Accepted by the old API but never rebound the listener, so the UI reported
            // a port that was not in use. Reject it explicitly instead of lying.
            const double v = (*parsed)["server_port"].as_double();
            if (!std::isfinite(v) || v < 1.0 || v > 65535.0) {
                rejected.push_back("server_port must be between 1 and 65535");
            } else if (static_cast<int>(v) != port_) {
                rejected.push_back("server_port cannot be changed at runtime; restart with --port");
            }
        }

        if (!rejected.empty()) {
            json::JsonObject err;
            json::JsonArray reasons;
            for (const auto& r : rejected) reasons.push_back(json::JsonValue(r));
            err["error"] = "Invalid settings";
            err["reasons"] = json::JsonValue(reasons);
            return net::HttpResponse::json(400, json::JsonValue(err).serialize());
        }

        config_->update(conf);
        return net::HttpResponse::json(200, "{\"status\": \"updated\"}");
    });

    // POST /api/exec
    http_router_->add_route(net::HttpMethod::Post, "/api/exec", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed || !parsed->contains("command")) {
            return net::HttpResponse::json(400, "{\"error\": \"Missing command field\"}");
        }
        std::string cmd = (*parsed)["command"].as_string();
        auto result = sys::ProcessRunner::execute(cmd, project_dir_.string());

        json::JsonObject res;
        res["exit_code"] = result.exit_code;
        res["stdout"] = result.stdout_output;
        res["stderr"] = result.stderr_output;
        res["timed_out"] = result.timed_out;
        res["truncated_stdout"] = result.truncated_stdout;
        res["truncated_stderr"] = result.truncated_stderr;
        res["duration_ms"] = result.duration_ms;
        return net::HttpResponse::json(200, json::JsonValue(res).serialize());
    });

    // GET /api/fs/list
    http_router_->add_route(net::HttpMethod::Get, "/api/fs/list", [this](const net::HttpRequest& req) {
        std::string rel_path = "";
        auto it = req.query_params.find("path");
        if (it != req.query_params.end()) {
            rel_path = it->second;
        }
        auto safe_path = sys::FsEngine::resolve_safe_path(project_dir_, rel_path);
        if (!safe_path) {
            return net::HttpResponse::json(403, "{\"error\": \"Access denied: path outside project directory\"}");
        }

        auto entries = sys::FsEngine::list_directory(*safe_path, false);
        json::JsonArray arr;
        for (const auto& de : entries) {
            json::JsonObject item;
            item["name"] = de.name;
            item["path"] = de.path;
            item["is_directory"] = de.is_directory;
            item["size"] = static_cast<double>(de.size);
            item["permissions"] = static_cast<int>(de.permissions);
            arr.push_back(json::JsonValue(item));
        }
        return net::HttpResponse::json(200, json::JsonValue(arr).serialize());
    });

    // POST /api/fs/read
    http_router_->add_route(net::HttpMethod::Post, "/api/fs/read", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed || !parsed->contains("path")) {
            return net::HttpResponse::json(400, "{\"error\": \"Missing path field\"}");
        }
        std::string p = (*parsed)["path"].as_string();
        auto safe_path = sys::FsEngine::resolve_safe_path(project_dir_, p);
        if (!safe_path) {
            return net::HttpResponse::json(403, "{\"error\": \"Access denied: path outside project directory\"}");
        }

        auto content = sys::FsEngine::read_file(*safe_path);
        if (!content) {
            return net::HttpResponse::json(404, "{\"error\": \"File not found or unreadable\"}");
        }
        json::JsonObject res;
        res["path"] = safe_path->string();
        res["content"] = *content;
        return net::HttpResponse::json(200, json::JsonValue(res).serialize());
    });

    // POST /api/fs/write
    http_router_->add_route(net::HttpMethod::Post, "/api/fs/write", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed || !parsed->contains("path") || !parsed->contains("content")) {
            return net::HttpResponse::json(400, "{\"error\": \"Missing path or content field\"}");
        }
        std::string p = (*parsed)["path"].as_string();
        std::string content = (*parsed)["content"].as_string();
        bool append = parsed->contains("append") && (*parsed)["append"].as_bool();

        auto safe_path = sys::FsEngine::resolve_safe_path(project_dir_, p);
        if (!safe_path) {
            return net::HttpResponse::json(403, "{\"error\": \"Access denied: path outside project directory\"}");
        }

        bool ok = sys::FsEngine::write_file(*safe_path, content, append);
        if (!ok) {
            return net::HttpResponse::json(500, "{\"error\": \"Failed to write file\"}");
        }
        return net::HttpResponse::json(200, "{\"status\": \"written\"}");
    });

    // GET /api/tools
    http_router_->add_route(net::HttpMethod::Get, "/api/tools", [this](const net::HttpRequest&) {
        auto schemas = tool_registry_->get_schemas();
        return net::HttpResponse::json(200, json::JsonValue(schemas).serialize());
    });

    // POST /api/tools/execute
    http_router_->add_route(net::HttpMethod::Post, "/api/tools/execute", [this](const net::HttpRequest& req) {
        auto parsed = json::JsonParser::parse(req.body);
        if (!parsed || !parsed->contains("tool") || !(*parsed)["tool"].is_string()) {
            return net::HttpResponse::json(400, "{\"error\": \"Missing or invalid tool field\"}");
        }
        std::string tool_name = (*parsed)["tool"].as_string();
        json::JsonObject args_obj;
        if (parsed->contains("arguments") && (*parsed)["arguments"].is_object()) {
            args_obj = (*parsed)["arguments"].as_object();
        }

        tool::ToolExecutionContext ctx{project_dir_, event_bus_};
        auto result = tool_registry_->execute(tool_name, args_obj, ctx);
        return net::HttpResponse::json(200, json::JsonValue(result.to_json()).serialize());
    });
}

} // namespace lxace::core
