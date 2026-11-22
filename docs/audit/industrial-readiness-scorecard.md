# LXACE Industrial Readiness Scorecard

Assessment of LXACE against industrial standards for Linux control engines, scored
against the operating goal "Linux controlling engine using AI". Every score below is
backed by a file and line reference or by an executed command. No dimension is scored
from impression or from documentation claims alone.

---

## 1. Assessment Baseline

| Field | Value |
| --- | --- |
| Repository | `github.com/gtkrshnaaa/lxace` |
| Branch | `devv` |
| Commit | `411a20d` (chore(task): close task-009 capability exposure and reset ledger) |
| Source files | 74 header and translation units across `core/`, `sys/`, `json/`, `net/`, `agent/`, `cli/`, `tool/` |
| Tools registered | 11 across `src/tool/` |
| Build | `cmake --build build -j 1`, zero warnings |
| Unit suite | `./build/lxace_tests`, 63 passed, 0 failed |
| Live API harness | `bash scripts/sh/api-smoke.sh`, 36 passed, 0 failed |
| Prior audit | `docs/audit/defect-report.md`, 37 findings, 37 remediated |

### 1.1 Scoring Method

The industrial standard was decomposed into ten weighted dimensions that together
describe a production-grade privileged control plane. Each dimension was then scored
from 0 to 10 against direct source evidence. Documentation claims were treated as
untrusted until confirmed in code, and any claim that the code contradicts is recorded
as a defect rather than credited as a feature.

Weights reflect consequence of failure, not volume of work. A control engine that
executes privileged operations is judged primarily on whether its privileges are
bounded and observable, which is why security and observability carry 35 percent
between them.

### 1.2 Maturity Bands

| Band | Range | Meaning |
| --- | --- | --- |
| Reference grade | 9.0 to 10.0 | Deployable to a managed fleet without qualification |
| Production grade | 7.0 to 8.9 | Deployable with documented operational runbooks |
| Conditional | 5.5 to 6.9 | Deployable to internal or non-critical hosts only |
| Prototype | 3.0 to 5.4 | Engineering artifact, not an operating control plane |
| Experimental | 0.0 to 2.9 | Not ready for any privileged environment |

---

## 2. The Industrial Standard

### Tier 1: OS Integration and Packaging

- Filesystem Hierarchy Standard layout: configuration in `/etc`, variable state in
  `/var/lib`, runtime state in `/run`, logs in `/var/log`.
- Native packaging as `deb` or `rpm`, with signed and reproducible artifacts, a
  dependency manifest, and a generated SBOM.
- A systemd unit using `Type=notify` with `sd_notify` readiness, `WatchdogSec`, and
  socket activation where appropriate.
- systemd hardening directives: `ProtectSystem=strict`, `ProtectHome=true`,
  `NoNewPrivileges=true`, `PrivateTmp=true`, `SystemCallFilter`,
  `CapabilityBoundingSet`, and `RestrictAddressFamilies`.

### Tier 2: Security and Privilege Containment

- POSIX capability model, a seccomp filter, and an AppArmor or SELinux profile.
- OWASP API Top 10 coverage: broken object level authorization, broken authentication,
  broken function level authorization, and security misconfiguration.
- NIST AU-2 and AU-3 audit logging of authentication events and privileged actions.
- Token expiry and rotation, per-client rate limiting, and role based access control.

### Tier 3: Observability

- Structured JSON logs to stdout or journald, carrying level, timestamp, and
  correlation identifier.
- Prometheus `/metrics` or OpenTelemetry traces exposing RED and USE signals.
- Distinct `/healthz` liveness and `/readyz` readiness endpoints for orchestrator probes.
- A durable audit trail of every tool invocation with actor, arguments, and outcome.

### Tier 4: Reliability, Lifecycle, and Concurrency

- Graceful shutdown with a bounded drain deadline, and `sigaction` registration using
  async-signal-safe handler logic.
- Configuration reload on `SIGHUP` without a restart.
- A worker pool sized from `std::thread::hardware_concurrency()` or cgroup quota, with
  bounded queues, backpressure, timeouts, and circuit breaking on external calls.
- cgroup resource quotas and supervisor restart policy.

### Tier 5: API Contract and Verification Discipline

- A published OpenAPI document, a versioned path prefix such as `/api/v1`, and RFC 9457
  problem details for errors.
- Idempotency keys on mutating endpoints.
- Unit, integration, and end-to-end test layers, plus AddressSanitizer,
  UndefinedBehaviorSanitizer, ThreadSanitizer, fuzz targets on every parser, and an
  enforced coverage floor.

---

## 3. Scorecard

| # | Dimension | Score | Weight | Weighted | Decisive evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Security and privilege containment | 6 | 20% | 1.20 | Bearer gate and rlimits real; no seccomp, capabilities, or RBAC |
| 2 | Observability | 2 | 15% | 0.30 | `core/engine.cpp:20` logs only to an in-memory ring buffer |
| 3 | Packaging and OS integration | 2 | 12% | 0.24 | No systemd unit, no package, no FHS layout |
| 4 | Lifecycle and reliability | 5 | 10% | 0.50 | `main.cpp:25` uses `std::signal`, no `SIGHUP`, no drain deadline |
| 5 | Concurrency safety | 5 | 10% | 0.50 | `http_server.cpp:13` hardcodes four workers, no sanitizer run |
| 6 | API contract | 6 | 10% | 0.60 | Typed validation and `428` gate, but no OpenAPI, versioning, or rate limit |
| 7 | Control capability breadth | 6 | 10% | 0.60 | 11 tools, but no systemd, cgroup, package, or interface control |
| 8 | Test and verification rigor | 7 | 8% | 0.56 | 63 unit and 36 live checks, but no sanitizer, fuzzing, or coverage gate |
| 9 | Documentation accuracy | 6 | 3% | 0.18 | Strong audit corpus, but live drift against the code |
| 10 | AI reasoning layer | 1 | 2% | 0.02 | Keyword dispatch only, no model transport, no observation loop |
| | **Total** | | **100%** | **4.70** | **47 of 100, prototype band** |

**Overall: 4.70 / 10. Maturity band: prototype. Not deployable as a privileged control
plane, and not yet an AI control engine.**

---

## 4. Dimension Findings

### 4.1 Security and Privilege Containment: 6 of 10

What is genuinely industrial:

- Authentication is enforced before dispatch. `net/http_router.cpp` returns `401` with a
  `WWW-Authenticate` header, and the SSE fast path at `net/http_server.cpp:175` repeats
  the gate rather than bypassing it.
- Token material is 256 bits from `getrandom`, compared in constant time, persisted
  through an atomic `fsync` and `rename`, and written with mode `0600`.
- Child containment is real: `RLIMIT_NPROC`, `RLIMIT_AS`, `RLIMIT_NOFILE`,
  `RLIMIT_CPU`, and `RLIMIT_FSIZE` are applied in the child, with an opt-in
  `setuid`/`setgid`/`setgroups` drop that refuses to proceed on failure.
- Destructive operations are gated by `ToolRiskLevel::Destructive` and a `428`
  confirmation contract, and `tool_proc_signal.cpp:23-29` allowlists seven signals while
  deliberately omitting `SIGKILL`.
- Path containment is symlink safe, using an `lstat` walk rather than `weakly_canonical`
  alone.

Why it is not higher:

- There is no seccomp filter, no capability bounding set, and no AppArmor or SELinux
  profile. A memory-safety defect in the HTTP or JSON parser yields full daemon
  privileges. A search for `seccomp`, `capabilities`, and `NoNewPrivs` across `src`
  returns zero matches.
- No role based access control and no per-client rate limiting. `429` exists only as a
  reason phrase in `net/http_types.hpp:93` and is never emitted.
- No token expiry or rotation path exists.
- Failed authentication is not recorded as an event, so there is no NIST AU-2 trail for
  credential attacks.

### 4.2 Observability: 2 of 10

This is the weakest axis, and it contains a documented falsehood.

- `core/engine.cpp:20` constructs `RingBuffer<std::string>(500)`. That is the entire
  logging facility: bounded, in memory, lost on restart, with no severity levels, no
  timestamps, and no correlation identifiers.
- There is no journald sink, no stdout JSON lines, no `/metrics`, and no tracing. A
  headless deployment cannot emit a single durable log line.
- `docs/api/api-reference.md:894` documents `GET /api/health` as the one unauthenticated
  endpoint. It does not exist. `add_public_route` is declared at
  `net/http_router.hpp:25` and has **zero call sites**, and a search for `health` across
  `src` returns no implementation. Kubernetes liveness and systemd readiness probes have
  nothing to call.
- No tool invocation is persisted. There is no actor, argument, or outcome trail outside
  the same volatile ring buffer.

Two of ten, awarded for having a bounded diagnostic buffer and a working SSE stream at
all. Neither survives a restart.

### 4.3 Packaging and OS Integration: 2 of 10

- No systemd unit exists: a search for `*.service` across the repository returns nothing.
- No `deb` or `rpm` packaging, no SBOM, no signed artifact, and no reproducible release
  pipeline.
- No FHS discipline. Configuration lands in a project-local `.lxace/config.json` rather
  than `/etc`, and no `/var/lib`, `/run`, or `/var/log` paths are used.
- Deployment is "copy the binary and run it", which disqualifies the engine from any
  managed fleet or configuration management system.

Two of ten for shipping a working native binary and a Docker path, which is at least a
repeatable single-host install.

### 4.4 Lifecycle and Reliability: 5 of 10

- Shutdown handling is not async-signal-safe. `main.cpp:25-27` registers handlers with
  `std::signal`, whose guarantees are weaker than `sigaction`, and the handler touches a
  `std::shared_ptr<CliApp>` and other non-atomic state. That is undefined behavior under
  POSIX async-signal-safety rules and is a genuine correctness hazard under load.
- `SIGHUP` is unhandled, so any configuration change requires a full restart, which for a
  control plane means dropping in-flight work.
- There is no bounded drain deadline. Shutdown does not wait for in-flight tool executions
  with a ceiling.
- In its favor: graceful `SIGINT` and `SIGTERM` exist at all, `waitpid` is `EINTR` safe,
  output truncation is explicit rather than silent, and `ProcessRunner` reports exit code,
  both streams, and timeout state faithfully.

### 4.5 Concurrency Safety: 5 of 10

- `net/http_server.cpp:13` declares `constexpr size_t kWorkerCount = 4`. Parallelism is a
  compile-time constant, so an 8-core host and a 128-core host receive identical
  throughput. A search for `hardware_concurrency` across `src` returns zero matches.
- No ThreadSanitizer run has ever executed against the worker pool or the SSE broadcast
  threads, and no sanitizer flag appears anywhere in the build configuration.
- In its favor: the queue is bounded, responses are written through a resuming
  `send_all` rather than a single `send`, and the SSE broadcast snapshots its client list
  under lock then writes outside the lock, which was a real defect fixed during this
  programme.

### 4.6 API Contract: 6 of 10

- Typed argument validation now enforces declared `type` and `enum`, returning errors that
  name the offending parameter. That is a real improvement over presence-only checking and
  is what an LLM tool loop needs to self-correct.
- Risk levels are declared and enforced, with a `428` confirmation contract on
  `Mutating` and `Destructive` tools.
- Reason phrases are complete and `Content-Length` is emitted unconditionally, so strict
  HTTP/1.1 clients no longer block.
- Gaps: no OpenAPI document, no `/api/v1` prefix (every route is registered unversioned at
  `core/engine.cpp:124-346`), no RFC 9457 problem details, and no idempotency keys.
- Worse, `cli/view_home.cpp:38` displays `http://127.0.0.1:<port>/api/v1` to the operator
  while no versioned route exists. The UI advertises an API shape the server does not
  serve.

### 4.7 Control Capability Breadth: 6 of 10

Eleven tools are registered: `fs_view`, `fs_replace`, `fs_find`, `fs_grep`, `fs_mutate`,
`fs_watch`, `proc_execute`, `proc_list`, `proc_signal`, `net_probe`, `sys_metrics`.

That is a credible base for filesystem, process, and network control. It is not yet a
Linux control plane, because the subsystems that distinguish one are absent:

- No systemd or service control: start, stop, restart, enable, and unit status.
- No cgroup control: quotas, limits, and slice placement.
- No package management: query, install, and version inspection.
- No network interface configuration: link state, addressing, and routes.
- No user or group management, and no scheduled task control.
- No container or namespace control.

An agent asked to "restart nginx" or "cap this service at 1 GB" cannot act.

### 4.8 Test and Verification Rigor: 7 of 10

This is the strongest axis.

- 63 unit tests across 12 files, using a custom auto-registering harness that keys on
  process exit status.
- A 36-check live harness that boots the real binary, sets a synthetic token, polls for
  `401` as a readiness signal, and verifies auth rejection, CORS behavior, exit code
  propagation, path traversal refusal, the `428` gate, and SSE authorization.
- Zero warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
  -Wold-style-cast`, which functions as a static analysis gate on a codebase with heavy
  integer-width churn.
- The live harness has already proven its worth: two socket-level defects, response
  truncation and dropped POST bodies, were invisible to 63 green unit tests and obvious to
  the first live request.

Why not higher: no AddressSanitizer, UndefinedBehaviorSanitizer, or ThreadSanitizer in any
configuration; no fuzz targets on `json_parser.cpp` or `http_parser.cpp`, which is exactly
where parser defects concentrate; no coverage floor enforced by CI; and the live harness
asserts status codes rather than response shape, and does not participate in `ctest`.

### 4.9 Documentation Accuracy: 6 of 10

- The auditing corpus is unusually strong for a project of this size: a 37-finding defect
  report with stable identifiers, per-finding evidence, severity definitions, and honest
  partial-remediation disclosure rather than blanket claims of closure.
- Task ledgers record acceptance criteria and verification evidence per unit of work.
- But drift is live and material. `docs/api/api-reference.md:894` promises `/api/health`
  and no such route exists. `cli/view_home.cpp:38` advertises an `/api/v1` prefix that no
  route serves. Documentation that overstates capability is worse than absent
  documentation in a privileged system, because operators build probes and runbooks on it.

### 4.10 AI Reasoning Layer: 1 of 10

This is the product-defining gap, and it is the reason the overall score cannot rise much
regardless of the other axes.

- `agent/sub_agent.cpp` performs keyword dispatch: it lowercases the prompt and branches
  on `find`, `files`, `search`, and `grep`, then emits fixed status strings. One branch is
  hardcoded to the literal token `"Engine"`.
- There is no outbound model transport anywhere in `src`: no TLS client, no HTTP client,
  no `api_key` consumer. A search for `https://api` returns no matches, and `api_key` is
  read only by configuration load, save, and masking.
- There is no multi-step planning, no tool-selection reasoning, no observation loop, and
  no termination condition based on a model answer.
- Every task is marked `Succeeded`; no code path reaches a failure state.

One of ten, awarded because the orchestrator, task registry, and typed tool surface are
genuinely the right substrate to build the loop on. The loop itself does not exist.

---

## 5. Where the Engine Meets Industrial Bar

These are not marketing claims; each is enforced in code and exercised by a test.

- Privileged child execution is bounded by five resource limits, with an optional
  privilege drop that fails closed.
- Destructive capability is classified and gated behind an explicit confirmation
  contract, and signal delivery deliberately excludes `SIGKILL`.
- Credential handling is correct: high-entropy generation, constant-time comparison,
  atomic persistence, restrictive mode, and masking in every API response.
- Path containment resists symlink escape through an `lstat` walk rather than a naive
  string prefix check.
- Data integrity is deliberate: atomic config writes, `EINTR`-safe reaping, explicit
  truncation flags, and resuming socket writes.
- The engineering process itself is industrial: audit with stable identifiers, phased
  remediation, a green suite at every phase boundary, and honest disclosure of partial
  fixes.

---

## 6. Blocking Gaps, Ordered by Consequence

| Rank | Gap | Consequence | Evidence |
| --- | --- | --- | --- |
| 1 | No OS-level confinement | Parser exploit yields full daemon privilege | No `seccomp`, no capability bounding set, no LSM profile in `src` |
| 2 | `/api/health` documented but absent | No orchestrator liveness or readiness probe possible | `docs/api/api-reference.md:894`; `http_router.hpp:25` has zero call sites |
| 3 | Logging is volatile only | Zero forensic capability after restart | `core/engine.cpp:20` |
| 4 | Auth failures never recorded | No AU-2 audit trail for credential attacks | `http_router.cpp` 401 path emits no event |
| 5 | Handler is not async-signal-safe | Undefined behavior on shutdown under load | `main.cpp:25-27` touches a `shared_ptr` in the handler |
| 6 | No packaging or systemd unit | Cannot be deployed by a fleet | No `*.service`, no `deb`, no FHS paths |
| 7 | Worker count hardcoded | Throughput does not scale with host capacity | `net/http_server.cpp:13` |
| 8 | No systemd, cgroup, or package control | Core Linux control tasks are impossible | 11 tools, none for services or cgroups |
| 9 | No rate limiting or RBAC | Authenticated client can saturate the engine | `429` never emitted |
| 10 | No model transport | Not an AI engine, only an AI-shaped API | No TLS or HTTP client in `src` |

---

## 7. Path to Production Grade

The following sequence targets 7.0 or better, ordered by score gained per unit of risk.
The first three items are small, low-risk, and together move the total roughly one full
point.

**Phase A: observability and lifecycle, roughly plus 1.0**

1. Implement `GET /api/health` and `GET /api/readyz` using the existing, currently unused
   `add_public_route` mechanism. Readiness must fail while the listener is unbound.
   Removes a documented falsehood and unblocks orchestration.
2. Add a JSON-lines logger with severity levels, timestamps, and a correlation identifier,
   writing to stdout and optionally journald. Retain the ring buffer only as an in-memory
   tail.
3. Emit a structured `auth.failed` event, including remote address and path, from both
   the router `401` path and the SSE fast path.
4. Replace `std::signal` with `sigaction` and `SA_RESTART`, and move handler logic out of
   signal context into a `signalfd` or self-pipe drain. Add a bounded drain deadline and a
   `SIGHUP` configuration reload.

**Phase B: confinement and deployment, roughly plus 1.0**

5. Ship a hardened systemd unit with `ProtectSystem=strict`, `ProtectHome=true`,
   `NoNewPrivileges=true`, `PrivateTmp=true`, `SystemCallFilter`,
   `CapabilityBoundingSet`, and `RestrictAddressFamilies`, plus `Type=notify` and
   `WatchdogSec` once readiness exists.
6. Add a seccomp profile and an AppArmor profile, and honour `drop_uid`/`drop_gid` by
   default when running as root.
7. Produce a `deb` through a containerized builder with FHS paths, and add an SBOM step.

**Phase C: API and concurrency, roughly plus 0.6**

8. Derive `kWorkerCount` from `std::thread::hardware_concurrency()`, clamped to the 50
   percent host rule, and make the queue depth configurable.
9. Add a `/api/v1` prefix and correct `cli/view_home.cpp:38`, which currently advertises a
   prefix that does not exist.
10. Publish an OpenAPI document generated from the registry, and adopt RFC 9457 problem
    details for error bodies.
11. Add per-token rate limiting on `Destructive` tools, using the already-declared `429`.

**Phase D: control breadth, roughly plus 0.4**

12. Add `svc_control` for systemd unit status, start, stop, and restart, gated
    `Destructive`.
13. Add `cgroup_control` for slice and limit inspection, and `pkg_query` for package
    status.

**Phase E: verification depth, roughly plus 0.3**

14. Add AddressSanitizer and UndefinedBehaviorSanitizer builds, then ThreadSanitizer
    against the worker pool and SSE broadcast.
15. Add libFuzzer targets on `json_parser.cpp` and `http_parser.cpp` with a corpus and a
    crash regression directory.
16. Enforce a coverage floor in CI and assert response shape, not only status codes, in
    the live harness.

**Phase F: the actual product, roughly plus 0.9**

17. Implement a single `POST /api/prompt` reasoning loop: call a model, parse tool calls,
    execute through the existing typed registry, feed observations back, and terminate on
    a final answer. The substrate for this already exists and is sound.
18. Record every tool invocation with actor, arguments, and outcome as a durable audit
    record.

---

## 8. Score Projection

| Milestone | Projected score | Band |
| --- | --- | --- |
| Current state (`411a20d`) | 4.70 | Prototype |
| After Phase A | 5.70 | Conditional |
| After Phases A and B | 6.70 | Conditional |
| After Phases A, B, and C | 7.30 | Production grade |
| After all phases | 8.10 | Production grade |

Projections assume each phase is verified by the existing test discipline and that no new
defect class is introduced. They are estimates of dimension movement, not guarantees.

---

## 9. Summary

- Overall industrial readiness is **4.70 of 10**, placing LXACE in the prototype band.
- The engine is a well-hardened privileged execution substrate with unusually rigorous
  verification for its size, scoring 7 on test discipline and 6 on security.
- Observability at 2 and packaging at 2 are the dominant drags, and observability contains
  a documented `/api/health` endpoint that has never existed.
- The AI layer scores 1: `agent/sub_agent.cpp` is keyword dispatch with no model transport,
  so the stated goal is not yet met by the implementation.
- Cheapest meaningful gain is Phase A, three focused changes that move the total about one
  full point.
- Production grade requires Phases A through C, which is achievable without touching the
  security model that is already sound.
