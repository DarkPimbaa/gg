#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdexcept>

#include <string>
#include <cstring>

namespace ggnet {

class Socket {
public:
    int fd = -1;

    Socket() {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
        }
    }

    // Move constructor
    Socket(Socket&& other) noexcept : fd(other.fd) {
        other.fd = -1;
    }

    // Move assignment
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }

    // Disable copy
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    ~Socket() {
        close();
    }

    void close() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }


    void connect(const std::string& host, int port) {

        struct addrinfo hints, *res;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // IPv4 for now
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(port);
        int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
        if (status != 0) {
            throw std::runtime_error("DNS resolution failed for " + host + ": " + std::string(gai_strerror(status)));
        }

        // Try to connect to first address
        // Note: In a robust lib we should try iteration, but for simplicity/speed taking first is okay for now.
        // We reuse the existing socket fd.
        
        if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            if (errno != EINPROGRESS) {
                freeaddrinfo(res);
                throw std::runtime_error("Connection failed: " + std::string(strerror(errno)));
            }
        }
        
        freeaddrinfo(res);
    }

    void setNonBlocking() {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("fcntl F_GETFL failed");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
        }
    }

    void setNoDelay() {
        int flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int)) < 0) {
            throw std::runtime_error("setsockopt TCP_NODELAY failed");
        }
    }

    void setReuseAddr() {
        int flag = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(int)) < 0) {
            throw std::runtime_error("setsockopt SO_REUSEADDR failed");
        }
    }
};

} // namespace ggnet
