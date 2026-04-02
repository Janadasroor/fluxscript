// HTTP Client Implementation
#include "flux/package/http_client.h"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace Flux {
namespace HTTP {

// Callback for curl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static size_t WriteFileCallback(void* contents, size_t size, size_t nmemb, std::FILE* file) {
    return std::fwrite(contents, size, nmemb, file);
}

Client::Client() : m_timeout(30), m_userAgent("FluxScript-PackageManager/1.0") {}

Client::~Client() = default;

void Client::setTimeout(int seconds) {
    m_timeout = seconds;
}

void Client::setUserAgent(const std::string& ua) {
    m_userAgent = ua;
}

void Client::setBaseUrl(const std::string& url) {
    m_baseUrl = url;
}

Response Client::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    return request("GET", url, "", headers);
}

Response Client::post(const std::string& url, const std::string& body,
                       const std::map<std::string, std::string>& headers) {
    return request("POST", url, body, headers);
}

Response Client::request(const std::string& method, const std::string& url,
                          const std::string& body,
                          const std::map<std::string, std::string>& headers) {
    Response response;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_lastError = "Failed to initialize CURL";
        return response;
    }
    
    std::string responseBody;
    std::string fullUrl = buildUrl(url);
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    
    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }
    
    // Set headers
    struct curl_slist* headerList = nullptr;
    headerList = curl_slist_append(headerList, ("User-Agent: " + m_userAgent).c_str());
    for (const auto& pair : headers) {
        std::string header = pair.first + ": " + pair.second;
        headerList = curl_slist_append(headerList, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout);
    
    // Follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // SSL verification
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    
    // Execute request
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        m_lastError = std::string("CURL error: ") + curl_easy_strerror(res);
        response.success = false;
    } else {
        // Get status code
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = httpCode;
        response.body = responseBody;
        response.success = (httpCode >= 200 && httpCode < 300);
    }
    
    // Cleanup
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    
    return response;
}

bool Client::downloadFile(const std::string& url, const std::string& destPath,
                           std::function<void(double progress)> callback) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_lastError = "Failed to initialize CURL";
        return false;
    }
    
    std::FILE* file = std::fopen(destPath.c_str(), "wb");
    if (!file) {
        m_lastError = "Failed to open file for writing: " + destPath;
        curl_easy_cleanup(curl);
        return false;
    }
    
    std::string fullUrl = buildUrl(url);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Progress callback
    if (callback) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            [](void* clientp, curl_off_t dltotal, curl_off_t, curl_off_t, curl_off_t) -> int {
                if (dltotal > 0) {
                    auto* cb = static_cast<std::function<void(double)>*>(clientp);
                    (*cb)(1.0);  // Simplified
                }
                return 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &callback);
    }
    
    CURLcode res = curl_easy_perform(curl);
    std::fclose(file);
    
    if (res != CURLE_OK) {
        m_lastError = std::string("CURL error: ") + curl_easy_strerror(res);
        std::remove(destPath.c_str());
        curl_easy_cleanup(curl);
        return false;
    }
    
    curl_easy_cleanup(curl);
    return true;
}

std::string Client::buildUrl(const std::string& path) const {
    if (path.find("http://") == 0 || path.find("https://") == 0) {
        return path;
    }
    if (!m_baseUrl.empty()) {
        return m_baseUrl + (path[0] == '/' ? path : "/" + path);
    }
    return path;
}

// URL utilities
std::string URL::encode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }
    
    return escaped.str();
}

std::string URL::decode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex(str.substr(i + 1, 2));
            hex >> std::hex >> value;
            decoded += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

std::string URL::getDomain(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    
    size_t end = url.find('/', start);
    if (end == std::string::npos) end = url.length();
    
    return url.substr(start, end - start);
}

std::string URL::getPath(const std::string& url) {
    size_t start = url.find("://");
    if (start == std::string::npos) start = 0;
    else start += 3;
    
    size_t pathStart = url.find('/', start);
    if (pathStart == std::string::npos) return "/";
    
    return url.substr(pathStart);
}

} // namespace HTTP
} // namespace Flux
