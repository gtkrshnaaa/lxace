#!/usr/bin/env bash
# End-to-end smoke verification against the running engine HTTP surface.
#
# Boots the engine headless on loopback with a synthetic bearer token, exercises
# every security and functional contract that the unit suite can only assert in
# isolation, then tears the server down. Exits nonzero if any check fails.
#
# The token here is synthetic by design. The engine has no outbound LLM transport,
# so a real provider key has no code path to travel and is never required.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${ROOT}/build/lxace"
PORT="${LXACE_SMOKE_PORT:-18099}"
BASE="http://127.0.0.1:${PORT}"
TOKEN="lxace-smoke-$(date +%s)-synthetic"
ALLOWED_ORIGIN="https://allowed.example"
BLOCKED_ORIGIN="https://blocked.example"
LOG="$(mktemp)"

PASS=0
FAIL=0
SERVER_PID=""

cleanup() {
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -f "${LOG}"
}
trap cleanup EXIT

check() {
    local label="$1" expected="$2" actual="$3" context="${4:-}"
    if [[ "${actual}" == "${expected}" ]]; then
        printf '  [PASS] %s\n' "${label}"
        PASS=$((PASS + 1))
    else
        printf '  [FAIL] %s (expected %s, got %s)\n' "${label}" "${expected}" "${actual}"
        if [[ -n "${context}" ]]; then
            printf '         response: %s\n' "${context}"
        fi
        FAIL=$((FAIL + 1))
    fi
}

status() {
    curl -sS -o /dev/null -w '%{http_code}' -m 5 "$@"
}

body() {
    curl -sS -m 5 "$@"
}

if [[ ! -x "${BIN}" ]]; then
    printf 'error: %s not found, run make build first\n' "${BIN}" >&2
    exit 1
fi

printf 'Booting engine on %s (project: %s)\n' "${BASE}" "${ROOT}"

(
    cd "${ROOT}" || exit 1
    LXACE_API_TOKEN="${TOKEN}" \
    LXACE_ALLOWED_ORIGINS="${ALLOWED_ORIGIN}" \
    exec "${BIN}" --headless --port "${PORT}"
) >"${LOG}" 2>&1 &
SERVER_PID=$!

ready=0
for _ in $(seq 1 100); do
    if [[ "$(status "${BASE}/api/session")" == "401" ]]; then
        ready=1
        break
    fi
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        printf 'error: server exited during startup\n' >&2
        cat "${LOG}" >&2
        exit 1
    fi
    sleep 0.1
done

if [[ "${ready}" -ne 1 ]]; then
    printf 'error: server did not become ready on %s\n' "${BASE}" >&2
    cat "${LOG}" >&2
    exit 1
fi

printf 'Server ready, running checks\n'

# Auth gate
check "unauthenticated request rejected" "401" \
    "$(status "${BASE}/api/session")"
check "wrong bearer token rejected" "401" \
    "$(status -H "Authorization: Bearer wrong-token" "${BASE}/api/session")"
check "authenticated session returns 200" "200" \
    "$(status -H "Authorization: Bearer ${TOKEN}" "${BASE}/api/session")"
check "WWW-Authenticate advertised on 401" "Bearer" \
    "$(curl -s -D - -o /dev/null -m 5 "${BASE}/api/session" 2>/dev/null \
        | tr -d '\r' | awk 'tolower($1) == "www-authenticate:" {print $2}')"

# SSE endpoint must not bypass the gate
check "SSE endpoint requires token" "401" \
    "$(status -m 2 "${BASE}/api/events")"

# CORS preflight
check "unlisted origin preflight rejected" "403" \
    "$(status -X OPTIONS -H "Origin: ${BLOCKED_ORIGIN}" "${BASE}/api/session")"
check "listed origin preflight accepted" "204" \
    "$(status -X OPTIONS -H "Origin: ${ALLOWED_ORIGIN}" "${BASE}/api/session")"
check "preflight never returns wildcard" "0" \
    "$(curl -s -D - -o /dev/null -m 5 -X OPTIONS -H "Origin: ${ALLOWED_ORIGIN}" \
        "${BASE}/api/session" 2>/dev/null | tr -d '\r' | grep -ci 'access-control-allow-origin: \*' || true)"

# Command execution
exec_raw="$(body -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
    -d '{"command":"echo lxace-smoke-ok"}' "${BASE}/api/exec")"
exec_code="$(printf '%s' "${exec_raw}" | jq -r '.exit_code // "missing"')"
check "exec runs command with exit 0" "0" "${exec_code}" "${exec_raw}"
check "exec returns command stdout" "lxace-smoke-ok" \
    "$(printf '%s' "${exec_raw}" | jq -r '.stdout // "missing"' | tr -d '\n')" "${exec_raw}"
check "exec propagates nonzero exit code" "7" \
    "$(body -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
        -d '{"command":"exit 7"}' "${BASE}/api/exec" | jq -r '.exit_code // "missing"')"

# Filesystem surface
check "fs list returns 200" "200" \
    "$(status -H "Authorization: Bearer ${TOKEN}" "${BASE}/api/fs/list")"
check "fs list includes src directory" "src" \
    "$(body -H "Authorization: Bearer ${TOKEN}" "${BASE}/api/fs/list" \
        | jq -r '[.[] | select(.name == "src")] | .[0].name')"
check "path traversal rejected" "403" \
    "$(status -H "Authorization: Bearer ${TOKEN}" "${BASE}/api/fs/list?path=../../etc")"

# Tool risk gating
check "readonly tool runs without confirm" "true" \
    "$(body -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
        -d '{"tool":"fs_view","arguments":{"path":"README.md","max_bytes":256}}' \
        "${BASE}/api/tools/execute" | jq -r '.success // "missing"')"
check "mutating tool requires confirm" "428" \
    "$(status -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
        -d '{"tool":"proc_execute","arguments":{"command":"echo gated"}}' \
        "${BASE}/api/tools/execute")"
confirmed_raw="$(body -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
    -d '{"tool":"proc_execute","arguments":{"command":"echo confirmed"},"confirm":true}' \
    "${BASE}/api/tools/execute")"
check "mutating tool runs with confirm" "true" \
    "$(printf '%s' "${confirmed_raw}" | jq -r '.success // "missing"')" "${confirmed_raw}"
check "unknown tool returns 404" "404" \
    "$(status -X POST -H "Authorization: Bearer ${TOKEN}" -H 'Content-Type: application/json' \
        -d '{"tool":"no_such_tool","arguments":{}}' "${BASE}/api/tools/execute")"

# Secret handling
settings_raw="$(body -H "Authorization: Bearer ${TOKEN}" "${BASE}/api/settings")"
check "settings masks api_key" "true" \
    "$(printf '%s' "${settings_raw}" | jq -r 'has("api_key_masked")')" "${settings_raw}"
check "settings omits raw api_key" "false" \
    "$(printf '%s' "${settings_raw}" | jq -r 'has("api_key")')" "${settings_raw}"
check "settings omits raw base_url" "false" \
    "$(printf '%s' "${settings_raw}" | jq -r 'has("base_url")')" "${settings_raw}"

printf '\nResults: %d passed, %d failed\n' "${PASS}" "${FAIL}"

if [[ "${FAIL}" -ne 0 ]]; then
    printf '\nEngine log:\n' >&2
    cat "${LOG}" >&2
    printf 'Smoke verification FAILED\n' >&2
    exit 1
fi

printf 'Smoke verification PASSED\n'
exit 0
