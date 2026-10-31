#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
export CXX="${CXX:-g++}"

echo "=== LXACE Installation ==="

# 1. Build the release binary
echo "Building release binary..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake -DCMAKE_BUILD_TYPE=Release "${PROJECT_ROOT}"
make -j"$(nproc)" lxace

if [ ! -f "${BUILD_DIR}/lxace" ]; then
    echo "Error: Build failed, binary not found." >&2
    exit 1
fi

# 2. Determine installation destination
PREFIX="${PREFIX:-}"
if [ -z "${PREFIX}" ]; then
    if [ -w "/usr/local/bin" ]; then
        PREFIX="/usr/local/bin"
    elif [ -d "/home/user/.local/bin" ] && [ -w "/home/user/.local/bin" ]; then
        PREFIX="/home/user/.local/bin"
    else
        PREFIX="${HOME}/.local/bin"
    fi
fi

mkdir -p "${PREFIX}"
TARGET_BIN="${PREFIX}/lxace"

echo "Installing binary to ${TARGET_BIN}..."
cp "${BUILD_DIR}/lxace" "${TARGET_BIN}"
chmod 755 "${TARGET_BIN}"

# 3. Verify PATH availability
case ":${PATH}:" in
    *:"${PREFIX}":*)
        echo "Prefix ${PREFIX} is already in PATH."
        ;;
    *)
        echo "Notice: ${PREFIX} is not in your current PATH."
        echo "Add the following line to your ~/.bashrc or ~/.profile:"
        echo "  export PATH=\"${PREFIX}:\$PATH\""
        ;;
esac

# 4. Verification test
echo "Verifying installation..."
if "${TARGET_BIN}" --help >/dev/null 2>&1; then
    echo "Successfully installed LXACE to ${TARGET_BIN}"
    echo "You can now run 'lxace' from anywhere in your terminal."
else
    echo "Warning: Binary installed but returned error on execution." >&2
fi
