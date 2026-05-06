#include "smasttrafik/http_client.hpp"

#include <chrono>
#include <stdexcept>

#include <curl/curl.h>

namespace smasttrafik {

namespace {

size_t write_body(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t write_header(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, size * nitems);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        const auto first = value.find_first_not_of(" \t\r\n");
        const auto last = value.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            (*headers)[key] = value.substr(first, last - first + 1);
        }
    }
    return size * nitems;
}

HttpResponse perform_request(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds
) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        throw std::runtime_error("Failed to initialize curl");
    }

    struct curl_slist* header_list = nullptr;
    for (const auto& [key, value] : headers) {
        const std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    const auto start = std::chrono::steady_clock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto end = std::chrono::steady_clock::now();
    response.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (code != CURLE_OK) {
        const std::string error = curl_easy_strerror(code);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        throw std::runtime_error(error);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status_code = static_cast<int>(status);

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return response;
}

} // namespace

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

HttpResponse HttpClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds
) const {
    return perform_request(url, "GET", "", headers, timeout_seconds);
}

HttpResponse HttpClient::post_form(
    const std::string& url,
    const std::string& form_body,
    const std::map<std::string, std::string>& headers,
    int timeout_seconds
) const {
    return perform_request(url, "POST", form_body, headers, timeout_seconds);
}

std::string url_encode(const std::string& value) {
    char* escaped = curl_easy_escape(nullptr, value.c_str(), static_cast<int>(value.size()));
    if (escaped == nullptr) {
        return value;
    }
    std::string result(escaped);
    curl_free(escaped);
    return result;
}

} // namespace smasttrafik
