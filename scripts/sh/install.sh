#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
export CXX="${CXX:-g++}"

echo "========================================"
echo "LXACE Engine OS Installation"
echo "========================================"

# 1. Build release binary inside isolated container (50% caps) and export cleanly
echo "Step 1: Compiling release binary in containerized builder..."
mkdir -p "${BUILD_DIR}"
docker build --target artifacts --output type=local,dest="${BUILD_DIR}" "${PROJECT_ROOT}"

if [ ! -f "${BUILD_DIR}/lxace" ]; then
    echo "Error: Containerized build failed, binary not found." >&2
    exit 1
fi

# 2. Determine installation destinations
PREFIX="${PREFIX:-}"
DEST_DIRS=()

if [ -n "${PREFIX}" ]; then
    if [[ "${PREFIX}" == */bin ]]; then
        DEST_DIRS+=("${PREFIX}")
    else
        DEST_DIRS+=("${PREFIX}/bin")
    fi
else
    if [ -w "/usr/local/bin" ]; then
        DEST_DIRS+=("/usr/local/bin")
    fi
    if [ -d "/home/user/.local/bin" ] && [ -w "/home/user/.local/bin" ]; then
        DEST_DIRS+=("/home/user/.local/bin")
    fi
    if [ -n "${HOME:-}" ] && [ -w "${HOME}" ]; then
        USER_LOCAL_BIN="${HOME}/.local/bin"
        # Avoid duplicate entry
        if [[ ! " ${DEST_DIRS[*]} " =~ " ${USER_LOCAL_BIN} " ]]; then
            DEST_DIRS+=("${USER_LOCAL_BIN}")
        fi
    fi
fi

# 3. Atomically install binary to designated locations
echo "Step 2: Installing binary to destination paths..."
for BIN_DIR in "${DEST_DIRS[@]}"; do
    mkdir -p "${BIN_DIR}"
    TARGET_BIN="${BIN_DIR}/lxace"
    echo "  Installing to ${TARGET_BIN}..."
    if command -v install >/dev/null 2>&1; then
        install -m 755 "${BUILD_DIR}/lxace" "${TARGET_BIN}"
    else
        cp "${BUILD_DIR}/lxace" "${TARGET_BIN}.tmp"
        chmod 755 "${TARGET_BIN}.tmp"
        mv -f "${TARGET_BIN}.tmp" "${TARGET_BIN}"
    fi
done

# 4. Verify and configure shell PATH persistence
echo "Step 3: Verifying shell PATH configuration..."
for BIN_DIR in "${DEST_DIRS[@]}"; do
    for PROFILE_FILE in "${HOME}/.bashrc" "${HOME}/.profile" "/home/user/.bashrc" "/home/user/.profile"; do
        if [ -f "${PROFILE_FILE}" ] && [ -w "${PROFILE_FILE}" ]; then
            if ! grep -q "${BIN_DIR}" "${PROFILE_FILE}"; then
                echo "  Adding ${BIN_DIR} to ${PROFILE_FILE}..."
                echo "export PATH=\"${BIN_DIR}:\$PATH\"" >> "${PROFILE_FILE}"
            fi
        fi
    done
done

# 5. Verification probes across system paths
echo "Step 4: Executing verification probes..."
for BIN_DIR in "${DEST_DIRS[@]}"; do
    TARGET_BIN="${BIN_DIR}/lxace"
    if [ -x "${TARGET_BIN}" ]; then
        VERSION_OUTPUT="$("${TARGET_BIN}" -v)"
        echo "  [OK] ${TARGET_BIN}: ${VERSION_OUTPUT}"
    fi
done

# Test execution from arbitrary location (/tmp) using system PATH
RESOLVED_BIN="$(cd /tmp && which lxace)"
GLOBAL_VERSION="$(cd /tmp && lxace -v)"
echo "  [OK] Global PATH resolution: ${RESOLVED_BIN}"
echo "  [OK] Executed from /tmp: ${GLOBAL_VERSION}"

echo "========================================"
echo "Installation verified successfully."
echo "You can now run 'lxace -v' or 'lxace' from any path on the OS."
echo "========================================"
