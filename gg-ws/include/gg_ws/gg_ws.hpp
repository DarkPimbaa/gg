#pragma once

/**
 * @file gg_ws.hpp
 * @brief Header único para biblioteca GG_ws - WebSocket Client C++ Thread-Safe
 * 
 * Inclui automaticamente todos os componentes:
 * - types.hpp: Tipos, enums e configurações
 * - json.hpp: Parser JSON minimalista
 * - websocket.hpp: Cliente WebSocket
 * 
 * Exemplo de uso:
 * @code
 *   #include <gg_ws/gg_ws.hpp>
 *   
 *   int main() {
 *       gg::WebSocket ws({
 *           .url = "wss://example.com/ws",
 *           .ping = {
 *               .mode = gg::PingMode::Opcode,
 *               .interval = std::chrono::seconds(30)
 *           }
 *       });
 *       
 *       ws.pinThread(0);  // Fixa no núcleo 0
 *       
 *       ws.onMessage([](const gg::Json& msg) {
 *           std::cout << msg["type"].getString() << "\n";
 *       });
 *       
 *       ws.onPong([](std::string_view) {
 *           std::cout << "Pong recebido!\n";
 *       });
 *       
 *       if (ws.connect()) {
 *           gg::Json req;
 *           req["action"] = "subscribe";
 *           req["channel"] = "trades";
 *           ws.send(req);
 *           
 *           ws.wait();  // Aguarda até desconectar
 *       }
 *       
 *       return 0;
 *   }
 * @endcode
 */

#include "types.hpp"
#include "json.hpp"
#include "websocket.hpp"
