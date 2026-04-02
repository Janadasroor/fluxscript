#ifndef FLUX_PROFILER_H
#define FLUX_PROFILER_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>

namespace Flux {
namespace Profiler {

// Profile entry for a single function call
struct ProfileEntry {
    std::string functionName;
    std::string file;
    int line;
    
    std::chrono::nanoseconds totalTime;
    std::chrono::nanoseconds selfTime;  // Excluding children
    int callCount;
    int recursiveDepth;
    
    ProfileEntry() : totalTime(0), selfTime(0), callCount(0), recursiveDepth(0) {}
};

// Call stack entry
struct CallStackEntry {
    std::string functionName;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::nanoseconds childTime;
};

// Profile report
struct ProfileReport {
    std::vector<ProfileEntry> entries;
    std::chrono::nanoseconds totalDuration;
    int totalCalls;
    
    void print(int topN = 20) const;
    std::string toFlameGraph() const;
    std::string toJSON() const;
};

// Memory allocation record
struct AllocationRecord {
    void* address;
    size_t size;
    std::string file;
    int line;
    std::chrono::steady_clock::time_point time;
    bool freed;
};

// Memory statistics
struct MemoryStats {
    size_t totalAllocated;
    size_t totalFreed;
    size_t currentUsage;
    size_t peakUsage;
    size_t allocationCount;
    size_t leakCount;
    
    MemoryStats() : totalAllocated(0), totalFreed(0), currentUsage(0), 
                    peakUsage(0), allocationCount(0), leakCount(0) {}
};

// Main profiler class
class Profiler {
public:
    static Profiler& instance();
    
    // Start/stop profiling
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Function entry/exit
    void functionEntry(const std::string& name, const std::string& file, int line);
    void functionExit(const std::string& name);
    
    // Manual timing
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name, const std::string& file = "", int line = 0);
        ~ScopedTimer();
    private:
        std::string m_name;
        std::string m_file;
        int m_line;
        std::chrono::steady_clock::time_point m_start;
    };
    
    // Get report
    ProfileReport getReport() const;
    void printReport(int topN = 20);
    void saveReport(const std::string& filename, int topN = 20);
    void saveFlameGraph(const std::string& filename);
    
    // Reset
    void reset();
    
    // Memory tracking
    void trackAllocation(void* ptr, size_t size, const std::string& file, int line);
    void trackDeallocation(void* ptr);
    MemoryStats getMemoryStats() const;
    void printMemoryReport();
    std::vector<AllocationRecord> getLeaks() const;
    
    // Configuration
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    void setMinDuration(std::chrono::microseconds min) { m_minDuration = min; }
    
private:
    Profiler();
    ~Profiler();
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    
    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    bool m_enabled{true};
    std::chrono::microseconds m_minDuration{0};
    
    std::map<std::string, ProfileEntry> m_entries;
    std::vector<CallStackEntry> m_callStack;
    std::chrono::steady_clock::time_point m_startTime;
    
    // Memory tracking
    std::map<void*, AllocationRecord> m_allocations;
    MemoryStats m_memoryStats;
};

// RAII timer macro
#define FLUX_PROFILE_FUNCTION() \
    Flux::Profiler::Profiler::ScopedTimer _profiler_timer(__FUNCTION__, __FILE__, __LINE__)

#define FLUX_PROFILE_SCOPE(name) \
    Flux::Profiler::Profiler::ScopedTimer _profiler_timer(name, __FILE__, __LINE__)

// Memory tracking macros
#ifdef FLUX_ENABLE_MEMORY_TRACKING
#define FLUX_TRACK_ALLOC(ptr, size) \
    Flux::Profiler::Profiler::instance().trackAllocation(ptr, size, __FILE__, __LINE__)
#define FLUX_TRACK_FREE(ptr) \
    Flux::Profiler::Profiler::instance().trackDeallocation(ptr)
#else
#define FLUX_TRACK_ALLOC(ptr, size)
#define FLUX_TRACK_FREE(ptr)
#endif

} // namespace Profiler
} // namespace Flux

#endif // FLUX_PROFILER_H
