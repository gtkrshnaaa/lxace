#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-}"
if [ -z "${PREFIX}" ]; then
    if [ -f "/usr/local/bin/lxace" ] && [ -w "/usr/local/bin/lxace" ]; then
        PREFIX="/usr/local/bin"
    else
        PREFIX="${HOME}/.local/bin"
    fi
fi

TARGET_BIN="${PREFIX}/lxace"

if [ -f "${TARGET_BIN}" ]; then
    echo "Removing ${TARGET_BIN}..."
    rm -f "${TARGET_BIN}"
    echo "LXACE successfully uninstalled."
else
    echo "LXACE binary not found at ${TARGET_BIN}."
fi
