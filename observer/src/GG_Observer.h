#pragma once

/**
 * @file GG_Observer.h
 * @brief GG_Observer - C++ Event Library with Thread Affinity
 * 
 * A modern C++17 event system using the Observer pattern.
 * Callbacks execute on the subscriber's original thread.
 * 
 * @example
 * #include "GG_Observer.h"
 * 
 * struct DamageEvent { int amount; };
 * 
 * gg::EventBus bus;
 * 
 * // Subscribe (captures current thread)
 * auto sub = bus.subscribe<DamageEvent>([](const DamageEvent& e) {
 *     std::cout << "Damage: " << e.amount << "\n";
 * });
 * 
 * // Emit from any thread
 * bus.emit(DamageEvent{50});
 * 
 * // Process events on subscriber's thread
 * bus.poll();
 */

#include "EventBus.hpp"

// Version info
#define GG_OBSERVER_VERSION_MAJOR 1
#define GG_OBSERVER_VERSION_MINOR 0
#define GG_OBSERVER_VERSION_PATCH 0
