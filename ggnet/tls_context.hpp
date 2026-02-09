#pragma once

#ifdef GGNET_ENABLE_SSL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <memory>
#include <mutex>

#include <stdexcept>
#include "utils.hpp"

namespace ggnet {

class TlsContext {
    SSL_CTX* ctx = nullptr;
    std::mutex ctx_mutex;

public:
    TlsContext() {
        init();
    }

    ~TlsContext() {
        cleanup();
    }

    void init() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        if (ctx) return;

        // Ensure global initialization (only needed for older OpenSSL versions, but safe to do)
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            SSL_library_init();
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
        });

        const SSL_METHOD* method = TLS_client_method();
        ctx = SSL_CTX_new(method);
        if (!ctx) {
            throw std::runtime_error("Unable to create SSL context");
        }

        // Default options for security
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1); 
        SSL_CTX_set_default_verify_paths(ctx);
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        if (ctx) {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }

    // TLS Rotation: Destroy and recreate context to allow parameter changes or session ticket clearing
    void rotate() {
        log("Rotating TLS Context...");
        cleanup();
        init();
    }

    SSL* createSSL(int fd, const std::string& host) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        if (!ctx) init();

        SSL* ssl = SSL_new(ctx);
        if (!ssl) {
            throw std::runtime_error("Unable to create SSL object");
        }

        SSL_set_fd(ssl, fd);
        
        // SNI (Server Name Indication) is crucial for virtual hosting (Cloudflare etc)
        SSL_set_tlsext_host_name(ssl, host.c_str());
        
        // Hostname verification
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        if (!SSL_set1_host(ssl, host.c_str())) {
             // Treat as warning or error depending on needs. Modern OpenSSL handles this well.
        }

        return ssl;
    }
};

} // namespace ggnet

#endif
