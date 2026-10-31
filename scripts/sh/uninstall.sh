#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-}"
DEST_DIRS=()

if [ -n "${PREFIX}" ]; then
    if [[ "${PREFIX}" == */bin ]]; then
        DEST_DIRS+=("${PREFIX}")
    else
        DEST_DIRS+=("${PREFIX}/bin")
    fi
else
    if [ -f "/usr/local/bin/lxace" ] && [ -w "/usr/local/bin/lxace" ]; then
        DEST_DIRS+=("/usr/local/bin")
    fi
    if [ -f "/home/user/.local/bin/lxace" ] && [ -w "/home/user/.local/bin/lxace" ]; then
        DEST_DIRS+=("/home/user/.local/bin")
    fi
    if [ -n "${HOME:-}" ] && [ -f "${HOME}/.local/bin/lxace" ] && [ -w "${HOME}/.local/bin/lxace" ]; then
        DEST_DIRS+=("${HOME}/.local/bin")
    fi
fi

echo "=== LXACE Uninstallation ==="
UNINSTALLED=false

for BIN_DIR in "${DEST_DIRS[@]}"; do
    TARGET_BIN="${BIN_DIR}/lxace"
    if [ -f "${TARGET_BIN}" ]; then
        echo "Removing binary: ${TARGET_BIN}..."
        rm -f "${TARGET_BIN}"
        UNINSTALLED=true
        echo "Removed ${TARGET_BIN}."
    fi
done

if [ "${UNINSTALLED}" = true ]; then
    echo "LXACE successfully uninstalled."
else
    echo "No LXACE installation found to remove."
fi
