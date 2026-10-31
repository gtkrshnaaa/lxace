#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
export CXX="${CXX:-g++}"

echo "Building test suite..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"
cmake -DCMAKE_BUILD_TYPE=Debug "${SCRIPT_DIR}"
make -j"$(nproc)" lxace_tests

echo "Running tests..."
sync
chmod +x "${BUILD_DIR}/lxace_tests"
"${BUILD_DIR}/lxace_tests"
