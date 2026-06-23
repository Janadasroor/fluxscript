/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/runtime/runtime_helpers.h"
#include "flux/runtime/string_pool.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>

#ifdef FLUX_HAS_CURL
#include <curl/curl.h>
#endif

extern "C" double flux_fopen(double filename_ptr, double mode_ptr)
{
    auto* fn = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(filename_ptr));
    auto* md = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(mode_ptr));
    if (!fn || !*fn)
        return 0.0;
    // Reject path traversal (..) to prevent escaping working directory
    std::string path(fn);
    if (path.find("..") != std::string::npos)
        return 0.0;
    auto* fp = std::fopen(fn, md ? md : "r");
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(fp));
}

extern "C" double flux_fclose(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp)
        return 0.0;
    return std::fclose(fp) == 0 ? 1.0 : 0.0;
}

extern "C" double flux_fflush(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp)
        return 0.0;
    return std::fflush(fp) == 0 ? 1.0 : 0.0;
}

extern "C" double flux_feof(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp)
        return 1.0;
    return std::feof(fp) ? 1.0 : 0.0;
}

extern "C" double flux_fgets(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp || std::feof(fp))
        return 0.0;
    g_fileio_buffer.clear();
    g_fileio_buffer.resize(4096);
    if (!std::fgets(&g_fileio_buffer[0], g_fileio_buffer.size(), fp))
        return 0.0;
    g_fileio_buffer.resize(std::strlen(g_fileio_buffer.c_str()));
    while (!g_fileio_buffer.empty() && (g_fileio_buffer.back() == '\n' || g_fileio_buffer.back() == '\r'))
        g_fileio_buffer.pop_back();
    g_fileio_pool.push_back(g_fileio_buffer);
    pool_evict_if_full(g_fileio_pool);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(g_fileio_pool.back().c_str()));
}

extern "C" double flux_fprintf(double handle, double format_ptr, double arg)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    auto* fmt = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(format_ptr));
    if (!fp || !fmt)
        return 0.0;
    // Reject dangerous format specifiers (%n writes memory, %s/%p read arbitrary memory)
    if (std::strstr(fmt, "%n") || std::strstr(fmt, "%s") || std::strstr(fmt, "%p") || std::strstr(fmt, "%hn") ||
        std::strstr(fmt, "%ln")) {
        return 0.0;
    }
    // Use snprintf to limit output size and prevent format string exploits
    char buf[4096];
    int ret = std::snprintf(buf, sizeof(buf), fmt, arg);
    if (ret > 0) {
        std::fwrite(buf, 1, static_cast<size_t>(ret < (int)sizeof(buf) ? ret : (int)sizeof(buf) - 1), fp);
        std::fflush(fp);
    }
    return static_cast<double>(ret);
}

extern "C" double flux_fetch_url(double url_ptr)
{
#ifdef FLUX_HAS_CURL
    auto* url = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(url_ptr));
    if (!url || !*url)
        return 0.0;

    CURL* curl = curl_easy_init();
    if (!curl)
        return 0.0;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, +[](void* data, size_t size, size_t nmemb, std::string* buf) -> size_t {
            size_t total = size * nmemb;
            buf->append(static_cast<char*>(data), total);
            return total;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
#if CURL_AT_LEAST_VERSION(7, 85, 0)
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "FluxScript/1.0");
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, 10L * 1024L * 1024L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return 0.0;

    g_fileio_pool.push_back(std::move(response));
    pool_evict_if_full(g_fileio_pool);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(g_fileio_pool.back().c_str()));
#else
    (void)url_ptr;
    return 0.0;
#endif
}
