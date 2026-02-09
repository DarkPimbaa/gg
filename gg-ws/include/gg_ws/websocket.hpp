#pragma once

#include "types.hpp"
#include "json.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace gg {

/**
 * @brief Cliente WebSocket thread-safe focado em performance.
 * 
 * Features:
 * - Thread-safe para envio concorrente
 * - Auto ping/pong configurável (opcode ou texto)
 * - Reconexão automática
 * - CPU affinity via pinThread()
 * - Parser JSON integrado
 * 
 * Exemplo:
 * @code
 *   gg::WebSocket ws({
 *       .url = "wss://example.com/ws",
 *       .ping = { .mode = gg::PingMode::Opcode, .interval = std::chrono::seconds(30) }
 *   });
 *   
 *   ws.pinThread(2);  // Fixa thread no núcleo 2
 *   
 *   ws.onMessage([](const gg::Json& msg) {
 *       std::cout << msg["type"].getString() << "\n";
 *   });
 *   
 *   ws.connect();
 * @endcode
 */
class WebSocket {
public:
    explicit WebSocket(WebSocketConfig config);
    ~WebSocket();
    
    // Não copiável
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    
    // Movível
    WebSocket(WebSocket&& other) noexcept;
    WebSocket& operator=(WebSocket&& other) noexcept;

    // ============================================
    // CPU Affinity
    // ============================================
    
    /**
     * @brief Fixa a thread de I/O em um núcleo específico da CPU.
     * @param core Número do núcleo (0-based)
     * @return true se sucesso, false se falhou (núcleo inválido ou sem permissão)
     * @note Deve ser chamado ANTES de connect() para maior efetividade
     */
    bool pinThread(int core);
    
    /**
     * @brief Retorna o número de núcleos disponíveis na CPU.
     */
    static int getCoreCount() noexcept;

    // ============================================
    // Lifecycle
    // ============================================
    
    /**
     * @brief Conecta ao servidor WebSocket.
     * @return true se conexão bem-sucedida
     */
    bool connect();
    
    /**
     * @brief Desconecta do servidor.
     * @param code Código de fechamento (default: CloseCode::Normal)
     */
    void disconnect(int code = CloseCode::Normal);
    
    /**
     * @brief Verifica se está conectado.
     */
    [[nodiscard]] bool isConnected() const noexcept;
    
    /**
     * @brief Aguarda até desconectar (bloqueante).
     */
    void wait();

    // ============================================
    // Envio (Thread-Safe)
    // ============================================
    
    /**
     * @brief Envia mensagem de texto.
     * @param message Mensagem a enviar
     * @return true se enviado com sucesso
     */
    bool send(std::string_view message);
    
    /**
     * @brief Envia objeto JSON.
     * @param message JSON a enviar
     * @return true se enviado com sucesso
     */
    bool send(const Json& message);
    
    /**
     * @brief Envia dados binários.
     * @param data Dados binários
     * @param size Tamanho em bytes
     * @return true se enviado com sucesso
     */
    bool sendBinary(const void* data, size_t size);
    
    /**
     * @brief Envia mensagem de forma assíncrona (não bloqueia).
     * @param message Mensagem a enviar
     * @note Mensagem é copiada para fila interna
     */
    void sendAsync(std::string_view message);

    // ============================================
    // Ping/Pong
    // ============================================
    
    /**
     * @brief Envia ping com opcode WebSocket (0x9).
     * @return true se enviado com sucesso
     */
    bool sendPing();
    
    /**
     * @brief Envia ping com payload customizado.
     * @param payload Dados do ping (max 125 bytes)
     * @return true se enviado com sucesso
     */
    bool sendPing(std::string_view payload);
    
    /**
     * @brief Envia pong com opcode WebSocket (0xA).
     * @param payload Dados do pong (deve ecoar o ping)
     * @return true se enviado com sucesso
     */
    bool sendPong(std::string_view payload = "");
    
    /**
     * @brief Altera modo de ping em runtime.
     */
    void setPingMode(PingMode mode);
    
    /**
     * @brief Altera intervalo de ping em runtime.
     */
    void setPingInterval(std::chrono::milliseconds interval);
    
    /**
     * @brief Altera timeout de pong em runtime.
     */
    void setPingTimeout(std::chrono::milliseconds timeout);
    
    /**
     * @brief Ativa/desativa auto pong.
     */
    void setAutoPong(bool enabled);

    // ============================================
    // Callbacks
    // ============================================
    
    /**
     * @brief Define callback para mensagens JSON.
     * @note Tenta parsear como JSON, se falhar não é chamado
     */
    void onMessage(OnMessage callback);
    
    /**
     * @brief Define callback para mensagens raw (texto).
     */
    void onRawMessage(OnRawMessage callback);
    
    /**
     * @brief Define callback para erros.
     */
    void onError(OnError callback);
    
    /**
     * @brief Define callback para conexão estabelecida.
     */
    void onConnect(OnConnect callback);
    
    /**
     * @brief Define callback para desconexão.
     */
    void onDisconnect(OnDisconnect callback);
    
    /**
     * @brief Define callback para pings recebidos.
     */
    void onPing(OnPing callback);
    
    /**
     * @brief Define callback para pongs recebidos.
     */
    void onPong(OnPong callback);

    // ============================================
    // Configuração
    // ============================================
    
    /**
     * @brief Retorna a URL de conexão.
     */
    [[nodiscard]] std::string_view url() const noexcept;
    
    /**
     * @brief Ativa/desativa reconexão automática.
     */
    void setAutoReconnect(bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
