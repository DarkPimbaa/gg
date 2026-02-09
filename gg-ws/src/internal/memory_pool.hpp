#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace gg::internal {

/**
 * @brief Pool de buffers pré-alocados para evitar fragmentação de memória.
 * 
 * Thread-safe para aquisição e liberação de buffers.
 * Usa RAII wrapper para devolver buffer automaticamente.
 */
class BufferPool {
public:
    /**
     * @brief RAII wrapper para buffer do pool.
     * 
     * Devolve automaticamente ao pool quando destruído.
     */
    class Buffer {
    public:
        Buffer() noexcept : pool_(nullptr), data_(nullptr), size_(0) {}
        
        Buffer(BufferPool* pool, char* data, size_t size) noexcept
            : pool_(pool), data_(data), size_(size) {}
        
        ~Buffer() {
            release();
        }
        
        // Movível
        Buffer(Buffer&& other) noexcept
            : pool_(other.pool_), data_(other.data_), size_(other.size_) {
            other.pool_ = nullptr;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        
        Buffer& operator=(Buffer&& other) noexcept {
            if (this != &other) {
                release();
                pool_ = other.pool_;
                data_ = other.data_;
                size_ = other.size_;
                other.pool_ = nullptr;
                other.data_ = nullptr;
                other.size_ = 0;
            }
            return *this;
        }
        
        // Não copiável
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        
        [[nodiscard]] char* data() noexcept { return data_; }
        [[nodiscard]] const char* data() const noexcept { return data_; }
        [[nodiscard]] size_t size() const noexcept { return size_; }
        [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
        
        explicit operator bool() const noexcept { return valid(); }
        
        char& operator[](size_t index) noexcept { return data_[index]; }
        const char& operator[](size_t index) const noexcept { return data_[index]; }
        
    private:
        BufferPool* pool_;
        char* data_;
        size_t size_;
        
        void release() {
            if (pool_ && data_) {
                pool_->release(data_);
                data_ = nullptr;
                pool_ = nullptr;
            }
        }
    };
    
    /**
     * @brief Cria pool com buffers pré-alocados.
     * @param bufferSize Tamanho de cada buffer em bytes
     * @param poolSize Número de buffers no pool
     */
    explicit BufferPool(size_t bufferSize, size_t poolSize = 16)
        : bufferSize_(bufferSize) {
        
        buffers_.reserve(poolSize);
        
        for (size_t i = 0; i < poolSize; ++i) {
            buffers_.push_back(std::make_unique<char[]>(bufferSize));
            available_.push_back(buffers_.back().get());
        }
    }
    
    /**
     * @brief Adquire buffer do pool (thread-safe).
     * @return Buffer RAII ou buffer inválido se pool vazio
     */
    Buffer acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.empty()) {
            // Pool vazio - aloca novo buffer
            buffers_.push_back(std::make_unique<char[]>(bufferSize_));
            return Buffer(this, buffers_.back().get(), bufferSize_);
        }
        
        char* ptr = available_.back();
        available_.pop_back();
        return Buffer(this, ptr, bufferSize_);
    }
    
    /**
     * @brief Retorna tamanho de cada buffer.
     */
    [[nodiscard]] size_t bufferSize() const noexcept { return bufferSize_; }
    
    /**
     * @brief Retorna número de buffers disponíveis.
     */
    [[nodiscard]] size_t available() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size();
    }
    
private:
    size_t bufferSize_;
    std::vector<std::unique_ptr<char[]>> buffers_;
    std::vector<char*> available_;
    mutable std::mutex mutex_;
    
    void release(char* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(ptr);
    }
    
    friend class Buffer;
};

} // namespace gg::internal
