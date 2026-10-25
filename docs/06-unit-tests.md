# Unit Tests

## Overview

The project uses Google Test (GTest v1.14.0) as a git submodule, managed by CTest. All tests are registered via `add_test()` in CMakeLists.txt. Two test binaries are built:

| Test Binary | Source | Tests |
|-------------|--------|-------|
| `run-server-test` | `server/tests/test_server.cc` | 1 server integration test (dual-mode) |
| WASM module tests | `tests/test_main.cc` | 11 WASM module tests |

Both binaries link GTest via `add_subdirectory(lib/gtest EXCLUDE_FROM_ALL)`. The WASM module tests link `file_manager.cc` directly (not compiled separately) so they access the same state variables.

## Running Tests

```bash
# All registered tests
ctest --test-dir build

# Verbose (per-test details)
ctest --test-dir build -V

# Only failing tests
ctest --test-dir build --output-on-failure

# Single test
ctest --test-dir build --tests-regex ReadAtOffsetFailsWhenIdle -V
```

---

## WASM Module Tests

**Source**: `tests/test_main.cc` (187 lines, 11 tests)

The tests exercise all decision points and state transitions in `file_manager.cc`:

### Test Cases

| # | Test Name | Category | What It Verifies |
|---|-----------|----------|------------------|
| 1 | `FileManager.RejectsZeroLength` | Error path | `load_file()` returns `-1` and sets status to `"error"` when `length == 0` |
| 2 | `FileManager.MountsValidFile` | Happy path | `load_file()` returns `0` and sets status to `"file_loaded"` for valid input |
| 3 | `FileManager.FreeReturnsToIdle` | State transition | `free_file()` resets status to `"idle"` |
| 4 | `FileManager.ReadAtOffsetFailsWhenIdle` | Guard clause | `read_at_offset()` returns `0` and sets status to `"error"` when no file is mounted |
| 5 | `FileManager.ReadsNormalBytes` | Happy path | `read_at_offset()` returns requested size (4) for a normal read within bounds |
| 6 | `FileManager.ReadAtExactEndReturnsZero` | Boundary | `read_at_offset()` returns `0` when offset is exactly at file length (no bytes remaining) |
| 7 | `FileManager.ReadBeyondFileEndReturnsZero` | Guard clause | `read_at_offset()` returns `0` when offset is beyond file length |
| 8 | `FileManager.ClampsReadSizeAtFileEnd` | Boundary / clamp | `read_at_offset()` returns clamped size (2) when requested size exceeds available bytes |
| 9 | `FileManager.ReadFailsAfterFree` | State transition | `read_at_offset()` returns `0` after a file has been unmounted |
| 10 | `FileManager.ReadPartialFromMiddle` | Happy path | `read_at_offset()` returns full requested size (3) from a mid-file offset within bounds |
| 11 | `FileManager.MountsAfterFree` | State reset | File can be re-mounted after `free_file()` (full lifecycle: mount → free → mount) |

### Decision Tree Coverage

Each test exercises a specific branch in the `read_at_offset()` decision tree:

```
read_at_offset(buffer, offset, size)
    │
    ├─ No file mounted?
    │   └── Test #4: ReadAtOffsetFailsWhenIdle
    │
    ├─ Offset at or past file end?
    │   └── Test #6: ReadAtExactEndReturnsZero  (offset == file_len)
    │   └── Test #7: ReadBeyondFileEndReturnsZero  (offset > file_len)
    │
    ├─ Size > available?
    │   └── Test #8: ClampsReadSizeAtFileEnd  (returns clamped)
    │
    ├─ Size <= available?
    │   └── Test #5: ReadsNormalBytes  (full read from offset 0)
    │   └── Test #10: ReadPartialFromMiddle  (partial read from offset 2)
    │
    └─ After unmount?
        └── Test #9: ReadFailsAfterFree  (0 return value)
```

### `load_file()` Decision Tree

```
load_file(src, length)
    │
    ├─ length == 0?
    │   └── Test #1: RejectsZeroLength  (returns -1, status = "error")
    │
    └─ Valid length?
        └── Test #2: MountsValidFile  (returns 0, status = "file_loaded")
```

### State Transition Tests

| Test | Transition | What Changes |
|------|-----------|--------------|
| `FreeReturnsToIdle` | `"file_loaded"` → `"idle"` | `free_file()` resets all state |
| `ReadFailsAfterFree` | No file → read returns 0 | Guards against reading after unmount |
| `MountsAfterFree` | `"file_loaded"` → `"idle"` → `"file_loaded"` | Full lifecycle: mount, free, re-mount with different data |

### All Tests (Complete List)

```
FileManager.RejectsZeroLength
FileManager.MountsValidFile
FileManager.FreeReturnsToIdle
FileManager.ReadAtOffsetFailsWhenIdle
FileManager.ReadsNormalBytes
FileManager.ReadAtExactEndReturnsZero
FileManager.ReadBeyondFileEndReturnsZero
FileManager.ClampsReadSizeAtFileEnd
FileManager.ReadFailsAfterFree
FileManager.ReadPartialFromMiddle
FileManager.MountsAfterFree
```

---

## Server Integration Test

**Source**: `server/tests/test_server.cc` (182 lines, 1 test case with 3 assertions)

This is a dual-mode test: when GTest is available (`#ifdef GTEST_HAS_GTEST_GTEST_H`), it runs as a GTest; otherwise falls back to `assert()`/`cout` mode.

### Test Case: `Server.ServesFiles`

| Sub-Test | What It Verifies | How |
|----------|------------------|-----|
| Index page served | `GET /index.html` returns status 200 | `httplib::Client::Get()` |
| WASM file served | `GET /build/virt-rpi.wasm` returns status 200 with non-empty body | `httplib::Client::Get()` |
| Port fallback | `find_free_port()` skips an occupied port 8000 and returns a different port | Uses `socket()` + `bind()` to occupy port 8000, then calls `find_free_port(8000)` |

### Test Flow

```cpp
// 1. Find a free port (starting from 8000)
const int port = find_free_port(8000);

// 2. Start server in a background thread
httplib::Server svr;
svr.set_mount_point("/", PROJECT_ROOT);
std::thread server_thread([&]() { svr.listen("127.0.0.1", port); });

// 3. Poll for readiness (socket connect, up to 50 attempts × 100ms)
for (int i = 0; i < 50; i++) {
    // ... connect to port, if succeeds break ...
    usleep(100000); // 100ms
}

// 4. Create cpp-httplib Client
httplib::Client cli("127.0.0.1", port);

// 5. Assert index.html returns 200
// 6. Assert WASM returns 200 with body
// 7. Assert find_free_port skips occupied port 8000

// 8. Stop server (unblocks listen), join thread
svr.stop();
server_thread.join();
```

### Server Readiness Polling

The test polls for server readiness by attempting socket connects:
- Up to 50 attempts × 100ms sleep = 5 second max wait
- Successful `connect()` → server is ready
- Failed `connect()` → sleep 100ms, retry

### Port Fallback Sub-Test

```cpp
// Occupy port 8000 by binding a socket to it
int fd = socket(AF_INET, SOCK_STREAM, 0);
sockaddr_in addr{};
// ...
bool occupied = (bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0);
if (occupied) {
    listen(fd, 1);
    int fallback_port = find_free_port(8000);
    EXPECT_NE(8000, fallback_port);  // Must skip occupied port 8000
    close(fd);
} else {
    close(fd);
    GTEST_SKIP() << "port fallback (8000 was already free)";  // Skip if port 8000 is free
}
```

If port 8000 is not already occupied on the test system, this sub-test is skipped (not a failure).

### Notes on Dual-Mode

When GTest is unavailable, the same code compiles to `run_server_tests()` which uses:
- `assert()` instead of `EXPECT_EQ`/`EXPECT_GT`
- `std::cout << "TEST [pass] ..."` instead of GTest logging
- `std::cout << "TEST [skip] ..."` instead of `GTEST_SKIP()`
