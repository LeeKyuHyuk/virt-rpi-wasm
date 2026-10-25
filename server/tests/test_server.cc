/// CTest unit tests for the HTTP server using cpp-httplib Client.
/// Links against server_lib and server_port_lib for find_free_port().
/// Supports two modes:
///   - With GTest: GTest-based TEST() macros
///   - Without GTest: assert/cout fallback (original format)

#include <iostream>
#include <thread>
#include <atomic>
#include <cassert>
#include <unistd.h>
#include <cstring>
#include "httplib.h"

extern int find_free_port(int);

// --- GTest mode ---
#ifdef GTEST_HAS_GTEST_GTEST_H
#include <gtest/gtest.h>

// Full integration test: start server, serve index and WASM, test port fallback.
TEST(Server, ServesFiles) {
    const int port = find_free_port(8000);
    ASSERT_GT(port, 0) << "find_free_port returned invalid port";
    std::cout << "TEST [info] using port " << port << std::endl;

    std::atomic<bool> done{false};

    // Start server in a background thread.
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
                break;
            }
            close(fd);
        }
        usleep(100000); // 100ms
    }

    httplib::Client cli("127.0.0.1", port);
    cli.set_connection_timeout(0, 500000); // 0.5s

    // --- Index page served (status code check) ---
    {
        auto res = cli.Get("/index.html");
        ASSERT_TRUE(res) << "GET /index.html returned null response";
        EXPECT_EQ(200, res->status);
    }

    // --- WASM file served (status + non-empty body) ---
    {
        auto res = cli.Get("/build/virt-rpi.wasm");
        ASSERT_TRUE(res) << "GET /build/virt-rpi.wasm returned null response";
        EXPECT_EQ(200, res->status);
        EXPECT_GT(res->body.size(), 0u);
    }

    // --- Port fallback (occupied port skipped) ---
    {
        const int placeholder = 8000;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0) << "socket() failed";
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(placeholder);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool occupied = (bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0);
        if (occupied) {
            listen(fd, 1);
            int fallback_port = find_free_port(8000);
            EXPECT_NE(8000, fallback_port)
                << "find_free_port should skip occupied port 8000 (returned " << fallback_port << ")";
            close(fd);
        } else {
            close(fd);
            GTEST_SKIP() << "port fallback (8000 was already free)";
        }
    }

    // Stop the server so listen() returns, then join the thread.
    svr.stop();
    server_thread.join();
}

}
#else
// --- Fallback mode (no GTest): original assert/cout format ---

int run_server_tests() {
    const int port = find_free_port(8000);
    assert(port > 0);
    std::cout << "TEST [info] using port " << port << "\n";

    std::atomic<bool> done{false};

    // Start server in a background thread.
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
                break;
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
#endif
