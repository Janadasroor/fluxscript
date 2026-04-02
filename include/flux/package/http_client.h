#ifndef FLUX_HTTP_CLIENT_H
#define FLUX_HTTP_CLIENT_H

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace Flux {
namespace HTTP {

// HTTP Response
struct Response {
    int statusCode;
    std::string statusMessage;
    std::map<std::string, std::string> headers;
    std::string body;
    bool success;
    
    Response() : statusCode(0), success(false) {}
    
    std::string getHeader(const std::string& name) const {
        auto it = headers.find(name);
        return (it != headers.end()) ? it->second : "";
    }
};

// HTTP Client
class Client {
public:
    Client();
    ~Client();
    
    // Configuration
    void setTimeout(int seconds);
    void setUserAgent(const std::string& ua);
    void setBaseUrl(const std::string& url);
    
    // GET request
    Response get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    
    // POST request
    Response post(const std::string& url, const std::string& body,
                  const std::map<std::string, std::string>& headers = {});
    
    // Download file
    bool downloadFile(const std::string& url, const std::string& destPath,
                      std::function<void(double progress)> callback = nullptr);
    
    // Error handling
    std::string getLastError() const { return m_lastError; }
    
private:
    int m_timeout;
    std::string m_userAgent;
    std::string m_baseUrl;
    std::string m_lastError;
    
    // Internal methods
    Response request(const std::string& method, const std::string& url,
                     const std::string& body, const std::map<std::string, std::string>& headers);
    std::string buildUrl(const std::string& path) const;
};

// URL utilities
class URL {
public:
    static std::string encode(const std::string& str);
    static std::string decode(const std::string& str);
    static std::string getDomain(const std::string& url);
    static std::string getPath(const std::string& url);
};

} // namespace HTTP
} // namespace Flux

#endif // FLUX_HTTP_CLIENT_H
