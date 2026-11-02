---
id: "004"
title: "Implement AI tool calling runtime and native tool suite"
type: "feature"
status: "done"
created_at: "2026-11-02"
updated_at: "2026-11-02"
---

# [Feature] Implement AI tool calling runtime and native tool suite

## Description
Build a production-grade C++20 tool calling subsystem for AI models and agents in LXACE, including standard schema generation, unified execution dispatcher, native surgical file editing (fs_view, fs_replace), native file and content search (fs_find, fs_grep), process execution, REST endpoints, and sub-agent telemetry integration.

## Acceptance Criteria
- [x] Create `src/tool/tool_definition.hpp` and `src/tool/tool_registry.hpp` with standardized JSON schemas and execution envelopes
- [x] Implement core native tools: `fs_view`, `fs_replace`, `fs_find`, `fs_grep`, `proc_execute`, and `sys_metrics`
- [x] Register `GET /api/tools` and `POST /api/tools/execute` in `src/core/engine.cpp` with input validation
- [x] Integrate tool registry into `src/agent/sub_agent.cpp` for native action dispatching and telemetry
- [x] Add comprehensive unit test suite in `tests/test_tools.cpp` covering schemas, validation, surgical replacement, and execution boundaries
- [x] Verify test suite passes 100% (`lxace_tests` passing without regressions)
- [x] Update `docs/api/api-reference.md` documenting new tool endpoints and schemas
- [x] Verify containerized build (`make build`) and clean git state

## Context / Notes
- Strict zero external dependencies (C++20 stdlib and Linux POSIX syscalls only)
- Strictly enforce 50% CPU/RAM cap and project boundary sandboxing
