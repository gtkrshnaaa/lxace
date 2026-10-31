#include "core/engine.hpp"
#include "json/json_value.hpp"
#include "json/json_parser.hpp"
#include "sys/fs.hpp"
#include "sys/process.hpp"
#include "sys/procfs.hpp"

namespace lxace::core {

Engine::Engine(std::filesystem::path project_dir, int port)
    : project_dir_(std::move(project_dir)), port_(port) {
    auto config_file = project_dir_ / ".lxace" / "config.json";
    config_ = std::make_shared<ConfigManager>(config_file);
    event_bus_ = std::make_shared<EventBus>();
    ring_buffer_ = std::make_shared<RingBuffer<std::string>>(500);
    orchestrator_ = std::make_shared<agent::Orchestrator>(event_bus_);
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

    register_api_routes();
    http_server_->set_router(http_router_);
    http_server_->set_sse_manager(sse_manager_);

    return true;
}

void Engine::start_services() {
    http_server_->start();
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
        obj["base_url"] = config_->get().base_url;
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
        obj["base_url"] = conf.base_url;
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
        if (parsed->contains("base_url")) conf.base_url = (*parsed)["base_url"].as_string();
        if (parsed->contains("api_key")) conf.api_key = (*parsed)["api_key"].as_string();
        if (parsed->contains("model")) conf.model = (*parsed)["model"].as_string();
        if (parsed->contains("max_tokens")) conf.max_tokens = static_cast<int>((*parsed)["max_tokens"].as_int());
        if (parsed->contains("temperature")) conf.temperature = (*parsed)["temperature"].as_double();
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
}

} // namespace lxace::core
