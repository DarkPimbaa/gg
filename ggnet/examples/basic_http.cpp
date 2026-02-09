#define GGNET_ENABLE_SSL
#include "../include/ggnet/socket.hpp"
#include "../include/ggnet/epoll.hpp"
#include "../include/ggnet/http_client.hpp"
#include "../include/ggnet/json.hpp"
#include <iostream>

void test_binance_ticker() {
    ggnet::EpollLoop loop;
    ggnet::HttpClient client(loop);

    std::cout << "Starting request to Binance..." << std::endl;

    client.get("https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT", 
        [&loop](ggnet::HttpResponse resp) {
            std::cout << "Response Status: " << resp.status_code << std::endl; // Note: status_code parsing not fully implemented yet in HttpClient
            std::cout << "Body: " << resp.body << std::endl;
            
            // Extract content from body (rudimentary check since status code parsing is missing in V1)
            size_t json_start = resp.body.find("{");
            if(json_start != std::string::npos) {
                std::string json_str = resp.body.substr(json_start);
                try {
                    ggnet::Json json = ggnet::Json::parse(json_str);
                    std::cout << "Parsed Symbol: " << json["symbol"].as_string() << std::endl;
                    std::cout << "Parsed Price: " << json["price"].as_double() << std::endl;
                } catch(const std::exception& e) {
                    std::cout << "JSON Parse error: " << e.what() << std::endl;
                }
            }
            
            loop.stop(); // Stop loop after response
        }
    );

    loop.run();
}

int main() {
    try {
        test_binance_ticker();
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
    }
    return 0;
}
