#pragma once
// Production HttpTransport on libcurl (T028).
//
// TLS verification is ALWAYS on (peer and host name); there is no option, setting or environment
// variable that turns it off (constitution Principle III). Only https:// URLs are accepted, also
// for redirects. The trust anchors are the single CA file given at construction (the effective
// bundle: device store or bundled CA file, plus an optional user-supplied custom-ca.pem).
// Credentials are sent as HTTP Basic auth and, by libcurl's default, never to another host/port
// after a redirect.

#include <string>

#include "pbnotes/http_transport.h"

namespace pbnotes {

class CurlTransport : public HttpTransport {
public:
    explicit CurlTransport(std::string ca_file);
    Response perform(const Request& request) override;

    const std::string& ca_file() const { return ca_file_; }

private:
    std::string ca_file_;
};

}  // namespace pbnotes
