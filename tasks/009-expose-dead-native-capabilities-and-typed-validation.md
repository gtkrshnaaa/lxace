# 009 - Expose dead native capabilities and typed tool validation

- **Type**: feature
- **Status**: in-progress
- **Created**: 2026-11-21
- **Origin**: capability audit against the goal "Linux controlling engine using AI"

## Problem

The engine has a hardened execution substrate, but six native capabilities are
implemented and unreachable, two modules are entirely dead code, and tool
argument validation only checks key presence. An agent driving this engine
cannot mutate the filesystem, signal processes, list processes, probe the
network, or watch files, and receives no actionable feedback when it sends a
malformed tool call.

## Scope

Expose the already-written native layer behind the existing risk gate, and make
validation typed. No new third-party dependencies.

## Acceptance criteria

- [x] `fs_mutate` exposes remove, copy, mkdir, chmod, chown through the audited `FsEngine` API
- [x] `fs_mutate` is registered `Destructive` so the `428` confirmation gate applies
- [x] `fs_mutate` rejects any path outside the project root, including symlink escapes
- [x] `proc_list` reports per-PID RSS, state, and CPU ticks from `ProcfsReader`
- [x] `proc_signal` uses an allowlist that omits SIGKILL and refuses to signal the engine's own pid
- [x] `net_probe` exposes port-in-use and TCP reachability from `NetworkUtils`
- [x] `fs_watch` bounds observation with a hard timeout and returns collected events
- [x] `ToolRegistry::validate_arguments` rejects wrong types and values outside a declared enum
- [x] Validation error payload names the offending parameter and the expected type
- [x] Every new tool has a positive and a rejection test in `tests/`
- [x] `cmake --build build -j 1` is clean under the strict warning set
- [x] `./build/lxace_tests` reports 63 passed, 0 failed
- [x] `bash scripts/sh/api-smoke.sh` reports 36 passed, 0 failed against a live server

## Verification evidence

- Build: zero warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast`.
- Unit suite: 63 passed, 0 failed (16 new tests in `tests/test_capability_tools.cpp`).
- Live harness: 36 passed, 0 failed, including `428` gating and path-escape rejection.
- Defect found during verification: jq's `//` operator treats `false` as empty, so the
  pre-existing `.success // "missing"` accessor reported every real failure as `missing`.
  Replaced with an explicit `success_flag` helper used at all eleven call sites.

## Out of scope

The LLM reasoning loop (audit Gap 1) requires an outbound transport that does
not exist yet. Tracked separately.

## Progress log

- Task created from the capability audit result.
