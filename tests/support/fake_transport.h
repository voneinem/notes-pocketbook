#pragma once
// Scripted HttpTransport for contract and integration tests (T016). Records every request
// including credentials and headers. Accepts any URL scheme.

#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "pbnotes/http_transport.h"

namespace pbtest {

class FakeTransport : public pbnotes::HttpTransport {
public:
    struct Expectation {
        std::string method;
        std::string url_contains;  // substring of the full URL
        pbnotes::Response response;
    };

    // Queue of expectations, consumed in order. A request that does not match the next
    // expectation is recorded as a mismatch and answered with a Server error.
    FakeTransport& expect(const std::string& method, const std::string& url_contains, pbnotes::Response response) {
        queue_.push_back({method, url_contains, std::move(response)});
        return *this;
    }

    // Alternative to the queue: a handler decides every answer (fake server logic).
    void set_handler(std::function<pbnotes::Response(const pbnotes::Request&)> handler) { handler_ = std::move(handler); }

    // Failure injection: the n-th request (0-based) fails with a transport error instead.
    void fail_request(size_t index, pbnotes::ErrorClass error) { failures_.push_back({index, error}); }

    // Simulates a full device for the next file downloads (Response::storage_full, nothing written).
    bool simulate_storage_full = false;

    pbnotes::Response perform(const pbnotes::Request& request) override {
        pbnotes::Response r = perform_scripted(request);
        apply_file_semantics(request, &r);
        return r;
    }

    const std::vector<pbnotes::Request>& requests() const { return requests_; }
    void clear_requests() { requests_.clear(); }
    const std::vector<std::string>& mismatches() const { return mismatches_; }
    bool all_consumed() const { return next_ == queue_.size(); }

private:
    // What the real transport does with save_body_to / max_body_bytes: the body of a 2xx answer goes into the
    // file (Response::body stays empty); a Content-Length or a body over the cap stops the transfer.
    void apply_file_semantics(const pbnotes::Request& request, pbnotes::Response* r) {
        if (request.save_body_to.empty() || r->status < 200 || r->status >= 300 || r->error != pbnotes::ErrorClass::None) return;
        if (r->body_too_large || r->storage_full) return;
        if (request.method == "HEAD") return;
        const std::string length = pbnotes::header_value(r->headers, "Content-Length");
        if (request.max_body_bytes > 0) {
            const long long declared = length.empty() ? -1 : std::atoll(length.c_str());
            if (declared > request.max_body_bytes || static_cast<long long>(r->body.size()) > request.max_body_bytes) {
                r->body_too_large = true;
                r->body.clear();
                return;
            }
        }
        if (simulate_storage_full) {
            r->storage_full = true;
            r->body.clear();
            return;
        }
        std::ofstream f(request.save_body_to, std::ios::binary | std::ios::trunc);
        f << r->body;
        r->body.clear();
    }

    pbnotes::Response perform_scripted(const pbnotes::Request& request) {
        const size_t index = requests_.size();
        requests_.push_back(request);
        for (const auto& f : failures_) {
            if (f.first == index) {
                pbnotes::Response r;
                r.error = f.second;
                r.error_detail = "injected failure";
                return r;
            }
        }
        if (handler_) return handler_(request);
        if (next_ < queue_.size()) {
            const Expectation& e = queue_[next_];
            if (e.method == request.method && request.url.find(e.url_contains) != std::string::npos) {
                ++next_;
                return e.response;
            }
        }
        mismatches_.push_back(request.method + " " + request.url);
        pbnotes::Response r;
        r.status = 599;
        r.error = pbnotes::ErrorClass::Server;
        r.error_detail = "unexpected request";
        return r;
    }

public:
    static pbnotes::Response ok(const std::string& body, pbnotes::Headers headers = {}) {
        pbnotes::Response r;
        r.status = 200;
        r.body = body;
        r.headers = std::move(headers);
        return r;
    }
    static pbnotes::Response status(long code, const std::string& body = "", pbnotes::Headers headers = {}) {
        pbnotes::Response r;
        r.status = code;
        r.body = body;
        r.headers = std::move(headers);
        return r;
    }

private:
    std::vector<Expectation> queue_;
    size_t next_ = 0;
    std::function<pbnotes::Response(const pbnotes::Request&)> handler_;
    std::vector<std::pair<size_t, pbnotes::ErrorClass>> failures_;
    std::vector<pbnotes::Request> requests_;
    std::vector<std::string> mismatches_;
};

}  // namespace pbtest
