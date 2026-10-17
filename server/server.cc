/// Lightweight HTTP server that serves project files on a free port starting at 8000.
/// Uses cpp-httplib (header-only) for cross-platform support (Windows, Linux, macOS).

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
