# Project Architecture

## Overview

`virt-rpi-wasm` is a WebAssembly-based file mount and read system with an HTTP server. It consists of three layers:

1. **WASM Module (C++)** — Exports C functions for message retrieval and file operations
2. **HTTP Server (C++)** — Serves project files (HTML, WASM) on a dynamically selected TCP port
3. **JavaScript UI Layer** — Loads the WASM module, provides file mount/read UI, handles large file streaming

## Build System

The project uses CMake 3.20+ with C++20. Three compilation targets are produced:

| Target | Output | Description |
|--------|--------|-------------|
| `virt-rpi_wasm` (custom) | `build/virt-rpi.wasm` | WebAssembly module (no stdlib, no start files) |
| `run-server` | `build/run-server` | HTTP server binary |
| `run-server-test` | `build/run-server-test` | Server unit test binary |

### Compilation Details

**WASM module** is built with a CMake `custom_command` (avoiding `--dependency-file` incompatibility with `wasm-ld`):

```
clang++ --target=wasm32 -O2 -nostdlib -nostartfiles \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    -o build/virt-rpi.wasm src/main.cc
```

Key flags:
- `--target=wasm32` — Target WebAssembly 32-bit
- `-O2` — Optimization level 2
- `-nostdlib -nostartfiles` — No C standard library; all types are raw C built-in types
- `--export-all` — Export all symbols (needed since `-nostdlib` means no automatic exports)
- `--allow-undefined` — Allow undefined imports (JS will import `memory`)

**Test binaries** link GTest via `add_subdirectory(lib/gtest EXCLUDE_FROM_ALL)`. Both test binaries link GTest::gtest and GTest::gtest_main.

### Library Linking Strategy

- `server_port_lib` — Static library (no `main`), exported via `find_free_port()`. Linked by both `run-server` and `run-server-test` to avoid "multiple definition of main" errors.
- `server_lib` — Static library wrapping cpp-httplib server. Also no `main`, linked via `$<TARGET_OBJECTS:server_lib>`.
- All libraries define `PROJECT_ROOT` compile definition pointing to the source directory.

### Git Submodules

| Submodule | Path | Purpose |
|-----------|------|---------|
| cpp-httplib | `lib/cpp-httplib/` | Header-only HTTP library (server + client) |
| Google Test | `lib/gtest/` | Unit testing framework (v1.14.0) |

## File Layout

```
.
├── src/main.cc              → Minimal stub; includes file_manager.cc
├── src/file_manager.cc      → All 10 extern "C" exports
├── server/server.cc         → HTTP server (main)
├── server/server_port.cc    → Port scanner (no main)
├── server/tests/test_server.cc → Server GTest tests (dual-mode)
├── tests/test_main.cc       → WASM module GTest tests (11 tests)
├── js/main.js               → JavaScript WASM loader + UI logic
├── index.html               → HTML entry page
├── CMakeLists.txt           → Build configuration
├── lib/cpp-httplib/         → Git submodule (header-only HTTP library)
└── lib/gtest/               → Git submodule (Google Test v1.14.0)
```

## Key Design Decisions

1. **C Types Only in WASM**: Since `-nostdlib` is used, all extern "C" exports use raw C types (`unsigned char*`, `unsigned long`, `unsigned int`). No std::string, no std::vector.
2. **JavaScript-Side State**: `fileMounted` (boolean) and `fileLength` (number) are the source of truth for UI behavior in JavaScript, avoiding WASM ABI issues (e.g., `get_file_length()` returns 0 on wasm32 due to `unsigned long` ABI).
3. **Large File Streaming**: Files > 100 MB use Blob URL (`URL.createObjectURL(file)` + `fetch(blobUrl)` chunked streaming) to avoid Linux OS file handle expiration. Small files use `File.arrayBuffer()` directly.
4. **Dual-Mode Server Tests**: `server/tests/test_server.cc` supports both GTest mode and fallback assert/cout mode when GTest is unavailable.
5. **Pre-commit Verification Gate**: CLAUDE.md mandates `cmake --build build` and `ctest --test-dir build -V` before any commit.
