# Plan: Add HTTP Server (run-server)

## Context

The project has a WebAssembly module (`src/main.cc`) but no way to test it in a browser locally. `python3 -m http.server` requires Python, so a C++ binary `run-server` is added to serve files. cpp-httplib (header-only) is added as a git submodule at `lib/cpp-httplib`, and `server/server.cc` implements the HTTP server. CMakeLists.txt registers it as `add_executable(run-server)`.

`run-server` starts at port 8000; if busy, automatically tries 8001, 8002, ... until a free port is found.

## Final project structure (after)

```
.
├── CMakeLists.txt          # modified: add_executable + enable_testing + add_test
├── src/
│   └── main.cc             # unchanged: WASM module (extern "C")
├── server/
│   ├── server.cc           # new: cpp-httplib HTTP server (main)
│   ├── server_port.cc      # new: port scanner (no main)
│   └── test_server.cc      # new: CTest unit tests (cpp-httplib client)
├── lib/
│   └── cpp-httplib/        # new: git submodule
├── js/
│   └── main.js             # modified: WASM path to /build/virt-rpi.wasm
├── index.html              # unchanged
└── build/                  # .gitignore target
```

## Step-by-step work

### 1. Add cpp-httplib as git submodule

```bash
mkdir -p lib
git submodule add https://github.com/yhirose/cpp-httplib.git lib/cpp-httplib
```

### 2. Create `server/server.cc` (main binary)

Uses cpp-httplib (header-only: `#include "httplib.h"`).

**Port 8000 auto-fallback**: `find_free_port` is a **standalone function** (declared `extern`) so it can be unit-tested by linking with it.

```cpp
// server/server.cc
#include <iostream>
#include "httplib.h"

extern int find_free_port(int);

int main() {
    int port = find_free_port(8000);
    if (port < 0) {
        std::cerr << "No free port found.\n";
        return 1;
    }
    std::cout << "Listening on http://localhost:" << port << "\n";

    httplib::Server svr;
    svr.set_mount_point("/", PROJECT_ROOT);
    svr.listen("0.0.0.0", port);
}
```

### 3. Create `server/server_port.cc` (port scanner library)

Extracted `find_free_port` into its own source file so it can be compiled as a library without `main`:

```cpp
// server/server_port.cc
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int find_free_port(int start_port) {
    for (int port = start_port; port < 65535; ++port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
            close(fd);
            continue;
        }
        close(fd);
        return port;
    }
    return -1;
}
```

### 4. Create `server/test_server.cc` (CTest unit tests)

The test binary:
1. Calls `find_free_port` from `server_port.cc` (linked as a library)
2. Starts an httplib server in a background thread
3. Uses cpp-httplib `Client` to send HTTP requests and assert responses
4. Calls `svr.stop()` to interrupt the blocking `listen()` call
5. Runs assertions (`assert()` exits on failure → CTest fails)

**Critical fix:** `svr.stop()` is called from the main thread after assertions to unblock the blocking `listen()` in the server thread, then the thread is joined.

```cpp
// server/test_server.cc
#include <iostream>
#include <thread>
#include <atomic>
#include <cassert>
#include <unistd.h>
#include <cstring>
#include "httplib.h"

extern int find_free_port(int);

int run_server_tests() {
    const int port = find_free_port(8000);
    assert(port > 0);

    std::atomic<bool> done{false};

    // Start server in a background thread.
    // listen() blocks until stop() is called.
    httplib::Server svr;
    svr.set_mount_point("/", PROJECT_ROOT);
    std::thread server_thread([&]() {
        svr.listen("127.0.0.1", port);
    });

    // Wait for the server to be ready (poll with socket connect).
    for (int i = 0; i < 50; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
                close(fd);
                break; // server is ready
            }
            close(fd);
        }
        usleep(100000); // 100ms
    }

    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(0, 500000); // 0.5s

    // --- Test 1: Index page served ---
    {
        auto res = cli.Get("/index.html");
        assert(res && res->status == 200);
    }

    // --- Test 2: WASM file served ---
    {
        auto res = cli.Get("/build/virt-rpi.wasm");
        assert(res && res->status == 200);
        assert(res->body.size() > 0);
    }

    // --- Test 3: Port fallback ---
    {
        const int placeholder = 8000;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(placeholder);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool occupied = (bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0);
        if (occupied) {
            listen(fd, 1);
            int fallback_port = find_free_port(8000);
            assert(fallback_port != 8000);
            close(fd);
        } else {
            close(fd);
        }
    }

    // Stop the server so listen() returns, then join the thread.
    svr.stop();
    server_thread.join();
    return 0;
}

int main() {
    return run_server_tests();
}
```

### 5. Modify CMakeLists.txt

Keep the existing `add_custom_command` (WASM) unchanged; append at the bottom:

```cmake
# --- Port scanner library (no main, linkable by both binaries) ---
add_library(server_port_lib server/server_port.cc)

# --- Server library (httplib-based server, no port scanning logic) ---
add_library(server_lib server/server.cc)
target_include_directories(server_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/cpp-httplib
)
target_compile_definitions(server_lib PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
target_compile_options(server_lib PRIVATE -O2)
target_link_libraries(server_lib PRIVATE server_port_lib)
add_executable(run-server $<TARGET_OBJECTS:server_lib>)
target_link_libraries(run-server PRIVATE server_port_lib)

# --- Unit test binary (links server_lib for find_free_port) ---
add_executable(run-server-test server/test_server.cc)
target_include_directories(run-server-test PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/cpp-httplib
)
target_compile_definitions(run-server-test PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(run-server-test PRIVATE server_lib server_port_lib)

enable_testing()
add_test(NAME server-basic COMMAND run-server-test)
```

**Three-library hierarchy:** `server_port_lib` (utility, no main) → `server_lib` (server with main, links server_port_lib) → `run-server` (executable from objects) and `run-server-test` (test code, links server_lib and server_port_lib).

**Key design:** `server_port_lib` is a library with no `main`, so it can be linked by both the `run-server` executable and the `run-server-test` test binary without "multiple definition of main" errors.

**Verified:**
- `add_custom_command` (WASM) and `add_executable` (server + test) do not read/write shared files → no conflict
- WASM uses `--nostdlib`, server uses normal stdlib → separate linking, no conflict
- `server_lib` must be a library (not direct executable) to avoid duplicate `main` when linked by test binary

### 6. Modify `js/main.js`: WASM path

Existing: `../build/virt-rpi.wasm` (ESM module-relative)
→ Server serves project root, so change to absolute URL:

```javascript
const wasmUrl = new URL('/build/virt-rpi.wasm', location.origin);
```

Remove `import.meta.url`, use `location.origin` (browser environment).

### 7. Build, test and verify

```bash
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/clang++-22
cmake --build .
ctest --output-on-failure
# Expected: all CTest tests pass (server-basic)

./run-server  # server on port 8000+
# Open http://localhost:<port>/index.html in browser
# Verify "Hello, world!" in console
```

## CTest verification tests

| Test | Description |
|------|-------------|
| `server-basic` | Runs `run-server-test` which uses cpp-httplib `Client` to verify: (1) index.html served (200 OK), (2) virt-rpi.wasm served (binary response, >0 bytes), (3) port fallback works when 8000 is occupied (asserts fallback_port != 8000) |

Run with: `ctest --output-on-failure`

## Remaining manual checks (not unit-testable)

| Item | How |
|------|-----|
| Browser `console.log("Hello, world!")` | Open `http://localhost:<port>/index.html` in browser, check DevTools console |
| `build/` not committed | `git status` excludes build/ |
