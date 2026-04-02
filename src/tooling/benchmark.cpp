#include "flux/tooling/benchmark.h"
#include <random>
#include <fstream>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace Flux {
namespace Benchmark {

// ============================================================================
// BenchmarkRunner Implementation
// ============================================================================

BenchmarkRunner& BenchmarkRunner::instance() {
    static BenchmarkRunner runner;
    return runner;
}

void BenchmarkRunner::registerTest(const std::string& name, const std::string& language,
                                    const std::string& category, BenchmarkFunc func,
                                    BenchmarkFunc validation) {
    TestInfo info;
    info.name = name;
    info.language = language;
    info.category = category;
    info.func = func;
    info.validation = validation;
    m_tests.push_back(info);
}

std::vector<double> BenchmarkRunner::runTimed(BenchmarkFunc func, int iterations) {
    std::vector<double> times;
    times.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        
        double elapsedMs = std::chrono::duration<double, std::milli>(
            end - start).count();
        times.push_back(elapsedMs);
    }
    
    return times;
}

BenchmarkResult BenchmarkRunner::analyzeResults(const std::string& name,
                                                 const std::string& language,
                                                 const std::string& category,
                                                 const std::vector<double>& times,
                                                 int iterations) {
    BenchmarkResult result;
    result.name = name;
    result.language = language;
    result.category = category;
    result.iterations = iterations;
    
    if (times.empty()) return result;
    
    // Calculate statistics
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    result.meanTime = sum / times.size();
    
    // Min, Max
    result.minTime = *std::min_element(times.begin(), times.end());
    result.maxTime = *std::max_element(times.begin(), times.end());
    
    // Median
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    size_t mid = sorted.size() / 2;
    result.medianTime = (sorted.size() % 2 == 0) ?
        (sorted[mid-1] + sorted[mid]) / 2.0 : sorted[mid];
    
    // Standard deviation
    double sqSum = 0;
    for (double t : times) {
        sqSum += (t - result.meanTime) * (t - result.meanTime);
    }
    result.stdDev = std::sqrt(sqSum / times.size());
    
    // Operations per second
    result.opsPerSecond = 1000.0 / result.meanTime;  // Convert ms to ops/s
    
    // Calculate speedup vs Python baseline
    for (const auto& r : m_allResults) {
        if (r.name == name && r.language.find("Python") != std::string::npos &&
            r.language.find("NumPy") == std::string::npos) {
            result.speedup = r.meanTime / result.meanTime;
            break;
        }
    }
    
    return result;
}

BenchmarkResult BenchmarkRunner::runTest(const std::string& name, int iterations,
                                          int warmupIterations) {
    // Find test
    TestInfo* test = nullptr;
    for (auto& t : m_tests) {
        if (t.name == name) {
            test = &t;
            break;
        }
    }
    
    if (!test) {
        std::cerr << "Test not found: " << name << std::endl;
        return BenchmarkResult();
    }
    
    if (m_verbose) {
        std::cout << "Running: " << name << " (" << test->language << ")... ";
        std::cout.flush();
    }
    
    // Warmup
    runTimed(test->func, warmupIterations);
    
    // Run timed iterations
    auto times = runTimed(test->func, iterations);
    
    // Validate
    bool passed = true;
    if (test->validation) {
        try {
            test->validation();
        } catch (...) {
            passed = false;
        }
    }
    
    // Analyze results
    auto result = analyzeResults(name, test->language, test->category, times, iterations);
    result.passed = passed;
    
    m_allResults.push_back(result);
    
    if (m_verbose) {
        std::cout << std::fixed << std::setprecision(3) << result.meanTime 
                  << " ms (" << result.iterations << " iterations)" << std::endl;
    }
    
    return result;
}

BenchmarkSuiteResult BenchmarkRunner::runAll(int iterations, int warmupIterations) {
    BenchmarkSuiteResult suite;
    suite.suiteName = "FluxScript Performance Benchmark";
    suite.timestamp = std::chrono::system_clock::now();
    
    // Machine info
#ifdef _WIN32
    suite.machineInfo = "Windows";
#else
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        suite.machineInfo = std::string(buffer.sysname) + " " + buffer.release;
    }
#endif
    
    suite.compilerInfo = "GCC " + std::to_string(__GNUC__) + "." + 
                         std::to_string(__GNUC_MINOR__);
    
    if (m_verbose) {
        std::cout << "\n=== FluxScript Benchmark Suite ===\n";
        std::cout << "Machine: " << suite.machineInfo << "\n";
        std::cout << "Compiler: " << suite.compilerInfo << "\n";
        std::cout << "Iterations: " << iterations << "\n\n";
    }
    
    // Run all tests
    for (const auto& test : m_tests) {
        runTest(test.name, iterations, warmupIterations);
    }
    
    suite.results = m_allResults;
    
    if (m_verbose) {
        std::cout << "\n";
        suite.printSummary();
    }
    
    return suite;
}

BenchmarkSuiteResult BenchmarkRunner::runByCategory(const std::string& category,
                                                     int iterations) {
    std::vector<std::string> testNames;
    for (const auto& test : m_tests) {
        if (test.category == category) {
            testNames.push_back(test.name);
        }
    }
    
    BenchmarkSuiteResult suite;
    suite.suiteName = "FluxScript Benchmark - " + category;
    
    for (const auto& name : testNames) {
        auto result = runTest(name, iterations);
        suite.results.push_back(result);
    }
    
    return suite;
}

BenchmarkSuiteResult BenchmarkRunner::runByLanguage(const std::string& language,
                                                     int iterations) {
    std::vector<std::string> testNames;
    for (const auto& test : m_tests) {
        if (test.language == language) {
            testNames.push_back(test.name);
        }
    }
    
    BenchmarkSuiteResult suite;
    suite.suiteName = "FluxScript Benchmark - " + language;
    
    for (const auto& name : testNames) {
        auto result = runTest(name, iterations);
        suite.results.push_back(result);
    }
    
    return suite;
}

double BenchmarkRunner::calculateSpeedup(const BenchmarkResult& baseline,
                                          const BenchmarkResult& comparison) {
    if (baseline.meanTime == 0) return 1.0;
    return baseline.meanTime / comparison.meanTime;
}

std::string BenchmarkRunner::generateComparisonTable(
    const std::vector<BenchmarkResult>& results)
{
    std::ostringstream oss;
    
    // Group by test name
    std::map<std::string, std::vector<BenchmarkResult>> byTest;
    for (const auto& r : results) {
        byTest[r.name].push_back(r);
    }
    
    oss << "\n";
    oss << "┌─────────────────────────┬──────────────────┬────────────┬────────────┬──────────┐\n";
    oss << "│ Test                    │ Language         │ Time (ms)  │ Speedup    │ Ops/sec  │\n";
    oss << "├─────────────────────────┼──────────────────┼────────────┼────────────┼──────────┤\n";
    
    for (const auto& pair : byTest) {
        // Find Python baseline
        double pythonTime = 0;
        for (const auto& r : pair.second) {
            if (r.language.find("Python") != std::string::npos &&
                r.language.find("NumPy") == std::string::npos) {
                pythonTime = r.meanTime;
                break;
            }
        }
        
        for (const auto& r : pair.second) {
            double speedup = (pythonTime > 0) ? pythonTime / r.meanTime : 1.0;
            
            oss << "│ " << std::left << std::setw(23) << r.name
                << "│ " << std::left << std::setw(16) << r.language
                << "│ " << std::right << std::setw(10) << std::fixed << std::setprecision(3) << r.meanTime
                << "│ " << std::right << std::setw(10) << std::fixed << std::setprecision(2) << speedup << "x"
                << "│ " << std::right << std::setw(8) << std::fixed << std::setprecision(0) << r.opsPerSecond
                << " │\n";
        }
        oss << "├─────────────────────────┴──────────────────┴────────────┴────────────┴──────────┤\n";
    }
    
    oss << "└──────────────────────────────────────────────────────────────────────────────────┘\n";
    
    return oss.str();
}

// ============================================================================
// BenchmarkSuiteResult Implementation
// ============================================================================

void BenchmarkSuiteResult::printSummary() const {
    std::cout << BenchmarkRunner::generateComparisonTable(results);
    
    // Overall statistics
    std::map<std::string, double> avgSpeedup;
    std::map<std::string, int> count;
    
    for (const auto& r : results) {
        avgSpeedup[r.language] += r.speedup;
        count[r.language]++;
    }
    
    std::cout << "\n=== Average Speedup by Language ===\n";
    std::cout << std::left << std::setw(25) << "Language" 
              << std::right << std::setw(15) << "Avg Speedup" << "\n";
    std::cout << std::string(40, '-') << "\n";
    
    for (const auto& pair : avgSpeedup) {
        double avg = pair.second / count[pair.first];
        std::cout << std::left << std::setw(25) << pair.first
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << avg << "x" << "\n";
    }
}

void BenchmarkSuiteResult::printDetailed() const {
    std::cout << "\n=== Detailed Benchmark Results ===\n\n";
    
    for (const auto& r : results) {
        std::cout << "Test: " << r.name << "\n";
        std::cout << "  Language:    " << r.language << "\n";
        std::cout << "  Category:    " << r.category << "\n";
        std::cout << "  Mean:        " << std::fixed << std::setprecision(3) << r.meanTime << " ms\n";
        std::cout << "  Std Dev:     " << std::fixed << std::setprecision(3) << r.stdDev << " ms\n";
        std::cout << "  Min:         " << std::fixed << std::setprecision(3) << r.minTime << " ms\n";
        std::cout << "  Max:         " << std::fixed << std::setprecision(3) << r.maxTime << " ms\n";
        std::cout << "  Median:      " << std::fixed << std::setprecision(3) << r.medianTime << " ms\n";
        std::cout << "  Iterations:  " << r.iterations << "\n";
        std::cout << "  Ops/sec:     " << std::fixed << std::setprecision(0) << r.opsPerSecond << "\n";
        std::cout << "  Speedup:     " << std::fixed << std::setprecision(2) << r.speedup << "x\n";
        std::cout << "  Passed:      " << (r.passed ? "Yes" : "No") << "\n";
        if (!r.notes.empty()) {
            std::cout << "  Notes:       " << r.notes << "\n";
        }
        std::cout << "\n";
    }
}

void BenchmarkSuiteResult::saveCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    // Header
    file << "Test,Language,Category,Mean_ms,StdDev_ms,Min_ms,Max_ms,Median_ms,"
         << "Iterations,Ops_per_sec,Speedup,Passed\n";
    
    // Data
    for (const auto& r : results) {
        file << r.name << ","
             << r.language << ","
             << r.category << ","
             << std::fixed << std::setprecision(3) << r.meanTime << ","
             << std::fixed << std::setprecision(3) << r.stdDev << ","
             << std::fixed << std::setprecision(3) << r.minTime << ","
             << std::fixed << std::setprecision(3) << r.maxTime << ","
             << std::fixed << std::setprecision(3) << r.medianTime << ","
             << r.iterations << ","
             << std::fixed << std::setprecision(0) << r.opsPerSecond << ","
             << std::fixed << std::setprecision(2) << r.speedup << ","
             << (r.passed ? "1" : "0") << "\n";
    }
    
    file.close();
    std::cout << "Results saved to: " << filename << std::endl;
}

void BenchmarkSuiteResult::saveJSON(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    file << "{\n";
    file << "  \"suite\": \"" << suiteName << "\",\n";
    std::time_t t_c = std::chrono::system_clock::to_time_t(timestamp);
    file << "  \"timestamp\": \"" << std::ctime(&t_c) << "\",\n";
    file << "  \"machine\": \"" << machineInfo << "\",\n";
    file << "  \"compiler\": \"" << compilerInfo << "\",\n";
    file << "  \"results\": [\n";
    
    bool first = true;
    for (const auto& r : results) {
        if (!first) file << ",\n";
        first = false;
        
        file << "    {\n";
        file << "      \"name\": \"" << r.name << "\",\n";
        file << "      \"language\": \"" << r.language << "\",\n";
        file << "      \"category\": \"" << r.category << "\",\n";
        file << "      \"mean_ms\": " << std::fixed << std::setprecision(3) << r.meanTime << ",\n";
        file << "      \"stddev_ms\": " << std::fixed << std::setprecision(3) << r.stdDev << ",\n";
        file << "      \"min_ms\": " << std::fixed << std::setprecision(3) << r.minTime << ",\n";
        file << "      \"max_ms\": " << std::fixed << std::setprecision(3) << r.maxTime << ",\n";
        file << "      \"median_ms\": " << std::fixed << std::setprecision(3) << r.medianTime << ",\n";
        file << "      \"iterations\": " << r.iterations << ",\n";
        file << "      \"ops_per_sec\": " << std::fixed << std::setprecision(0) << r.opsPerSecond << ",\n";
        file << "      \"speedup\": " << std::fixed << std::setprecision(2) << r.speedup << ",\n";
        file << "      \"passed\": " << (r.passed ? "true" : "false") << "\n";
        file << "    }";
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    file.close();
    std::cout << "Results saved to: " << filename << std::endl;
}

} // namespace Benchmark
} // namespace Flux
