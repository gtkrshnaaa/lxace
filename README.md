# Linux AI Control Engine (LXACE)

LXACE is a self-contained, high-performance C++ control engine built exclusively for Linux environments. It provides native control over the underlying operating system through direct POSIX system calls and standard library abstractions rather than relying on bash subshells. 

The engine exposes a REST and Server-Sent Events (SSE) API surface for external wrappers (desktop environments, web applications, and external CLIs) while concurrently running an adaptive, menu-driven terminal interface.

---

## Key Characteristics

1. Zero External Dependencies
   The entire codebase compiles using only the C++20 standard library and direct Linux system calls (`sys/socket.h`, `sys/epoll.h`, `sys/wait.h`, `sys/inotify.h`, `sys/ioctl.h`, `unistd.h`, `fcntl.h`). No external HTTP servers, JSON libraries, or terminal GUI frameworks are utilized.

2. Native Linux Operations
   Provides dedicated C++ APIs for filesystem manipulation, process group spawning with pipe capture, `/proc` metrics extraction, and non-blocking socket probes.

3. Single-Project Focus
   Scoped to a target project directory. Operations are sandboxed to the target workspace with path traversal defenses.

4. Dual-Interface Core
   Serves external integration via an epoll-driven HTTP/1.1 and SSE server while maintaining a terminal CLI that adapts dynamically to window height and streams background sub-agent activity in real time.

---

## System Architecture

```
┌─────────────────┐  ┌──────────────┐  ┌─────────────────┐
│  Desktop App    │  │   Web App    │  │  Custom CLI     │
│  (wrapper)      │  │  (wrapper)   │  │  (wrapper)      │
└────────┬────────┘  └──────┬───────┘  └────────┬────────┘
         │                  │                   │
         └──────────────────┼───────────────────┘
                            ▼ HTTP / SSE (Port 8080)
┌────────────────────────────────────────────────────────┐
│                   LXACE Core Engine                    │
│                                                        │
│  ┌──────────────────┐  ┌────────────────────────────┐  │
│  │   HTTP/SSE Net   │  │     Sub-Agent Runtime      │  │
│  │ (epoll / socket) │  │  (orchestrator, workers)   │  │
│  └────────┬─────────┘  └─────────────┬──────────────┘  │
│           │                          │                 │
│           ▼                          ▼                 │
│  ┌──────────────────────────────────────────────────┐  │
│  │       Central EventBus & Circular RingBuffer     │  │
│  └───────────────────────────┬──────────────────────┘  │
│                              ▼                         │
│  ┌──────────────────┐  ┌────────────────────────────┐  │
│  │ Native Linux Sys │  │   Adaptive Terminal CLI    │  │
│  │  (fs/proc/exec)  │  │   (TIOCGWINSZ / raw mode)  │  │
│  └──────────────────┘  └────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
lxace/
├── CMakeLists.txt              # Primary CMake configuration (C++20, strict flags)
├── Makefile                    # Make targets (build, test, run, clean)
├── Dockerfile                  # Multi-stage isolated build and runtime container
├── docker-compose.yml          # Container deployment with 50% CPU and memory limits
├── test.sh                     # Automated test execution script
├── README.md                   # Documentation and usage guide
├── LICENSE                     # MIT License
├── docs/
│   └── prd/
│       └── prd.md             # Product Requirements Document
├── src/
│   ├── main.cpp                # Application entry point and signal orchestration
│   ├── core/                   # State, configuration, event bus, and ring buffer
│   ├── sys/                    # Native POSIX syscall wrappers (fs, proc, exec, inotify)
│   ├── json/                   # Zero-dependency JSON parser, serializer, and AST
│   ├── net/                    # Epoll reactor, HTTP/1.1 router, and SSE broadcaster
│   ├── agent/                  # Sub-agent workers and prompt orchestrator
│   └── cli/                    # Terminal dimension detection and view state machine
└── tests/
    ├── test_runner.hpp         # Lightweight zero-dependency unit test framework
    ├── test_main.cpp           # Test runner entry point
    ├── test_ring_buffer.cpp    # Ring buffer capacity, eviction, and versioning tests
    ├── test_json.cpp           # JSON parser and serializer roundtrip tests
    ├── test_fs.cpp             # Filesystem operations and traversal protection tests
    ├── test_process.cpp        # Process runner timeout and exit code tests
    └── test_http.cpp           # HTTP parser and router dispatch tests
```

---

## API Endpoints

The background HTTP server listens on port 8080 by default:

| Endpoint | Method | Description |
|---|---|---|
| `/api/session` | GET | Returns project path, masked settings, and task statuses |
| `/api/prompt` | POST | Submits a prompt payload: `{"prompt": "..."}` |
| `/api/events` | GET | Continuous SSE stream broadcasting agent insights, actions, and messages |
| `/api/stop` | POST | Terminates all active background sub-agent tasks |
| `/api/settings` | GET | Retrieves current configuration parameters |
| `/api/settings` | POST | Updates settings: `base_url`, `api_key`, `model`, `max_tokens`, `temperature` |
| `/api/exec` | POST | Executes native command: `{"command": "..."}` |
| `/api/fs/list` | GET | Lists directory contents with query parameter `?path=...` |
| `/api/fs/read` | POST | Reads file within project boundary: `{"path": "..."}` |
| `/api/fs/write` | POST | Writes file within project boundary: `{"path": "...", "content": "..."}` |

---

## Building and Running

### Prerequisites
- Linux operating system (kernel 5.x or later)
- GNU Compiler Collection (`g++` supporting C++20)
- CMake (version 3.20 or later)
- GNU Make

### Build Targets

Compile the engine and test runner:
```bash
make build
```

Run all automated unit tests:
```bash
./test.sh
```

Run LXACE in interactive CLI mode:
```bash
./build/lxace
```

Run in headless mode (API server only):
```bash
./build/lxace --headless --port 8080 --project-dir /path/to/project
```

### Docker Deployment

Build and run in a resource-capped container (2 CPUs, 2048MB RAM):
```bash
docker compose up --build
```

---

## Terminal Interface Guide

The built-in CLI adapts dynamically to window height via `TIOCGWINSZ` and hooks `SIGWINCH`:

- **Home Screen**: Displays active project path, menu options 1 to 4, and the Background Working Status feed. The status area dynamically recalculates vertical space and crops oldest lines first when the window is shortened. Updates stream in real time without screen tearing or cursor flickering.
- **Settings Screen**: Allows inspecting and updating the API Base URL, API Key (masked with asterisks), Model selection (claude-sonnet-4, claude-opus-4, gpt-4o, custom), Max Tokens, and Temperature.
- **Write Prompt**: Direct input prompt submitting tasks to the background sub-agent orchestrator.
- **Stop All**: Immediately halts all in-flight sub-agent jobs and appends confirmation logs.
- **Exit**: Cleanly unhooks signal handlers, restores terminal canonical mode, and shuts down network services.
