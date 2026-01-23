#pragma once
#include <util/generic/ptr.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

namespace LdapMock {

class TSocket {
private:
    struct TSslDestroy {
        static void Destroy(SSL* ssl) noexcept {
            SSL_free(ssl);
        }

        static void Destroy(X509* cert) noexcept {
            X509_free(cert);
        }
    };

    template <typename T>
    using TSslHolder = THolder<T, TSslDestroy>;

public:
    TSocket(int fd)
        : Fd(fd)
        , UseTls(false)
    {}

    bool isTls() const {
        return UseTls;
    }

    bool Receive(void* buf, size_t len) {
        if(UseTls) {
            return ReceiveTls(buf, len);
        }
        return ReceivePlain(buf, len);
    }

    bool Send(const void* msg, size_t len) {
        if (UseTls) {
            return SendTls(msg, len);
        }
        return SendPlain(msg, len);
    }

    bool UpgradeToTls(SSL_CTX* ctx) {
        Ssl.Reset(SSL_new(ctx));
        if (!Ssl) {
            return false;
        }

        SSL_set_fd(Ssl.Get(), Fd);

        if (SSL_accept(Ssl.Get()) != 1) {
            // ERR_print_errors_fp(stderr);
            Ssl.Reset(nullptr);
            return false;
        }

        UseTls = true;
        return true;
    }

private:

    bool ReceivePlain(void* buf, size_t len) {
        uint8_t* p = (uint8_t*)buf;
        while (len) {
            ssize_t r = ::recv(Fd, p, len, 0);
            if (r <= 0) {
                return false;
            }
            p += (size_t)r;
            len -= (size_t)r;
        }
        return true;
    }

    bool SendPlain(const void* msg, size_t len) {
        uint8_t* p = (uint8_t*)msg;
        while (len) {
            ssize_t w = ::send(Fd, p, len, 0);
            if (w <= 0) {
                return false;
            }
            p += (size_t)w;
            len -= (size_t)w;
        }
        return true;
    }

    bool ReceiveTls(void* buf, size_t len) {
        uint8_t* p = (uint8_t*)buf;
        while (len) {
            ssize_t r = SSL_read(Ssl.Get(), p, len);
            if (r <= 0) {
                return false;
            }
            p += (size_t)r;
            len -= (size_t)r;
        }
        return true;
    }

    bool SendTls(const void* msg, size_t len) {
        uint8_t* p = (uint8_t*)msg;
        while (len) {
            ssize_t w = SSL_write(Ssl.Get(), p, len);
            if (w <= 0) {
                return false;
            }
            p += (size_t)w;
            len -= (size_t)w;
        }
        return true;
    }

    // static bool RecvAllTls(SSL* ssl, void* buf, size_t n) {
    //     uint8_t* p = (uint8_t*)buf;
    //     while (n) {
    //         int r = SSL_read(ssl, p, (int)n);
    //         if (r <= 0) return false;
    //         p += (size_t)r;
    //         n -= (size_t)r;
    //     }
    //     return true;
    // }

    // static bool SendAllTls(SSL* ssl, const uint8_t* buf, size_t n) {
    //     while (n) {
    //         int w = SSL_write(ssl, buf, (int)n);
    //         if (w <= 0) return false;
    //         buf += (size_t)w;
    //         n -= (size_t)w;
    //     }
    //     return true;
    // }

private:
    int Fd{0};
    bool UseTls = false;
    TSslHolder<SSL> Ssl = nullptr;
};

} // LdapMock
