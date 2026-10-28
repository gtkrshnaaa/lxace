# Product Requirements Document (PRD)

**Product Name:** Linux AI Control Engine (LXACE)  
**Version:** 1.0  
**Date:** 2026-10-29  
**Status:** Draft

---

## 1. Overview

LXACE is a C++-based AI control engine designed exclusively for Linux. It provides deep, native control over the Linux environment through its own APIs and functions rather than relying primarily on bash. The engine exposes a rich set of API endpoints for external wrappers (desktop apps, web apps, and custom CLIs) while also including a built-in menu-based CLI for direct use.

The system operates in a single-project mode: it is launched inside a project folder and focuses all work there. Internally it may use multiple sub-agents, but from the user’s perspective it remains one focused workspace.

---

## 2. Goals

- Deliver a high-performance C++ engine that can control Linux broadly and reliably.
- Provide native Linux operations (file system, process, network, permissions, packages, etc.) via dedicated functions/APIs so agents with limited bash skills can still be effective.
- Expose a comprehensive API surface for wrappers (desktop, web, custom CLI).
- Ship a clean, menu-based CLI interface that works without any wrapper.
- Support real-time visibility into agent and sub-agent activity, including insights and messages.
- Keep the CLI responsive and adaptive to terminal size.

## 3. Non-Goals

- Cross-platform support (Windows/macOS) in v1.
- Multi-workspace / multi-project switching inside a single session.
- Full TUI framework with mouse support or complex widgets (plain terminal text is sufficient).
- Replacing a full IDE.
- Any external library or third-party dependency.

---

## 4. Target Users

- Developers who want a local AI control engine tightly integrated with Linux.
- Other AI coding agents / tools that need a reliable Linux execution backend.
- Teams building desktop or web wrappers on top of the engine.
- Users who prefer a simple menu-driven CLI over a REPL or free-form shell.

---

## 5. Architecture Overview

```
┌─────────────────┐  ┌──────────────┐  ┌─────────────────┐
│  Desktop App    │  │   Web App    │  │  Custom CLI     │
│  (wrapper)      │  │  (wrapper)   │  │  (wrapper)      │
└────────┬────────┘  └──────┬───────┘  └────────┬────────┘
         │                  │                   │
         └──────────────────┼───────────────────┘
                            ▼
              ┌─────────────────────────┐
              │   LXACE Engine (C++)    │
              │                         │
              │  - Rich API endpoints   │
              │  - Native Linux funcs   │
              │  - Sub-agent runtime    │
              │  - Built-in menu CLI    │
              └─────────────────────────┘
```

- **Engine**: Core logic, session state, sub-agent orchestration, native Linux operations.
- **API Layer**: HTTP/WebSocket (or similar) endpoints consumed by wrappers.
- **Built-in CLI**: Menu-based interface that talks to the same engine.

---

## 6. Tech Stack & Constraints

- **Language**: C++ (C++17 or later)
- **Dependencies**: **None**. Strictly no external libraries, frameworks, or third-party packages.
- **Allowed**: C++ Standard Library only + direct Linux/POSIX system calls.
- **Networking**: Implemented manually using raw sockets or Linux-native interfaces (no libcurl, no Boost.Asio, no external HTTP libraries).
- **JSON / Serialization**: Hand-rolled or minimal stdlib-based implementation.
- **Build System**: Pure Makefile or CMake with zero external dependencies.
- **Threading**: `std::thread` and standard synchronization primitives only.
- **File & Process Control**: Direct syscalls (`open`, `read`, `write`, `fork`, `exec`, `waitpid`, etc.) or their C++ stdlib equivalents where sufficient.

This constraint exists to keep the engine fully self-contained, auditable, and free of supply-chain risk.

---

## 7. Functional Requirements

### 7.1 Engine Core
- Single-project focus: launched in a directory and works only inside that project tree.
- Sub-agent support: multiple internal agents can collaborate; their activity is aggregated into one status stream.
- Persistent session state for the duration of the process.
- Streaming of real-time events (actions, insights, messages, command output).

### 7.2 Native Linux Capabilities
The engine must provide its own functions/APIs (not only bash) for at least:
- File system: read, write, create, delete, move, copy, list, watch, permissions, ownership.
- Process management: spawn, kill, signal, list, resource usage.
- Command execution with structured capture (stdout/stderr/exit code) and truncation support.
- Package management helpers (where safe and appropriate).
- Network utilities (basic connectivity checks, port status).
- User/group and permission operations.
- Environment and systemd interaction (limited, safe subset).

These native functions exist so agents with weak bash skills can still perform complex operations reliably.

### 7.3 API Endpoints
A comprehensive set of endpoints must be exposed for wrappers, including but not limited to:
- Session / project info
- Prompt submission
- Real-time event stream (WebSocket or SSE)
- File operations
- Process / command execution
- Settings management
- Status and log retrieval
- Stop / cancel operations

### 7.4 Built-in Menu-Based CLI
The CLI is a first-class interface. It must be fully usable without any wrapper.

---

## 8. CLI Interface Specification

All screens use plain text (no box-drawing borders). The layout adapts to terminal height. The background status area occupies all remaining vertical space and crops oldest lines first when the window is shortened.

### 8.1 Home — Tall Terminal
```
=== LXACE ===
Project: /home/user/my-app

1. Settings
2. Write Prompt
3. Stop All
4. Exit

────────────────────────────────────────
BACKGROUND WORKING STATUS
[12:34:01] Sub-agent A: Reading src/auth.js
[12:34:03] Insight: The token validation logic is currently duplicated 
           across login, refresh, and middleware. This makes it easy 
           to introduce inconsistencies when one of them is updated.
[12:34:05] Sub-agent B: Editing src/middleware/auth.js
           extracting validateToken() into a shared utility so all 
           three places call the same function.
[12:34:08] Running: npm test -- --coverage
[12:34:12] Message: All 42 tests passed. Coverage on the auth module 
           went from 71% to 89%. The remaining uncovered lines are 
           the error paths for expired refresh tokens.
[12:34:15] Sub-agent A: Creating src/utils/token.js
           (new shared validation helper, 38 lines)
[12:34:18] Sub-agent C: Reading tests/auth.test.js
[12:34:20] Insight: Missing test case for malformed JWT payload.
────────────────────────────────────────

choose menu: 
```

### 8.2 Home — Short Terminal
```
=== LXACE ===
Project: /home/user/my-app

1. Settings
2. Write Prompt
3. Stop All
4. Exit

────────────────────────────────────────
BACKGROUND WORKING STATUS
[12:34:12] Message: All 42 tests passed. Coverage on the auth module 
           went from 71% to 89%. The remaining uncovered lines are 
           the error paths for expired refresh tokens.
[12:34:15] Sub-agent A: Creating src/utils/token.js
[12:34:18] Sub-agent C: Reading tests/auth.test.js
[12:34:20] Insight: Missing test case for malformed JWT payload.
────────────────────────────────────────

choose menu: 
```

### 8.3 Settings
```
=== SETTINGS ===

1. Base URL
2. API Key
3. Model
4. Max Tokens
5. Temperature
6. Back

choose menu: 
```

### 8.4 Settings > Base URL
```
=== SETTINGS > BASE URL ===

Current: https://api.example.com/v1

Enter new base URL (empty to cancel):
: 
```

### 8.5 Settings > API Key
```
=== SETTINGS > API KEY ===

Current: sk-****************************ab12

Enter new API key (empty to cancel):
: 
```

### 8.6 Settings > Model
```
=== SETTINGS > MODEL ===

Current: claude-sonnet-4

1. claude-sonnet-4
2. claude-opus-4
3. gpt-4o
4. custom
5. Back

choose menu: 
```

### 8.7 Settings > Max Tokens
```
=== SETTINGS > MAX TOKENS ===

Current: 8192

Enter new value (empty to cancel):
: 
```

### 8.8 Settings > Temperature
```
=== SETTINGS > TEMPERATURE ===

Current: 0.3

Enter new value 0.0 - 1.0 (empty to cancel):
: 
```

### 8.9 Write Prompt
```
write your prompt here.
: 
```
After submission the CLI returns to Home and the background status begins updating in real time.

### 8.10 Stop All
Direct action. Returns to Home and appends log lines such as:
```
[12:35:01] Stopping all sub-agents...
[12:35:02] All tasks stopped.
```

### 8.11 Exit
Terminates the process cleanly.

---

## 9. Non-Functional Requirements

- **Performance**: Low latency for native Linux operations; responsive CLI even while sub-agents are active.
- **Reliability**: Safe defaults for destructive operations; clear error reporting.
- **Adaptivity**: CLI must correctly reflow and crop the status area on terminal resize.
- **Observability**: All significant agent actions, insights, and messages must appear in the status stream.
- **Security**: API keys and sensitive settings must be masked in the CLI; file operations respect Linux permissions.
- **Zero Dependencies**: The entire codebase must compile and run using only the C++ standard library and direct Linux system calls.

---

## 10. Success Metrics

- Engine can perform common Linux file/process operations without falling back to bash for agents with limited shell skills.
- Wrappers can drive the full agent lifecycle solely through the API.
- Built-in CLI is fully usable and shows real-time activity without requiring external tools.
- Status area remains readable and correctly cropped across typical terminal sizes.
- Project builds and runs with zero external dependencies.

---

## 11. Open Questions / Future Considerations

- Exact transport for the API (REST + WebSocket vs gRPC) — must be implemented without external libraries.
- Persistence of settings and session history across restarts.
- Additional native Linux capability priorities.
- Optional non-interactive / scriptable mode for the CLI later.

---

