# GGNet (Green Global Network) - C++ Network Library

A modern, header-only, Linux-native C++ networking library designed for high-performance trading systems (HFT), bots, and backend services.

## Features
- **Header-only**: Easy integration (`#include "ggnet/..."`).
- **High Performance**: Native Linux `epoll` event loop.
- **Async & Thread-Safe**: Thread-safe message dispatching mechanism.
- **Protocols**: 
  - HTTP/1.1 (Keep-Alive, Warmup, Persistent Connections).
  - WebSocket (RFC 6455, Auto-Reassembly, Masking).
- **Security**: TLS 1.2/1.3 via OpenSSL (robust rotation support).
- **Zero-Dependency Core**: Only depends on OpenSSL (optional but recommended).
- **Built-in JSON**: Tiny, fast header-only JSON parser integrated.

## Installation
Just copy the `include/ggnet` folder to your project or include it directly.
Dependencies: `openssl` (dev package).

```bash
sudo apt install libssl-dev
```

## Quick Start

### 1. HTTP GET (Binance Price)
```cpp
#define GGNET_ENABLE_SSL
#include "ggnet/http_client.hpp"
#include "ggnet/json.hpp"
#include <iostream>

int main() {
    ggnet::EpollLoop loop;
    ggnet::HttpClient client(loop);

    // Optional: Pre-warm connection for lower latency on first request
    client.warmup("https://api.binance.com");

    client.get("https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT", 
        [&loop](ggnet::HttpResponse resp) {
            if (resp.status_code == 200) {
                auto json = ggnet::Json::parse(resp.body);
                std::cout << "BTC Price: " << json["price"].as_double() << std::endl;
            }
            loop.stop();
        }
    );

    loop.run();
    return 0;
}
```

### 2. WebSocket Bot (Thread-Safe Sending)
Você pode chamar `ws.send()` de qualquer thread!

```cpp
#define GGNET_ENABLE_SSL
#include "ggnet/ws_client.hpp"
#include "ggnet/json.hpp"
#include <iostream>
#include <thread>

int main() {
    ggnet::EpollLoop loop;
    ggnet::WsClient ws(loop);

    ws.onOpen([&ws]() {
        std::cout << "Connected!" << std::endl;
        
        // Exemplo: Thread separada enviando mensagens
        std::thread([&ws]() {
            while(true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ws.send("{\"action\": \"ping\"}"); // Thread-safe!
            }
        }).detach();
    });

    // Parseando JSON recebido
    ws.onMessage([](std::string_view msg) {
        try {
            // Converter string_view para string para o parser (se necessário)
            auto json = ggnet::Json::parse(std::string(msg));
            
            // Acesso seguro e fácil
            if (json["type"].as_string() == "ticker") {
                double price = json["data"]["price"].as_double();
                std::cout << "Price Update: " << price << std::endl;
            }
        } catch (...) {
            std::cout << "Raw Msg: " << msg << std::endl;
        }
    });


    ws.connect("wss://ws.postman-echo.com/raw");
    loop.run();
    return 0;
}
```

### 3. Manipulando Arrays JSON

O parser JSON (`ggnet::Json`) expõe os vetores internos publicamente, permitindo acesso direto e fácil aos dados.

```cpp
auto json = ggnet::Json::parse(R"({"precos": [10.5, 20.0, 30.2]})");
auto array = json["precos"];

// Verificar se é array
if (array.type == ggnet::Json::ARRAY) {
    
    // Iterar (Range-based for)
    for (auto& item : array.arr_val) {
        std::cout << item.as_double() << " ";
    }

    // Acesso direto por índice
    double primeiro = array[0].as_double();

    // Pegar o ÚLTIMO elemento (tamanho desconhecido)
    if (!array.arr_val.empty()) {
        double ultimo = array.arr_val.back().as_double();
    }
    
    // Tamanho do array
    size_t tamanho = array.arr_val.size();
}
```

## Compilação
```bash
g++ -o app main.cpp -Iinclude -lssl -lcrypto
```
