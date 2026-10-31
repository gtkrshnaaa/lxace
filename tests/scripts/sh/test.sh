#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
export CXX="${CXX:-g++}"

echo "Executing test suite inside container (50% CPU and memory cap)..."
docker build -t lxace-engine:latest "${PROJECT_ROOT}"
docker run --rm \
    --cpus 4.0 \
    --memory 3500M \
    --entrypoint /usr/local/bin/lxace_tests \
    lxace-engine:latest
