#pragma once
#include "socket.h"
#include "ldap_response.h"
#include "ldap_message_processor.h"
#include "ldap_defines.h"

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
#include <mutex>

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
        TString CaCert;
        TString Cert;
        TString Key;
        bool RequireClientCert = false;
        bool UseTls = false;
    };

    TSimpleServer(const TOptions& options, TLdapMockResponses responses)
        : Opt(options)
        , Responses(std::make_shared<const TLdapMockResponses>(std::move(responses)))
    {}

    bool Start() {
        if (Running.exchange(true)) {
            return true;
        }

        InitOpenSsl();

        Cerr << "+++ 1111111" << Endl;
        if (!InitListenSocket()) {
            Running = false;
            Cerr << "+++ InitListenSocket false" << Endl;
            return false;
        }
        if (!InitTlsCtx()) {
            Running = false;
            Cerr << "+++ aaaaaaaaaaaaaaa" << Endl;
            // Cleanup_();
            return false;
        }
        Cerr << "+++ qqqqqqqqqqqq" << Endl;
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

    void ReplaceResponses(TLdapMockResponses&& responses) {
        auto p = std::make_shared<const TLdapMockResponses>(std::move(responses));
        std::lock_guard<std::mutex> g(Mutex);
        Responses = std::move(p);
    }

private:
    void ThreadMain() {
        Cerr << "+++ 22222" << Endl;
        while (Running) {
            int fd = ::accept(ListenSocket, nullptr, nullptr);
            if (fd < 0) continue;
            Cerr << "+++ 33333" << Endl;
            HandleClient_(fd);

            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
    }

    void HandleClient_(int fd) {
        std::shared_ptr<TSocket> socket = std::make_shared<TSocket>(fd);
        if (Opt.UseTls) {
            socket->UpgradeToTls(Ctx.Get());
        }
        Cerr << "+++ Handle client" << Endl;

        while (Running) {
            TLdapRequestProcessor requestProcessor(socket);
            unsigned char elementType = requestProcessor.GetByte();
            if (elementType != EElementType::SEQUENCE) {
                if (TLdapResponse().Send(socket)) {
                    break;
                }
            }
            size_t messageLength = requestProcessor.GetLength();
            if (messageLength == 0) {
                if (TLdapResponse().Send(socket)) {
                    break;
                }
            }
            int messageId = requestProcessor.ExtractMessageId();
            std::shared_ptr<const TLdapMockResponses> responsesSnapshot;
            {
                std::lock_guard<std::mutex> g(Mutex);
                responsesSnapshot = Responses;
            }
            std::vector<TLdapRequestProcessor::TProtocolOpData> operationData = requestProcessor.Process(responsesSnapshot);
            TLdapResponse response = TLdapResponse(messageId, operationData);
            if (!response.Send(socket)) {
                break;
            }
            if (!socket->isTls() && response.EnableTls()) {
                if (!socket->UpgradeToTls(Ctx.Get())) {
                    Cerr << "+++ UpgradeToTls false" << Endl;
                    break;
                }
                Cerr << "+++ UpgradeToTls" << Endl;
            }
        }
    }

    void InitOpenSsl() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
    }

    bool InitListenSocket() {
        ListenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (ListenSocket < 0) {
            Cerr << "+++ socket" << Endl;
            return false;
        }

        int one = 1;
        ::setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(Opt.Port);
        if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
            Cerr << "+++ inet_pton" << Endl;
            return false;
        }

        if (::bind(ListenSocket, (sockaddr*)&addr, sizeof(addr)) != 0) {
            Cerr << "+++ bind" << Endl;
            return false;
        }

        if (::listen(ListenSocket, 16) != 0) {
            Cerr << "+++ listen" << Endl;
            return false;
        }

        socklen_t len = sizeof(addr);
        if (::getsockname(ListenSocket, (sockaddr*)&addr, &len) != 0) {
            Cerr << "+++ getsockname" << Endl;
            return false;
        }

        Port = ntohs(addr.sin_port);

        return true;
    }

    static int VerifyCb(int ok, X509_STORE_CTX* store) {
        (void)store;
        return ok;
    }

    bool InitTlsCtx() {
        Ctx.Reset(SSL_CTX_new(TLS_server_method()));
        if (!Ctx) {
            return false;
        }

        if (SSL_CTX_use_certificate_file(Ctx.Get(), Opt.Cert.c_str(), SSL_FILETYPE_PEM) != 1) {
            return false;
        }

        if (SSL_CTX_use_PrivateKey_file(Ctx.Get(), Opt.Key.c_str(), SSL_FILETYPE_PEM) != 1) {
            return false;
        }

        int mode = SSL_VERIFY_NONE;
        if (Opt.RequireClientCert) {
            mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        } else if (!Opt.CaCert.empty()) {
            mode = SSL_VERIFY_PEER;
        }

        // if (!Opt.CaCert.empty()) {
        //     if (SSL_CTX_load_verify_locations(Ctx.Get(), Opt.CaCert.c_str(), nullptr) != 1) {
        //         return false;
        //     }

        //     STACK_OF(X509_NAME)* caList = SSL_load_client_CA_file(Opt.CaCert.c_str());
        //     if (caList) SSL_CTX_set_client_CA_list(Ctx_, caList);
        // }

        SSL_CTX_set_verify(Ctx.Get(), mode, VerifyCb);
        return true;
    }

private:
    TOptions Opt;
    ui16 Port{0};
    int ListenSocket{-1};
    std::atomic<bool> Running{false};
    std::thread Worker;
    TSslHolder<SSL_CTX> Ctx{nullptr};
    std::mutex Mutex;
    std::shared_ptr<const TLdapMockResponses> Responses;
};

} // LdapMock
