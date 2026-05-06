#pragma once

#include <map>
#include <string>

namespace smasttrafik {

struct HttpResponse {
    int status_code = 0;
    long latency_ms = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds
    ) const;

    HttpResponse post_form(
        const std::string& url,
        const std::string& form_body,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds
    ) const;
};

std::string url_encode(const std::string& value);

} // namespace smasttrafik
