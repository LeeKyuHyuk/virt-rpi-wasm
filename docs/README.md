# docs — Developer Documentation

This directory contains developer-facing documentation for the `virt-rpi-wasm` project. All documents are written in Markdown and cover every feature and the full unit test suite.

## Table of Contents

| File | Topic |
|------|-------|
| [01-project-architecture.md](01-project-architecture.md) | Build system, library linking, git submodules, file layout, key design decisions |
| [02-greet-message.md](02-greet-message.md) | Greet message WASM exports (`get_message()`, `get_message_length()`), JS interaction |
| [03-file-mount.md](03-file-mount.md) | File mount exports (`load_file()`, `get_file_buffer()`, `get_file_length()`, `free_file()`), small/large file streaming, auto-unmount, JS-side state |
| [04-read-at-offset.md](04-read-at-offset.md) | `read_at_offset()` implementation, decision flow, error states, offset parsing (decimal + 0x-hex), clamping, hex formatting |
| [05-http-server.md](05-http-server.md) | Port scanning (`find_free_port()`), cpp-httplib server, file serving, server lifecycle, dual-mode test |
| [06-unit-tests.md](06-unit-tests.md) | All 12 tests (11 WASM + 1 server) in table format, decision tree coverage, state transitions, dual-mode notes |

## Folder Description

The `docs/` folder documents the complete feature set of `virt-rpi-wasm` — a WebAssembly-based file mount and read system with an HTTP server. The documentation is organized by feature and layer:

- **01-project-architecture.md** — Big-picture overview: how the project is built, how the three compilation units (WASM module, HTTP server, unit tests) relate, and key design decisions (C types only, JS-side state, large file streaming, pre-commit verification).
- **02-greet-message.md** — The original (pre-file-mount) feature: a hardcoded greeting message exported from WASM and displayed via `console.log`.
- **03-file-mount.md** — File mount feature: `extern "C"` exports for loading and freeing files, JavaScript interaction (small files via `File.arrayBuffer()`, large files >100 MB via Blob URL streaming), auto-unmount on double-mount, and JS-side state management.
- **04-read-at-offset.md** — Read-at-offset feature: `read_at_offset()` implementation details, offset parsing (decimal and `0x`-prefixed hex), offset clamping at file boundary, and hex output formatting.
- **05-http-server.md** — HTTP server feature: port scanning via `find_free_port()`, cpp-httplib usage, file serving, server lifecycle, and dual-mode (GTest / fallback) server test.
- **06-unit-tests.md** — Complete unit test documentation covering all 11 WASM module tests and 1 server integration test, organized by test case, decision tree coverage, and state transitions.
