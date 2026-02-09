#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <functional>
#include <map>
#include <stdexcept>
#include <cstring>
#include <iostream>

#include <sys/eventfd.h>
#include <mutex>
#include <vector>
#include <atomic>

namespace ggnet {

class EpollLoop {
public:
    using EventCallback = std::function<void()>;
    using Task = std::function<void()>;

private:
    int epoll_fd;
    int wakeup_fd;
    bool running = false;
    struct Handler {
        EventCallback onRead;
        EventCallback onWrite;
    };
    std::map<int, Handler> handlers;
    
    std::mutex tasks_mutex;
    std::vector<Task> pending_tasks;

public:
    EpollLoop() {
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            throw std::runtime_error("Failed to create epoll instance");
        }
        
        // Create eventfd for cross-thread wakeups
        wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wakeup_fd < 0) {
            throw std::runtime_error("Failed to create wakeup eventfd");
        }
        
        // Register wakeup handler
        addFd(wakeup_fd, EPOLLIN, [this]() {
            uint64_t val;
            read(this->wakeup_fd, &val, sizeof(val)); // Clear buffer
            this->executePendingTasks();
        });
    }

    ~EpollLoop() {
        if (epoll_fd >= 0) close(epoll_fd);
        if (wakeup_fd >= 0) close(wakeup_fd);
    }
    
    // Thread-safe method to schedule work on the loop thread
    void runInLoop(Task task) {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex);
            pending_tasks.push_back(std::move(task));
        }
        // Wakeup loop
        uint64_t val = 1;
        write(wakeup_fd, &val, sizeof(val));
    }

private:
    void executePendingTasks() {
        std::vector<Task> tasks;
        {
            std::lock_guard<std::mutex> lock(tasks_mutex);
            tasks.swap(pending_tasks);
        }
        for (const auto& task : tasks) {
            task();
        }
    }

public:
    // Adiciona ou modifica um FD no monitoramento
    void addFd(int fd, uint32_t events, EventCallback onRead = nullptr, EventCallback onWrite = nullptr) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;

        if (handlers.find(fd) == handlers.end()) {
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                 throw std::runtime_error("epoll_ctl ADD failed: " + std::string(strerror(errno)));
            }
        } else {
            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                 throw std::runtime_error("epoll_ctl MOD failed: " + std::string(strerror(errno)));
            }
        }
        handlers[fd] = {onRead, onWrite};
    }

    void removeFd(int fd) {
        if (handlers.erase(fd)) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
        }
    }

    void stop() {
        running = false;
    }

    void run() {
        running = true;
        const int MAX_EVENTS = 64;
        struct epoll_event events[MAX_EVENTS];

        while (running) {
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // Block indefinitely
            if (nfds < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("epoll_wait failed: " + std::string(strerror(errno)));
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                uint32_t ev = events[i].events;
                
                auto it = handlers.find(fd);
                if (it != handlers.end()) {
                    if ((ev & (EPOLLIN | EPOLLHUP | EPOLLERR)) && it->second.onRead) {
                        it->second.onRead();
                    }
                    if ((ev & EPOLLOUT) && it->second.onWrite) {
                        it->second.onWrite();
                    }
                }
            }
        }
    }
};

} // namespace ggnet
