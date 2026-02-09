/**
 * @file main.cpp
 * @brief Example demonstrating GG_Observer with thread affinity
 */

#include "GG_Observer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

// Define custom events
struct PlayerDamageEvent {
    int playerId;
    float damage;
};

struct PlayerHealEvent {
    int playerId;
    float amount;
};

struct GameOverEvent {
    std::string winner;
};

std::atomic<bool> running{true};

void printThreadId(const char* prefix) {
    std::cout << prefix << " [Thread " << std::this_thread::get_id() << "]\n";
}

int main() {
    std::cout << "=== GG_Observer Example ===\n\n";
    
    // Create EventBus on main thread
    gg::EventBus bus;
    
    printThreadId("Main thread");
    
    // Subscribe on main thread
    auto mainSub = bus.subscribe<PlayerDamageEvent>([](const PlayerDamageEvent& e) {
        printThreadId("  -> Main received damage event");
        std::cout << "     Player " << e.playerId << " took " << e.damage << " damage\n";
    });
    
    // Worker thread that subscribes to events
    std::thread worker([&bus]() {
        printThreadId("Worker thread started");
        
        // Subscribe from worker thread - callbacks will run here!
        auto workerSub = bus.subscribe<PlayerDamageEvent>([](const PlayerDamageEvent& e) {
            printThreadId("  -> Worker received damage event");
            std::cout << "     Player " << e.playerId << " took " << e.damage << " damage\n";
        });
        
        auto healSub = bus.subscribe<PlayerHealEvent>([](const PlayerHealEvent& e) {
            printThreadId("  -> Worker received heal event");
            std::cout << "     Player " << e.playerId << " healed " << e.amount << " HP\n";
        });
        
        // Worker thread event loop
        while (running) {
            size_t processed = bus.poll();
            if (processed > 0) {
                std::cout << "     Worker processed " << processed << " event(s)\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        printThreadId("Worker thread ending");
    });
    
    // Give worker time to subscribe
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::cout << "\n--- Emitting events from main thread ---\n\n";
    
    // Emit events from main thread
    bus.emit(PlayerDamageEvent{1, 25.5f});
    bus.emit(PlayerHealEvent{2, 10.0f});
    bus.emit(PlayerDamageEvent{1, 15.0f});
    
    // Process main thread events
    std::cout << "\nMain thread polling...\n";
    size_t mainProcessed = bus.poll();
    std::cout << "Main processed " << mainProcessed << " event(s)\n";
    
    // Wait for worker to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop worker
    running = false;
    worker.join();
    
    std::cout << "\n=== Example Complete ===\n";
    return 0;
}
