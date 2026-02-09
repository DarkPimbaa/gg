# GG_Observer

**Biblioteca C++17 de Eventos com Thread Affinity**

Uma biblioteca header-only moderna usando o padrão Observer, onde callbacks executam na thread que se inscreveu.

## ✨ Características

- 🧵 **Thread Affinity** - Callbacks executam na thread do subscriber
- 📦 **Header-only** - Basta incluir, sem necessidade de linkar
- 🔒 **Thread-safe** - Seguro para uso multi-threaded
- 🎯 **Type-safe** - Eventos tipados em tempo de compilação
- 🗑️ **RAII** - Auto-unsubscribe quando Subscription sai de escopo

## 📥 Instalação

### Opção 1: add_requires (recomendado)

Registre o repositório local (apenas uma vez):
```bash
xmake repo --add local-gg /mnt/ssd2/Documentos/GG/GG_Observer/packages
```

Depois use em qualquer projeto:
```lua
-- xmake.lua
add_requires("gg_observer")

target("MeuProjeto")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("gg_observer")
```

### Opção 2: xmake install

```bash
cd GG_Observer
xmake install -o ~/.local
```

Depois adicione o include path:
```lua
add_includedirs("~/.local/include")
```

### Opção 3: Copiar headers

Copie a pasta `src/` para seu projeto e inclua diretamente.

## 🚀 Uso Rápido

```cpp
#include <GG_Observer/GG_Observer.h>
#include <iostream>

// 1. Defina seus eventos
struct PlayerDamageEvent {
    int playerId;
    float damage;
};

int main() {
    // 2. Crie o EventBus
    gg::EventBus bus;
    
    // 3. Se inscreva (callback roda nesta thread)
    auto sub = bus.subscribe<PlayerDamageEvent>([](const PlayerDamageEvent& e) {
        std::cout << "Player " << e.playerId << " took " << e.damage << " damage\n";
    });
    
    // 4. Emita eventos
    bus.emit(PlayerDamageEvent{1, 25.5f});
    
    // 5. Processe eventos pendentes
    bus.poll();
    
    return 0;
}
```

## 🧵 Multi-threading

```cpp
#include <GG_Observer/GG_Observer.h>
#include <thread>
#include <atomic>

std::atomic<bool> running{true};
gg::EventBus bus;

// Worker thread
std::thread worker([&]() {
    // Inscrição captura esta thread
    auto sub = bus.subscribe<MyEvent>([](const MyEvent& e) {
        // EXECUTA AQUI, na worker thread!
    });
    
    while (running) {
        bus.poll();  // Processa eventos desta thread
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
});

// Main thread emite
bus.emit(MyEvent{});  // Enfileira para worker
```

## 📚 API

### EventBus

| Método | Descrição |
|--------|-----------|
| `subscribe<T>(callback)` | Inscreve callback para evento T |
| `emit(event)` | Emite evento para todos os subscribers |
| `poll()` | Processa eventos pendentes da thread atual |
| `hasPending()` | Verifica se há eventos pendentes |
| `clear<T>()` | Remove todos os listeners do tipo T |
| `clearAll()` | Remove todos os listeners |

### Subscription

| Método | Descrição |
|--------|-----------|
| `cancel()` | Cancela a inscrição manualmente |
| `isActive()` | Verifica se ainda está inscrito |

## 📁 Estrutura

```
GG_Observer/
├── src/
│   ├── GG_Observer.h      # Header principal
│   ├── EventBus.hpp       # Core do sistema
│   ├── Subscription.hpp   # RAII wrapper
│   └── ThreadQueue.hpp    # Fila thread-safe
├── examples/
│   └── main.cpp           # Exemplo completo
└── xmake.lua
```

## 🔧 Build

```bash
xmake                          # Build library
xmake build GG_Observer_Example # Build example
xmake run GG_Observer_Example   # Run example
```

## 📋 Requisitos

- C++17 ou superior
- xmake (para build)

## 📄 Licença

MIT License
