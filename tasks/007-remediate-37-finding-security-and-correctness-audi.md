---
id: "007"
title: "Remediate 37-finding security and correctness audit"
type: "fix"
status: "done"
created_at: "2026-11-21"
updated_at: "2026-11-21"
---

# [Fix] Remediate 37-finding security and correctness audit

## Description
Remediate every finding in `docs/audit/defect-report.md` (LX-001 to LX-037), following the
report's own four-phase remediation order. Each phase lands as its own atomic commit with a
matching regression test, so no fix ships unverified.

## Acceptance Criteria
### Phase 1: Critical containment (LX-001 to LX-005)
- [x] LX-001 bind loopback by default plus bearer-token gate before route dispatch
- [x] LX-002 restrict CORS to an explicit origin allowlist, validate `Origin` on preflight
- [x] LX-003 apply `setrlimit` hardening in the executed child
- [x] LX-004 gate `Mutating` and `Destructive` tools behind an explicit confirmation token
- [x] LX-005 add `.lxace` to every filesystem ignore list and redact secret-shaped output

### Phase 2: Correctness and I/O integrity (LX-006 to LX-017)
- [x] LX-006 move client handling onto a worker pool so long commands cannot block the API
- [x] LX-007 implement real `\uXXXX` decoding with surrogate pairs and UTF-8 encoding
- [x] LX-008 reject non-finite numbers at the parse boundary and in the writer
- [x] LX-009 reject symlinked final components in `resolve_safe_path`
- [x] LX-010 retry `waitpid` on `EINTR` and stop reporting false exit code 0
- [x] LX-011 add `truncated_stdout` and `truncated_stderr` flags to `ProcessResult`
- [x] LX-012 clear traversal error codes and continue instead of aborting
- [x] LX-013 bound `max_bytes` in `fs_view` with a hard ceiling
- [x] LX-014 check stream state in `save_to_file` and return failure
- [x] LX-015 serialize config through `JsonWriter` escaping
- [x] LX-016 write the config file with `0600` permissions atomically
- [x] LX-017 make filesystem writes atomic via temp file plus rename

### Phase 3: Agent runtime truth (LX-018 to LX-021)
- [x] LX-018 document actual sub-agent behavior, transport decision recorded
- [x] LX-019 derive `TaskState::Failed` from the real tool result
- [x] LX-020 use per-task cancellation tokens instead of a shared reset flag
- [x] LX-021 hold tasks by stable identity so trimming cannot orphan a worker

### Phase 4: Protocol and polish (LX-022 to LX-037)
- [x] LX-022 case-insensitive `Content-Length` parsing
- [x] LX-023 percent-decode query strings
- [x] LX-024 full status reason-phrase table
- [x] LX-025 always emit `Content-Length` including zero
- [x] LX-026 snapshot the SSE client list before sending, outside the lock
- [x] LX-027 invoke `remove_client` on disconnect
- [x] LX-028 tolerate spaces in the procfs `(comm)` field
- [x] LX-029 single shared CPU-cap helper, consistent rounding
- [x] LX-030 `SOCK_CLOEXEC` on the network probe socket
- [x] LX-031 validate the CLI port range
- [x] LX-032 propagate bind failure and exit non-zero
- [x] LX-033 validate `max_tokens`, `temperature`, and reject `server_port` changes
- [x] LX-034 return only the masked `base_url`
- [x] LX-035 exception-isolate event bus listeners
- [x] LX-036 align settings key bindings with printed labels and unmask non-secret input
- [x] LX-037 correct README claims and container cap drift

### Verification
- [x] Regression tests cover every phase, zero mock theater
- [x] Full suite green with zero warnings
- [x] Audit report annotated with remediation status

## Context / Notes
- Authority: `docs/audit/defect-report.md` (37 findings, 4 phases)
- Target files: `src/net/`, `src/sys/`, `src/core/`, `src/json/`, `src/agent/`, `src/tool/`, `src/cli/`
- Supersedes: task 005 (config persistence) is absorbed into Phase 2 LX-014 to LX-016
- Dependencies: none beyond the standard library and POSIX
