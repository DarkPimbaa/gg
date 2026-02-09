#include "gg_ws/websocket.hpp"
#include "internal/cpu_affinity.hpp"
#include "internal/heartbeat_manager.hpp"
#include "internal/message_queue.hpp"
#include "internal/memory_pool.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <condition_variable>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "crypt32.lib")
    using socket_t = SOCKET;
    #define SOCKET_ERROR_VALUE INVALID_SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    using socket_t = int;
    #define SOCKET_ERROR_VALUE (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace gg {

// ============================================
// URL Parser
// ============================================
namespace {

struct ParsedUrl {
    bool secure = false;
    std::string host;
    uint16_t port = 0;
    std::string path;
    
    bool valid() const { return !host.empty() && port > 0; }
};

ParsedUrl parseUrl(std::string_view url) {
    ParsedUrl result;
    
    // Verifica protocolo
    if (url.substr(0, 6) == "wss://") {
        result.secure = true;
        url.remove_prefix(6);
    } else if (url.substr(0, 5) == "ws://") {
        result.secure = false;
        url.remove_prefix(5);
    } else {
        return result;  // Inválido
    }
    
    // Porta padrão
    result.port = result.secure ? 443 : 80;
    
    // Encontra path
    size_t pathPos = url.find('/');
    std::string_view hostPort;
    if (pathPos != std::string_view::npos) {
        hostPort = url.substr(0, pathPos);
        result.path = std::string(url.substr(pathPos));
    } else {
        hostPort = url;
        result.path = "/";
    }
    
    // Encontra porta
    size_t colonPos = hostPort.rfind(':');
    if (colonPos != std::string_view::npos) {
        result.host = std::string(hostPort.substr(0, colonPos));
        auto portStr = hostPort.substr(colonPos + 1);
        result.port = static_cast<uint16_t>(std::stoi(std::string(portStr)));
    } else {
        result.host = std::string(hostPort);
    }
    
    return result;
}

// Base64 encoding para handshake
std::string base64Encode(const unsigned char* data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
        
        result += table[(n >> 18) & 0x3F];
        result += table[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? table[n & 0x3F] : '=';
    }
    
    return result;
}

// Gera chave WebSocket aleatória
std::string generateWebSocketKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    unsigned char key[16];
    for (auto& b : key) {
        b = static_cast<unsigned char>(dis(gen));
    }
    
    return base64Encode(key, 16);
}

} // anonymous namespace

// ============================================
// WebSocket Implementation
// ============================================
struct WebSocket::Impl {
    WebSocketConfig config;
    ParsedUrl parsedUrl;
    
    // Socket e SSL
    socket_t socket = SOCKET_ERROR_VALUE;
    SSL_CTX* sslCtx = nullptr;
    SSL* ssl = nullptr;
    
    // Estado
    std::atomic<bool> connected{false};
    std::atomic<bool> running{false};
    std::atomic<bool> shouldReconnect{false};
    int reconnectAttempts = 0;
    
    // Thread de I/O
    std::thread ioThread;
    int pinnedCore = -1;
    
    // Callbacks
    OnMessage onMessageCb;
    OnRawMessage onRawMessageCb;
    OnError onErrorCb;
    OnConnect onConnectCb;
    OnDisconnect onDisconnectCb;
    OnPing onPingCb;
    OnPong onPongCb;
    
    // Mutexes
    mutable std::shared_mutex stateMutex;
    mutable std::mutex callbackMutex;
    mutable std::mutex sendMutex;
    std::condition_variable_any waitCv;
    
    // Fila de mensagens assíncronas
    internal::LockFreeQueue<std::string> sendQueue;
    
    // Heartbeat
    std::unique_ptr<internal::HeartbeatManager> heartbeat;
    
    // Buffer pool
    internal::BufferPool bufferPool{8192, 8};
    
    Impl(WebSocketConfig cfg) : config(std::move(cfg)) {
        parsedUrl = parseUrl(config.url);
        heartbeat = std::make_unique<internal::HeartbeatManager>(config.ping);
    }
    
    ~Impl() {
        disconnect(CloseCode::GoingAway);
    }
    
    // ============================================
    // Conexão
    // ============================================
    bool connect() {
        if (!parsedUrl.valid()) {
            triggerError(ErrorCode::InvalidUrl, "URL inválida");
            return false;
        }
        
        // Inicializa socket
        if (!initSocket()) {
            return false;
        }
        
        // Conecta ao servidor
        if (!connectSocket()) {
            cleanup();
            return false;
        }
        
        // Inicializa TLS se necessário
        if (parsedUrl.secure && !initTls()) {
            cleanup();
            return false;
        }
        
        // Handshake WebSocket
        if (!performHandshake()) {
            cleanup();
            return false;
        }
        
        // Marca como conectado
        {
            std::unique_lock lock(stateMutex);
            connected.store(true, std::memory_order_release);
            running.store(true, std::memory_order_release);
            reconnectAttempts = 0;
        }
        
        // Inicia thread de I/O
        ioThread = std::thread(&Impl::ioLoop, this);
        
        // Inicia heartbeat
        heartbeat->start(
            [this]() { return sendPingFrame(""); },
            [this](std::string_view msg) { return sendTextMessage(std::string(msg)); },
            [this]() { triggerError(ErrorCode::PingTimeout, "Pong timeout"); }
        );
        
        // Callback
        triggerConnect();
        
        return true;
    }
    
    void disconnect(int code) {
        bool wasConnected = connected.exchange(false, std::memory_order_acq_rel);
        running.store(false, std::memory_order_release);
        
        // Para heartbeat
        if (heartbeat) {
            heartbeat->stop();
        }
        
        // Envia close frame
        if (wasConnected) {
            sendCloseFrame(code);
        }
        
        // Notifica thread
        waitCv.notify_all();
        
        // Aguarda thread
        if (ioThread.joinable()) {
            ioThread.join();
        }
        
        // Limpa recursos
        cleanup();
        
        // Callback
        if (wasConnected) {
            triggerDisconnect(code);
        }
    }
    
    // ============================================
    // Envio
    // ============================================
    bool send(std::string_view message) {
        if (!connected.load(std::memory_order_acquire)) {
            return false;
        }
        return sendFrame(Opcode::Text, message);
    }
    
    bool sendBinary(const void* data, size_t size) {
        if (!connected.load(std::memory_order_acquire)) {
            return false;
        }
        return sendFrame(Opcode::Binary, std::string_view(static_cast<const char*>(data), size));
    }
    
    void sendAsync(std::string_view message) {
        sendQueue.push(std::string(message));
    }
    
    bool sendPingFrame(std::string_view payload) {
        if (!connected.load(std::memory_order_acquire)) {
            return false;
        }
        return sendFrame(Opcode::Ping, payload);
    }
    
    bool sendPongFrame(std::string_view payload) {
        if (!connected.load(std::memory_order_acquire)) {
            return false;
        }
        return sendFrame(Opcode::Pong, payload);
    }
    
    bool sendTextMessage(const std::string& msg) {
        return send(msg);
    }

private:
    // ============================================
    // Socket Init
    // ============================================
    bool initSocket() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            triggerError(ErrorCode::ConnectionFailed, "WSAStartup falhou");
            return false;
        }
#endif
        
        socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == SOCKET_ERROR_VALUE) {
            triggerError(ErrorCode::ConnectionFailed, "Falha ao criar socket");
            return false;
        }
        
        // Desabilita Nagle para menor latência
        int flag = 1;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));
        
        return true;
    }
    
    bool connectSocket() {
        // Resolve hostname
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        std::string portStr = std::to_string(parsedUrl.port);
        if (getaddrinfo(parsedUrl.host.c_str(), portStr.c_str(), &hints, &result) != 0) {
            triggerError(ErrorCode::ConnectionFailed, "DNS lookup falhou: " + parsedUrl.host);
            return false;
        }
        
        // Conecta
        int err = ::connect(socket, result->ai_addr, static_cast<int>(result->ai_addrlen));
        freeaddrinfo(result);
        
        if (err != 0) {
            triggerError(ErrorCode::ConnectionFailed, "Conexão falhou");
            return false;
        }
        
        return true;
    }
    
    bool initTls() {
        // Inicializa OpenSSL
        SSL_library_init();
        SSL_load_error_strings();
        
        sslCtx = SSL_CTX_new(TLS_client_method());
        if (!sslCtx) {
            triggerError(ErrorCode::TlsError, "Falha ao criar SSL context");
            return false;
        }
        
        // Configurações de segurança
        SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(sslCtx);
        SSL_CTX_set_min_proto_version(sslCtx, TLS1_2_VERSION);
        
        ssl = SSL_new(sslCtx);
        if (!ssl) {
            triggerError(ErrorCode::TlsError, "Falha ao criar SSL");
            return false;
        }
        
        SSL_set_fd(ssl, static_cast<int>(socket));
        SSL_set_tlsext_host_name(ssl, parsedUrl.host.c_str());
        
        if (SSL_connect(ssl) != 1) {
            char errBuf[256];
            ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
            triggerError(ErrorCode::TlsError, std::string("TLS handshake falhou: ") + errBuf);
            return false;
        }
        
        return true;
    }
    
    bool performHandshake() {
        std::string key = generateWebSocketKey();
        
        // Monta request
        std::ostringstream request;
        request << "GET " << parsedUrl.path << " HTTP/1.1\r\n";
        request << "Host: " << parsedUrl.host;
        if ((parsedUrl.secure && parsedUrl.port != 443) || (!parsedUrl.secure && parsedUrl.port != 80)) {
            request << ":" << parsedUrl.port;
        }
        request << "\r\n";
        request << "Upgrade: websocket\r\n";
        request << "Connection: Upgrade\r\n";
        request << "Sec-WebSocket-Key: " << key << "\r\n";
        request << "Sec-WebSocket-Version: 13\r\n";
        request << "\r\n";
        
        std::string req = request.str();
        if (!rawSend(req.data(), req.size())) {
            triggerError(ErrorCode::HandshakeFailed, "Falha ao enviar handshake");
            return false;
        }
        
        // Lê resposta
        char buffer[1024];
        ssize_t received = rawRecv(buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            triggerError(ErrorCode::HandshakeFailed, "Sem resposta do servidor");
            return false;
        }
        buffer[received] = '\0';
        
        // Verifica resposta
        std::string response(buffer, received);
        if (response.find("101") == std::string::npos ||
            response.find("Upgrade") == std::string::npos) {
            triggerError(ErrorCode::HandshakeFailed, "Handshake rejeitado pelo servidor");
            return false;
        }
        
        return true;
    }
    
    // ============================================
    // I/O Loop
    // ============================================
    void ioLoop() {
        // Aplica CPU affinity se configurado
        if (pinnedCore >= 0) {
            internal::pinCurrentThread(pinnedCore);
        }
        
        std::vector<char> frameBuffer;
        frameBuffer.reserve(config.maxMessageSize);
        
        while (running.load(std::memory_order_acquire)) {
            // Processa fila de envio assíncrono
            while (auto msg = sendQueue.pop()) {
                send(*msg);
            }
            
            // Verifica dados disponíveis
            if (!waitForData(100)) {
                continue;
            }
            
            // Lê frame
            if (!readFrame(frameBuffer)) {
                if (connected.load(std::memory_order_acquire)) {
                    handleDisconnect();
                }
                break;
            }
        }
    }
    
    bool waitForData(int timeoutMs) {
#ifdef _WIN32
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket, &readSet);
        
        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        
        return select(static_cast<int>(socket) + 1, &readSet, nullptr, nullptr, &timeout) > 0;
#else
        pollfd pfd;
        pfd.fd = socket;
        pfd.events = POLLIN;
        return poll(&pfd, 1, timeoutMs) > 0 && (pfd.revents & POLLIN);
#endif
    }
    
    bool readFrame(std::vector<char>& buffer) {
        // Lê header (2 bytes mínimo)
        uint8_t header[2];
        if (rawRecv(reinterpret_cast<char*>(header), 2) != 2) {
            return false;
        }
        
        bool fin = (header[0] & 0x80) != 0;
        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7F;
        
        // Lê payload length estendido
        if (payloadLen == 126) {
            uint8_t lenBytes[2];
            if (rawRecv(reinterpret_cast<char*>(lenBytes), 2) != 2) return false;
            payloadLen = (static_cast<uint64_t>(lenBytes[0]) << 8) | lenBytes[1];
        } else if (payloadLen == 127) {
            uint8_t lenBytes[8];
            if (rawRecv(reinterpret_cast<char*>(lenBytes), 8) != 8) return false;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | lenBytes[i];
            }
        }
        
        // Verifica tamanho máximo
        if (payloadLen > config.maxMessageSize) {
            triggerError(ErrorCode::MessageTooLarge, "Mensagem muito grande");
            return false;
        }
        
        // Lê mask key (se masked)
        uint8_t maskKey[4] = {0};
        if (masked) {
            if (rawRecv(reinterpret_cast<char*>(maskKey), 4) != 4) return false;
        }
        
        // Lê payload
        buffer.resize(payloadLen);
        if (payloadLen > 0) {
            size_t totalRead = 0;
            while (totalRead < payloadLen) {
                ssize_t n = rawRecv(buffer.data() + totalRead, payloadLen - totalRead);
                if (n <= 0) return false;
                totalRead += n;
            }
            
            // Aplica unmask
            if (masked) {
                for (size_t i = 0; i < payloadLen; ++i) {
                    buffer[i] ^= maskKey[i % 4];
                }
            }
        }
        
        // Processa baseado no opcode
        std::string_view payload(buffer.data(), buffer.size());
        
        switch (opcode) {
            case Opcode::Text:
            case Opcode::Binary:
                handleMessage(payload);
                break;
                
            case Opcode::Close:
                handleClose(payload);
                return false;
                
            case Opcode::Ping:
                handlePing(payload);
                break;
                
            case Opcode::Pong:
                handlePong(payload);
                break;
                
            default:
                break;
        }
        
        return true;
    }
    
    // ============================================
    // Frame Sending
    // ============================================
    bool sendFrame(uint8_t opcode, std::string_view payload) {
        std::lock_guard<std::mutex> lock(sendMutex);
        
        std::vector<uint8_t> frame;
        frame.reserve(14 + payload.size());
        
        // Header
        frame.push_back(0x80 | opcode);  // FIN + opcode
        
        // Payload length + mask bit (client sempre mascara)
        if (payload.size() < 126) {
            frame.push_back(0x80 | static_cast<uint8_t>(payload.size()));
        } else if (payload.size() <= 65535) {
            frame.push_back(0x80 | 126);
            frame.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
            frame.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
        } else {
            frame.push_back(0x80 | 127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back(static_cast<uint8_t>((payload.size() >> (i * 8)) & 0xFF));
            }
        }
        
        // Mask key
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        uint8_t maskKey[4];
        for (auto& b : maskKey) {
            b = static_cast<uint8_t>(dis(gen));
            frame.push_back(b);
        }
        
        // Masked payload
        for (size_t i = 0; i < payload.size(); ++i) {
            frame.push_back(static_cast<uint8_t>(payload[i]) ^ maskKey[i % 4]);
        }
        
        return rawSend(reinterpret_cast<const char*>(frame.data()), frame.size());
    }
    
    void sendCloseFrame(int code) {
        uint8_t payload[2] = {
            static_cast<uint8_t>((code >> 8) & 0xFF),
            static_cast<uint8_t>(code & 0xFF)
        };
        sendFrame(Opcode::Close, std::string_view(reinterpret_cast<char*>(payload), 2));
    }
    
    // ============================================
    // Raw I/O
    // ============================================
    bool rawSend(const char* data, size_t len) {
        size_t totalSent = 0;
        while (totalSent < len) {
            ssize_t sent;
            if (ssl) {
                sent = SSL_write(ssl, data + totalSent, static_cast<int>(len - totalSent));
            } else {
                sent = ::send(socket, data + totalSent, len - totalSent, 0);
            }
            
            if (sent <= 0) {
                return false;
            }
            totalSent += sent;
        }
        return true;
    }
    
    ssize_t rawRecv(char* buffer, size_t len) {
        if (ssl) {
            return SSL_read(ssl, buffer, static_cast<int>(len));
        } else {
            return ::recv(socket, buffer, len, 0);
        }
    }
    
    // ============================================
    // Handlers
    // ============================================
    void handleMessage(std::string_view data) {
        // Callback raw
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (onRawMessageCb) {
                onRawMessageCb(data);
            }
        }
        
        // Tenta parsear como JSON
        auto json = Json::parse(data);
        if (json) {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (onMessageCb) {
                onMessageCb(*json);
            }
        }
    }
    
    void handlePing(std::string_view payload) {
        // Auto pong
        if (config.ping.autoPong) {
            sendPongFrame(payload);
        }
        
        // Callback
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (onPingCb) {
            onPingCb(payload);
        }
    }
    
    void handlePong(std::string_view payload) {
        // Notifica heartbeat
        if (heartbeat) {
            heartbeat->onPongReceived();
        }
        
        // Callback
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (onPongCb) {
            onPongCb(payload);
        }
    }
    
    void handleClose(std::string_view payload) {
        int code = CloseCode::NoStatusReceived;
        if (payload.size() >= 2) {
            code = (static_cast<uint8_t>(payload[0]) << 8) | static_cast<uint8_t>(payload[1]);
        }
        
        connected.store(false, std::memory_order_release);
        running.store(false, std::memory_order_release);
    }
    
    void handleDisconnect() {
        connected.store(false, std::memory_order_release);
        
        // Tenta reconectar se configurado
        if (config.autoReconnect && reconnectAttempts < config.maxReconnectAttempts) {
            reconnectAttempts++;
            std::this_thread::sleep_for(std::chrono::seconds(1) * reconnectAttempts);
            
            cleanup();
            if (connect()) {
                return;
            }
        }
        
        running.store(false, std::memory_order_release);
        triggerDisconnect(CloseCode::AbnormalClosure);
    }
    
    // ============================================
    // Callbacks
    // ============================================
    void triggerError(int code, std::string_view msg) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (onErrorCb) {
            onErrorCb(code, msg);
        }
    }
    
    void triggerConnect() {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (onConnectCb) {
            onConnectCb();
        }
    }
    
    void triggerDisconnect(int code) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (onDisconnectCb) {
            onDisconnectCb(code);
        }
    }
    
    // ============================================
    // Cleanup
    // ============================================
    void cleanup() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
        
        if (sslCtx) {
            SSL_CTX_free(sslCtx);
            sslCtx = nullptr;
        }
        
        if (socket != SOCKET_ERROR_VALUE) {
            CLOSE_SOCKET(socket);
            socket = SOCKET_ERROR_VALUE;
        }
        
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

// ============================================
// WebSocket Public API
// ============================================
WebSocket::WebSocket(WebSocketConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

WebSocket::~WebSocket() = default;

WebSocket::WebSocket(WebSocket&& other) noexcept = default;
WebSocket& WebSocket::operator=(WebSocket&& other) noexcept = default;

bool WebSocket::pinThread(int core) {
    if (!internal::isValidCore(core)) {
        return false;
    }
    impl_->pinnedCore = core;
    return true;
}

int WebSocket::getCoreCount() noexcept {
    return internal::getCoreCount();
}

bool WebSocket::connect() {
    return impl_->connect();
}

void WebSocket::disconnect(int code) {
    impl_->disconnect(code);
}

bool WebSocket::isConnected() const noexcept {
    return impl_->connected.load(std::memory_order_acquire);
}

void WebSocket::wait() {
    if (impl_->ioThread.joinable()) {
        impl_->ioThread.join();
    }
}

bool WebSocket::send(std::string_view message) {
    return impl_->send(message);
}

bool WebSocket::send(const Json& message) {
    return impl_->send(message.stringify());
}

bool WebSocket::sendBinary(const void* data, size_t size) {
    return impl_->sendBinary(data, size);
}

void WebSocket::sendAsync(std::string_view message) {
    impl_->sendAsync(message);
}

bool WebSocket::sendPing() {
    return impl_->sendPingFrame("");
}

bool WebSocket::sendPing(std::string_view payload) {
    return impl_->sendPingFrame(payload);
}

bool WebSocket::sendPong(std::string_view payload) {
    return impl_->sendPongFrame(payload);
}

void WebSocket::setPingMode(PingMode mode) {
    impl_->heartbeat->setMode(mode);
}

void WebSocket::setPingInterval(std::chrono::milliseconds interval) {
    impl_->heartbeat->setInterval(interval);
}

void WebSocket::setPingTimeout(std::chrono::milliseconds timeout) {
    impl_->heartbeat->setTimeout(timeout);
}

void WebSocket::setAutoPong(bool enabled) {
    impl_->config.ping.autoPong = enabled;
}

void WebSocket::onMessage(OnMessage callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onMessageCb = std::move(callback);
}

void WebSocket::onRawMessage(OnRawMessage callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onRawMessageCb = std::move(callback);
}

void WebSocket::onError(OnError callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onErrorCb = std::move(callback);
}

void WebSocket::onConnect(OnConnect callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onConnectCb = std::move(callback);
}

void WebSocket::onDisconnect(OnDisconnect callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onDisconnectCb = std::move(callback);
}

void WebSocket::onPing(OnPing callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onPingCb = std::move(callback);
}

void WebSocket::onPong(OnPong callback) {
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->onPongCb = std::move(callback);
}

std::string_view WebSocket::url() const noexcept {
    return impl_->config.url;
}

void WebSocket::setAutoReconnect(bool enabled) {
    impl_->config.autoReconnect = enabled;
}

} // namespace gg
