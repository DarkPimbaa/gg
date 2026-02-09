#define GGNET_ENABLE_SSL
#include "../include/ggnet/ws_client.hpp"
#include <iostream>

bool running = true;

int main() {
    try {
        ggnet::EpollLoop loop;
        ggnet::WsClient ws(loop);

        ws.onOpen([&ws]() {
            std::cout << "Connected to WebSocket!" << std::endl;
            // Subscribe to Polymarket (Example payload)
            // Just sending a ping-like message or subscription
            // {"assets_ids":["210668f4-72b5-415d-a60d-6e3e1c66297d"],"type":"market"} - random example
            // Actually echo server is safer for generic test but user asked for Polymarket context likely.
            // Let's use Echo Server first for reliability of test.
            // wss://echo.websocket.org is often flaky.
            // Let's use Polymarket CLOB WS: wss://ws-subscriptions-clob.polymarket.com/ws/market
            
            // Send a subscription specific to Clob
            // Note: Polymarket CLOB requires valid asset IDs, if invalid closing connection?
            // Let's try sending a simple string for now if it accepts or connect to echo.
            // Echo: wss://ws.postman-echo.com/raw
            
            ws.send("Hello GGNet");
        });

        ws.onMessage([](std::string_view msg) {
            std::cout << "Received: " << msg << std::endl;
        });

        ws.onClose([]() {
            std::cout << "Disconnected." << std::endl;
            running = false;
        });

        // Using Postman Echo for reliability
        std::cout << "Connecting to wss://ws.postman-echo.com/raw ..." << std::endl;
        ws.connect("wss://ws.postman-echo.com/raw");

        // Run loop
        // In a real bot, you might run loop in a thread or main thread.
        // loop.run() blocks.
        loop.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
