#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace gg {

// Forward declaration
class EventBus;

using SubscriptionId = uint64_t;

/**
 * @brief RAII wrapper for event subscriptions
 * 
 * Automatically unsubscribes when destroyed.
 * Can be moved but not copied.
 */
class Subscription {
public:
    Subscription() = default;
    
    Subscription(EventBus* bus, SubscriptionId id, std::function<void()> unsubscribeFn)
        : m_bus(bus)
        , m_id(id)
        , m_unsubscribeFn(std::move(unsubscribeFn))
        , m_active(true)
    {}

    ~Subscription() {
        cancel();
    }

    // Move-only
    Subscription(Subscription&& other) noexcept
        : m_bus(other.m_bus)
        , m_id(other.m_id)
        , m_unsubscribeFn(std::move(other.m_unsubscribeFn))
        , m_active(other.m_active)
    {
        other.m_active = false;
        other.m_bus = nullptr;
    }

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            cancel();
            m_bus = other.m_bus;
            m_id = other.m_id;
            m_unsubscribeFn = std::move(other.m_unsubscribeFn);
            m_active = other.m_active;
            other.m_active = false;
            other.m_bus = nullptr;
        }
        return *this;
    }

    // Non-copyable
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    /**
     * @brief Manually cancel the subscription
     */
    void cancel() {
        if (m_active && m_unsubscribeFn) {
            m_unsubscribeFn();
            m_active = false;
        }
    }

    /**
     * @brief Check if subscription is still active
     */
    bool isActive() const { return m_active; }

    /**
     * @brief Get the subscription ID
     */
    SubscriptionId getId() const { return m_id; }

private:
    EventBus* m_bus = nullptr;
    SubscriptionId m_id = 0;
    std::function<void()> m_unsubscribeFn;
    bool m_active = false;
};

/**
 * @brief Shared subscription that can be copied
 * 
 * Useful when multiple objects need to share ownership of a subscription.
 */
class SharedSubscription {
public:
    SharedSubscription() = default;
    
    explicit SharedSubscription(Subscription&& sub)
        : m_sub(std::make_shared<Subscription>(std::move(sub)))
    {}

    void cancel() {
        if (m_sub) {
            m_sub->cancel();
        }
    }

    bool isActive() const {
        return m_sub && m_sub->isActive();
    }

private:
    std::shared_ptr<Subscription> m_sub;
};

} // namespace gg
