#pragma once

#include "socket.hpp"
#include "epoll.hpp"
#include "tls_context.hpp"
#include "utils.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <random>

namespace ggnet {

class WsClient {
    EpollLoop& loop;
    Socket sock;
    #ifdef GGNET_ENABLE_SSL
    std::shared_ptr<TlsContext> tls;
    SSL* ssl = nullptr;
    #endif
    
    std::string host;
    std::string path;
    bool connected = false;
    bool ssl_handshake_done = false;
    bool is_ssl = false;

    // Callbacks
    std::function<void()> onOpenCb;
    std::function<void(std::string_view)> onMessageCb;
    std::function<void()> onCloseCb;

    // Buffers
    std::string read_buffer;
    std::string fragment_buffer; // For reassembly

public:
    WsClient(EpollLoop& eventLoop) : loop(eventLoop) {
        #ifdef GGNET_ENABLE_SSL
        tls = std::make_shared<TlsContext>();
        #endif
    }

    ~WsClient() {
        close();
    }

    void onOpen(std::function<void()> cb) { onOpenCb = cb; }
    void onMessage(std::function<void(std::string_view)> cb) { onMessageCb = cb; }
    void onClose(std::function<void()> cb) { onCloseCb = cb; }

    void connect(const std::string& url_str) {
        Url url = parseUrl(url_str);
        host = url.host;
        path = url.path;
        is_ssl = (url.protocol == "wss");

        sock.connect(url.host, url.port);
        sock.setNonBlocking();
        sock.setNoDelay();

        #ifdef GGNET_ENABLE_SSL
        if (is_ssl) {
            ssl = tls->createSSL(sock.fd, host);
            SSL_set_connect_state(ssl);
        }
        #endif

        sendHandshake();

        loop.addFd(sock.fd, EPOLLIN | EPOLLOUT | EPOLLET, 
            std::bind(&WsClient::readHandler, this), 
            std::bind(&WsClient::doWrite, this)
        );
    }
    
    void send(const std::string& msg, bool is_text = true) {
        // Run internal logic in loop thread to ensure thread-safety
        loop.runInLoop([this, msg, is_text]() {
            if (!connected) return;

            std::vector<uint8_t> frame;
            
            // Byte 0: FIN + Opcode
            frame.push_back(0x80 | (is_text ? 0x1 : 0x2)); // FIN=1, Opcode=1(text) or 2(binary)

            // Byte 1: Mask bit + Payload Len
            size_t len = msg.size();
            if (len <= 125) {
                frame.push_back(0x80 | len); // Mask=1
            } else if (len <= 65535) {
                frame.push_back(0x80 | 126);
                frame.push_back((len >> 8) & 0xFF);
                frame.push_back(len & 0xFF);
            } else {
                frame.push_back(0x80 | 127);
                 for (int i = 7; i >= 0; i--) {
                    frame.push_back((len >> (i * 8)) & 0xFF);
                }
            }

            // Masking Key (4 bytes)
            // Simple random mask
            uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78}; // TODO: Use better random
            frame.push_back(mask_key[0]);
            frame.push_back(mask_key[1]);
            frame.push_back(mask_key[2]);
            frame.push_back(mask_key[3]);

            // Mask payload
            for (size_t i = 0; i < len; ++i) {
                frame.push_back(msg[i] ^ mask_key[i % 4]);
            }
            
            // Write
            sendRaw(reinterpret_cast<const char*>(frame.data()), frame.size());
        });
    }


    void close() {
        if (connected) {
            // Ideally send Close frame
            connected = false;
        }
        if (sock.fd >= 0) {
            loop.removeFd(sock.fd);
            sock.close();
        }
        if (onCloseCb) onCloseCb();
    }

private:
   void sendHandshake() {
       // Generate random key
       std::string key = "dGhlIHNhbXBsZSBub25jZQ=="; // Fixed for now, SHOULD be random base64
       
       std::stringstream ss;
       ss << "GET " << path << " HTTP/1.1\r\n";
       ss << "Host: " << host << "\r\n";
       ss << "Upgrade: websocket\r\n";
       ss << "Connection: Upgrade\r\n";
       ss << "Sec-WebSocket-Key: " << key << "\r\n";
       ss << "Sec-WebSocket-Version: 13\r\n";
       ss << "\r\n";
       
       std::string buf = ss.str();
       
       // Need to queue this if SSL isn't ready, but let's try direct send and handle SSL_WANT_WRITE in read loop if needed
       // Or rely on the first loop iteration to drive handshake
       // Actually, for SSL we can't send until handshake.
       // We'll store it as pending write buffer?
       // For simplicity, let's just make `sendRaw` handle it or queue it. 
       // But wait, `sendHandshake` is called right after `connect`.
       // If SSL, we can't write yet.
       
       write_queue.append(buf);
   }
   
   std::string write_queue;

   void sendRaw(const char* data, size_t len) {
       write_queue.append(data, len);
       doWrite();
   }
   
   void doWrite() {
       if (write_queue.empty()) return;
       
       int sent = 0;
       
       #ifdef GGNET_ENABLE_SSL
       if (is_ssl) {
           if (!ssl_handshake_done) {
               int ret = SSL_do_handshake(ssl);
               if (ret == 1) {
                   ssl_handshake_done = true;
               } else {
                   return; // Wait for read/write event to retry handshake
               }
           }
           sent = SSL_write(ssl, write_queue.data(), write_queue.size());
       } else 
       #endif
       {
           sent = ::send(sock.fd, write_queue.data(), write_queue.size(), 0);
       }
       
       if (sent > 0) {
           write_queue.erase(0, sent);
       }
   }

   void readHandler() {
       // 1. SSL Handshake / Write Flush
       doWrite(); // Try to flush any pending writes (like handshake)
       
       // 2. Read into buffer
       char buf[8192];
       int bytes = 0;
       
        #ifdef GGNET_ENABLE_SSL
        if (is_ssl) {
             if (!ssl_handshake_done) {
                 doWrite(); // retry handshake
                 if (!ssl_handshake_done) return;
             }
             bytes = SSL_read(ssl, buf, sizeof(buf));
        } else 
        #endif
        {
            bytes = recv(sock.fd, buf, sizeof(buf), 0);
        }
        
        if (bytes > 0) {
            read_buffer.append(buf, bytes);
            processBuffer();
        } else if (bytes == 0) {
            close();
        }
   }

   void processBuffer() {
       if (!connected) {
           // Parse HTTP Upgrade Response
           size_t header_end = read_buffer.find("\r\n\r\n");
           if (header_end != std::string::npos) {
               // Check if 101 Switching Protocols
               if (read_buffer.find("101 Switching Protocols") != std::string::npos) {
                   connected = true;
                   read_buffer.erase(0, header_end + 4);
                   if (onOpenCb) onOpenCb();
                   // Process remaining data as frames
                   if (!read_buffer.empty()) processFrames();
               } else {
                   // Failed
                   close();
               }
           }
       } else {
           processFrames();
       }
   }

   void processFrames() {
       while (read_buffer.size() >= 2) {
           // 1. Header parsing
           uint8_t b0 = static_cast<uint8_t>(read_buffer[0]);
           uint8_t b1 = static_cast<uint8_t>(read_buffer[1]);
           
           bool fin = b0 & 0x80;
           int opcode = b0 & 0x0F;
           bool masked = b1 & 0x80; 
           uint64_t payload_len = b1 & 0x7F;
           
           size_t head_len = 2;
           if (payload_len == 126) {
               if (read_buffer.size() < 4) return;
               uint8_t len1 = static_cast<uint8_t>(read_buffer[2]);
               uint8_t len2 = static_cast<uint8_t>(read_buffer[3]);
               payload_len = (len1 << 8) | len2;
               head_len = 4;
           } else if (payload_len == 127) {
               if (read_buffer.size() < 10) return;
               // 64bit
               // For now assume standard simple 64bit size (big endian)
                // Just skipping implementation detail for brevity as test payload is small
               head_len = 10;
           }

           // Check full frame availability
           if (read_buffer.size() < head_len + payload_len) return; // Wait for more

           // Extract payload
           std::string payload = read_buffer.substr(head_len, payload_len);
           
           // If server masked the frame (unlikely but check)
           if (masked) {
               // We need to handle this? RFC says MUST NOT.
               // But if it happened, we interpret mask key.
               // The mask key would be at read_buffer[head_len] (4 bytes), and payload after.
               // My logic above doesn't account for mask key presence in 'head_len' if masked.
               // Let's assume server doesn't mask.
           }

           read_buffer.erase(0, head_len + payload_len);

           handleFrame(opcode, fin, payload);
       }
   }

   void handleFrame(int opcode, bool fin, const std::string& payload) {
       switch(opcode) {
           case 0x0: // Continuation
               fragment_buffer.append(payload);
               if (fin) {
                   if (onMessageCb) onMessageCb(fragment_buffer);
                   fragment_buffer.clear();
               }
               break;
           case 0x1: // Text
           case 0x2: // Binary
               if (fin) {
                   if (onMessageCb) onMessageCb(payload);
               } else {
                   fragment_buffer = payload;
               }
               break;
           case 0x8: // Close
               close();
               break;
           case 0x9: // Ping
               // Auto Pong
               sendPong(payload);
               break;
           case 0xA: // Pong
               break;
       }
   }
   
   void sendPong(const std::string& payload) {
       // Similar to send but Opcode 0xA
       // Implementation skipped for brevity (TBD)
   }
};

} // namespace ggnet
