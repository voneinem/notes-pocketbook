#pragma once
// Shared harness for the TLS tests (T026, T103, T104, T106): a throw-away PKI generated with the
// `openssl` CLI and a tiny HTTPS server (libssl) on 127.0.0.1 that records the raw bytes of every
// request, so tests can prove that credentials were (not) sent. Tests skip when the CLI is absent.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "tmp_dir.h"

namespace pbtest {

inline bool run(const std::string& cmd) { return std::system((cmd + " >/dev/null 2>&1").c_str()) == 0; }

// A CA plus helpers that issue server certificates. Everything lives in a TmpDir.
class TestPki {
public:
    explicit TestPki(const TmpDir& dir, const std::string& ca_name = "ca") : dir_(dir), ca_name_(ca_name) {
        available_ = run("openssl version") &&
                     run("openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj /CN=pbnotes-test-" + ca_name +
                         " -keyout " + dir.file(ca_name + ".key") + " -out " + dir.file(ca_name + ".pem") +
                         " -addext basicConstraints=critical,CA:TRUE" + " -addext keyUsage=critical,keyCertSign,cRLSign");
    }
    bool available() const { return available_; }
    std::string ca_cert() const { return dir_.file(ca_name_ + ".pem"); }

    struct Leaf {
        std::string cert;
        std::string key;
        bool ok = false;
    };

    // Server certificate signed by this CA. `san` e.g. "DNS:localhost,IP:127.0.0.1".
    // not_before/not_after (e.g. "20200101000000Z") create an expired or not-yet-valid certificate.
    Leaf issue(const std::string& name, const std::string& san, const std::string& not_before = "",
               const std::string& not_after = "") const {
        Leaf l;
        l.key = dir_.file(name + ".key");
        l.cert = dir_.file(name + ".crt");
        const std::string csr = dir_.file(name + ".csr");
        const std::string ext = dir_.file(name + ".ext");
        dir_.write(name + ".ext", "subjectAltName=" + san + "\nbasicConstraints=CA:FALSE\n");
        l.ok = run("openssl req -new -newkey rsa:2048 -nodes -subj /CN=" + name + " -keyout " + l.key + " -out " + csr) &&
               run("openssl x509 -req -in " + csr + " -CA " + ca_cert() + " -CAkey " + dir_.file(ca_name_ + ".key") +
                   " -CAcreateserial -extfile " + ext +
                   (not_before.empty() ? " -days 30" : " -not_before " + not_before + " -not_after " + not_after) +
                   " -out " + l.cert);
        return l;
    }

    // Self-signed server certificate (its own trust anchor) with the given subject alt names.
    Leaf self_signed(const std::string& name, const std::string& san) const {
        Leaf l;
        l.key = dir_.file(name + ".key");
        l.cert = dir_.file(name + ".crt");
        l.ok = run("openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj /CN=" + name + " -keyout " + l.key +
                   " -out " + l.cert + " -addext subjectAltName=" + san);
        return l;
    }

private:
    const TmpDir& dir_;
    std::string ca_name_;
    bool available_ = false;
};

// HTTPS server on 127.0.0.1 (random port). The handler maps raw request bytes to raw response bytes.
class TlsTestServer {
public:
    using Handler = std::function<std::string(const std::string& request)>;

    TlsTestServer(const std::string& cert, const std::string& key, Handler handler) : handler_(std::move(handler)) {
        ::signal(SIGPIPE, SIG_IGN);  // a client that aborts the handshake must not kill the test
        ctx_ = SSL_CTX_new(TLS_server_method());
        if (!ctx_ || SSL_CTX_use_certificate_chain_file(ctx_, cert.c_str()) != 1 ||
            SSL_CTX_use_PrivateKey_file(ctx_, key.c_str(), SSL_FILETYPE_PEM) != 1)
            return;
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(listen_fd_, 8) != 0) return;
        socklen_t len = sizeof addr;
        ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        port_ = ntohs(addr.sin_port);
        thread_ = std::thread([this] { loop(); });
        running_ = true;
    }
    ~TlsTestServer() {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
        if (listen_fd_ >= 0) ::close(listen_fd_);
        if (ctx_) SSL_CTX_free(ctx_);
    }
    TlsTestServer(const TlsTestServer&) = delete;
    TlsTestServer& operator=(const TlsTestServer&) = delete;

    bool running() const { return running_; }
    int port() const { return port_; }
    std::string url(const std::string& path = "/") const { return "https://localhost:" + std::to_string(port_) + path; }

    // Raw bytes of every request that was received after a completed handshake.
    std::vector<std::string> requests() const {
        std::lock_guard<std::mutex> l(mu_);
        return requests_;
    }
    int failed_handshakes() const { return failed_handshakes_; }

    // True if any received request carried an Authorization header (any case).
    bool saw_authorization() const {
        for (const auto& r : requests()) {
            std::string low = r;
            for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (low.find("authorization:") != std::string::npos) return true;
        }
        return false;
    }

    static std::string http_response(int status, const std::string& body, const std::string& extra_headers = "") {
        return "HTTP/1.1 " + std::to_string(status) + " X\r\nContent-Length: " + std::to_string(body.size()) +
               "\r\nConnection: close\r\n" + extra_headers + "\r\n" + body;
    }

private:
    void loop() {
        while (!stop_) {
            pollfd p{listen_fd_, POLLIN, 0};
            if (::poll(&p, 1, 50) <= 0) continue;
            int fd = ::accept(listen_fd_, nullptr, nullptr);
            if (fd < 0) continue;
            serve(fd);
            ::close(fd);
        }
    }
    void serve(int fd) {
        SSL* ssl = SSL_new(ctx_);
        SSL_set_fd(ssl, fd);
        if (SSL_accept(ssl) != 1) {
            ++failed_handshakes_;
            SSL_free(ssl);
            return;
        }
        std::string req;
        char buf[4096];
        while (req.find("\r\n\r\n") == std::string::npos) {
            int n = SSL_read(ssl, buf, sizeof buf);
            if (n <= 0) break;
            req.append(buf, buf + n);
            if (req.size() > (1 << 20)) break;
        }
        // a request with a body (an upload): read it completely so the client can finish sending
        const size_t head_end = req.find("\r\n\r\n");
        if (head_end != std::string::npos) {
            std::string low = req.substr(0, head_end);
            for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const size_t cl = low.find("content-length:");
            const size_t want = cl == std::string::npos ? 0 : static_cast<size_t>(std::atoll(low.c_str() + cl + 15));
            while (req.size() < head_end + 4 + want && req.size() < (64u << 20)) {
                int n = SSL_read(ssl, buf, sizeof buf);
                if (n <= 0) break;
                req.append(buf, buf + n);
            }
        }
        if (req.empty()) {  // TLS 1.3: the server can finish before the client's rejection arrives
            ++failed_handshakes_;
            SSL_free(ssl);
            return;
        }
        {
            std::lock_guard<std::mutex> l(mu_);
            requests_.push_back(req);
        }
        const std::string resp = handler_(req);
        SSL_write(ssl, resp.data(), static_cast<int>(resp.size()));
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    Handler handler_;
    SSL_CTX* ctx_ = nullptr;
    int listen_fd_ = -1;
    int port_ = 0;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    bool running_ = false;
    mutable std::mutex mu_;
    std::vector<std::string> requests_;
    std::atomic<int> failed_handshakes_{0};
};

}  // namespace pbtest
