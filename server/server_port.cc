/// Platform-independent TCP port scanner.
/// Used by both the server binary and its unit tests.

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

/// Scans from `start_port` upward and returns the first free TCP port,
/// or -1 if no port is available.
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
