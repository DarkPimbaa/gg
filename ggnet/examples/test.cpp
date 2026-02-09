#define GGNET_ENABLE_SSL
#include "../include/ggnet/ws_client.hpp"
#include <iostream>
#include "../include/ggnet/json.hpp"

bool running = true;

int main() {
    try {
        ggnet::EpollLoop loop;
        ggnet::WsClient ws(loop);

        ws.onOpen([&ws]() {
            std::cout << "Conectando ws poly ..." << std::endl;
            
            ws.send(R"({
            "action": "subscribe",
            "subscriptions": [
                {
                "topic": "crypto_prices_chainlink",
                "type": "*",
                "filters": "{\"symbol\":\"btc/usd\"}"
                }
            ]
            })");
            std::cout << "Conectado ws poly." << std::endl;
        });

        ws.onMessage([](std::string_view msg) {
            try {
            // Converter string_view para string para o parser (se necessário)
            auto json = ggnet::Json::parse(std::string(msg));
            
            // Acesso seguro e fácil
            
            double price = json["payload"]["value"].as_double();
            std::cout << "Price Update: " << price << std::endl;
            
        } catch (...) {
            std::cout << "Raw Msg: " << msg << std::endl;
        }
        });

        ws.onClose([]() {
            std::cout << "Disconnected." << std::endl;
            running = false;
        });

        // Using Postman Echo for reliability
        std::cout << "Conectando ws poly ..." << std::endl;
        ws.connect("wss://ws-live-data.polymarket.com");

        // Run loop
        // In a real bot, you might run loop in a thread or main thread.
        // loop.run() blocks.
        loop.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
