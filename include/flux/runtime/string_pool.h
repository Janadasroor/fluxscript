/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#ifndef FLUX_STRING_POOL_H
#define FLUX_STRING_POOL_H

#include <cstddef>
#include <deque>
#include <string>

static constexpr size_t FLUX_POOL_MAX_SIZE = 10000;

static thread_local std::string g_fileio_buffer;
static thread_local std::deque<std::string> g_fileio_pool;

template <typename Pool>
static inline void pool_evict_if_full(Pool& pool)
{
    while (pool.size() > FLUX_POOL_MAX_SIZE) {
        pool.pop_front();
    }
}

#endif // FLUX_STRING_POOL_H
