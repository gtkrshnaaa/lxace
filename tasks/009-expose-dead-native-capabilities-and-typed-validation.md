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

- [ ] `fs_mutate` exposes remove, copy, mkdir, chmod, chown through the audited `FsEngine` API
- [ ] `fs_mutate` is registered `Destructive` so the `428` confirmation gate applies
- [ ] `fs_mutate` rejects any path outside the project root, including symlink escapes
- [ ] `proc_list` reports per-PID RSS, state, and CPU ticks from `ProcfsReader`
- [ ] `proc_signal` supports a safe signal allowlist and refuses SIGKILL/SIGTERM on the engine's own pid tree guard
- [ ] `net_probe` exposes port-in-use and TCP reachability from `NetworkUtils`
- [ ] `fs_watch` bounds observation with a hard timeout and returns collected events
- [ ] `ToolRegistry::validate_arguments` rejects wrong types and out-of-range numbers with a structured error
- [ ] Validation error payload names the offending parameter and the expected type
- [ ] Every new tool has a positive and a rejection test in `tests/`
- [ ] `cmake --build build -j 1` is clean under the strict warning set
- [ ] `./build/lxace_tests` is green with all new tests
- [ ] `bash scripts/sh/api-smoke.sh` remains green against a live server

## Out of scope

The LLM reasoning loop (audit Gap 1) requires an outbound transport that does
not exist yet. Tracked separately.

## Progress log

- Task created from the capability audit result.
