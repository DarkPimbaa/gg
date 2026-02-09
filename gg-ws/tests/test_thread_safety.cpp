#include "gg_ws/gg_ws.hpp"
#include "../src/internal/message_queue.hpp"
#include "../src/internal/memory_pool.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace gg;

// ============================================
// Macros de Teste
// ============================================
#define TEST(name) void test_##name()
#define RUN_TEST(name) \
    std::cout << "  " << #name << "... "; \
    test_##name(); \
    std::cout << "OK\n"

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cerr << "\nFALHA: " << #expr << " em " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    }

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

// ============================================
// Testes da LockFreeQueue
// ============================================
TEST(queue_single_thread) {
    internal::LockFreeQueue<int> queue;
    
    ASSERT(queue.empty());
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    ASSERT(!queue.empty());
    
    auto v1 = queue.pop();
    auto v2 = queue.pop();
    auto v3 = queue.pop();
    auto v4 = queue.pop();
    
    ASSERT(v1.has_value() && *v1 == 1);
    ASSERT(v2.has_value() && *v2 == 2);
    ASSERT(v3.has_value() && *v3 == 3);
    ASSERT(!v4.has_value());
    
    ASSERT(queue.empty());
}

TEST(queue_multiple_producers) {
    internal::LockFreeQueue<int> queue;
    std::atomic<int> produced{0};
    
    const int numProducers = 10;
    const int itemsPerProducer = 1000;
    
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.push(p * itemsPerProducer + i);
                produced++;
            }
        });
    }
    
    for (auto& t : producers) {
        t.join();
    }
    
    ASSERT_EQ(produced.load(), numProducers * itemsPerProducer);
    
    // Consome todos os itens
    int consumed = 0;
    while (queue.pop().has_value()) {
        consumed++;
    }
    
    ASSERT_EQ(consumed, numProducers * itemsPerProducer);
}

TEST(queue_producer_consumer) {
    internal::LockFreeQueue<int> queue;
    std::atomic<bool> done{false};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    const int numProducers = 4;
    const int itemsPerProducer = 1000;
    
    // Produtores
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.push(i);
                produced++;
            }
        });
    }
    
    // Consumidor (single)
    std::thread consumer([&]() {
        while (!done.load() || !queue.empty()) {
            if (queue.pop().has_value()) {
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& t : producers) {
        t.join();
    }
    
    done = true;
    consumer.join();
    
    ASSERT_EQ(produced.load(), numProducers * itemsPerProducer);
    ASSERT_EQ(consumed.load(), numProducers * itemsPerProducer);
}

// ============================================
// Testes do BufferPool
// ============================================
TEST(buffer_pool_basic) {
    internal::BufferPool pool(1024, 4);
    
    ASSERT_EQ(pool.bufferSize(), 1024);
    ASSERT_EQ(pool.available(), 4);
    
    {
        auto buf1 = pool.acquire();
        ASSERT(buf1.valid());
        ASSERT_EQ(buf1.size(), 1024);
        ASSERT_EQ(pool.available(), 3);
        
        auto buf2 = pool.acquire();
        ASSERT_EQ(pool.available(), 2);
    }
    
    // Buffers devolvidos ao pool
    ASSERT_EQ(pool.available(), 4);
}

TEST(buffer_pool_overflow) {
    internal::BufferPool pool(1024, 2);
    
    auto buf1 = pool.acquire();
    auto buf2 = pool.acquire();
    
    ASSERT_EQ(pool.available(), 0);
    
    // Deve alocar novo buffer
    auto buf3 = pool.acquire();
    ASSERT(buf3.valid());
}

TEST(buffer_pool_concurrent) {
    internal::BufferPool pool(1024, 8);
    std::atomic<int> acquired{0};
    
    const int numThreads = 10;
    const int iterationsPerThread = 100;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                auto buf = pool.acquire();
                if (buf.valid()) {
                    acquired++;
                    // Escreve para verificar que o buffer é válido
                    buf[0] = 'X';
                    buf[buf.size() - 1] = 'Y';
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(acquired.load(), numThreads * iterationsPerThread);
}

// ============================================
// Teste de stress do JSON parser
// ============================================
TEST(json_concurrent_parse) {
    const int numThreads = 8;
    const int parsesPerThread = 1000;
    std::atomic<int> successful{0};
    std::atomic<int> failed{0};
    
    std::string jsonStr = R"({
        "users": [
            {"id": 1, "name": "Alice", "active": true},
            {"id": 2, "name": "Bob", "active": false}
        ],
        "count": 2,
        "metadata": {"version": "1.0"}
    })";
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < parsesPerThread; ++i) {
                auto json = Json::parse(jsonStr);
                if (json && json->get("count").getInt() == 2) {
                    successful++;
                } else {
                    failed++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(successful.load(), numThreads * parsesPerThread);
    ASSERT_EQ(failed.load(), 0);
}

TEST(json_concurrent_modify) {
    const int numThreads = 4;
    const int operationsPerThread = 1000;
    std::atomic<int> completed{0};
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                // Cada thread cria seu próprio JSON (não compartilhado)
                Json json = Json::object();
                json["thread"] = t;
                json["iteration"] = i;
                json["data"] = Json::array();
                
                for (int j = 0; j < 10; ++j) {
                    json["data"].push(j);
                }
                
                std::string serialized = json.stringify();
                auto reparsed = Json::parse(serialized);
                
                if (reparsed && reparsed->get("thread").getInt() == t) {
                    completed++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(completed.load(), numThreads * operationsPerThread);
}

// ============================================
// Main
// ============================================
int main() {
    std::cout << "=== Testes de Thread-Safety ===\n\n";
    
    std::cout << "LockFreeQueue:\n";
    RUN_TEST(queue_single_thread);
    RUN_TEST(queue_multiple_producers);
    RUN_TEST(queue_producer_consumer);
    
    std::cout << "\nBufferPool:\n";
    RUN_TEST(buffer_pool_basic);
    RUN_TEST(buffer_pool_overflow);
    RUN_TEST(buffer_pool_concurrent);
    
    std::cout << "\nJSON concorrente:\n";
    RUN_TEST(json_concurrent_parse);
    RUN_TEST(json_concurrent_modify);
    
    std::cout << "\n=== TODOS OS TESTES PASSARAM ===\n";
    return 0;
}
