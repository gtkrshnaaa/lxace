#!/usr/bin/env bash
# Emits the 50% host resource caps as shell assignments so compose and scripts share one
# derivation. Hardcoded caps silently exceeded 50% on hosts smaller than the reference
# workstation.
set -euo pipefail

host_cpus="$(nproc 2>/dev/null || echo 2)"
if [ "$host_cpus" -lt 1 ]; then
  host_cpus=1
fi

# Round up so an odd core count still leaves headroom for the host.
docker_cpus=$(( (host_cpus + 1) / 2 ))
if [ "$docker_cpus" -lt 1 ]; then
  docker_cpus=1
fi

host_mem_mb="$(awk '/MemTotal/ { printf "%d", $2 / 1024 }' /proc/meminfo 2>/dev/null || echo 4096)"
if [ -z "$host_mem_mb" ] || [ "$host_mem_mb" -lt 1024 ]; then
  host_mem_mb=4096
fi

docker_mem_mb=$(( host_mem_mb / 2 ))

echo "DOCKER_CPUS=${docker_cpus}.0"
echo "DOCKER_MEM=${docker_mem_mb}M"
echo "LXACE_HOST_CPUS=${host_cpus}"
echo "LXACE_HOST_MEM_MB=${host_mem_mb}"
