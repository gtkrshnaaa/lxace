# Task 008: End-to-End API Smoke Verification

## Type
verification

## Status
done

## Defects found by this verification

1. `http_server.cpp` called `send()` once per response with no partial-write loop, so any
   response larger than the socket buffer was truncated and the socket closed. Fixed with
   `net::send_all` (`src/net/socket_write.cpp`), now used at all five write sites.
2. `socket.cpp` accepts clients with `SOCK_NONBLOCK`, but `RequestReader::read_request`
   treated `recv() < 0` as EOF. `EAGAIN` is a normal pending-data signal on a non-blocking
   descriptor, so POST bodies were dropped and the connection closed with no response at
   all. This was intermittent, which is why GET-only checks passed. Fixed with `poll()`
   readiness plus explicit `EINTR` and `EAGAIN` handling.

Both defects were invisible to the 47-test unit suite because it never exercises a real
socket. They were only reachable through live HTTP verification.

## Acceptance Criteria

- [x] Harness boots the engine headless on loopback with a synthetic token
- [x] Unauthenticated request is rejected with 401 live, not only in unit tests
- [x] Authenticated `/api/session` returns 200
- [x] `/api/exec` executes a command and returns exit code 0 with stdout
- [x] `fs_view` (ReadOnly) executes without a confirmation flag
- [x] `proc_execute` (Mutating) returns 428 without confirm and 200 with confirm
- [x] Path traversal outside the project root is rejected with 403
- [x] Unlisted CORS origin preflight is rejected with 403 and no wildcard
- [x] Listed CORS origin preflight returns 204 without a wildcard header
- [x] SSE endpoint requires the bearer token
- [x] `/api/settings` masks the stored api_key
- [x] Harness tears the server down and exits nonzero on any failed check
- [x] Verified run captured with exit code 0

## Verification Evidence

- `bash scripts/sh/api-smoke.sh` reports `Results: 21 passed, 0 failed`, exit code 0,
  reproduced across three consecutive runs to confirm the race is gone.
- `./build/lxace_tests` reports `47 passed, 0 failed in 376 ms`, exit code 0.
- `cmake --build build -j 1` compiles with zero warnings.
