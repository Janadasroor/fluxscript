/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/runtime/runtime_helpers.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace Flux {

// Fixed-size worker thread pool with work-stealing for parallel for loops.
// Threads are created once and reused across all parallel_for calls.
class ParallelWorkerPool
{
    int m_numThreads;
    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;

    std::atomic<int64_t> m_current;
    int64_t m_end = 0;
    void (*m_body)(int64_t, void*) = nullptr;
    void* m_userData = nullptr;
    int m_doneCount = 0;
    bool m_hasWork = false;

    void workerLoop()
    {
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_hasWork || m_stop; });
            if (m_stop)
                return;
            lock.unlock();

            // Work-stealing: atomically claim next iteration
            while (true) {
                int64_t i = m_current.fetch_add(1);
                if (i >= m_end)
                    break;
                m_body(i, m_userData);
            }

            // Signal completion
            lock.lock();
            m_doneCount++;
            if (m_doneCount >= m_numThreads) {
                m_hasWork = false;
                lock.unlock();
                m_cv.notify_all();
            }
        }
    }

public:
    ParallelWorkerPool() : m_numThreads(std::thread::hardware_concurrency())
    {
        for (int i = 0; i < m_numThreads; i++)
            m_workers.emplace_back(&ParallelWorkerPool::workerLoop, this);
    }

    ~ParallelWorkerPool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& w : m_workers)
            w.join();
    }

    int numThreads() const { return m_numThreads; }

    void dispatch(int64_t start, int64_t end, void (*body)(int64_t, void*), void* userData)
    {
        if (start >= end || !body)
            return;

        // Detect nested dispatch — run sequentially to avoid corrupting pool state
        if (m_hasWork) {
            for (int64_t i = start; i < end; i++)
                body(i, userData);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_current.store(start);
            m_end = end;
            m_body = body;
            m_userData = userData;
            m_doneCount = 0;
            m_hasWork = true;
        }
        m_cv.notify_all();

        // Main thread participates via work-stealing
        while (true) {
            int64_t i = m_current.fetch_add(1);
            if (i >= end)
                break;
            body(i, userData);
        }

        // Wait for all workers
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_hasWork; });
        }
    }
};

static ParallelWorkerPool& parallelPool()
{
    static ParallelWorkerPool pool;
    return pool;
}

typedef void (*ParallelBodyFunc)(int64_t, void*);

extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr, void* user_data)
{
    (void)chunk_size;
    auto body_func = reinterpret_cast<ParallelBodyFunc>(body_func_ptr);
    parallelPool().dispatch(start, end, body_func, user_data);
}

extern "C" int64_t flux_get_num_threads()
{
    return parallelPool().numThreads();
}

// --- Threading Primitives: spawn/join + channels ---

struct ThreadContext {
    std::thread thread;
    double result;
    std::atomic<bool> done{false};
};

static std::mutex g_thread_map_mutex;
static std::unordered_map<double, ThreadContext*> g_thread_map;
static double g_next_thread_id = 1.0;

extern "C" double flux_spawn(void* func_ptr, void* args, int64_t nargs)
{
    // Copy args array so it outlives the caller
    double* args_copy = new double[static_cast<size_t>(nargs)];
    std::memcpy(args_copy, args, static_cast<size_t>(nargs) * sizeof(double));

    auto ctx = std::make_unique<ThreadContext>();
    double* args_owned = args_copy;
    ctx->thread = std::thread([func_ptr, args_owned, nargs, ctx = ctx.get()]() {
        typedef double (*Fn0)();
        typedef double (*Fn1)(double);
        typedef double (*Fn2)(double, double);
        typedef double (*Fn3)(double, double, double);
        typedef double (*Fn4)(double, double, double, double);
        typedef double (*Fn5)(double, double, double, double, double);
        double result = 0.0;
        switch (nargs) {
            case 0: result = reinterpret_cast<Fn0>(func_ptr)(); break;
            case 1: result = reinterpret_cast<Fn1>(func_ptr)(args_owned[0]); break;
            case 2: result = reinterpret_cast<Fn2>(func_ptr)(args_owned[0], args_owned[1]); break;
            case 3: result = reinterpret_cast<Fn3>(func_ptr)(args_owned[0], args_owned[1], args_owned[2]); break;
            case 4: result = reinterpret_cast<Fn4>(func_ptr)(args_owned[0], args_owned[1], args_owned[2], args_owned[3]); break;
            case 5: result = reinterpret_cast<Fn5>(func_ptr)(args_owned[0], args_owned[1], args_owned[2], args_owned[3], args_owned[4]); break;
        }
        delete[] args_owned;
        ctx->result = result;
        ctx->done.store(true, std::memory_order_release);
    });

    // Assign ID and store in map
    double id;
    {
        std::lock_guard<std::mutex> lock(g_thread_map_mutex);
        id = g_next_thread_id++;
        g_thread_map[id] = ctx.release();
    }
    return id;
}

extern "C" double flux_join(double handle)
{
    ThreadContext* ctx = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_thread_map_mutex);
        auto it = g_thread_map.find(handle);
        if (it == g_thread_map.end())
            return 0.0;
        ctx = it->second;
        g_thread_map.erase(it);
    }
    if (ctx->thread.joinable())
        ctx->thread.join();
    double result = ctx->result;
    delete ctx;
    return result;
}

// Clean up all pending spawned threads (called on error paths / shutdown)
static void flux_join_all_pending()
{
    std::lock_guard<std::mutex> lock(g_thread_map_mutex);
    for (auto& [id, ctx] : g_thread_map) {
        if (ctx->thread.joinable())
            ctx->thread.join();
        delete ctx;
    }
    g_thread_map.clear();
}

extern "C" double flux_thread_self()
{
    // Return a unique double per thread — use std::hash on thread::id
    std::hash<std::thread::id> hasher;
    double id = static_cast<double>(hasher(std::this_thread::get_id()));
    return id;
}

// --- Channels (MPSC: multi-producer, single-consumer) ---

struct Channel {
    std::queue<double> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;
};

extern "C" double flux_chan_create()
{
    auto* ch = new Channel;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(ch));
}

extern "C" void flux_chan_send(double chan, double val)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        if (ch->closed) return;
        ch->queue.push(val);
    }
    ch->cv.notify_one();
}

extern "C" double flux_chan_recv(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    std::unique_lock<std::mutex> lock(ch->mtx);
    ch->cv.wait(lock, [ch]() { return !ch->queue.empty() || ch->closed; });
    if (ch->queue.empty())
        return 0.0;
    double val = ch->queue.front();
    ch->queue.pop();
    return val;
}

extern "C" void flux_chan_close(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        ch->closed = true;
    }
    ch->cv.notify_all();
}

extern "C" void flux_chan_destroy(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        ch->closed = true;
    }
    ch->cv.notify_all();
    delete ch;
}

// --- Mutex ---

extern "C" double flux_mutex_create()
{
    auto* m = new std::mutex;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(m));
}

extern "C" void flux_mutex_lock(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    m->lock();
}

extern "C" void flux_mutex_unlock(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    m->unlock();
}

extern "C" void flux_mutex_destroy(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    delete m;
}

// --- RwLock ---

extern "C" double flux_rwlock_create()
{
    auto* rw = new std::shared_mutex;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(rw));
}

extern "C" void flux_rwlock_read_lock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->lock_shared();
}

extern "C" void flux_rwlock_write_lock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->lock();
}

extern "C" void flux_rwlock_unlock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->unlock();
}

extern "C" void flux_rwlock_destroy(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    delete m;
}

} // namespace Flux
