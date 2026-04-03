#ifndef FLUX_PARALLEL_RUNTIME_H
#define FLUX_PARALLEL_RUNTIME_H

#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

namespace Flux {

class ParallelRuntime {
public:
    static ParallelRuntime& instance();
    
    // Get number of available hardware threads
    int getNumThreads() const { return m_numThreads; }
    
    // Set number of threads (0 = auto-detect)
    void setNumThreads(int n) { m_numThreads = (n > 0) ? n : std::thread::hardware_concurrency(); }
    
    // Parallel for loop
    void parallelFor(int start, int end, std::function<void(int)> body, int chunkSize = 1);
    
    // Parallel for with reduction
    double parallelReduce(int start, int end, std::function<double(int)> body,
                         std::function<double(double, double)> reduce, double identity = 0.0);
    
    // Get timing info
    double getLastExecutionTime() const { return m_lastTime; }
    double getSpeedup() const { return m_speedup; }
    
private:
    ParallelRuntime() : m_numThreads(std::thread::hardware_concurrency()), m_lastTime(0), m_speedup(1.0) {}
    
    int m_numThreads;
    double m_lastTime;
    double m_speedup;
};

} // namespace Flux

#endif // FLUX_PARALLEL_RUNTIME_H
