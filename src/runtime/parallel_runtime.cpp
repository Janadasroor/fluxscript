#include "flux/runtime/parallel_runtime.h"
#include <chrono>
#include <algorithm>

namespace Flux {

ParallelRuntime& ParallelRuntime::instance() {
    static ParallelRuntime runtime;
    return runtime;
}

void ParallelRuntime::parallelFor(int start, int end, std::function<void(int)> body, int chunkSize) {
    if (start >= end) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int numTasks = end - start;
    int numThreads = std::min(m_numThreads, (numTasks + chunkSize - 1) / chunkSize);
    
    if (numThreads <= 1) {
        // Sequential execution for small workloads
        for (int i = start; i < end; ++i) {
            body(i);
        }
    } else {
        // Parallel execution
        std::vector<std::thread> threads;
        std::atomic<int> counter(start);
        
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                while (true) {
                    int i = counter.fetch_add(1);
                    if (i >= end) break;
                    body(i);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    m_lastTime = elapsed.count();
    
    std::cout << "[Parallel] Executed " << numTasks << " iterations in "
              << (m_lastTime * 1000) << "ms using " << numThreads << " threads" << std::endl;
}

double ParallelRuntime::parallelReduce(int start, int end, std::function<double(int)> body,
                                        std::function<double(double, double)> reduce, double identity) {
    if (start >= end) return identity;
    
    int numTasks = end - start;
    int numThreads = std::min(m_numThreads, numTasks);
    
    std::vector<double> partialResults(numThreads, identity);
    
    if (numThreads <= 1) {
        double result = identity;
        for (int i = start; i < end; ++i) {
            result = reduce(result, body(i));
        }
        return result;
    }
    
    std::vector<std::thread> threads;
    std::atomic<int> counter(start);
    int chunkSize = (numTasks + numThreads - 1) / numThreads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            double localResult = identity;
            int count = 0;
            while (count < chunkSize) {
                int i = counter.fetch_add(1);
                if (i >= end) break;
                localResult = reduce(localResult, body(i));
                count++;
            }
            partialResults[t] = localResult;
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    double finalResult = identity;
    for (double val : partialResults) {
        finalResult = reduce(finalResult, val);
    }
    
    return finalResult;
}

} // namespace Flux
