#pragma once

#include <queue>
#include <functional>
#include <mutex>

namespace gg {

/**
 * @brief Thread-safe event queue for a single thread
 * 
 * Each thread that subscribes to events gets its own ThreadQueue.
 * Events are pushed by the emitting thread and polled by the owning thread.
 */
class ThreadQueue {
public:
    using Callback = std::function<void()>;

    ThreadQueue() = default;
    ~ThreadQueue() = default;

    // Non-copyable, non-movable (shared state)
    ThreadQueue(const ThreadQueue&) = delete;
    ThreadQueue& operator=(const ThreadQueue&) = delete;
    ThreadQueue(ThreadQueue&&) = delete;
    ThreadQueue& operator=(ThreadQueue&&) = delete;

    /**
     * @brief Push a callback to be executed by the owning thread
     * Thread-safe: can be called from any thread
     */
    void push(Callback&& fn) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.push(std::move(fn));
    }

    /**
     * @brief Push a callback (const ref version)
     */
    void push(const Callback& fn) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.push(fn);
    }

    /**
     * @brief Process all pending callbacks
     * Should only be called by the owning thread
     * @return Number of callbacks processed
     */
    size_t poll() {
        std::queue<Callback> toProcess;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::swap(toProcess, m_pending);
        }
        
        size_t count = toProcess.size();
        while (!toProcess.empty()) {
            toProcess.front()();
            toProcess.pop();
        }
        
        return count;
    }

    /**
     * @brief Check if there are pending callbacks
     */
    bool hasPending() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_pending.empty();
    }

    /**
     * @brief Get number of pending callbacks
     */
    size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pending.size();
    }

    /**
     * @brief Clear all pending callbacks without executing
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<Callback> empty;
        std::swap(m_pending, empty);
    }

private:
    std::queue<Callback> m_pending;
    mutable std::mutex m_mutex;
};

} // namespace gg
