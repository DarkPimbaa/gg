#include "gg_ws/gg_ws.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace gg;

int main() {
    std::cout << "=== Exemplo Básico GG_ws ===\n\n";
    
    // Configuração do WebSocket
    WebSocket ws({
        .url = "wss://echo.websocket.org",
        .connectTimeout = std::chrono::seconds(10),
        .autoReconnect = true,
        .maxReconnectAttempts = 3,
        .ping = {
            .mode = PingMode::Opcode,
            .interval = std::chrono::seconds(30),
            .timeout = std::chrono::seconds(10),
            .autoPong = true
        }
    });
    
    // CPU affinity (opcional)
    int cores = WebSocket::getCoreCount();
    std::cout << "Núcleos disponíveis: " << cores << "\n";
    if (cores > 1) {
        ws.pinThread(1);
        std::cout << "Thread de I/O fixada no núcleo 1\n";
    }
    
    // Callbacks
    ws.onConnect([]() {
        std::cout << "[CONECTADO]\n";
    });
    
    ws.onDisconnect([](int code) {
        std::cout << "[DESCONECTADO] Código: " << code << "\n";
    });
    
    ws.onError([](int code, std::string_view msg) {
        std::cerr << "[ERRO " << code << "] " << msg << "\n";
    });
    
    ws.onMessage([](const Json& msg) {
        std::cout << "[JSON] " << msg.stringify() << "\n";
    });
    
    ws.onRawMessage([](std::string_view msg) {
        std::cout << "[RAW] " << msg << "\n";
    });
    
    ws.onPong([](std::string_view payload) {
        std::cout << "[PONG] payload=" << (payload.empty() ? "(vazio)" : std::string(payload)) << "\n";
    });
    
    // Conecta
    std::cout << "\nConectando a " << ws.url() << "...\n";
    
    if (!ws.connect()) {
        std::cerr << "Falha ao conectar\n";
        return 1;
    }
    
    // Envia algumas mensagens
    std::cout << "\nEnviando mensagens...\n";
    
    // Mensagem de texto simples
    ws.send("Hello, WebSocket!");
    
    // JSON estruturado
    Json request = Json::object();
    request["action"] = "echo";
    request["data"] = "test message";
    request["timestamp"] = 1234567890;
    ws.send(request);
    
    // Ping manual
    ws.sendPing("manual-ping");
    
    // Envio assíncrono (não bloqueia)
    for (int i = 0; i < 5; ++i) {
        ws.sendAsync("Async message " + std::to_string(i));
    }
    
    // Aguarda um pouco para receber respostas
    std::cout << "\nAguardando respostas por 5 segundos...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Demonstra mudança de configuração em runtime
    std::cout << "\nAlterando intervalo de ping para 15s...\n";
    ws.setPingInterval(std::chrono::seconds(15));
    
    // Mais um pouco de espera
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Desconecta
    std::cout << "\nDesconectando...\n";
    ws.disconnect();
    
    std::cout << "\n=== Fim do exemplo ===\n";
    return 0;
}
