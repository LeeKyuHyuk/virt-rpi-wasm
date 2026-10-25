# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 0. Never Assume Project Intent from Name

**Do not implement or propose based solely on the project name.**

- The project name (`virt-rpi-wasm`) does not define requirements.
- Always wait for explicit direction from the user before suggesting or implementing anything.
- Suggestions are allowed — implementation without explicit approval is not.

## 0.1 Plan Output: Always Write to `.claude/plans/`

When creating a plan, the plan file **MUST** be written to this project's `.claude/plans/` directory:

```
.claude/plans/<plan-name>.md
```

- **Always create the directory first** — before writing any plan, run `mkdir -p .claude/plans` at the project root.
- **Never write plans outside of this project directory.** Writing to `~/.claude/plans/` (global user directory) is a violation.
- If a plan for the same task already exists, overwrite it rather than creating a duplicate.
- **Post-write verification:** after writing, `ls .claude/plans/` to confirm the file landed in the project-local directory, not `~/.claude/plans/`. If it landed elsewhere, move it immediately.

## 0.2 Git Commits: Only Staged Files, Conventional Commits Format

When asked to commit:

- **Do not run `git add` without explicit instruction from the user.** Only commit files the user has explicitly staged.
- **Do not run `git push` without explicit instruction from the user.**
- **Always verify `git status` before committing** — list what will be committed and confirm with the user if unsure.
- **Commit message format follows `.claude/skills/conventional-commits.md`** (Conventional Commits 1.0.0):
  ```
  <type>[optional scope]: <description>

  [optional body]

  [optional footer(s)]
  ```
  Types: `fix`, `feat`, `!` (breaking change), plus accepted types: `build`, `chore`, `ci`, `docs`, `style`, `refactor`, `perf`, `test`, `revert`.
- **The `[optional body]` is mandatory.** Every commit message MUST include a body section describing what changed and why, not just the subject line.
- Present the chosen type, scope, and body to the user before committing so they can confirm it is correct.

**Pre-commit verification gate:** Before committing, you MUST run `cmake --build build` and `ctest --test-dir build -V` (or `--tests-regex <name>` for a single test). **If any build step fails or any test fails, do NOT commit.** The commit proceeds only when both build and all tests pass. If a test fails, fix the failure and re-verify before attempting another commit.

## 0.3 Build Stack: Vanilla C/C++ via LLVM/Clang, No Emscripten

**WebAssembly for this project is produced from Vanilla C/C++ compiled with LLVM/Clang.**

- **Do not use Emscripten.** Only raw `clang` (or `clang++`) targeting `wasm32` is acceptable.
- Do not pull in Emscripten SDKs, ports, or compatibility shims.
- If you encounter references to Emscripten in tooling or dependencies, surface them for the user's decision — do not adopt them silently.

## 0.4 C++ Code: Always Follow Google C++ Style Guide

**Any C++ code written, reviewed, or refactored in this project MUST follow `.claude/skills/google-cpp-style-guide.md`.**

When producing C++ code (`.h`, `.cc`, `.cpp`):
- **Load the skill** — invoke the `google-cpp-style-guide` skill before writing or reviewing any C++ code.
- **Header files** must be self-contained, end in `.h`, and use `#define` guards (`_PROJECT_PATH_FILE_H_`).
- **Include What You Use** — no transitive inclusions; no forward declarations where a real include works.
- **Target C++20** — do not use C++23 features. No non-standard extensions.
- **Naming** — File: `snake_case`; Class/Struct: `CamelCase`; Variable/Function: `snake_case`/`CamelCase`; Constant: `snake_case`; Macro: `UPPER_SNAKE_CASE`.
- **RAII for all resources** — load the `cpp-raii` skill before writing or reviewing C++ code.
  - **Dynamic allocation:** any use of `new`, `malloc`, or other dynamic allocation MUST be immediately wrapped in an RAII wrapper (smart pointer or custom RAII class). Bare raw pointers from allocation are forbidden.
  - **Acquire-once-release-every-time resources:** any resource that must be released at the end of its scope (file I/O, mutex locks, network sockets, CUDA handles, glib objects, POSIX file descriptors, JNI references, etc.) MUST be managed via RAII. Manual `open`/`close`, `lock`/`unlock`, `allocate`/`free` pairs are forbidden.
  - **Smart pointers:** use `std::unique_ptr` for sole ownership, `std::shared_ptr` only when genuine shared ownership is required, raw pointers only for non-owning references.
- **`const`** — prefer `const` and `constexpr` wherever possible.
- **`auto`** — use for iteration and obvious types; not when it obscures meaning.
- **Formatting** — 4 spaces (no tabs); braced initializers; 2-space continuation indentation.
- **Functions** — prefer short functions; default arguments allowed.
- **Comments** — `//` for inline, `///` for docs; TODOs must include owner and issue number.

If project-specific C++ conventions exist and conflict, surface the conflict for the user's decision — do not silently pick one.

## 0.4-1 RAII Exception for WASM Target

**Any compilation unit compiled with `-nostdlib -nostartfiles` is exempt from the RAII rule above.** This exemption covers the current files (`src/main.cc`, `src/file_manager.cc`) and any future files added to the WASM build.

### Why RAII does not apply

The WASM module has zero dynamic allocation, zero system resources, and zero standard library:

- **No dynamic allocation exists:** The WASM module uses no `new`, `malloc`, or vectors. JavaScript copies file data into WASM linear memory directly via `Uint8Array`, and the WASM module only records a pointer and length. "Unmount" simply sets the pointer to `nullptr` and length to `0`.
- **No system resources:** No file I/O, sockets, mutexes, or handles exist in the WASM module. RAII has nothing to apply to.
- **No standard library:** `<memory>`, `<string>`, `<vector>` — none of these are available. Smart pointers and `std::unique_ptr` cannot be used.

### Host C++ is NOT exempt

All other compilation units (server, tests) compile with full stdlib support and MUST follow RAII rules without exception. This includes `server/server.cc`, `server/server_port.cc`, `tests/test_main.cc`, and `server/tests/test_server.cc`.

## 0.6 Documentation Update After Feature Implementation

After a feature is implemented **and** its unit tests pass (`ctest --test-dir build -V`), the corresponding documentation in `docs/` MUST be updated. This is mandatory — not optional.

```
Feature Implementation → Unit Test Verification → docs Update
```

### Workflow

1. **Verify unit tests pass** — Run `ctest --test-dir build -V`. If any test fails, do NOT update docs yet; fix the failure first.
2. **Update or create the corresponding doc file** — Check whether `docs/<feature-name>.md` already exists:
   - **If it exists:** Review the source code changes and update the documentation to reflect the new/changed behavior, API signatures, decision flows, error states, and JavaScript interaction patterns.
   - **If it does NOT exist:** Create a new documentation file following the naming convention `NN-feature-name.md` (e.g., `07-new-feature.md`). Follow the existing documentation style: WASM exports table, implementation details, decision flow, error states, JavaScript interaction.
3. **Update `docs/README.md`** — Add or update the entry in the Table of Contents table, and update the folder description if the new feature introduces a new category.

### Naming Convention

| File | Topic |
|------|-------|
| `01-project-architecture.md` | Build system, file layout, key design decisions |
| `02-greet-message.md` | Greet message feature |
| `03-file-mount.md` | File mount feature |
| `04-read-at-offset.md` | Read-at-offset feature |
| `05-http-server.md` | HTTP server feature |
| `06-unit-tests.md` | Complete unit test documentation |

When creating a new feature doc:
- Assign the next available number (`ls docs/ | grep -oE '^[0-9]+-' | sort -n | tail -1 | awk -F- '{print $1+1}'`)
- Prefix with zero-padded two-digit number
- Use a descriptive kebab-case name
- Write to `docs/NN-feature-name.md`

### Documentation Content Requirements

Every feature doc MUST include (where applicable):
- **Overview** — What the feature does, one paragraph
- **WASM Exports** — Table of all `extern "C"` functions (name, parameters, return value, description)
- **Implementation Details** — Source file, line numbers, key code snippets
- **Decision Flow** — Error paths, guard clauses, boundary conditions (as a diagram or table)
- **Error States** — When and how errors manifest (status values, return codes)
- **JavaScript Interaction** — How JS calls the WASM exports, state management, UI behavior
- **Notes** — Any caveats, known limitations, or design rationale

### `docs/README.md` Update

After creating or updating a feature doc:
1. Add or update the row in the Table of Contents table (File column, Topic column)
2. Update the folder description paragraph if the feature introduces a new concept not covered elsewhere
3. Ensure the file links resolve correctly (relative paths, correct filenames)

### When This Rule Is Skipped

- Trivial operations (typo fixes, comment updates, CLAUDE.md edits)
- The feature has no user-facing API (internal refactoring with no API change)
- The user explicitly waives the requirement (uncommon, must be explicit)

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## 5. Build, Test & Run

### Build

```bash
cmake -B build               # Configure (creates build/ directory)
cmake --build build          # Build everything (WASM + server + tests)
```

Or equivalently:

```bash
mkdir -p build && cd build
cmake ..
make -j
```

The build produces three artifacts:

| Target | Output | Description |
|--------|--------|-------------|
| `virt-rpi_wasm` (custom target) | `build/virt-rpi.wasm` | WebAssembly module, compiled with `clang++ --target=wasm32 -O2 -nostdlib -nostartfiles -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined` |
| `run-server` | `build/run-server` | HTTP server binary serving project files from project root |
| `run-server-test` | `build/run-server-test` | Unit test binary |

### Run the server

```bash
./build/run-server
# Outputs: "Listening on http://localhost:<port>"
# Opens http://localhost:<port>/index.html in browser
```

The server auto-finds a free TCP port starting at 8000. Watch the output to see which port was selected.

### Run all tests

```bash
ctest --test-dir build        # Run all registered CTest tests
ctest --test-dir build -V     # Verbose output (shows per-test details)
ctest --test-dir build --output-on-failure  # Only print output for failing tests
```

### Run a single test

```bash
ctest --test-dir build --tests-regex server-basic -V
```

This runs the `server-basic` test, which starts a temporary HTTP server in a background thread, fetches `/index.html` and `/build/virt-rpi.wasm` to verify they are served correctly, and tests port fallback when port 8000 is occupied.

### Project architecture

```
.
├── src/main.cc              → WebAssembly module (extern "C" exports)
├── server/server.cc         → HTTP server binary (cpp-httplib, main)
├── server/server_port.cc    → TCP port scanner library (no main)
├── server/tests/test_server.cc → Unit tests (cpp-httplib Client)
├── js/main.js               → JavaScript WASM loader + file-mount + read-at-offset UI
├── index.html               → HTML entry page (file input, debug checkbox, mount/unmount/read buttons)
├── lib/cpp-httplib/         → Git submodule (header-only HTTP library)
└── CMakeLists.txt           → Build configuration (CMake 3.20+, C++20)
```

**Three compilation units:**

1. **WASM module** (`src/main.cc`): Compiles to `virt-rpi.wasm` with no standard library (`-nostdlib -nostartfiles`). Exports `get_message()`, `get_message_length()`, and file-mount functions (`load_file()`, `get_file_buffer()`, `get_file_length()`, `free_file()`, `set_process_flag()`, `get_process_flag()`, `get_status()`, `read_at_offset()`) via `extern "C"`. Because `-nostdlib` is used, all types are raw built-in C types: `const unsigned char*` for data pointers, `unsigned long` for lengths, `unsigned int` for file sizes (supports up to 4.3 GB). JavaScript fetches the `.wasm` file, instantiates it, reads the exported `memory` buffer, copies file data into it via `Uint8Array.set()`, and calls `load_file()` to register the pointer + length. For large files (> 100 MB), file data is streamed into WASM memory via Blob URL (`URL.createObjectURL(file)` + `fetch(blobUrl)` chunked streaming with `Response.body.getReader()`). For small files (≤ 100 MB), `File.arrayBuffer()` reads the entire file at once.

2. **HTTP server** (`server/server.cc` + `server/server_port.cc`): A two-library hierarchy — `server_port_lib` (port scanner, no main) is linked by `server_lib` (httplib server with main) which is linked by `run-server` (executable). `find_free_port` scans TCP ports by attempting socket connects; a successful connect means the port is in use, a failure means it is free.

3. **Unit tests** (`server/tests/test_server.cc`): Links against `server_lib` and `server_port_lib`. Starts an HTTP server in a background thread, polls for readiness, then uses cpp-httplib `Client` to verify `GET /index.html` (status 200), `GET /build/virt-rpi.wasm` (status 200, non-empty body), and port fallback when port 8000 is occupied. Server is shut down via `svr.stop()` (unblocks `listen()`), then the thread is joined.

**Key CMake design decisions:**
- WASM uses a `custom_command` (avoids `--dependency-file` incompatibility with `wasm-ld`)
- `server_port_lib` is a static library with no `main`, so it can be linked by both `run-server` and `run-server-test` without "multiple definition of main" errors
- `server_lib` is also a static library (not an executable) to avoid duplicate `main` when linked by the test binary
- All libraries define `PROJECT_ROOT` to the source directory so the server serves the correct files
- GTest is linked by `run-server-test` and managed by CTest. New WASM export unit tests (e.g., testing `load_file()`, `get_file_buffer()`) are written in GTest style and registered with `add_test()` so CTest runs them alongside existing integration tests.

## 0.5 Unit Test Workflow

When requesting a feature implementation, the following workflow is mandatory (except for trivial operations: typo fixes, comment updates, CLAUDE.md edits):

```
Plan → Implement → Verify → Unit Test → Unit Test run
```

1. **Plan** — Write a plan to `.claude/plans/<feature-name>.md`. Define scope, success criteria, and verification steps.
2. **Implement** — Write the implementation code. Follow all existing CLAUDE.md rules (Google C++ style, RAII, etc.).
3. **Verify** — Build (`cmake --build build`) and run existing tests (`ctest --test-dir build`). For UI features, manually verify in browser (fetch `/index.html`, confirm UI works).
4. **Unit Test** — Write GTest tests for new functionality. Apply the `unit-test` skill (`.claude/skills/unit-test.md`).
5. **Unit Test run** — Run `ctest --test-dir build -V` to confirm all tests (existing + new) pass.

**Test file location convention:**

- **WASM module tests** (testing `load_file()`, `read_at_offset()`, etc.) go in the project `tests/` directory (e.g., `tests/test_main.cc`).
- **Server tests** (testing HTTP endpoints, port scanning) go in `server/tests/` (e.g., `server/tests/test_server.cc`).

**Trivial operations** (typo fixes, comment updates, CLAUDE.md edits) are exempt from this workflow.

When a new test breaks an existing test, fix the existing test together with the new implementation (use `fix!` or `feat!` commit type with a breaking change footer).

### GTest Integration

This project uses Google Test (GTest) managed by CTest. New unit tests for WASM exports (e.g., `load_file()`, `get_file_buffer()`, `read_at_offset()`) should be written in GTest style following the AAA pattern (`.claude/skills/unit-test.md`).

Example GTest for WASM export:
```cpp
TEST(FileMount, ReturnsZeroOnSuccess) {
    // Arrange
    unsigned char fileData[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    unsigned long length = sizeof(fileData);

    // Act
    int result = load_file(0, length);

    // Assert
    EXPECT_EQ(0, result);
    // Verify file is registered in WASM state
    EXPECT_EQ(fileData[0], read_at_offset(0, 1));
}
```

Register new GTest tests in CMakeLists.txt with `add_test()` so CTest manages them alongside existing integration tests.

### Tests Worth Writing (anti-coverage)

Do not write tests that only prove a public API exists. The following patterns are **forbidden** — they increase maintenance cost without catching bugs:

- **Getter/setter tests**: `EXPECT_EQ(42, object.GetId())` — the getter is one line; testing it tests nothing.
- **Simple assignment tests**: `object.SetName("foo"); EXPECT_EQ(std::string("foo"), object.GetName())` — this tests `std::string` assignment, which is not your code.
- **Trivial constructor tests**: `Foo f; EXPECT_EQ(0, f.value)` — where `value` is just an initialized member variable.

A test is worth writing only when it verifies **a non-trivial behavior you control**:

1. **A decision point** (if/else branches, boundary conditions, error paths)
2. **A state transition** (before → after, with meaningful change)
3. **An interaction between two components** (not just one in isolation)
4. **A failure mode** (what happens when inputs are invalid, resources exhausted)

When writing a test, ask: "What bug does this catch that I would not notice?" If the answer is "the test file stops compiling when I rename a function," skip it.

### JavaScript layer: Testing

The JavaScript layer (`js/main.js`) provides the following capabilities beyond the original `get_message()` display:

**File Mount:**
- Select a file via the file input, click "Mount" to load it into WASM memory.
- Small files (≤ 100 MB): read via `File.arrayBuffer()` — single synchronous read.
- Large files (> 100 MB): stream via Blob URL (`URL.createObjectURL(file)` + `fetch(blobUrl)` with `Response.body.getReader()`) to avoid Linux OS file handle expiration issues.
- WASM memory is grown (`wasmMemory.grow()`) to accommodate the file, and data is copied into memory at offset 0.
- `load_file(ptr, length)` is called to register the pointer and length in WASM.

**Read at Offset:**
- After a file is mounted, the "Read at Offset" section becomes visible.
- User enters an offset (decimal or `0x`-prefixed hex) and selects a read size (1, 2, 4, or 8 bytes).
- Clicking "Read" reads bytes from WASM linear memory at the specified offset and displays them as hex.
- Offset clamping: if the read would go past the end of the file, the size is clamped to available bytes.
- Offset is validated against JS-side `fileLength`; an alert is shown if the offset is beyond file end.

**Unmount:**
- "Unmount" button calls `free_file()` in WASM and resets JS-side state.
- The Read at Offset section is hidden after unmount.

**Auto-Unmount on Double-Mount:**
- If a file is already mounted and the user mounts another file, the existing file is auto-unmounted first.
- This prevents Blob URL leaks: when mounting a new large file, the old Blob URL is revoked via `URL.revokeObjectURL()` during unmount.

**Debug Hex Output:**
- A "Debug" checkbox in the UI, when checked, prints the first 64 bytes of a mounted file as space-separated uppercase hex pairs to the browser console.
- Reading at offset also prints the hex output to the console.

**JavaScript State Management (anti-pattern fixes):**
- `fileMounted` (boolean): Tracks mount state in JavaScript instead of relying on WASM `get_status()` string comparison. The WASM `get_status()` can be unreliable due to timing issues.
- `fileLength` (number): Tracks the mounted file size in JavaScript instead of calling WASM `get_file_length()`. This avoids ABI issues with `unsigned long` on wasm32 where the return value was observed to be 0.
- `fileBuffer` is hardcoded to 0: File data always starts at memory address 0 in WASM, so the buffer pointer from WASM is unnecessary — JavaScript reads directly from the WASM memory buffer.
- These JS-side state variables are the source of truth for UI behavior; WASM state is secondary.