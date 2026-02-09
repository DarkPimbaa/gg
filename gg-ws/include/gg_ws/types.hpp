#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace gg {

// ============================================
// Forward Declarations
// ============================================
class Json;
class WebSocket;

// ============================================
// Tipos de Ping
// ============================================
enum class PingMode {
    Disabled,       // Sem auto-ping
    Opcode,         // Ping via opcode WebSocket (0x9)
    TextMessage     // Ping via mensagem de texto customizada
};

// ============================================
// Configuração de Ping/Pong
// ============================================
struct PingConfig {
    PingMode mode{PingMode::Opcode};
    std::chrono::milliseconds interval{30000};      // Intervalo entre pings
    std::chrono::milliseconds timeout{10000};       // Timeout para pong
    std::string textMessage{"ping"};                // Mensagem para PingMode::TextMessage
    bool autoPong{true};                            // Responder pings automaticamente
};

// ============================================
// Configuração do WebSocket
// ============================================
struct WebSocketConfig {
    std::string url;
    std::chrono::milliseconds connectTimeout{10000};
    size_t maxMessageSize{16 * 1024 * 1024};        // 16MB
    bool autoReconnect{true};
    int maxReconnectAttempts{5};
    
    // Configuração de ping/pong
    PingConfig ping;
};

// ============================================
// Callbacks
// ============================================
using OnMessage = std::function<void(const Json& message)>;
using OnRawMessage = std::function<void(std::string_view message)>;
using OnError = std::function<void(int code, std::string_view msg)>;
using OnConnect = std::function<void()>;
using OnDisconnect = std::function<void(int code)>;
using OnPing = std::function<void(std::string_view payload)>;
using OnPong = std::function<void(std::string_view payload)>;

// ============================================
// Códigos de Erro
// ============================================
namespace ErrorCode {
    constexpr int Success = 0;
    constexpr int ConnectionFailed = 1001;
    constexpr int HandshakeFailed = 1002;
    constexpr int Timeout = 1003;
    constexpr int InvalidUrl = 1004;
    constexpr int TlsError = 1005;
    constexpr int SendFailed = 1006;
    constexpr int ReceiveFailed = 1007;
    constexpr int MessageTooLarge = 1008;
    constexpr int InvalidFrame = 1009;
    constexpr int PingTimeout = 1010;
    constexpr int Disconnected = 1011;
}

// ============================================
// WebSocket Close Codes (RFC 6455)
// ============================================
namespace CloseCode {
    constexpr int Normal = 1000;
    constexpr int GoingAway = 1001;
    constexpr int ProtocolError = 1002;
    constexpr int UnsupportedData = 1003;
    constexpr int NoStatusReceived = 1005;
    constexpr int AbnormalClosure = 1006;
    constexpr int InvalidPayload = 1007;
    constexpr int PolicyViolation = 1008;
    constexpr int MessageTooBig = 1009;
    constexpr int MandatoryExtension = 1010;
    constexpr int InternalError = 1011;
    constexpr int TlsHandshake = 1015;
}

// ============================================
// WebSocket Opcodes
// ============================================
namespace Opcode {
    constexpr uint8_t Continuation = 0x0;
    constexpr uint8_t Text = 0x1;
    constexpr uint8_t Binary = 0x2;
    constexpr uint8_t Close = 0x8;
    constexpr uint8_t Ping = 0x9;
    constexpr uint8_t Pong = 0xA;
}

} // namespace gg
