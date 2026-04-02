#ifndef FLUX_BENCHMARK_H
#define FLUX_BENCHMARK_H

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Flux {
namespace Benchmark {

// Benchmark result for a single test
struct BenchmarkResult {
    std::string name;
    std::string language;  // "FluxScript", "Python", "C++"
    std::string category;  // "Math", "Loop", "Vector", "Memory", etc.
    
    double meanTime;       // Mean execution time (ms)
    double stdDev;         // Standard deviation (ms)
    double minTime;        // Minimum time (ms)
    double maxTime;        // Maximum time (ms)
    double medianTime;     // Median time (ms)
    
    int iterations;        // Number of iterations
    double opsPerSecond;   // Operations per second
    double speedup;        // Speedup vs baseline (Python)
    
    bool passed;           // Test passed validation
    std::string notes;     // Additional notes
    
    BenchmarkResult() : meanTime(0), stdDev(0), minTime(0), maxTime(0),
                        medianTime(0), iterations(0), opsPerSecond(0),
                        speedup(1.0), passed(true) {}
};

// Benchmark suite result
struct BenchmarkSuiteResult {
    std::string suiteName;
    std::vector<BenchmarkResult> results;
    std::chrono::system_clock::time_point timestamp;
    std::string machineInfo;
    std::string compilerInfo;
    
    void printSummary() const;
    void printDetailed() const;
    void saveCSV(const std::string& filename) const;
    void saveJSON(const std::string& filename) const;
};

// Benchmark runner
class BenchmarkRunner {
public:
    static BenchmarkRunner& instance();
    
    // Register benchmark test
    using BenchmarkFunc = std::function<void()>;
    
    void registerTest(const std::string& name, const std::string& language,
                      const std::string& category, BenchmarkFunc func,
                      BenchmarkFunc validation = nullptr);
    
    // Run benchmarks
    BenchmarkResult runTest(const std::string& name, int iterations = 1000,
                            int warmupIterations = 100);
    
    BenchmarkSuiteResult runAll(int iterations = 1000, int warmupIterations = 100);
    BenchmarkSuiteResult runByCategory(const std::string& category,
                                        int iterations = 1000);
    BenchmarkSuiteResult runByLanguage(const std::string& language,
                                        int iterations = 1000);
    
    // Configuration
    void setVerbose(bool v) { m_verbose = v; }
    void setIterations(int n) { m_defaultIterations = n; }
    void setWarmupIterations(int n) { m_warmupIterations = n; }
    
    // Get results
    const std::vector<BenchmarkResult>& getAllResults() const { return m_allResults; }
    
    // Comparison utilities
    static double calculateSpeedup(const BenchmarkResult& baseline,
                                    const BenchmarkResult& comparison);
    static std::string generateComparisonTable(
        const std::vector<BenchmarkResult>& results);
    
private:
    BenchmarkRunner() = default;
    
    struct TestInfo {
        std::string name;
        std::string language;
        std::string category;
        BenchmarkFunc func;
        BenchmarkFunc validation;
    };
    
    std::vector<TestInfo> m_tests;
    std::vector<BenchmarkResult> m_allResults;
    
    bool m_verbose = false;
    int m_defaultIterations = 1000;
    int m_warmupIterations = 100;
    
    // Timing utilities
    std::vector<double> runTimed(BenchmarkFunc func, int iterations);
    BenchmarkResult analyzeResults(const std::string& name,
                                    const std::string& language,
                                    const std::string& category,
                                    const std::vector<double>& times,
                                    int iterations);
};

// Convenience macros
#define FLUX_BENCHMARK(name, language, category) \
    FLUX_BENCHMARK_IMPL(name, language, category, __FUNCTION__)

#define FLUX_BENCHMARK_IMPL(name, language, category, func_name) \
    static void func_name(); \
    static struct func_name##_reg { \
        func_name##_reg() { \
            Flux::Benchmark::BenchmarkRunner::instance().registerTest( \
                name, language, category, func_name); \
        } \
    } func_name##_instance; \
    static void func_name()

// Predefined benchmark categories
namespace Categories {
    const std::string MATH = "Math";
    const std::string LOOP = "Loop";
    const std::string VECTOR = "Vector";
    const std::string MATRIX = "Matrix";
    const std::string MEMORY = "Memory";
    const std::string ALGORITHM = "Algorithm";
    const std::string IO = "IO";
    const std::string PYTHON_BRIDGE = "PythonBridge";
}

// Predefined languages
namespace Languages {
    const std::string FLUXSCRIPT = "FluxScript";
    const std::string FLUXSCRIPT_NATIVE = "FluxScript_Native";
    const std::string FLUXSCRIPT_PYTHON = "FluxScript_PythonBridge";
    const std::string PYTHON = "Python";
    const std::string PYTHON_NUMPY = "Python_NumPy";
    const std::string CPP = "C++";
    const std::string CPP_OPTIMIZED = "C++_Optimized";
}

} // namespace Benchmark
} // namespace Flux

#endif // FLUX_BENCHMARK_H
