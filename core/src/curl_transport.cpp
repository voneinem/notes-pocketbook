#include "pbnotes/curl_transport.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <mutex>

namespace pbnotes {

namespace {

std::once_flag g_curl_init;

size_t write_body(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(data, size * nmemb);
    return size * nmemb;
}

size_t write_header(char* data, size_t size, size_t nmemb, void* userdata) {
    auto* headers = static_cast<Headers*>(userdata);
    const size_t total = size * nmemb;
    std::string line(data, total);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.compare(0, 5, "HTTP/") == 0) {  // a new response block (redirects): start over
        headers->clear();
    } else {
        const size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            const size_t b = value.find_first_not_of(" \t");
            value = b == std::string::npos ? std::string() : value.substr(b);
            headers->emplace_back(std::move(name), std::move(value));
        }
    }
    return total;
}

bool is_https_url(const std::string& url) {
    static const char kScheme[] = "https://";
    if (url.size() < sizeof(kScheme) - 1) return false;
    for (size_t i = 0; i < sizeof(kScheme) - 1; ++i)
        if (std::tolower(static_cast<unsigned char>(url[i])) != kScheme[i]) return false;
    return true;
}

ErrorClass classify_curl_error(CURLcode code) {
    switch (code) {
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_INVALIDCERTSTATUS:
        case CURLE_UNSUPPORTED_PROTOCOL:  // e.g. a redirect to http://
            return ErrorClass::Tls;
        case CURLE_TOO_MANY_REDIRECTS:
        case CURLE_URL_MALFORMAT:
        case CURLE_BAD_CONTENT_ENCODING:
            return ErrorClass::Server;
        default:
            return ErrorClass::Network;
    }
}

}  // namespace

CurlTransport::CurlTransport(std::string ca_file) : ca_file_(std::move(ca_file)) {
    std::call_once(g_curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

Response CurlTransport::perform(const Request& request) {
    Response out;
    if (!is_https_url(request.url)) {
        out.error = ErrorClass::Tls;
        out.error_detail = "refusing a non-https URL";
        return out;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        out.error = ErrorClass::Network;
        out.error_detail = "curl init failed";
        return out;
    }

    struct curl_slist* header_list = nullptr;
    for (const auto& h : request.headers) header_list = curl_slist_append(header_list, (h.first + ": " + h.second).c_str());
    header_list = curl_slist_append(header_list, "Expect:");  // no 100-continue round trip

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connect_timeout_s);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.total_timeout_s);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    // TLS: verification is mandatory and not configurable.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, ca_file_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    if (!request.username.empty() || !request.password.empty()) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, static_cast<long>(CURLAUTH_BASIC));
        curl_easy_setopt(curl, CURLOPT_USERNAME, request.username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, request.password.c_str());
    }

    if (request.method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
        if (!request.body.empty() || request.method == "POST" || request.method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size()));
        }
    }

    std::string body;
    Headers headers;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    const CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        out.status = status;
        out.headers = std::move(headers);
        out.body = std::move(body);
    } else {
        out.error = classify_curl_error(rc);
        // Only the generic curl description: curl's own error buffer can contain the host name.
        (void)errbuf;
        out.error_detail = "curl " + std::to_string(static_cast<int>(rc)) + ": " + curl_easy_strerror(rc);
    }
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return out;
}

}  // namespace pbnotes
