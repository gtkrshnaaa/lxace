---
id: "010"
title: "publish industrial standard readiness scorecard"
type: "docs"
status: "done"
created_at: "2026-11-23"
updated_at: "2026-11-23"
---

# [Docs] publish industrial standard readiness scorecard

## Description

Publish a weighted industrial readiness scorecard that scores LXACE against the
standards a production Linux control engine is measured by, and against the stated
goal "Linux controlling engine using AI". Every dimension must be evidenced by a file
and line reference or an executed command, and documentation claims that the code
contradicts must be recorded as defects rather than credited.

## Acceptance Criteria
- [x] Standard decomposed into weighted dimensions with stated failure consequence
- [x] Ten dimensions scored from direct source evidence, no impression scoring
- [x] Weighted arithmetic verified to total the published overall score
- [x] Every score cites a file and line or an executed command
- [x] Documented-but-absent `/api/health` recorded as a defect, not credited as a feature
- [x] Maturity bands defined so the published score maps to a deployment verdict
- [x] Blocking gaps ordered by consequence with per-gap evidence
- [x] Remediation sequence ordered by score gained per unit of risk
- [x] Score projection per milestone phase
- [x] Zero em dashes and zero emojis in the published document
- [x] Unit suite still green after the change

## Verification evidence

- Weighted total recomputed by hand: 1.20 + 0.30 + 0.24 + 0.50 + 0.50 + 0.60 + 0.60 +
  0.56 + 0.18 + 0.02 = 4.70, weights summing to 100 percent.
- `grep -c -P '\\x{2014}'` on the document returns `0` em dashes.
- `grep -c -P '[\\x{1F300}-\\x{1FAFF}\\x{2600}-\\x{27BF}]'` returns `0` emojis.
- `./build/lxace_tests` reports `63 passed, 0 failed in 582 ms`, exit code 0.
- Evidence re-verified this session: `add_public_route` has zero call sites,
  `kWorkerCount = 4` is a compile-time constant with no `hardware_concurrency` usage,
  `std::signal` is used with no `sigaction` and no `SIGHUP`, logging is only
  `RingBuffer<std::string>(500)` at `core/engine.cpp:20`, no `*.service` file exists, and
  `cli/view_home.cpp:38` advertises `/api/v1` while every route is registered unversioned.

## Findings surfaced during scoring

- `docs/api/api-reference.md:894` documents `GET /api/health` as the single
  unauthenticated endpoint, and no such route is implemented. `add_public_route`
  (`net/http_router.hpp:25`) is declared and never called.
- `cli/view_home.cpp:38` displays `/api/v1` to the operator while `core/engine.cpp`
  registers every route unversioned. The UI advertises an API shape the server does not
  serve.
- `429 Too Many Requests` exists only as a reason phrase in `net/http_types.hpp:93` and is
  never emitted, so no rate limiting exists despite the declaration.

## Out of scope

Remediation of the gaps the scorecard identifies. Phases A through F are recorded in
section 7 of the document for separate task tracking.

## Progress log

- Task created and locked.
- Re-verified all ten dimensions against source before scoring.
- Scorecard published at `docs/audit/industrial-readiness-scorecard.md`.
- Three previously undocumented documentation and capability drifts recorded.
