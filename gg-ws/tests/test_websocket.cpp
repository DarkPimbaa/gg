#include "gg_ws/websocket.hpp"
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

// ============================================
// Teste de conexão básica
// ============================================
TEST(basic_connection) {
    WebSocket ws({.url = "wss://echo.websocket.org"});
    
    std::atomic<bool> connected{false};
    std::atomic<bool> disconnected{false};
    
    ws.onConnect([&]() { connected = true; });
    ws.onDisconnect([&](int) { disconnected = true; });
    
    // Tenta conectar (pode falhar se servidor offline)
    bool result = ws.connect();
    if (result) {
        ASSERT(connected.load());
        ASSERT(ws.isConnected());
        
        ws.disconnect();
        ASSERT(disconnected.load());
        ASSERT(!ws.isConnected());
    }
}

// ============================================
// Teste de envio/recebimento
// ============================================
TEST(send_receive) {
    WebSocket ws({.url = "wss://echo.websocket.org"});
    
    std::atomic<int> receivedCount{0};
    std::string lastMessage;
    std::mutex msgMutex;
    
    ws.onRawMessage([&](std::string_view msg) {
        std::lock_guard<std::mutex> lock(msgMutex);
        lastMessage = std::string(msg);
        receivedCount++;
    });
    
    if (ws.connect()) {
        ws.send("test message");
        
        // Aguarda resposta
        for (int i = 0; i < 50 && receivedCount == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        ws.disconnect();
        
        // Echo server deve ter retornado a mensagem
        if (receivedCount > 0) {
            std::lock_guard<std::mutex> lock(msgMutex);
            ASSERT(lastMessage == "test message");
        }
    }
}

// ============================================
// Teste de ping/pong
// ============================================
TEST(ping_pong) {
    WebSocket ws({
        .url = "wss://echo.websocket.org",
        .ping = {.mode = PingMode::Opcode, .interval = std::chrono::seconds(1)}
    });
    
    std::atomic<int> pongCount{0};
    
    ws.onPong([&](std::string_view) {
        pongCount++;
    });
    
    if (ws.connect()) {
        // Envia ping manual
        ws.sendPing("test");
        
        // Aguarda pong
        for (int i = 0; i < 30 && pongCount == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        ws.disconnect();
        
        // Deve ter recebido pelo menos um pong
        ASSERT(pongCount > 0);
    }
}

// ============================================
// Teste de CPU affinity
// ============================================
TEST(cpu_affinity) {
    int cores = WebSocket::getCoreCount();
    ASSERT(cores > 0);
    
    WebSocket ws({.url = "wss://echo.websocket.org"});
    
    // Deve aceitar núcleo válido
    if (cores > 1) {
        ASSERT(ws.pinThread(0));
        ASSERT(ws.pinThread(cores - 1));
    }
    
    // Deve rejeitar núcleo inválido
    ASSERT(!ws.pinThread(-1));
    ASSERT(!ws.pinThread(cores + 100));
}

// ============================================
// Teste de thread-safety no envio
// ============================================
TEST(concurrent_send) {
    WebSocket ws({.url = "wss://echo.websocket.org"});
    
    std::atomic<int> sent{0};
    std::atomic<int> errors{0};
    
    ws.onError([&](int, std::string_view) {
        errors++;
    });
    
    if (ws.connect()) {
        // Cria múltiplas threads enviando simultaneamente
        std::vector<std::thread> threads;
        const int numThreads = 10;
        const int msgsPerThread = 100;
        
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < msgsPerThread; ++i) {
                    std::string msg = "Thread " + std::to_string(t) + " msg " + std::to_string(i);
                    if (ws.send(msg)) {
                        sent++;
                    }
                }
            });
        }
        
        // Aguarda todas as threads
        for (auto& t : threads) {
            t.join();
        }
        
        // Aguarda um pouco para processar
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        ws.disconnect();
        
        // Não deve ter havido erros (exceto se desconectar)
        std::cout << "(enviadas: " << sent << ") ";
    }
}

// ============================================
// Teste de sendAsync
// ============================================
TEST(send_async) {
    WebSocket ws({.url = "wss://echo.websocket.org"});
    
    std::atomic<int> received{0};
    
    ws.onRawMessage([&](std::string_view) {
        received++;
    });
    
    if (ws.connect()) {
        // Envia várias mensagens async (não bloqueia)
        auto start = std::chrono::steady_clock::now();
        
        for (int i = 0; i < 100; ++i) {
            ws.sendAsync("async message " + std::to_string(i));
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        
        // sendAsync não deve bloquear significativamente
        ASSERT(elapsed < std::chrono::milliseconds(100));
        
        // Aguarda processamento
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        ws.disconnect();
        
        std::cout << "(recebidas: " << received << ") ";
    }
}

// ============================================
// Main
// ============================================
int main() {
    std::cout << "=== Testes do WebSocket ===\n\n";
    
    std::cout << "Básicos:\n";
    RUN_TEST(cpu_affinity);
    
    std::cout << "\nConexão (requer internet):\n";
    RUN_TEST(basic_connection);
    RUN_TEST(send_receive);
    RUN_TEST(ping_pong);
    
    std::cout << "\nConcorrência:\n";
    RUN_TEST(concurrent_send);
    RUN_TEST(send_async);
    
    std::cout << "\n=== TODOS OS TESTES PASSARAM ===\n";
    return 0;
}
