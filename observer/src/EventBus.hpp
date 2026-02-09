#pragma once

#include "ThreadQueue.hpp"
#include "Subscription.hpp"

#include <unordered_map>
#include <vector>
#include <typeindex>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <algorithm>

namespace gg {

/**
 * @brief Central event dispatcher with thread affinity
 * 
 * Events are emitted from any thread, but callbacks execute on the
 * thread that originally subscribed (when that thread calls poll()).
 */
class EventBus {
public:
    EventBus() 
        : m_nextId(1)
    {}

    ~EventBus() = default;

    // Non-copyable, non-movable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /**
     * @brief Subscribe to an event type
     * @tparam EventT The event type to subscribe to
     * @param callback Function to call when event is emitted
     * @return Subscription RAII object (cancels on destruction)
     * 
     * The callback will be executed on THIS thread when poll() is called.
     */
    template<typename EventT>
    Subscription subscribe(std::function<void(const EventT&)> callback) {
        auto threadId = std::this_thread::get_id();
        auto queue = getOrCreateQueue(threadId);
        
        SubscriptionId id = m_nextId++;
        
        auto wrapper = [callback, queue](const void* eventPtr) {
            const EventT& event = *static_cast<const EventT*>(eventPtr);
            queue->push([callback, event]() {
                callback(event);
            });
        };

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& listeners = m_listeners[std::type_index(typeid(EventT))];
            listeners.push_back({id, threadId, std::move(wrapper)});
        }

        return Subscription(this, id, [this, id]() {
            unsubscribe(id);
        });
    }

    /**
     * @brief Subscribe with lambda (deduces event type)
     */
    template<typename Func>
    auto subscribe(Func&& func) -> decltype(subscribe(std::function(std::forward<Func>(func)))) {
        return subscribe(std::function(std::forward<Func>(func)));
    }

    /**
     * @brief Emit an event to all subscribers
     * @tparam EventT The event type
     * @param event The event data
     * 
     * Thread-safe: can be called from any thread.
     * Callbacks are queued to run on their subscriber's thread.
     */
    template<typename EventT>
    void emit(const EventT& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_listeners.find(std::type_index(typeid(EventT)));
        if (it == m_listeners.end()) {
            return;
        }

        for (const auto& listener : it->second) {
            listener.callback(&event);
        }
    }

    /**
     * @brief Emit an event (move version)
     */
    template<typename EventT>
    void emit(EventT&& event) {
        emit(static_cast<const EventT&>(event));
    }

    /**
     * @brief Process all pending events for the calling thread
     * @return Number of events processed
     * 
     * Must be called periodically by each subscribed thread.
     */
    size_t poll() {
        auto threadId = std::this_thread::get_id();
        auto queue = getQueue(threadId);
        
        if (!queue) {
            return 0;
        }
        
        return queue->poll();
    }

    /**
     * @brief Check if calling thread has pending events
     */
    bool hasPending() {
        auto threadId = std::this_thread::get_id();
        auto queue = getQueue(threadId);
        return queue && queue->hasPending();
    }

    /**
     * @brief Get number of pending events for calling thread
     */
    size_t pendingCount() {
        auto threadId = std::this_thread::get_id();
        auto queue = getQueue(threadId);
        return queue ? queue->pendingCount() : 0;
    }

    /**
     * @brief Remove all listeners for an event type
     */
    template<typename EventT>
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.erase(std::type_index(typeid(EventT)));
    }

    /**
     * @brief Remove all listeners for all event types
     */
    void clearAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners.clear();
    }

    /**
     * @brief Get number of subscribers for an event type
     */
    template<typename EventT>
    size_t subscriberCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_listeners.find(std::type_index(typeid(EventT)));
        return it != m_listeners.end() ? it->second.size() : 0;
    }

private:
    struct ListenerInfo {
        SubscriptionId id;
        std::thread::id threadId;
        std::function<void(const void*)> callback;
    };

    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& [type, listeners] : m_listeners) {
            auto it = std::remove_if(listeners.begin(), listeners.end(),
                [id](const ListenerInfo& info) { return info.id == id; });
            
            if (it != listeners.end()) {
                listeners.erase(it, listeners.end());
                return;
            }
        }
    }

    std::shared_ptr<ThreadQueue> getOrCreateQueue(std::thread::id threadId) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        auto it = m_queues.find(threadId);
        if (it != m_queues.end()) {
            return it->second;
        }
        
        auto queue = std::make_shared<ThreadQueue>();
        m_queues[threadId] = queue;
        return queue;
    }

    std::shared_ptr<ThreadQueue> getQueue(std::thread::id threadId) {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        auto it = m_queues.find(threadId);
        return it != m_queues.end() ? it->second : nullptr;
    }

    std::unordered_map<std::type_index, std::vector<ListenerInfo>> m_listeners;
    std::unordered_map<std::thread::id, std::shared_ptr<ThreadQueue>> m_queues;
    
    mutable std::mutex m_mutex;       // Protects m_listeners
    mutable std::mutex m_queueMutex;  // Protects m_queues
    
    std::atomic<SubscriptionId> m_nextId;
};

} // namespace gg
