#pragma once

#include <util/system/types.h>
#include <util/system/tempfile.h>
#include <util/generic/ptr.h>
#include <util/generic/string.h>

#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <thread>

namespace LdapMock {

class TSimpleServer {
private:
    struct TSslDestroy {
        static void Destroy(SSL_CTX* ctx) noexcept {
            SSL_CTX_free(ctx);
        }

        static void Destroy(SSL* ssl) noexcept {
            SSL_free(ssl);
        }

        static void Destroy(X509* cert) noexcept {
            X509_free(cert);
        }

        static void Destroy(EVP_PKEY* pkey) noexcept {
            EVP_PKEY_free(pkey);
        }

        static void Destroy(BIO* bio) noexcept {
            BIO_free(bio);
        }
    };

    template <typename T>
    using TSslHolder = THolder<T, TSslDestroy>;
public:
    struct TOptions {
        ui16 Port;
        TString CeCert;
        TString Cert;
        TString Key;
    };

    TSimpleServer(const TOptions& options)
        : Opt(options)
        , Port(options.Port)
    {}

    bool Start() {
        if (Running.exchange(true)) {
            return true;
        }

        InitOpenSsl_();

        if (!InitListenSocket_()) {
            Running = false;
            return false;
        }
        if (!InitTlsCtx()) {
            Running = false;
            // Cleanup_();
            return false;
        }

        Worker = std::thread([this]{ ThreadMain(); });
        return true;
    }

    void Stop() {
        if (!Running.exchange(false)) {
            return;
        }

        if (ListenSocket >= 0) {
            ::shutdown(ListenSocket, SHUT_RDWR);
            ::close(ListenSocket);
            ListenSocket = -1;
        }

        if (Worker.joinable()) {
            Worker.join();
        }
        // Cleanup_();
    }

    ui16 GetPort() const {
        return Port;
    }

private:
    void ThreadMain() {
        while (Running) {
            int fd = ::accept(ListenSocket, nullptr, nullptr);
            if (fd < 0) continue;

            HandleClient_(fd);

            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
    }

    bool InitTlsCtx() {
        Ctx.Reset(SSL_CTX_new(TLS_server_method()));
        if (!Ctx) {
            return false;
        }

        if (SSL_CTX_use_certificate_file(Ctx.Get(), Opt_.TlsServerCertPem.c_str(), SSL_FILETYPE_PEM) != 1)
            return false;
        if (SSL_CTX_use_PrivateKey_file(Ctx_, Opt_.TlsServerKeyPem.c_str(), SSL_FILETYPE_PEM) != 1)
            return false;

        int mode = SSL_VERIFY_NONE;
        if (Opt_.RequireClientCert) {
            mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        } else if (!Opt_.ClientCaPem.empty()) {
            mode = SSL_VERIFY_PEER;
        }

        if (!Opt_.ClientCaPem.empty()) {
            if (SSL_CTX_load_verify_locations(Ctx_, Opt_.ClientCaPem.c_str(), nullptr) != 1)
                return false;
            STACK_OF(X509_NAME)* caList = SSL_load_client_CA_file(Opt_.ClientCaPem.c_str());
            if (caList) SSL_CTX_set_client_CA_list(Ctx_, caList);
        }

        SSL_CTX_set_verify(Ctx_, mode, VerifyCb_);
        return true;
    }

private:
    TOptions Opt;
    ui16 Port{0};
    int ListenSocket{-1};
    std::atomic<bool> Running{false};
    std::thread Worker;
    TSslHolder<SSL_CTX> Ctx{nullptr};
    bool PendingUpgradeToTls{false};
};

} // LdapMock
