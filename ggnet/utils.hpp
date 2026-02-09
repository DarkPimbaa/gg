#pragma once

#include <string>
#include <iostream>

namespace ggnet {

struct Url {
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

// Parser de URL simples não-agressivo (http/https/ws/wss)
inline Url parseUrl(const std::string& url_str) {
    Url url;
    std::string u = url_str;
    
    // Protocol
    size_t protocol_pos = u.find("://");
    if (protocol_pos != std::string::npos) {
        url.protocol = u.substr(0, protocol_pos);
        u = u.substr(protocol_pos + 3);
    } else {
        url.protocol = "http"; // default
    }

    // Default ports
    if (url.protocol == "http" || url.protocol == "ws") url.port = 80;
    else if (url.protocol == "https" || url.protocol == "wss") url.port = 443;
    else url.port = 80;

    // Path
    size_t path_pos = u.find("/");
    if (path_pos != std::string::npos) {
        url.path = u.substr(path_pos);
        u = u.substr(0, path_pos);
    } else {
        url.path = "/";
    }

    // Host & Port (if specified)
    size_t port_pos = u.find(":");
    if (port_pos != std::string::npos) {
        url.host = u.substr(0, port_pos);
        url.port = std::stoi(u.substr(port_pos + 1));
    } else {
        url.host = u;
    }

    return url;
}


// Simple logger
inline void log(const std::string& msg) {
    std::cout << "[GGNet] " << msg << std::endl;
}

inline void debug(const std::string& msg) {
    #ifdef GGNET_DEBUG
    std::cout << "[GGNet:DEBUG] " << msg << std::endl;
    #endif
}

} // namespace ggnet
