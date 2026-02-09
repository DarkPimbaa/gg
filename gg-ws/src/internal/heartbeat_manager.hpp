#pragma once

#include "gg_ws/types.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace gg::internal {

/**
 * @brief Gerenciador de ping/pong com timer thread-safe.
 * 
 * Responsável por:
 * - Enviar pings automaticamente em intervalo configurável
 * - Detectar timeout de pong
 * - Suportar ping via opcode ou texto
 */
class HeartbeatManager {
public:
    using SendPingFn = std::function<bool()>;
    using SendTextPingFn = std::function<bool(std::string_view)>;
    using OnTimeoutFn = std::function<void()>;

    explicit HeartbeatManager(PingConfig config)
        : config_(std::move(config)) {}
    
    ~HeartbeatManager() {
        stop();
    }
    
    // Não copiável
    HeartbeatManager(const HeartbeatManager&) = delete;
    HeartbeatManager& operator=(const HeartbeatManager&) = delete;
    
    /**
     * @brief Inicia o timer de heartbeat.
     * @param sendPing Função para enviar ping opcode
     * @param sendTextPing Função para enviar ping texto
     * @param onTimeout Callback quando timeout
     */
    void start(SendPingFn sendPing, SendTextPingFn sendTextPing, OnTimeoutFn onTimeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_.load(std::memory_order_acquire)) {
            return;  // Já está rodando
        }
        
        if (config_.mode == PingMode::Disabled) {
            return;  // Ping desativado
        }
        
        sendPing_ = std::move(sendPing);
        sendTextPing_ = std::move(sendTextPing);
        onTimeout_ = std::move(onTimeout);
        
        running_.store(true, std::memory_order_release);
        waitingPong_.store(false, std::memory_order_release);
        
        timerThread_ = std::thread(&HeartbeatManager::timerLoop, this);
    }
    
    /**
     * @brief Para o timer de heartbeat.
     */
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_.load(std::memory_order_acquire)) {
                return;
            }
            running_.store(false, std::memory_order_release);
        }
        
        cv_.notify_all();
        
        if (timerThread_.joinable()) {
            timerThread_.join();
        }
    }
    
    /**
     * @brief Notifica que um pong foi recebido.
     */
    void onPongReceived() noexcept {
        waitingPong_.store(false, std::memory_order_release);
        lastPongReceived_ = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Altera intervalo de ping em runtime.
     */
    void setInterval(std::chrono::milliseconds interval) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.interval = interval;
        cv_.notify_all();  // Acorda para recalcular
    }
    
    /**
     * @brief Altera timeout de pong em runtime.
     */
    void setTimeout(std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.timeout = timeout;
    }
    
    /**
     * @brief Altera modo de ping em runtime.
     */
    void setMode(PingMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = mode;
        
        if (mode == PingMode::Disabled) {
            stop();
        }
    }
    
    /**
     * @brief Retorna configuração atual.
     */
    [[nodiscard]] PingConfig config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

private:
    PingConfig config_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> waitingPong_{false};
    std::chrono::steady_clock::time_point lastPingSent_;
    std::chrono::steady_clock::time_point lastPongReceived_;
    
    std::thread timerThread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    SendPingFn sendPing_;
    SendTextPingFn sendTextPing_;
    OnTimeoutFn onTimeout_;
    
    void timerLoop() {
        while (running_.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            auto interval = config_.interval;
            auto timeout = config_.timeout;
            auto mode = config_.mode;
            
            lock.unlock();
            
            // Aguarda intervalo ou stop
            {
                std::unique_lock<std::mutex> waitLock(mutex_);
                cv_.wait_for(waitLock, interval, [this] {
                    return !running_.load(std::memory_order_acquire);
                });
            }
            
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            
            // Verifica timeout do pong anterior
            if (waitingPong_.load(std::memory_order_acquire)) {
                auto elapsed = std::chrono::steady_clock::now() - lastPingSent_;
                if (elapsed > timeout) {
                    // Timeout! Chama callback
                    if (onTimeout_) {
                        onTimeout_();
                    }
                    waitingPong_.store(false, std::memory_order_release);
                    continue;
                }
            }
            
            // Envia ping
            bool sent = false;
            if (mode == PingMode::Opcode && sendPing_) {
                sent = sendPing_();
            } else if (mode == PingMode::TextMessage && sendTextPing_) {
                sent = sendTextPing_(config_.textMessage);
            }
            
            if (sent) {
                lastPingSent_ = std::chrono::steady_clock::now();
                waitingPong_.store(true, std::memory_order_release);
            }
        }
    }
};

} // namespace gg::internal
