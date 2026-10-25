# HTTP Server

## Overview

The HTTP server serves the project's static files (HTML, WASM, JavaScript) over HTTP. It is built with cpp-httplib (header-only) and automatically finds a free TCP port starting from 8000.

## Architecture

```
server_port_lib (no main)
    └── server_lib (wrapping cpp-httplib, no main)
        └── run-server (executable, main)
```

Two library layers ensure both the server and test binary can link against `find_free_port()` without "multiple definition of main" errors.

## Components

### `find_free_port()` — `server/server_port.cc`

**Source**: `server/server_port.cc` (26 lines)

```cpp
int find_free_port(int start_port) {
    // For each port from start_port to start_port + 10:
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // Attempt to connect. If connect() succeeds, the port is in use.
    // If connect() fails, the port is free — return it.
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
        close(fd);
        continue;  // Port is occupied
    }
    close(fd);
    return port;  // Port is free
}
```

**How it works**:
1. Create a TCP socket via `socket(AF_INET, SOCK_STREAM, 0)`
2. Attempt to `connect()` to `127.0.0.1:<port>`
3. Successful connect → port is in use by another service → try next port
4. Failed connect → port is free → return it
5. Scans ports 8000 through 8010 (10 ports max)

**Note**: This is a heuristic approach — a connect failure means the port is not actively listening, which for localhost scanning purposes means it is free for our use.

### Server — `server/server.cc`

**Source**: `server/server.cc` (20 lines)

```cpp
int main() {
    int port = find_free_port(8000);
    std::cout << "Listening on http://0.0.0.0:" << port << std::endl;

    httplib::Server svr;
    svr.set_mount_point("/", PROJECT_ROOT);
    svr.listen("0.0.0.0", port);
}
```

**How it works**:
1. Scan for a free port starting at 8000
2. Create an `httplib::Server` instance
3. Set `/` mount point to `PROJECT_ROOT` (defined at compile time as the source directory)
4. Listen on `0.0.0.0:<port>` (all interfaces)
5. Block until `svr.stop()` is called (unblocks `listen()`)

**How the user knows the port**: The server prints `Listening on http://0.0.0.0:<port>` to stdout. The user reads this to construct the URL.

## File Serving

| Request Path | Served From |
|-------------|-------------|
| `/` | `<PROJECT_ROOT>/index.html` |
| `/index.html` | `<PROJECT_ROOT>/index.html` |
| `/build/virt-rpi.wasm` | `<PROJECT_ROOT>/build/virt-rpi.wasm` |
| `/js/main.js` | `<PROJECT_ROOT>/js/main.js` |

The mount point maps URL paths directly to the source directory tree.

## JavaScript Interaction

```javascript
// Server starts in background
const process = spawn('./build/run-server', [], { stdio: 'inherit' });

// Read the listening port from server output
const port = parseInt(portMatch[1]);

// Fetch files from the server
const response = await fetch(`http://localhost:${port}/index.html`);
const html = await response.text();

const wasmResponse = await fetch(`http://localhost:${port}/build/virt-rpi.wasm`);
const wasmBuffer = await wasmResponse.arrayBuffer();
```

## Server Lifecycle (in tests)

The server test (`server/tests/test_server.cc`) starts a server in a background thread, waits for it to be ready (polling socket connect), verifies file serving, then stops it:

```cpp
httplib::Server svr;
svr.set_mount_point("/", PROJECT_ROOT);
std::thread server_thread([&]() {
    svr.listen("127.0.0.1", port);
});
// ... wait for ready, make requests ...
svr.stop();       // Unblocks listen()
server_thread.join();
```

`svr.stop()` is the key — it interrupts `listen()`, allowing the thread to return and be `join()`ed cleanly.
