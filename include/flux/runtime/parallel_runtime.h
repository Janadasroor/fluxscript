/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

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
