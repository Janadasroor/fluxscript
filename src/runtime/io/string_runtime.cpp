/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/runtime/runtime_helpers.h"
#include "flux/runtime/string_pool.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>

extern "C" double flux_strcmp(double a_ptr, double b_ptr)
{
    auto* a = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(a_ptr));
    auto* b = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(b_ptr));
    if (!a || !b)
        return (a == b) ? 0.0 : 1.0;
    return static_cast<double>(std::strcmp(a, b));
}

extern "C" double flux_vec_eq(double* a_data, int a_size, double* b_data, int b_size)
{
    if (a_size != b_size)
        return 0.0;
    for (int i = 0; i < a_size; ++i) {
        if (a_data[i] != b_data[i])
            return 0.0;
    }
    return 1.0;
}

extern "C" double flux_strlen(double s_ptr)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    return s ? static_cast<double>(std::strlen(s)) : 0.0;
}

extern "C" double flux_string_at(double s_ptr, double index)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return 0.0;
    int i = static_cast<int>(index);
    if (i < 0 || i >= static_cast<int>(std::strlen(s)))
        return 0.0;
    return static_cast<double>(static_cast<unsigned char>(s[i]));
}

extern "C" double flux_string_slice(double s_ptr, double start, double end)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return 0.0;
    int lo = std::max(0, static_cast<int>(start));
    int hi = std::min(static_cast<int>(std::strlen(s)), static_cast<int>(end));
    if (lo >= hi)
        return 0.0;
    g_fileio_pool.push_back(std::string(s + lo, s + hi));
    pool_evict_if_full(g_fileio_pool);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(g_fileio_pool.back().c_str()));
}

extern "C" double flux_string_find(double s_ptr, double c)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return -1.0;
    char ch = static_cast<char>(static_cast<int>(c));
    const char* p = std::strchr(s, ch);
    if (!p)
        return -1.0;
    return static_cast<double>(p - s);
}

extern "C" double flux_parse_number(double s_ptr)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s || !*s)
        return 0.0;
    char* end = nullptr;
    double val = std::strtod(s, &end);
    if (end && *end) {
        switch (*end) {
        case 'k':
        case 'K':
            val *= 1e3;
            break;
        case 'M':
        case 'm':
            if (*(end + 1) == 'e' || *(end + 1) == 'E') {
                val *= 1e6;
                break;
            }
            if (end > s && (*(end - 1) == 'm')) { /* already handled */
                break;
            }
            val *= 1e-3;
            break;
        case 'u':
        case 'U':
            val *= 1e-6;
            break;
        case 'n':
        case 'N':
            val *= 1e-9;
            break;
        case 'p':
        case 'P':
            val *= 1e-12;
            break;
        case 'f':
        case 'F':
            val *= 1e-15;
            break;
        case 'R':
        case 'r':
            break; // Ohm suffix
        case 'e':
        case 'E':
            break; // Already handled by strtod
        }
    }
    return val;
}

extern "C" double flux_string_concat(double a_dbl, double b_dbl)
{
    const char* a = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(a_dbl)));
    const char* b = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(b_dbl)));
    std::string aStr = a ? std::string(a) : "";
    std::string bStr = b ? std::string(b) : "";
    auto& pool = g_fileio_pool;
    pool.emplace_back(aStr + bStr);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
}

extern "C" double flux_double_to_string(double val)
{
    auto& pool = g_fileio_pool;
    pool.push_back(std::to_string(val));
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
}

extern "C" double flux_regex_match(double str_dbl, double pat_dbl)
{
    const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(str_dbl)));
    const char* pat = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(pat_dbl)));
    if (!str || !pat)
        return 0.0;
    try {
        return std::regex_search(str, std::regex(pat)) ? 1.0 : 0.0;
    } catch (...) {
        return 0.0;
    }
}

extern "C" double flux_regex_replace(double str_dbl, double pat_dbl, double repl_dbl)
{
    const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(str_dbl)));
    const char* pat = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(pat_dbl)));
    const char* repl = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(repl_dbl)));
    if (!str || !pat || !repl)
        return 0.0;
    try {
        auto& pool = g_fileio_pool;
        pool.push_back(std::regex_replace(std::string(str), std::regex(pat), std::string(repl)));
        return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
    } catch (...) {
        return 0.0;
    }
}
