---
id: "002"
title: "Implement -v version flag and robust OS installation"
type: "feature"
status: "done"
created_at: "2026-10-31"
updated_at: "2026-10-31"
---

# [Feature] Implement -v version flag and robust OS installation

## Description
Implement first-class `-v`, `-V`, `--version` CLI flags, dedicated version metadata header, robust OS installation pipeline with CMake install target, and verify execution from arbitrary OS paths.

## Acceptance Criteria
- [x] Create `src/core/version.hpp` with semantic version constants (`1.0.0`, architecture, C++ standard)
- [x] Add `-v`, `-V`, `--version` flags to `src/main.cpp` outputting clean version string and exiting with code 0
- [x] Update `--help` / `-h` in `src/main.cpp` to document the version options
- [x] Add automated unit test verifying version string constant and formatting
- [x] Enhance `CMakeLists.txt` with standard `install(TARGETS lxace RUNTIME DESTINATION bin)`
- [x] Harden `scripts/sh/install.sh` and `scripts/sh/uninstall.sh` with robust directory resolution, atomic installation, and verification
- [x] Install release binary to OS PATH and verify `lxace -v` and `lxace` work from arbitrary directory (`/tmp`)
- [x] Run full test suite (`make test`) and verify 100% passing tests

## Context / Notes
- Target files: `src/core/version.hpp`, `src/main.cpp`, `CMakeLists.txt`, `scripts/sh/install.sh`, `scripts/sh/uninstall.sh`, `tests/test_version.cpp`, `tests/test_main.cpp`
- Dependencies: None (zero external dependencies)
