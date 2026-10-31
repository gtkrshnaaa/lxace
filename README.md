# Linux AI Control Engine (LXACE)

> [!IMPORTANT]
> **MANDATORY RESOURCE POLICY: 50% HOST RESOURCE CAP AND CONTAINER ISOLATION**
> All developers, CI/CD pipelines, and runtime orchestration bundles must strictly enforce the 50% maximum host resource policy:
> 1. **Strict 50% CPU and RAM Cap**: Total container resource allocations across CPU cores and system memory must never exceed 50% of the host machine's total capacity (for example, maximum 4.0 CPUs and 3500M RAM on an 8-core / 7GB workstation). This preserves host OS responsiveness, battery longevity, and thermal headroom.
> 2. **Containerized Execution**: All persistent runtimes, background sub-agent processes, web servers, and compilation pipelines must execute inside isolated, unprivileged Docker containers (`USER appuser`).
> 3. **Native Runtime Metrics**: The engine dynamically probes `/proc/cpuinfo` and `/proc/meminfo` via `ProcfsReader::get_50_percent_cpu_cap()` and `ProcfsReader::get_50_percent_memory_cap()`, exposing active thresholds in `GET /api/session`.

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

5. Enforced 50% Resource Governance
   Container definitions and Makefile targets enforce strict resource ceilings (`--cpus` and `--memory` limits capped to 50% of the host capacity).

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
├── Makefile                    # Make targets including installation and container tasks
├── Dockerfile                  # Multi-stage isolated build and unprivileged runtime
├── docker-compose.yml          # Container deployment with 50% CPU and memory limits
├── README.md                   # Documentation and usage guide
├── LICENSE                     # MIT License
├── docs/
│   └── prd/
│       └── prd.md             # Product Requirements Document
├── scripts/
│   └── sh/
│       ├── install.sh         # System binary installation script
│       └── uninstall.sh       # Clean binary removal script
├── src/
│   ├── main.cpp                # Application entry point and signal orchestration
│   ├── core/                   # State, configuration, event bus, and ring buffer
│   ├── sys/                    # Native POSIX syscall wrappers (fs, proc, exec, inotify)
│   ├── json/                   # Zero-dependency JSON parser, serializer, and AST
│   ├── net/                    # Epoll reactor, HTTP/1.1 router, and SSE broadcaster
│   ├── agent/                  # Sub-agent workers and prompt orchestrator
│   └── cli/                    # Terminal dimension detection and view state machine
└── tests/
    ├── scripts/
    │   └── sh/
    │       └── test.sh        # Automated test execution script
    ├── test_runner.hpp         # Lightweight zero-dependency unit test framework
    ├── test_main.cpp           # Test runner entry point
    ├── test_ring_buffer.cpp    # Ring buffer capacity, eviction, and versioning tests
    ├── test_json.cpp           # JSON parser and serializer roundtrip tests
    ├── test_fs.cpp             # Filesystem operations and traversal protection tests
    ├── test_process.cpp        # Process runner timeout, process groups, and resource tests
    └── test_http.cpp           # HTTP parser and router dispatch tests
```

---

## API Endpoints

The background HTTP server listens on port 8080 by default:

| Endpoint | Method | Description |
|---|---|---|
| `/api/session` | GET | Returns project path, masked settings, tasks, and host 50% resource limits |
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
- Docker Engine (version 24.x or later)

### Native Build Targets

Compile the engine and test runner:
```bash
make build
```

Run all automated unit tests:
```bash
./tests/scripts/sh/test.sh
# or: make test
```

Install binary into system PATH (accessible from any directory):
```bash
./scripts/sh/install.sh
# or: make install
```

Uninstall binary:
```bash
./scripts/sh/uninstall.sh
# or: make uninstall
```

Run LXACE in interactive CLI mode:
```bash
lxace
# or: ./build/lxace
```

Run in headless mode (API server only):
```bash
lxace --headless --port 8080 --project-dir /path/to/project
```

### Containerized Execution (50% Resource Cap Enforced)

Build the multi-stage unprivileged Docker image:
```bash
make docker-build
```

Execute automated unit tests inside an isolated Docker container with 50% host CPU and RAM cap:
```bash
make docker-test
```

Run LXACE API engine in container with 50% host CPU and RAM cap:
```bash
make docker-run
```

Or deploy with Docker Compose (limits configured in `docker-compose.yml`):
```bash
# Run engine
docker compose up --build

# Run test suite in container
docker compose --profile test run --rm lxace-tests
```

---

## Terminal Interface Guide

The built-in CLI adapts dynamically to window height via `TIOCGWINSZ` and hooks `SIGWINCH`:

- **Home Screen**: Displays active project path, menu options 1 to 4, and the Background Working Status feed. The status area dynamically recalculates vertical space and crops oldest lines first when the window is shortened. Updates stream in real time without screen tearing or cursor flickering.
- **Settings Screen**: Allows inspecting and updating the API Base URL, API Key (masked with asterisks), Model selection (claude-sonnet-4, claude-opus-4, gpt-4o, custom), Max Tokens, and Temperature.
- **Write Prompt**: Direct input prompt submitting tasks to the background sub-agent orchestrator.
- **Stop All**: Immediately halts all in-flight sub-agent jobs and appends confirmation logs.
- **Exit**: Cleanly unhooks signal handlers, restores terminal canonical mode, and shuts down network services.
