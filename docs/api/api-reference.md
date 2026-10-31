# LXACE REST and Server-Sent Events (SSE) API Reference

This document provides a comprehensive technical specification for the Linux AI Control Engine (LXACE) application programming interface. The LXACE core exposes a high-performance HTTP/1.1 REST interface alongside a continuous Server-Sent Events (SSE) stream, allowing desktop environments, web applications, and CLI orchestrators to interact with the engine.

The engine runtime is implemented entirely in C++20 using POSIX system calls and standard library primitives, operating with zero external third-party library dependencies.

---

## 1. Protocol Architecture & Conventions

### 1.1 Base URL and Transport
By default, the LXACE HTTP and SSE services bind to port `8080` on all available interfaces:

```text
http://127.0.0.1:8080
```

The listen port can be customized via the `--port <number>` CLI flag or configured inside `.lxace/config.json`.

### 1.2 HTTP Specification Compliance
- **HTTP Version**: HTTP/1.1
- **Server Architecture**: Non-blocking I/O event reactor backed by Linux `epoll` (`sys/epoll.h`).
- **Connection Management**: REST endpoints issue `Connection: close` by default. The SSE stream at `/api/events` maintains a persistent `Connection: keep-alive` transport.
- **Request Size Ceiling**: Maximum incoming HTTP payload is enforced at 10 MB (10,485,760 bytes). Payloads exceeding this limit are terminated with status code 400.
- **Socket Timeouts**: Socket read and write timeouts are configured to 5 seconds (`SO_RCVTIMEO` and `SO_SNDTIMEO`) to protect against slowloris attacks and socket exhaustion.

### 1.3 Cross-Origin Resource Sharing (CORS)
All HTTP responses include universal CORS headers, allowing browser-based frontend wrappers to interact with the engine from any local origin:

```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
```

### 1.4 Content Types
- Standard API endpoints: `Content-Type: application/json`
- Real-time event stream: `Content-Type: text/event-stream`

### 1.5 Standard Error Response Model
When an operation fails or a request is malformed, the engine responds with an appropriate HTTP error status code and a JSON error payload:

```json
{
  "error": "Descriptive explanation of the error condition"
}
```

Common status codes:
- `200 OK`: Request succeeded.
- `400 Bad Request`: Missing mandatory fields, invalid JSON syntax, or malformed HTTP request line.
- `403 Forbidden`: Workspace boundary violation or path traversal attempt outside the target project directory.
- `404 Not Found`: Target file or directory does not exist, or unmapped route.
- `500 Internal Server Error`: Operating system failure during execution, file writing, or socket dispatch.

---

## 2. API Endpoint Matrix

| Method | Endpoint | Description | Payload | Status Codes |
|---|---|---|---|---|
| `GET` | `/api/session` | Inspect active session, masked credentials, tasks, and 50% resource limits | None | 200 |
| `POST` | `/api/prompt` | Submit a prompt to the agent orchestrator | JSON | 200, 400 |
| `GET` | `/api/events` | Continuous Server-Sent Events (SSE) telemetry stream | None | 200 |
| `POST` | `/api/stop` | Halt all in-flight sub-agent execution workers | Optional | 200 |
| `GET` | `/api/settings` | Retrieve active LLM configuration and server settings | None | 200 |
| `POST` | `/api/settings` | Update configuration parameters in memory and on disk | JSON | 200, 400 |
| `POST` | `/api/exec` | Execute shell command in isolated process group | JSON | 200, 400 |
| `GET` | `/api/fs/list` | List directory contents within project boundary | Query param | 200, 403 |
| `POST` | `/api/fs/read` | Read text file within project boundary | JSON | 200, 400, 403, 404 |
| `POST` | `/api/fs/write` | Write or append file content within project boundary | JSON | 200, 400, 403, 500 |

---

## 3. Endpoint Specifications

### 3.1 GET `/api/session`
Retrieves comprehensive state for the current workspace session, including project path, active LLM model, masked credentials, the 50% host resource allocation policy metrics, and active agent tasks.

#### Request
```http
GET /api/session HTTP/1.1
Host: 127.0.0.1:8080
```

#### Query Parameters
None.

#### Response Schema (`200 OK`)
```json
{
  "project_dir": "/home/user/space/project/my-app",
  "model": "claude-sonnet-4",
  "base_url": "https://api.anthropic.com",
  "base_url_masked": "https://api.anthropic.com",
  "api_key_masked": "sk-ant-****************************************",
  "resource_limits": {
    "host_cpu_cores": 8,
    "cpu_50_percent_cap": 4,
    "host_total_memory_bytes": 7340032000.0,
    "memory_50_percent_cap_bytes": 3670016000.0
  },
  "tasks": [
    {
      "id": "task-1",
      "prompt": "Inspect directory structure",
      "state": "Running",
      "assigned_agent": "Sub-agent A"
    }
  ]
}
```

#### Field Details
- `project_dir` (string): Absolute path to the sandboxed project root.
- `model` (string): Configured LLM identifier.
- `base_url` (string): Target API endpoint URL.
- `base_url_masked` (string): Masked version of the base URL for safe UI rendering.
- `api_key_masked` (string): Masked token displaying prefix and concealing secrets with asterisks.
- `resource_limits` (object): System resource ceiling enforced by LXACE runtime:
  - `host_cpu_cores` (integer): Total detected logical CPU cores on host OS.
  - `cpu_50_percent_cap` (integer): Maximum allowed CPU cores (50% host cap, minimum 1).
  - `host_total_memory_bytes` (number): Total host RAM read from `/proc/meminfo`.
  - `memory_50_percent_cap_bytes` (number): 50% memory limit in bytes.
- `tasks` (array of objects): Task objects currently queued or executing in the orchestrator.
  - `id` (string): Unique task identifier (e.g. `task-1`).
  - `prompt` (string): Prompt text assigned to the worker.
  - `state` (string): One of `Pending`, `Running`, `Succeeded`, `Failed`, `Cancelled`.
  - `assigned_agent` (string): Sub-agent worker designation.

#### Example Command
```bash
curl -X GET http://127.0.0.1:8080/api/session
```

---

### 3.2 POST `/api/prompt`
Dispatches a new task prompt to the agent orchestrator. The prompt is assigned an incremental task ID, pushed to the active task list, and assigned to a background worker.

#### Request
```http
POST /api/prompt HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json

{
  "prompt": "Analyze test coverage across the repository"
}
```

#### Request Body Schema
- `prompt` (string, required): The task instruction to execute.

#### Response (`200 OK`)
```json
{
  "task_id": "task-2",
  "status": "queued"
}
```

#### Error Responses
- `400 Bad Request`: If the JSON payload cannot be parsed or lacks the `prompt` key.
  ```json
  {
    "error": "Missing prompt field"
  }
  ```

#### Example Command
```bash
curl -X POST http://127.0.0.1:8080/api/prompt \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Run unit tests and check results"}'
```

---

### 3.3 GET `/api/events` (Server-Sent Events)
Establishes a persistent, real-time HTTP event stream adhering to W3C Server-Sent Events (RFC 8895). The stream broadcasts sub-agent telemetry, command execution logs, and system notices.

#### Request
```http
GET /api/events HTTP/1.1
Host: 127.0.0.1:8080
Accept: text/event-stream
```

#### Response Headers (`200 OK`)
```http
HTTP/1.1 200 OK
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
Access-Control-Allow-Origin: *
```

#### Stream Handshake
Upon initial socket handshake, the server emits an SSE comment frame to confirm connection establishment:
```text
: connected

```

#### Wire Format
Each event is serialized as a JSON object on the `data:` line followed by two newline characters (`\n\n`):

```text
data: {"agent_id":"Sub-agent A","details":"Executing command: make test","id":"evt-102","kind":"Action","text":"Running test suite","time":"18:42:01"}

```

#### Payload Properties
- `id` (string): Event identifier.
- `agent_id` (string): Originating sub-agent identifier (e.g. `Sub-agent A`).
- `kind` (string): Event classification category:
  - `Insight`: Reasoning deduction, architectural decision, or plan.
  - `Action`: File modification, execution command, or tool invocation.
  - `Message`: High-level user notification or workflow update.
  - `CommandOutput`: Incremental stdout or stderr data captured from processes.
  - `System`: Internal engine status, lifecycle hook, or error report.
- `text` (string): Primary log text message.
- `details` (string): Extended metadata, parameters, or full command invocation line.
- `time` (string): Wall-clock timestamp formatted as `HH:MM:SS`.

#### Capacity and Client Management
- The stream manager supports up to 128 concurrent SSE client connections.
- When the 128-client limit is reached, the oldest client socket is gracefully closed to accommodate incoming listeners and prevent file descriptor starvation.
- Socket writes utilize the `MSG_NOSIGNAL` flag to prevent SIGPIPE termination when remote clients disconnect abruptly. Broken sockets are automatically detected, closed, and purged from the client registry.

#### Example Command
```bash
curl -N -H "Accept: text/event-stream" http://127.0.0.1:8080/api/events
```

#### JavaScript Client Example
```javascript
const eventSource = new EventSource("http://127.0.0.1:8080/api/events");

eventSource.onopen = () => {
  console.log("Connected to LXACE real-time stream");
};

eventSource.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(`[${data.time}] [${data.kind}] ${data.agent_id}: ${data.text}`);
};

eventSource.onerror = (err) => {
  console.error("SSE connection error:", err);
};
```

---

### 3.4 POST `/api/stop`
Issues an immediate cancellation signal to the sub-agent orchestrator. All running worker threads are flagged for termination, child processes are halted, and queued tasks transition to `Cancelled` state.

#### Request
```http
POST /api/stop HTTP/1.1
Host: 127.0.0.1:8080
```

#### Request Body
None required (empty body or `{}`).

#### Response (`200 OK`)
```json
{
  "status": "all tasks stopped"
}
```

#### Example Command
```bash
curl -X POST http://127.0.0.1:8080/api/stop
```

---

### 3.5 GET `/api/settings`
Retrieves current configuration parameters stored in memory and persisted in `.lxace/config.json`.

#### Request
```http
GET /api/settings HTTP/1.1
Host: 127.0.0.1:8080
```

#### Response (`200 OK`)
```json
{
  "base_url": "https://api.anthropic.com",
  "base_url_masked": "https://api.anthropic.com",
  "api_key_masked": "sk-ant-****************************************",
  "model": "claude-sonnet-4",
  "max_tokens": 4096,
  "temperature": 0.2,
  "server_port": 8080
}
```

#### Example Command
```bash
curl -X GET http://127.0.0.1:8080/api/settings
```

---

### 3.6 POST `/api/settings`
Updates engine configuration fields. Supports partial updates: any omitted keys retain their existing configured values. Persists modified configuration to `.lxace/config.json`.

#### Request
```http
POST /api/settings HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json

{
  "model": "claude-opus-4",
  "temperature": 0.1,
  "max_tokens": 8192
}
```

#### Updatable Fields
- `base_url` (string, optional): Endpoint URL for API inference.
- `api_key` (string, optional): API token or secret key.
- `model` (string, optional): Target LLM model name.
- `max_tokens` (integer, optional): Maximum response token budget.
- `temperature` (number, optional): Sampling temperature between 0.0 and 1.0.

#### Response (`200 OK`)
```json
{
  "status": "updated"
}
```

#### Error Responses
- `400 Bad Request`: If the request body contains invalid JSON.
  ```json
  {
    "error": "Invalid JSON"
  }
  ```

#### Example Command
```bash
curl -X POST http://127.0.0.1:8080/api/settings \
  -H "Content-Type: application/json" \
  -d '{"model": "claude-sonnet-4", "temperature": 0.3}'
```

---

### 3.7 POST `/api/exec`
Executes a native Linux command within the sandboxed project directory. Commands execute under isolated POSIX process groups (`setpgid`), ensuring complete subtree process cleanup upon termination or timeout.

#### Security & Execution Rules
- Working directory is anchored to `project_dir`.
- Spawns in an isolated process group.
- Hard timeout enforced at 30 seconds. If execution exceeds 30 seconds, `kill(-pid, SIGKILL)` terminates the process group and marks `timed_out: true`.
- Standard output and standard error are captured via separate UNIX pipes.

#### Request
```http
POST /api/exec HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json

{
  "command": "git status --short"
}
```

#### Request Body Schema
- `command` (string, required): Shell command line to execute.

#### Response (`200 OK`)
```json
{
  "exit_code": 0,
  "stdout": " M src/core/engine.cpp\n?? docs/api/api-reference.md\n",
  "stderr": "",
  "timed_out": false,
  "duration_ms": 14
}
```

#### Field Details
- `exit_code` (integer): Linux exit status code returned by `waitpid()`.
- `stdout` (string): Captured standard output stream.
- `stderr` (string): Captured standard error stream.
- `timed_out` (boolean): `true` if execution exceeded 30 seconds and was killed.
- `duration_ms` (integer): Elapsed execution time in milliseconds.

#### Error Responses
- `400 Bad Request`: Missing `command` field in request body.
  ```json
  {
    "error": "Missing command field"
  }
  ```

#### Example Command
```bash
curl -X POST http://127.0.0.1:8080/api/exec \
  -H "Content-Type: application/json" \
  -d '{"command": "make test"}'
```

---

### 3.8 GET `/api/fs/list`
Lists files and directories located inside the target workspace. Enforces project boundary security using canonical path verification (`resolve_safe_path`).

#### Security Boundary Enforcement
Any relative path that escapes the project root (e.g. `../../etc/passwd` or symbolic links pointing outside `project_dir`) is immediately rejected with HTTP status code `403 Forbidden`.

#### Request
```http
GET /api/fs/list?path=src/core HTTP/1.1
Host: 127.0.0.1:8080
```

#### Query Parameters
- `path` (string, optional): Relative directory path within the project root. If omitted, lists the project root directory.

#### Response (`200 OK`)
```json
[
  {
    "name": "engine.cpp",
    "path": "src/core/engine.cpp",
    "is_directory": false,
    "size": 9128.0,
    "permissions": 420
  },
  {
    "name": "engine.hpp",
    "path": "src/core/engine.hpp",
    "is_directory": false,
    "size": 2450.0,
    "permissions": 420
  }
]
```

#### Field Details
- `name` (string): Entry file or folder name.
- `path` (string): Relative path from project directory root.
- `is_directory` (boolean): `true` if directory, `false` if regular file.
- `size` (number): Size in bytes.
- `permissions` (integer): POSIX permission bitmask (e.g. 420 for octal 0644).

#### Error Responses
- `403 Forbidden`: Attempted directory traversal outside the project directory.
  ```json
  {
    "error": "Access denied: path outside project directory"
  }
  ```

#### Example Command
```bash
# List project root
curl -X GET http://127.0.0.1:8080/api/fs/list

# List subdirectory
curl -X GET "http://127.0.0.1:8080/api/fs/list?path=src/core"
```

---

### 3.9 POST `/api/fs/read`
Reads the contents of a UTF-8 text file within the workspace boundary.

#### Request
```http
POST /api/fs/read HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json

{
  "path": "CMakeLists.txt"
}
```

#### Request Body Schema
- `path` (string, required): Relative path to the target file.

#### Response (`200 OK`)
```json
{
  "path": "/home/user/space/project/my-app/CMakeLists.txt",
  "content": "cmake_minimum_required(VERSION 3.20)\nproject(lxace CXX)\nset(CMAKE_CXX_STANDARD 20)\n..."
}
```

#### Error Responses
- `400 Bad Request`: Missing `path` field.
  ```json
  {
    "error": "Missing path field"
  }
  ```
- `403 Forbidden`: Path traversal attempt escaping the project workspace.
  ```json
  {
    "error": "Access denied: path outside project directory"
  }
  ```
- `404 Not Found`: File does not exist or cannot be opened for reading.
  ```json
  {
    "error": "File not found or unreadable"
  }
  ```

#### Example Command
```bash
curl -X POST http://127.0.0.1:8080/api/fs/read \
  -H "Content-Type: application/json" \
  -d '{"path": "Makefile"}'
```

---

### 3.10 POST `/api/fs/write`
Writes or appends UTF-8 string content to a file within the project boundary. If parent directories do not exist, they are created automatically.

#### Request
```http
POST /api/fs/write HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json

{
  "path": "build_notes.txt",
  "content": "Build verified on Linux 6.x.\n",
  "append": false
}
```

#### Request Body Schema
- `path` (string, required): Relative path to the destination file.
- `content` (string, required): String content to write.
- `append` (boolean, optional): If `true`, appends to the file; if `false` or omitted, truncates and overwrites.

#### Response (`200 OK`)
```json
{
  "status": "written"
}
```

#### Error Responses
- `400 Bad Request`: Missing `path` or `content` parameter.
  ```json
  {
    "error": "Missing path or content field"
  }
  ```
- `403 Forbidden`: Path traversal attempt outside project workspace.
  ```json
  {
    "error": "Access denied: path outside project directory"
  }
  ```
- `500 Internal Server Error`: Operating system failure writing to file descriptor.
  ```json
  {
    "error": "Failed to write file"
  }
  ```

#### Example Command
```bash
# Overwrite mode
curl -X POST http://127.0.0.1:8080/api/fs/write \
  -H "Content-Type: application/json" \
  -d '{"path": "test.txt", "content": "Hello World\n", "append": false}'

# Append mode
curl -X POST http://127.0.0.1:8080/api/fs/write \
  -H "Content-Type: application/json" \
  -d '{"path": "test.txt", "content": "Second Line\n", "append": true}'
```

---

## 4. Resource Governance & 50% Cap Policy

LXACE implements a strict 50% host resource ceiling policy to prevent interference with host OS stability, development tools, and user interaction.

### 4.1 System Metric Probing
The engine queries `/proc/cpuinfo` and `/proc/meminfo` via the native `ProcfsReader` system interface:
- **CPU Cores**: Discovered by counting processor entries in `/proc/cpuinfo`.
  - Cap calculation: `cpu_50_percent_cap = max(1, host_cpu_cores / 2)`
- **Memory**: Read directly from `MemTotal` in `/proc/meminfo`.
  - Cap calculation: `memory_50_percent_cap_bytes = host_total_memory_bytes / 2`

These metrics are reported in `GET /api/session` under the `resource_limits` object.

### 4.2 Container Enforcement
For Docker deployments, the container limits in `docker-compose.yml` reflect this policy:
```yaml
deploy:
  resources:
    limits:
      cpus: '4.0'
      memory: 3500M
```

---

## 5. Security & Boundary Architecture

### 5.1 Project Boundary Path Resolution
Every filesystem interaction (`/api/fs/list`, `/api/fs/read`, `/api/fs/write`) routes through `sys::FsEngine::resolve_safe_path`:
1. Strips leading slashes from requested relative paths.
2. Constructs the combined path `project_dir / rel_path`.
3. Evaluates canonical paths using `std::filesystem::canonical` and `std::filesystem::weakly_canonical`.
4. Compares canonical prefix against canonical `project_dir`.
5. Rejects any path with prefix mismatch, preventing directory traversal via `..` or external symbolic links.

### 5.2 Process Group Isolation
Subprocess execution via `/api/exec` utilizes POSIX `setpgid(0, 0)`:
- Isolates child processes from the engine process group.
- Enables complete subtree process cleanup via `kill(-pid, SIGKILL)`.
- Eliminates orphaned child processes, daemon leaks, or zombie build runners.

### 5.3 Credential Masking
API keys and authentication credentials are treated as sensitive data:
- `api_key_masked` retains the first 7 characters followed by asterisks.
- Base URLs containing embedded tokens are scrubbed before serialization.
- Engine logs and SSE telemetry never broadcast raw API tokens.

---

## 6. End-to-End Client Integration Examples

### 6.1 Python Integration Script
```python
import requests
import json

BASE_URL = "http://127.0.0.1:8080"

# 1. Inspect session and resource limits
session_res = requests.get(f"{BASE_URL}/api/session")
session = session_res.json()
print(f"Project: {session['project_dir']}")
print(f"CPU Cap: {session['resource_limits']['cpu_50_percent_cap']} cores")

# 2. Dispatch a prompt
prompt_res = requests.post(
    f"{BASE_URL}/api/prompt",
    headers={"Content-Type": "application/json"},
    data=json.dumps({"prompt": "Check code structure"})
)
print(f"Dispatched task: {prompt_res.json()['task_id']}")

# 3. Execute a shell command
exec_res = requests.post(
    f"{BASE_URL}/api/exec",
    headers={"Content-Type": "application/json"},
    data=json.dumps({"command": "ls -la"})
)
print(f"Exit code: {exec_res.json()['exit_code']}")
print(f"Output:\n{exec_res.json()['stdout']}")
```

### 6.2 Node.js Full-Stack Client
```javascript
const http = require("http");

async function runClient() {
  const baseUrl = "http://127.0.0.1:8080";

  // Check session
  const sessionRes = await fetch(`${baseUrl}/api/session`);
  const sessionData = await sessionRes.json();
  console.log("Connected to LXACE session:", sessionData.project_dir);

  // Read file safely
  const readRes = await fetch(`${baseUrl}/api/fs/read`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ path: "README.md" })
  });

  if (readRes.ok) {
    const fileData = await readRes.json();
    console.log(`README.md length: ${fileData.content.length} characters`);
  }
}

runClient().catch(console.error);
```
