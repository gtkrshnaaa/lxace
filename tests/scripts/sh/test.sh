#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
export CXX="${CXX:-g++}"

echo "Building test suite..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake -DCMAKE_BUILD_TYPE=Debug "${PROJECT_ROOT}"
make -j"$(nproc)" lxace_tests

echo "Running tests..."
sync
chmod +x "${BUILD_DIR}/lxace_tests"

# Handle potential ETXTBSY from immediate linker flush
for i in {1..5}; do
    if "${BUILD_DIR}/lxace_tests"; then
        exit 0
    fi
    sleep 0.2
done
exit 1
