#pragma once

#include <atomic>
#include <memory>
#include <optional>

namespace gg::internal {

/**
 * @brief Fila lock-free MPSC (Multiple Producer Single Consumer).
 * 
 * Thread-safe para múltiplos produtores e um único consumidor.
 * Usa ponteiros atômicos para evitar locks.
 */
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        
        Node() = default;
        explicit Node(T value) : data(std::move(value)) {}
    };
    
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    
public:
    LockFreeQueue() {
        // Dummy node para simplificar a lógica
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }
    
    ~LockFreeQueue() {
        // Limpa todos os nodes restantes
        while (pop().has_value()) {}
        
        // Deleta o dummy node
        delete head_.load(std::memory_order_relaxed);
    }
    
    // Não copiável
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    
    /**
     * @brief Adiciona item à fila (thread-safe, não bloqueia).
     * @param item Item a adicionar
     */
    void push(T item) {
        Node* newNode = new Node(std::move(item));
        
        Node* prevTail = tail_.exchange(newNode, std::memory_order_acq_rel);
        prevTail->next.store(newNode, std::memory_order_release);
    }
    
    /**
     * @brief Remove e retorna item da fila (apenas single consumer).
     * @return Item removido ou nullopt se vazia
     */
    std::optional<T> pop() {
        Node* head = head_.load(std::memory_order_relaxed);
        Node* next = head->next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            return std::nullopt;
        }
        
        // Move dados antes de deletar
        T result = std::move(next->data);
        
        head_.store(next, std::memory_order_release);
        delete head;
        
        return result;
    }
    
    /**
     * @brief Verifica se a fila está vazia (aproximado).
     * @note Resultado pode estar desatualizado devido à concorrência
     */
    [[nodiscard]] bool empty() const noexcept {
        Node* head = head_.load(std::memory_order_relaxed);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }
};

} // namespace gg::internal
