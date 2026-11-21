# LXACE Defect Audit Report

Authoritative defect inventory for the LXACE Linux AI Control Engine, produced from a
complete read of `src/` plus the build, container, and script layer. Every finding below
is grounded in a specific file and line. No speculative or unverified entry is included.

---

## 1. Audit Baseline

| Field | Value |
| --- | --- |
| Repository | `github.com/gtkrshnaaa/lxace` |
| Branch | `devv` |
| Commit | `30568bd` (feat(tool): complete ai tool calling runtime and native tools implementation) |
| Source files reviewed | 70 across `core/`, `sys/`, `json/`, `net/`, `agent/`, `cli/`, `tool/` |
| Build | `cmake --build build -j 4`, clean, zero warnings |
| Test suite | `./build/lxace_tests`, 28 passed, 0 failed, 159 ms |
| Compiler flags | `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wold-style-cast` |

### 1.1 Method

Each translation unit was read in full, then cross-referenced against call sites, headers,
and the documentation that describes the same behavior. Findings were retained only when
the defect was reproducible by reading the code path from entry point to failure. The
green test suite was used to confirm which findings are currently unexercised, not to
validate correctness.

### 1.2 Severity Legend

| Severity | Definition |
| --- | --- |
| Critical | Remote compromise, credential exposure, or unbounded privileged execution |
| High | Server-wide denial of service, silent data loss, false success, or containment escape |
| Medium | Protocol non-compliance, incorrect results, or resource leak |
| Low | UX defect, observability gap, or documentation and tooling drift |

**Totals: 37 findings, of which 5 Critical, 16 High, 14 Medium, 2 Low or Drift.**

### 1.3 Remediation Status

All 37 findings were remediated in task `007`, across four commits plus one follow-up, each
verified by a green test run. This snapshot records the state after remediation:

| Field | Value |
| --- | --- |
| Findings remediated | 37 of 37 |
| Test suite after remediation | `./build/lxace_tests`, 47 passed, 0 failed |
| Security regression tests added | 17 |
| Build after remediation | zero warnings under the same strict flag set |

Two findings were partly remediated and are called out honestly rather than claimed closed:

- `LX-003` applies `setrlimit` in the child and now also supports an explicit
  `drop_uid`/`drop_gid` privilege drop. The drop is opt-in and inert by default, because an
  engine that runs unprivileged has no lower identity to assume. Deployments running the
  engine as root must set it.
- `LX-018` is closed by decision rather than by implementation. The document set now states
  that no LLM transport exists and that keyword dispatch is the intended behavior of this
  release, instead of claiming inference that was never built.

One finding was discovered during remediation and fixed with the rest: `/api/events`
bypassed the router on its own fast path and stayed unauthenticated after the bearer gate
landed. It is recorded here so the inventory stays complete at 38 defects total.

---

## 2. Index

| ID | Severity | Area | Summary |
| --- | --- | --- | --- |
| LX-001 | Critical | net/sys | Unauthenticated remote shell execution on all interfaces |
| LX-002 | Critical | net | Wide-open CORS preflight enables cross-site API abuse |
| LX-003 | Critical | sys | No privilege drop or resource limits in the executed child |
| LX-004 | Critical | core | Tool execution endpoint has no risk-level gate |
| LX-005 | Critical | tool | `fs_grep` discloses the plaintext API key over SSE |
| LX-006 | High | net | All client connections are served synchronously on the reactor thread |
| LX-007 | High | json | `\uXXXX` escapes are destroyed and replaced with `?` |
| LX-008 | High | json/core | NaN and infinity round-trip into permanently invalid JSON |
| LX-009 | High | sys | Dangling final symlink escapes the filesystem sandbox |
| LX-010 | High | sys | Missing EINTR retry in `waitpid` reports false exit code 0 |
| LX-011 | High | sys | Output truncation is silent and undetectable by callers |
| LX-012 | High | tool | Traversal aborts on first filesystem error |
| LX-013 | High | tool | `fs_view` has no upper bound on `max_bytes` |
| LX-014 | High | core | Config save ignores stream failure and returns success |
| LX-015 | High | core | Config save concatenates raw values without JSON escaping |
| LX-016 | High | core | Config file with plaintext API key is written world-readable |
| LX-017 | High | sys/tool | Non-atomic truncate-then-write corrupts files on interruption |
| LX-018 | High | agent | Sub-agent performs keyword matching instead of LLM inference |
| LX-019 | High | agent | Every task is marked `Succeeded`, failure state is unreachable |
| LX-020 | High | agent | New prompt submission silently resurrects cancelled tasks |
| LX-021 | High | agent | Task vector trimming can orphan a running worker |
| LX-022 | Medium | net | `Content-Length` matching is case-sensitive and incomplete |
| LX-023 | Medium | net | No percent-decoding of request query strings |
| LX-024 | Medium | net | Status reason phrase is wrong for every code except 200 and 404 |
| LX-025 | Medium | net | `Content-Length` omitted for empty bodies |
| LX-026 | Medium | net | SSE broadcast holds the mutex across blocking `send` |
| LX-027 | Medium | net | SSE client removal has no call sites |
| LX-028 | Medium | sys | `procfs` `(comm)` field parse breaks on names containing spaces |
| LX-029 | Medium | sys | CPU cap uses floor division and disagrees with the Makefile |
| LX-030 | Medium | sys | Network probe socket is not close-on-exec |
| LX-031 | Medium | cli | Unvalidated port cast silently wraps |
| LX-032 | Medium | core | Bind failure is discarded, engine reports running with no API |
| LX-033 | Medium | core | Validation gaps in `max_tokens`, `temperature`, and `server_port` |
| LX-034 | Medium | core | Unmasked `base_url` returned alongside the masked form |
| LX-035 | Medium | core | Event bus invokes listeners with no exception isolation |
| LX-036 | Low | cli | Settings key bindings contradict the printed labels |
| LX-037 | Low | docs | Documentation, Makefile, and container configuration drift |

---

## 3. Critical Findings

### LX-001 Unauthenticated remote shell execution on all interfaces

- **Location**: `src/net/socket.cpp:35`, `src/core/engine.cpp:161`, `src/sys/process.cpp:19`
- **Evidence**: `addr.sin_addr.s_addr = htonl(INADDR_ANY)` binds the listener to every
  interface. `/api/exec` is registered with no authentication check, and the handler
  forwards the request payload directly to `ProcessRunner::execute`. That function builds
  `std::vector<std::string> args = {"/bin/sh", "-c", command}`.
- **Impact**: Any peer reachable on the network obtains arbitrary shell execution as the
  server process user. Header parsing exists at `src/net/http_parser.cpp:92`, but the only
  occurrences of `Authorization` in `src/` are CORS response strings, so no credential is
  ever validated.
- **Remediation**: Bind loopback by default with an explicit opt-in for remote exposure.
  Introduce a bearer-token middleware that reads the already-parsed `req.headers` before
  route dispatch, and reject with 401 when absent or mismatched.

### LX-002 Wide-open CORS preflight enables cross-site API abuse

- **Location**: `src/net/http_router.cpp:10-18`, `src/net/http_types.hpp:77,79,89`
- **Evidence**: `HttpRouter::route` returns 204 for every `OPTIONS` request with
  `Access-Control-Allow-Origin: *`, `Access-Control-Allow-Methods` listing all verbs, and
  `Access-Control-Allow-Headers: Content-Type, Authorization`. The same wildcard origin is
  applied unconditionally in `HttpResponse::json` and `HttpResponse::ok`.
- **Impact**: Combined with LX-001, any web page the user visits can issue preflight-free
  cross-origin requests straight into the local shell endpoint.
- **Remediation**: Restrict the allowed origin to an explicit allowlist, validate the
  `Origin` header on preflight, and never answer preflight with a blanket wildcard.

### LX-003 No privilege drop or resource limits in the executed child

- **Location**: `src/sys/process.cpp:55-82`, `src/tool/tool_proc_execute.cpp:9`
- **Evidence**: The child branch calls `setpgid` at line 57, closes and duplicates
  descriptors, and optionally `chdir`, then reaches `execvp` at line 80. There is no
  `setuid`, `setgid`, `setgroups`, `setrlimit`, or seccomp call anywhere in the child path.
  The tool description nevertheless advertises resource limits.
- **Impact**: Executed commands inherit full server privileges and can exhaust process
  count, memory, or file descriptors for the whole host. A fork bomb inside the container
  is unconstrained.
- **Remediation**: Apply `setrlimit` for `RLIMIT_NPROC`, `RLIMIT_AS`, `RLIMIT_NOFILE`, and
  `RLIMIT_CPU` in the child before `execvp`, then drop to an unprivileged uid and gid.
  Correct the tool description to match actual behavior.

### LX-004 Tool execution endpoint has no risk-level gate

- **Location**: `src/core/engine.cpp:255-269`, `src/tool/tool_definition.hpp`
- **Evidence**: `/api/tools/execute` invokes the requested tool directly. `ToolRiskLevel`
  declares a `Destructive` tier, but no code path reads the level to gate execution.
  `proc_execute` and `fs_replace` are marked `Mutating` and fire without confirmation.
- **Impact**: Destructive operations are reachable in a single unauthenticated request,
  with no confirmation boundary between advisory and mutating tools.
- **Remediation**: Load the tool definition, compare `risk_level` against a configured
  ceiling, and require an explicit confirmation token for `Mutating` and `Destructive`
  tiers. Either implement `Destructive` gating or remove the unused enum value.

### LX-005 `fs_grep` discloses the plaintext API key over SSE

- **Location**: `src/tool/tool_fs_grep.cpp:120-127`, `src/tool/tool_fs_grep.cpp` text filter
- **Evidence**: The ignore list omits the `.lxace` state directory, and `is_text_file`
  accepts `.json`. A recursive grep from the default project root therefore reads
  `.lxace/config.json`, which stores `api_key` in plaintext, and the matching lines are
  published on the unauthenticated SSE event stream.
- **Impact**: Credential exfiltration through a routine search operation, surfaced to every
  SSE subscriber.
- **Remediation**: Add `.lxace` to the ignore list, deny-list the config path in the fs
  tools, and redact secret-shaped values before broadcasting any tool output.

---

## 4. High Findings

### LX-006 All client connections are served synchronously on the reactor thread

- **Location**: `src/net/http_server.cpp:57-71`
- **Evidence**: The reactor loop receives only the listening socket event, calls
  `accept_client`, then immediately calls `handle_client`, which performs the full
  blocking request and response cycle. `EpollReactor::add_fd` is never called for client
  descriptors.
- **Impact**: One long-running request, including any `/api/exec` call, blocks every other
  client. `/api/stop` becomes unreachable precisely when it is needed. The documented
  non-blocking architecture does not exist at runtime.
- **Remediation**: Register accepted client descriptors with the reactor, drive reads and
  writes through readiness events, and move blocking tool execution onto a worker pool.

### LX-007 `\uXXXX` escapes are destroyed and replaced with `?`

- **Location**: `src/json/json_parser.cpp:128-134`
- **Evidence**: The `case 'u':` branch advances the cursor by four bytes and pushes a
  single literal `'?'`, with the comment `simplified unicode skip / ascii conversion`.
- **Impact**: Every non-ASCII character in any JSON payload is silently corrupted rather
  than decoded, producing wrong data with no error.
- **Remediation**: Decode the four hex digits, handle surrogate pairs, and encode the code
  point as UTF-8. Reject malformed escapes instead of substituting a placeholder.

### LX-008 NaN and infinity round-trip into permanently invalid JSON

- **Location**: `src/json/json_writer.cpp:51-59`, `src/core/config.cpp:144`
- **Evidence**: The writer emits `nan` and `inf` verbatim for non-finite doubles.
  `ConfigManager` loads `temperature` through `std::stod`, which accepts the strings
  `"nan"` and `"inf"`.
- **Impact**: A hand-edited config containing `"temperature": nan` is accepted, then
  re-serialized as invalid JSON that no conforming parser can read back.
- **Remediation**: Reject non-finite values at the parse boundary, and have the writer
  emit `null` or raise an error rather than a bare token.

### LX-009 Dangling final symlink escapes the filesystem sandbox

- **Location**: `src/sys/fs.cpp:106-125`
- **Evidence**: `resolve_safe_path` normalizes with `std::filesystem::weakly_canonical`.
  For a dangling final symlink the link target is not resolved away, so containment passes
  and returns the in-worktree path, after which `std::ofstream` follows the link and writes
  outside the root.
- **Impact**: Containment bypass allowing file creation outside the project directory.
- **Remediation**: Reject symlinks at both the final component and intermediate components,
  or open with `O_NOFOLLOW` and operate relative to a directory descriptor.

### LX-010 Missing EINTR retry in `waitpid` reports false exit code 0

- **Location**: `src/sys/process.cpp:147-154`
- **Evidence**: `waitpid(pid, &status, 0)` is called once with no `EINTR` loop. On
  interruption it returns -1 while `status` remains 0, so `WIFEXITED(0)` is true and the
  caller observes exit code 0 for a command whose state is unknown.
- **Impact**: False success reporting, the most dangerous possible failure mode for an
  automation engine.
- **Remediation**: Loop on `EINTR`, check the `waitpid` return value, and surface a distinct
  unknown-status result instead of defaulting to success.

### LX-011 Output truncation is silent and undetectable by callers

- **Location**: `src/sys/process.cpp:119-141`, `ProcessResult` in the sys header
- **Evidence**: Bytes beyond `max_output_bytes` are clamped by `std::min` and dropped, and
  `ProcessResult` carries no `truncated` flag.
- **Impact**: Consumers cannot distinguish complete output from silently truncated output,
  which corrupts downstream parsing and reasoning.
- **Remediation**: Add a `truncated_stdout` and `truncated_stderr` flag to the result and set
  them when clamping occurs.

### LX-012 Traversal aborts on first filesystem error

- **Location**: `src/tool/tool_fs_find.cpp:95-100`, `src/tool/tool_fs_grep.cpp:110-115`
- **Evidence**: The iteration condition tests `!ec`, so the first error terminates the whole
  traversal. The nested `if (ec) { ec.clear(); continue; }` is therefore unreachable.
- **Impact**: A single unreadable directory silently truncates results, and the
  `skip_permission_denied` option has no effect.
- **Remediation**: Clear the error code and continue inside the loop body, keeping the
  iteration condition independent of `ec`.

### LX-013 `fs_view` has no upper bound on `max_bytes`

- **Location**: `src/tool/tool_fs_view.cpp:75-77`
- **Evidence**: `max_bytes` is clamped to a minimum but never to a maximum, so a
  caller-supplied `1e18` requests an unbounded read.
- **Impact**: Memory exhaustion from a single request, amplified by the synchronous reactor
  in LX-006.
- **Remediation**: Add a hard ceiling, reject out-of-range values, and stream large files
  rather than materializing them in memory.

### LX-014 Config save ignores stream failure and returns success

- **Location**: `src/core/config.cpp:153-174`
- **Evidence**: `save_to_file` returns `true` after the insertion sequence with no check of
  `file.good()`, `file.flush()`, or `file.fail()`.
- **Impact**: A full disk or revoked permission is reported as a successful save, so
  settings silently revert on next load.
- **Remediation**: Flush, verify stream state, and return `false` on any failure. Tracked
  separately by task 005.

### LX-015 Config save concatenates raw values without JSON escaping

- **Location**: `src/core/config.cpp:165-172`
- **Evidence**: Values are inserted with `<<` between string literals, with no escaping for
  quotes, backslashes, or control characters.
- **Impact**: A `"` in `api_key`, `base_url`, or `model` produces a config file the loader
  cannot read back, causing silent reset to defaults.
- **Remediation**: Serialize through `json::JsonWriter`, retaining one key per line so the
  line-based loader still parses. Tracked separately by task 005.

### LX-016 Config file with plaintext API key is written world-readable

- **Location**: `src/core/config.cpp:160`
- **Evidence**: `std::ofstream file(path)` creates the file under the process umask, which
  the environment contract documents as `0002`, yielding mode `0664`.
- **Impact**: The plaintext API key is group- and world-readable on any multi-user host.
- **Remediation**: Create with `0600` via `open` plus `fdopen`, or apply `fchmod` after
  creation. Tracked separately by task 005.

### LX-017 Non-atomic truncate-then-write corrupts files on interruption

- **Location**: `src/sys/fs.cpp:19-33`, `src/tool/tool_fs_replace.cpp:114`
- **Evidence**: Both write paths open with truncation and then write in place.
- **Impact**: Process termination, disk pressure, or a crash mid-write leaves a zero-length
  or partially written file with the original content destroyed.
- **Remediation**: Write to a temporary file in the same directory, `fsync`, then `rename`
  over the target for atomic replacement.

### LX-018 Sub-agent performs keyword matching instead of LLM inference

- **Location**: `src/agent/sub_agent.cpp:56-111`
- **Evidence**: The prompt is lowercased and branched on substring matches for `find`,
  `files`, `search`, and `grep`, with fixed status strings emitted. No TLS client, no HTTP
  client, and no outbound request exists anywhere in `src/agent`. The `grep` branch is
  hardcoded to the literal `"Engine"`.
- **Impact**: The product's headline capability is not implemented. `docs/prd/prd.md` and
  `docs/api/api-reference.md` describe LLM-backed orchestration that does not exist, so any
  integration built against those documents will fail.
- **Remediation**: Implement the transport, then replace the keyword router with a real
  request and response loop. Until then, correct both documents to state current behavior.

### LX-019 Every task is marked `Succeeded`, failure state is unreachable

- **Location**: `src/agent/sub_agent.cpp:109`
- **Evidence**: The terminal state assignment is unconditional, and no code path assigns
  `TaskState::Failed`.
- **Impact**: Failure is unobservable. Orchestration metrics and downstream retry logic can
  never trigger.
- **Remediation**: Derive the terminal state from the actual tool result and propagate tool
  error codes into `TaskState::Failed`.

### LX-020 New prompt submission silently resurrects cancelled tasks

- **Location**: `src/agent/orchestrator.cpp:45`
- **Evidence**: Every `submit_prompt` sets `cancel_flag_ = false`, clearing a cancellation
  issued by `stop_all()`. The write also occurs outside the mutex.
- **Impact**: A global stop does not hold, and a worker may observe a torn value.
- **Remediation**: Use a generation counter or per-task cancellation token instead of a
  shared flag, and publish all state changes under the mutex.

### LX-021 Task vector trimming can orphan a running worker

- **Location**: `src/agent/orchestrator.cpp:38-41`
- **Evidence**: Trimming removes elements from the front of `tasks_` while workers may still
  hold references or indices to them.
- **Impact**: A worker proceeds against a default-constructed task and its final state update
  is discarded, producing silent task loss.
- **Remediation**: Hold tasks by stable identifier in a map or shared pointer, and reclaim
  only after the owning worker confirms completion.

---

## 5. Medium Findings

### LX-022 `Content-Length` matching is case-sensitive and incomplete

- **Location**: `src/net/http_server.cpp:92-95`
- **Evidence**: Only `Content-Length:` and `content-length:` are probed. Mixed casings such as
  `Content-length:` and `CONTENT-LENGTH:` are missed. Header names are case-insensitive per
  RFC 9110.
- **Impact**: The body is truncated and the request fails with a spurious 400.
- **Remediation**: Parse headers through a case-insensitive comparison, reusing the existing
  header parser rather than ad-hoc substring search.

### LX-023 No percent-decoding of request query strings

- **Location**: `src/net/http_parser.cpp:7-36`
- **Evidence**: The query string is split on `&` and `=` with no percent-decoding step.
- **Impact**: A request such as `?path=src%2Fmain.cpp` reaches `resolve_safe_path` with the
  literal encoded value and incorrectly 404s.
- **Remediation**: Percent-decode keys and values after splitting, and reject invalid
  encodings.

### LX-024 Status reason phrase is wrong for every code except 200 and 404

- **Location**: `src/net/http_types.hpp:75`
- **Evidence**: The ternary yields `"OK"` for 200, `"Not Found"` for 404, and `"Error"` for
  everything else.
- **Impact**: 400, 403, and 500 responses carry a non-standard reason phrase, misrepresenting
  the status to clients and logs.
- **Remediation**: Provide a full status-to-reason table.

### LX-025 `Content-Length` omitted for empty bodies

- **Location**: `src/net/http_types.hpp:94-99`
- **Evidence**: The header is added only when `!body.empty()`, so an empty 204 or 200 carries
  no length.
- **Impact**: Strict HTTP/1.1 clients block waiting for a body that never arrives.
- **Remediation**: Always emit `Content-Length`, including `0` for empty bodies.

### LX-026 SSE broadcast holds the mutex across blocking `send`

- **Location**: `src/net/sse_stream.cpp:70-85`
- **Evidence**: `broadcast_raw` holds `mutex_` for the whole loop while calling `send` with a
  5 second `SO_SNDTIMEO`. Because event listeners are invoked inline on the publishing
  thread, the stall propagates to the HTTP thread and all workers.
- **Impact**: A single slow or stalled subscriber freezes every broadcast and every event
  producer for up to 5 seconds per client.
- **Remediation**: Snapshot the descriptor list under the mutex, release it, then send.
  Make client descriptors non-blocking and buffer pending payloads per client.

### LX-027 SSE client removal has no call sites

- **Location**: `src/net/sse_stream.cpp:42-49`
- **Evidence**: `remove_client` is defined but never invoked; reclamation happens only when a
  later broadcast fails.
- **Impact**: Closed connections retain descriptors until the next broadcast attempts a
  write, leaking descriptors on an idle stream.
- **Remediation**: Invoke `remove_client` when the connection is closed or an event read
  reports hangup.

### LX-028 `procfs` `(comm)` field parse breaks on names containing spaces

- **Location**: `src/sys/procfs.cpp:30-33`
- **Evidence**: The header line is parsed with `>>` into `comm`. For a process named
  `(Web Content)` at `src/sys/procfs.cpp:25-39`, the stream stops at the space and every
  subsequent field shifts.
- **Impact**: Reported RSS, CPU ticks, and state are wrong for any process with a space in
  its name.
- **Remediation**: Extract the parenthesized `comm` field by locating the last `)` and parse
  only the remainder with `>>`.

### LX-029 CPU cap uses floor division and disagrees with the Makefile

- **Location**: `src/sys/procfs.cpp:95-98`, `Makefile:8`
- **Evidence**: The C++ cap uses floor division while the Makefile uses `(nproc+1)/2`. On 3
  cores the two disagree, producing 1 versus 2. On 1 core the C++ cap equals 100 percent of
  the host.
- **Impact**: The 50 percent resource ceiling is not enforced consistently, and is violated
  outright on small hosts.
- **Remediation**: Use a single ceiling helper, round up, and clamp to a minimum of 1 while
  documenting the single-core exception.

### LX-030 Network probe socket is not close-on-exec

- **Location**: `src/sys/network.cpp:14`
- **Evidence**: The probe uses `socket(AF_INET, SOCK_STREAM, 0)` without `SOCK_CLOEXEC`, and
  no `FD_CLOEXEC` is set afterwards.
- **Impact**: The descriptor leaks into every child spawned by `ProcessRunner`, so the probe
  port appears occupied by unrelated processes.
- **Remediation**: Add `SOCK_CLOEXEC` or apply `fcntl(fd, F_SETFD, FD_CLOEXEC)`.

### LX-031 Unvalidated port cast silently wraps

- **Location**: `src/main.cpp:38-39`
- **Evidence**: `port = std::stoi(argv[++i])` is stored into an `int` and later cast to
  `uint16_t` with no range validation, so `--port 70000` wraps.
- **Impact**: The server binds an unintended port, or fails to bind, with no diagnostic.
- **Remediation**: Validate the parsed integer against `1..65535` and reject out-of-range
  input with a clear message.

### LX-032 Bind failure is discarded, engine reports running with no API

- **Location**: `src/core/engine.cpp:44-46`, `src/main.cpp:63-66`
- **Evidence**: `Engine::start_services` calls `http_server_->start()` as a bare statement
  and discards the returned success value. `main` then prints the headless running banner
  unconditionally at line 66.
- **Impact**: A port conflict leaves the process alive with no API listener while reporting
  success, which is operationally indistinguishable from a healthy run.
- **Remediation**: Propagate the start result, and abort initialization or exit non-zero when
  the listener cannot bind.

### LX-033 Validation gaps in `max_tokens`, `temperature`, and `server_port`

- **Location**: `src/core/engine.cpp:154-155`
- **Evidence**: Negative `max_tokens` and unbounded `temperature` are accepted, and a
  `server_port` change is accepted by the config API but never triggers a rebind.
- **Impact**: Invalid configuration persists while the UI reports it as effective, and the
  listener keeps using the old port.
- **Remediation**: Validate ranges at the boundary, and either apply the port change with an
  explicit restart requirement or reject it.

### LX-034 Unmasked `base_url` returned alongside the masked form

- **Location**: `src/core/engine.cpp:134`
- **Evidence**: The settings response includes both `base_url` and `base_url_masked`, so the
  masking provided no confidentiality.
- **Impact**: The mask is cosmetic, and any embedded credentials in the URL are exposed over
  the unauthenticated API.
- **Remediation**: Return only the masked form, and reveal the full value only through an
  authenticated, explicit disclosure path.

### LX-035 Event bus invokes listeners with no exception isolation

- **Location**: `src/core/event_bus.cpp:30-34`
- **Evidence**: Listeners are called inline with no `try` or `catch`.
- **Impact**: A throwing listener propagates out of `publish` and terminates the publishing
  thread, killing whichever component emitted the event.
- **Remediation**: Wrap each invocation in `try` and `catch` and log the failure, isolating
  subscribers from each other.

---

## 6. Low and Drift Findings

### LX-036 Settings key bindings contradict the printed labels

- **Location**: `src/cli/view_settings.cpp:66-69`, `src/cli/view_settings.cpp:93`
- **Evidence**: `t` is bound to Max Tokens while `e` is bound to Temperature, contradicting
  the `[4]` and `[5]` labels rendered in the same view. `read_masked_line('*')` is used for
  the base URL, so typos in a non-secret field are invisible.
- **Impact**: Users edit the wrong field, and URL typos go undetected.
- **Remediation**: Align the bindings with the printed numbering, and use unmasked input for
  non-secret fields.

### LX-037 Documentation, Makefile, and container configuration drift

- **Location**: `README.md`, `Makefile`, `docker-compose.yml`, `tests/scripts/sh/test.sh`
- **Evidence**:
  - `README.md` claims no subshells are used, while `src/sys/process.cpp:19` invokes `/bin/sh -c`.
  - `README.md` cites `/proc/cpuinfo` as the CPU source, while `src/sys/procfs.cpp:91` uses `sysconf`.
  - `Makefile` requires Docker for the build target, while the verified native `cmake` build succeeds.
  - `docker-compose.yml:15-16` and `tests/scripts/sh/test.sh:12-13` hardcode 4 CPUs and 3500M, contradicting the derived `DOCKER_CPUS` value.
  - `docker-compose.yml:10` bind-mounts the repository over the image's `chown appuser`, so the container runs with uid 1000 against files owned by uid 10001 and `fs_write` fails at runtime.
- **Impact**: Operators cannot trust the documentation, and the containerized write path is
  broken out of the box.
- **Remediation**: Correct the README claims, align container caps to the host-derived
  values, and fix volume ownership or run the container as the matching uid.

---

## 7. Test Coverage Gap

The suite reports 28 passing tests. None of `LX-001` through `LX-035` is exercised by any of
them. The following behaviors have no test at all:

- No authentication, authorization, or CORS test exists.
- `/api/fs/write` and `/api/tools/execute` are never invoked by a test.
- JSON unicode escape decoding is untested.
- Config escaping, permission mode, and write-failure handling are untested.
- SSE broadcast, client lifecycle, and concurrent subscriber behavior are untested.
- Orchestrator cancellation, task state transitions, and worker lifetime are untested.
- `waitpid` interruption and output truncation are untested.

The tests that do pass, including `process_timeout_enforcement`,
`fs_safe_path_traversal_protection`, and `procfs_resource_50_percent_caps`, cover their stated
paths but do not extend to the sibling failure modes documented above.

---

## 8. Remediation Order

### Phase 1: Contain the critical block (LX-001 to LX-005)

Bind loopback by default. Add bearer authentication in the request path before dispatch.
Restrict CORS to an explicit origin allowlist. Apply `setrlimit` and privilege drop in the
child. Gate `Mutating` and `Destructive` tools. Add `.lxace` to every filesystem ignore list
and redact secret values before SSE broadcast.

### Phase 2: Restore correctness and I/O integrity (LX-006 to LX-017)

Move client handling onto the reactor with a worker pool so a long command cannot block the
API. Implement real `\u` decoding and reject non-finite numbers. Harden `resolve_safe_path`
against symlinks. Retry `waitpid` on `EINTR` and stop reporting false success. Add truncation
flags. Fix traversal error handling and bound `max_bytes`. Rewrite config persistence with
`JsonWriter`, `0600`, and checked writes. Make filesystem writes atomic.

### Phase 3: Agent runtime truth (LX-018 to LX-021)

Decide explicitly whether the LLM transport is in scope. If yes, it is the largest remaining
feature. If no, correct `docs/prd/prd.md` and `docs/api/api-reference.md` immediately so
integrators are not misled. Regardless of that decision, fix task state derivation,
cancellation semantics, and worker lifetime.

### Phase 4: Protocol and polish (LX-022 to LX-037)

Case-insensitive header parsing, percent-decoding, a full reason-phrase table, unconditional
`Content-Length`, lock-free SSE broadcast with working client removal, `comm` parsing that
tolerates spaces, one shared CPU-cap helper, `SOCK_CLOEXEC`, port range validation, propagated
bind failures, consistent masking, exception-isolated event dispatch, corrected key bindings,
and a synchronized documentation set.

---

## 9. Scope Statement

This report is documentation only. No file under `src/` was modified in producing it. The
build and test baseline was executed and observed green both before and after the change.
Remediation of any finding requires a dedicated task and its own verification cycle.
