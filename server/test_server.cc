/// CTest unit tests for the HTTP server using cpp-httplib Client.
/// Links against server_lib and server_port_lib for find_free_port().

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
    std::cout << "TEST [info] using port " << port << "\n";

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
        std::cout << "TEST [pass] index.html served (status=" << res->status << ")\n";
    }

    // --- Test 2: WASM file served ---
    {
        auto res = cli.Get("/build/virt-rpi.wasm");
        assert(res && res->status == 200);
        assert(res->body.size() > 0);
        std::cout << "TEST [pass] virt-rpi.wasm served (" << res->body.size() << " bytes)\n";
    }

    // --- Test 3: Port fallback ---
    {
        // Bind a placeholder socket on 8000 to occupy it.
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
            std::cout << "TEST [pass] port fallback: occupied 8000, got " << fallback_port << "\n";
            close(fd);
        } else {
            close(fd);
            std::cout << "TEST [skip] port fallback (8000 was already free)\n";
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
