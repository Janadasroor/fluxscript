#include <mutex>
#include "flux/simulation/monte_carlo.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <numeric>

namespace Flux {
namespace Simulation {

// ============================================================================
// MonteCarloEngine Implementation
// ============================================================================

MonteCarloEngine::MonteCarloEngine()
    : m_numRuns(1000)
    , m_numThreads(4)
    , m_currentRun(0)
    , m_convergenceThreshold(0.01)
    , m_converged(false)
    , m_running(false)
    , m_stopRequested(false)
{
    // Seed with random value
    m_rng.seed(std::random_device{}());
}

MonteCarloEngine::~MonteCarloEngine() {
    stop();
}

void MonteCarloEngine::setSeed(unsigned int seed) {
    m_rng.seed(seed);
}

void MonteCarloEngine::addVariation(const ComponentVariation& var) {
    m_variations.push_back(var);
}

void MonteCarloEngine::addVariation(const std::string& name, double nominal, 
                                     double tolerance, DistributionType dist) {
    ComponentVariation var;
    var.name = name;
    var.nominal = nominal;
    var.tolerance = tolerance;
    var.distribution = dist;
    m_variations.push_back(var);
}

void MonteCarloEngine::addSpecLimit(const SpecLimits& spec) {
    m_specs.push_back(spec);
}

void MonteCarloEngine::setMeasurementFunction(MeasurementFunction func) {
    m_measurementFunc = func;
}

double MonteCarloEngine::sampleDistribution(const ComponentVariation& var) {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    double u = uniform(m_rng);
    
    switch (var.distribution) {
        case DistributionType::Uniform: {
            double range = 2 * var.tolerance * var.nominal;
            return var.nominal * (1 - var.tolerance) + u * range;
        }
        
        case DistributionType::Normal: {
            std::normal_distribution<double> normal(var.nominal, var.tolerance * var.nominal);
            return normal(m_rng);
        }
        
        case DistributionType::TruncatedNormal: {
            std::normal_distribution<double> normal(var.nominal, var.tolerance * var.nominal);
            double value;
            do {
                value = normal(m_rng);
            } while (value < var.nominal * (1 - 3*var.tolerance) || 
                     value > var.nominal * (1 + 3*var.tolerance));
            return value;
        }
        
        case DistributionType::LogNormal: {
            double mu = std::log(var.nominal);
            double sigma = var.tolerance;
            std::lognormal_distribution<double> lognorm(mu, sigma);
            return lognorm(m_rng);
        }
        
        case DistributionType::Weibull: {
            double shape = var.param1 > 0 ? var.param1 : 2.0;
            double scale = var.param2 > 0 ? var.param2 : var.nominal;
            std::weibull_distribution<double> weibull(shape, scale);
            return weibull(m_rng);
        }
        
        default:
            return var.nominal;
    }
}

void MonteCarloEngine::run() {
    if (!m_measurementFunc) {
        std::cerr << "No measurement function set!\n";
        return;
    }
    
    m_running = true;
    m_stopRequested = false;
    m_currentRun = 0;
    m_results.clear();
    m_converged = false;
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Run simulations
    for (int run = 0; run < m_numRuns && !m_stopRequested; ++run) {
        MCRunResult result;
        
        // Sample component values
        std::map<std::string, double> params;
        for (const auto& var : m_variations) {
            params[var.name] = sampleDistribution(var);
        }
        result.parameters = params;
        
        // Run measurement
        try {
            auto measurements = m_measurementFunc(params);
            result.measurements = measurements;
            result.converged = true;
        } catch (...) {
            result.converged = false;
        }
        
        auto runEnd = std::chrono::steady_clock::now();
        result.executionTime = std::chrono::duration<double>(runEnd - startTime).count();
        result.iterations = 1;
        
        m_results.push_back(result);
        m_currentRun = run + 1;
        
        // Progress callback
        if (m_progressCallback) {
            m_progressCallback(m_currentRun, m_numRuns);
        }
        
        // Check convergence periodically
        if (run % 100 == 0 && run > 0) {
            checkConvergence();
        }
    }
    
    m_running = false;
    
    // Final convergence check
    checkConvergence();
}

void MonteCarloEngine::runAsync() {
    std::vector<std::thread> threads;
    
    int runsPerThread = m_numRuns / m_numThreads;
    
    for (int t = 0; t < m_numThreads; ++t) {
        threads.emplace_back(&MonteCarloEngine::runWorker, this, t);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
}

void MonteCarloEngine::runWorker(int threadId) {
    // Each thread has its own RNG
    std::mt19937_64 threadRng(m_rng() + threadId);
    
    int runsPerThread = m_numRuns / m_numThreads;
    int startRun = threadId * runsPerThread;
    int endRun = (threadId == m_numThreads - 1) ? m_numRuns : startRun + runsPerThread;
    
    for (int run = startRun; run < endRun && !m_stopRequested; ++run) {
        MCRunResult result;
        
        // Sample component values
        std::map<std::string, double> params;
        for (const auto& var : m_variations) {
            std::uniform_real_distribution<double> uniform(0.0, 1.0);
            double u = uniform(threadRng);
            
            if (var.distribution == DistributionType::Normal) {
                std::normal_distribution<double> normal(var.nominal, var.tolerance * var.nominal);
                params[var.name] = normal(threadRng);
            } else {
                params[var.name] = sampleDistribution(var);
            }
        }
        result.parameters = params;
        
        // Run measurement
        try {
            result.measurements = m_measurementFunc(params);
            result.converged = true;
        } catch (...) {
            result.converged = false;
        }
        
        result.iterations = 1;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_results.push_back(result);
            m_currentRun++;
        }
        
        if (m_progressCallback) {
            m_progressCallback(m_currentRun, m_numRuns);
        }
    }
}

void MonteCarloEngine::stop() {
    m_stopRequested = true;
}

void MonteCarloEngine::checkConvergence() {
    if (m_results.size() < 100) return;
    
    // Check if mean has stabilized
    std::vector<double> values;
    std::string firstMeasurement;
    
    for (const auto& pair : m_results.back().measurements) {
        firstMeasurement = pair.first;
        break;
    }
    
    if (firstMeasurement.empty()) return;
    
    for (const auto& result : m_results) {
        auto it = result.measurements.find(firstMeasurement);
        if (it != result.measurements.end()) {
            values.push_back(it->second);
        }
    }
    
    if (values.size() < 100) return;
    
    // Calculate mean of first and second half
    size_t mid = values.size() / 2;
    double mean1 = std::accumulate(values.begin(), values.begin() + mid, 0.0) / mid;
    double mean2 = std::accumulate(values.begin() + mid, values.end(), 0.0) / (values.size() - mid);
    
    double relDiff = std::abs(mean2 - mean1) / std::abs(mean1);
    
    if (relDiff < m_convergenceThreshold) {
        m_converged = true;
    }
}

MCStatistics MonteCarloEngine::getStatistics() const {
    MCStatistics stats;
    
    if (m_results.empty()) return stats;
    
    // Collect all measurements
    std::map<std::string, std::vector<double>> measurementValues;
    
    for (const auto& result : m_results) {
        if (!result.converged) continue;
        
        for (const auto& pair : result.measurements) {
            measurementValues[pair.first].push_back(pair.second);
        }
    }
    
    // Calculate statistics for each measurement
    for (auto& pair : measurementValues) {
        const std::string& name = pair.first;
        std::vector<double>& values = pair.second;
        
        if (values.empty()) continue;
        
        MCStatistics::MeasurementStats ms;
        ms.name = name;
        
        // Sort for percentiles
        std::sort(values.begin(), values.end());
        
        // Basic statistics
        ms.min = values.front();
        ms.max = values.back();
        ms.mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        
        double sqSum = 0;
        for (double v : values) {
            sqSum += (v - ms.mean) * (v - ms.mean);
        }
        ms.std_dev = std::sqrt(sqSum / values.size());
        
        // Median
        size_t mid = values.size() / 2;
        ms.median = (values.size() % 2 == 0) ? 
            (values[mid-1] + values[mid]) / 2 : values[mid];
        
        // Percentiles
        size_t p5 = static_cast<size_t>(0.05 * values.size());
        size_t p95 = static_cast<size_t>(0.95 * values.size());
        size_t p99 = static_cast<size_t>(0.99 * values.size());
        
        ms.percentile_5 = values[p5];
        ms.percentile_95 = values[p95];
        ms.percentile_99 = values[p99];
        
        // Skewness and kurtosis
        if (ms.std_dev > 0) {
            double m3 = 0, m4 = 0;
            for (double v : values) {
                double z = (v - ms.mean) / ms.std_dev;
                m3 += z * z * z;
                m4 += z * z * z * z;
            }
            m3 /= values.size();
            m4 /= values.size();
            
            ms.skewness = m3;
            ms.kurtosis = m4 - 3;  // Excess kurtosis
        }
        
        ms.values = values;
        stats.measurements[name] = ms;
    }
    
    // Yield calculation
    if (!m_specs.empty()) {
        int passCount = 0;
        for (const auto& result : m_results) {
            bool allPass = true;
            for (const auto& spec : m_specs) {
                auto it = result.measurements.find(spec.measurement);
                if (it != result.measurements.end()) {
                    if (!spec.withinSpec(it->second)) {
                        allPass = false;
                        break;
                    }
                }
            }
            if (allPass) passCount++;
        }
        stats.yield = static_cast<double>(passCount) / m_results.size();
    }
    
    stats.converged = m_converged;
    stats.runsForConvergence = m_converged ? m_currentRun : m_numRuns;
    
    return stats;
}

double MonteCarloEngine::getYield() const {
    return getStatistics().yield;
}

double MonteCarloEngine::getCp() const {
    auto stats = getStatistics();
    
    if (m_specs.empty()) return 0;
    
    const auto& spec = m_specs[0];
    auto it = stats.measurements.find(spec.measurement);
    if (it == stats.measurements.end()) return 0;
    
    const auto& ms = it->second;
    if (ms.std_dev == 0) return 0;
    
    double tolerance = (spec.hasUpper && spec.hasLower) ? 
        (spec.upper - spec.lower) / 2 : ms.std_dev * 6;
    
    return tolerance / (3 * ms.std_dev);
}

double MonteCarloEngine::getCpk() const {
    auto stats = getStatistics();
    
    if (m_specs.empty()) return 0;
    
    const auto& spec = m_specs[0];
    auto it = stats.measurements.find(spec.measurement);
    if (it == stats.measurements.end()) return 0;
    
    const auto& ms = it->second;
    if (ms.std_dev == 0) return 0;
    
    double cpu = 0, cpl = 0;
    
    if (spec.hasUpper) {
        cpu = (spec.upper - ms.mean) / (3 * ms.std_dev);
    }
    if (spec.hasLower) {
        cpl = (ms.mean - spec.lower) / (3 * ms.std_dev);
    }
    
    if (spec.hasUpper && spec.hasLower) {
        return std::min(cpu, cpl);
    } else if (spec.hasUpper) {
        return cpu;
    } else {
        return cpl;
    }
}

void MonteCarloEngine::saveResults(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    // Header
    file << "Run,";
    for (const auto& var : m_variations) {
        file << var.name << ",";
    }
    for (const auto& result : m_results) {
        for (const auto& m : result.measurements) {
            file << m.first << ",";
        }
        break;
    }
    file << "Converged,Time\n";
    
    // Data
    for (size_t i = 0; i < m_results.size(); ++i) {
        const auto& result = m_results[i];
        file << (i + 1) << ",";
        
        for (const auto& var : m_variations) {
            auto it = result.parameters.find(var.name);
            file << ((it != result.parameters.end()) ? it->second : 0) << ",";
        }
        
        for (const auto& m : result.measurements) {
            file << m.second << ",";
        }
        
        file << (result.converged ? "1" : "0") << "," 
             << result.executionTime << "\n";
    }
    
    file.close();
}

void MonteCarloEngine::saveStatistics(const std::string& filename) {
    auto stats = getStatistics();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    file << "Monte Carlo Statistics\n";
    file << "=====================\n\n";
    file << "Total Runs: " << m_results.size() << "\n";
    file << "Converged: " << (stats.converged ? "Yes" : "No") << "\n";
    file << "Yield: " << (stats.yield * 100) << "%\n";
    file << "Cp: " << getCp() << "\n";
    file << "Cpk: " << getCpk() << "\n\n";
    
    for (const auto& pair : stats.measurements) {
        const auto& ms = pair.second;
        file << "Measurement: " << ms.name << "\n";
        file << "  Mean:     " << ms.mean << "\n";
        file << "  Std Dev:  " << ms.std_dev << "\n";
        file << "  Min:      " << ms.min << "\n";
        file << "  Max:      " << ms.max << "\n";
        file << "  Median:   " << ms.median << "\n";
        file << "  5%:       " << ms.percentile_5 << "\n";
        file << "  95%:      " << ms.percentile_95 << "\n";
        file << "  Skewness: " << ms.skewness << "\n";
        file << "  Kurtosis: " << ms.kurtosis << "\n\n";
    }
    
    file.close();
}

double MonteCarloEngine::getProgress() const {
    return (m_numRuns > 0) ? static_cast<double>(m_currentRun) / m_numRuns : 0;
}

// ============================================================================
// Convenience Function
// ============================================================================

MCStatistics monte_carlo_simulate(
    const std::vector<ComponentVariation>& variations,
    const std::vector<SpecLimits>& specs,
    MonteCarloEngine::MeasurementFunction func,
    int numRuns,
    unsigned int seed)
{
    MonteCarloEngine engine;
    engine.setSeed(seed);
    engine.setNumRuns(numRuns);
    
    for (const auto& var : variations) {
        engine.addVariation(var);
    }
    
    for (const auto& spec : specs) {
        engine.addSpecLimit(spec);
    }
    
    engine.setMeasurementFunction(func);
    engine.run();
    
    return engine.getStatistics();
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" {

void* flux_mc_create() {
    return new MonteCarloEngine();
}

void flux_mc_destroy(void* mc) {
    delete static_cast<MonteCarloEngine*>(mc);
}

void flux_mc_add_variation(void* mc, const char* name, double nominal, 
                           double tolerance, int dist_type) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    ComponentVariation var;
    var.name = name ? name : "";
    var.nominal = nominal;
    var.tolerance = tolerance;
    var.distribution = static_cast<DistributionType>(dist_type);
    engine->addVariation(var);
}

void flux_mc_add_spec(void* mc, const char* measurement, 
                      double lower, double upper, int has_lower, int has_upper) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    SpecLimits spec;
    spec.measurement = measurement ? measurement : "";
    spec.lower = lower;
    spec.upper = upper;
    spec.hasLower = (has_lower != 0);
    spec.hasUpper = (has_upper != 0);
    engine->addSpecLimit(spec);
}

void flux_mc_set_runs(void* mc, int n) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    engine->setNumRuns(n);
}

void flux_mc_run(void* mc) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    engine->run();
}

double flux_mc_get_yield(void* mc) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    return engine->getYield();
}

double flux_mc_get_mean(void* mc, const char* measurement) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    auto stats = engine->getStatistics();
    
    std::string name = measurement ? measurement : "";
    auto it = stats.measurements.find(name);
    return (it != stats.measurements.end()) ? it->second.mean : 0;
}

double flux_mc_get_std(void* mc, const char* measurement) {
    auto* engine = static_cast<MonteCarloEngine*>(mc);
    auto stats = engine->getStatistics();
    
    std::string name = measurement ? measurement : "";
    auto it = stats.measurements.find(name);
    return (it != stats.measurements.end()) ? it->second.std_dev : 0;
}

}

} // namespace Simulation
} // namespace Flux
