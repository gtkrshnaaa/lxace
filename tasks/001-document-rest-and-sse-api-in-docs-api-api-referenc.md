---
id: "001"
title: "Document REST and SSE API in docs/api/api-reference.md"
type: "docs"
status: "done"
created_at: "2026-10-31"
updated_at: "2026-10-31"
---

# [Docs] Document REST and SSE API in docs/api/api-reference.md

## Description
Comprehensive, production-grade API documentation for the LXACE REST and Server-Sent Events (SSE) interface.

## Acceptance Criteria
- [x] Detail architectural overview and protocol specifications (HTTP/1.1 REST + SSE RFC 8895)
- [x] Document all 10 API endpoints with methods, parameters, request/response JSON schemas, error codes, and curl examples
- [x] Document Server-Sent Events stream specification, event kinds, and heartbeat mechanism
- [x] Document resource governance and 50% CPU/RAM cap representation
- [x] Link `docs/api/api-reference.md` in `README.md` and verify zero broken links
- [x] Run test suite (`make test`) and verify 100% passing tests

## Context / Notes
- Target files: `docs/api/api-reference.md`, `README.md`
- Endpoints in scope:
  1. GET /api/session
  2. POST /api/prompt
  3. GET /api/events
  4. POST /api/stop
  5. GET /api/settings
  6. POST /api/settings
  7. POST /api/exec
  8. GET /api/fs/list
  9. POST /api/fs/read
  10. POST /api/fs/write
