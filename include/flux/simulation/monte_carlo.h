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

#ifndef FLUX_MONTE_CARLO_H
#define FLUX_MONTE_CARLO_H

#include <string>
#include <vector>
#include <map>
#include <random>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

namespace Flux {
namespace Simulation {

// Distribution types for component variation
enum class DistributionType {
    Uniform,        // Equal probability in range
    Normal,         // Gaussian distribution
    TruncatedNormal,// Gaussian with bounds
    LogNormal,      // Log-normal distribution
    Weibull,        // Weibull distribution
    Custom          // User-defined PDF
};

// Component variation specification
struct ComponentVariation {
    std::string name;
    double nominal;
    double tolerance;  // e.g., 0.01 for 1%
    DistributionType distribution;
    double param1;     // For custom distributions
    double param2;
    
    ComponentVariation() : nominal(0), tolerance(0.01), 
                           distribution(DistributionType::Normal) {}
};

// Single Monte Carlo run result
struct MCRunResult {
    std::map<std::string, double> parameters;  // Randomized values
    std::map<std::string, double> measurements; // Results
    bool converged;
    int iterations;
    double executionTime;
};

// Statistical analysis results
struct MCStatistics {
    // For each measurement
    struct MeasurementStats {
        std::string name;
        double mean;
        double std_dev;
        double min;
        double max;
        double median;
        double percentile_5;
        double percentile_95;
        double percentile_99;
        double skewness;
        double kurtosis;
        std::vector<double> values;
    };
    
    std::map<std::string, MeasurementStats> measurements;
    
    // Yield analysis
    double yield;  // Fraction within specs
    double cp;     // Process capability
    double cpk;    // Process capability index
    
    // Convergence info
    bool converged;
    int runsForConvergence;
};

// Specification limits
struct SpecLimits {
    std::string measurement;
    double lower;
    double upper;
    bool hasLower;
    bool hasUpper;
    
    SpecLimits() : lower(0), upper(0), hasLower(false), hasUpper(false) {}
    
    bool withinSpec(double value) const {
        if (hasLower && value < lower) return false;
        if (hasUpper && value > upper) return false;
        return true;
    }
};

// Monte Carlo simulation engine
class MonteCarloEngine {
public:
    MonteCarloEngine();
    ~MonteCarloEngine();
    
    // Configuration
    void setSeed(unsigned int seed);
    void setNumRuns(int n) { m_numRuns = n; }
    void setNumThreads(int n) { m_numThreads = n; }
    void setConvergenceThreshold(double threshold) { m_convergenceThreshold = threshold; }
    
    // Add component variations
    void addVariation(const ComponentVariation& var);
    void addVariation(const std::string& name, double nominal, double tolerance,
                      DistributionType dist = DistributionType::Normal);
    
    // Add specification limits
    void addSpecLimit(const SpecLimits& spec);
    
    // Define measurement function
    using MeasurementFunction = std::function<std::map<std::string, double>(
        const std::map<std::string, double>& params)>;
    
    void setMeasurementFunction(MeasurementFunction func);
    
    // Run simulation
    void run();
    void runAsync();
    void stop();
    
    // Results
    const std::vector<MCRunResult>& results() const { return m_results; }
    MCStatistics getStatistics() const;
    bool isConverged() const { return m_converged; }
    
    // Yield analysis
    double getYield() const;
    double getCp() const;
    double getCpk() const;
    
    // Export
    void saveResults(const std::string& filename);
    void saveStatistics(const std::string& filename);
    
    // Progress
    int getCurrentRun() const { return m_currentRun; }
    int getTotalRuns() const { return m_numRuns; }
    double getProgress() const;
    
    // Callbacks
    using ProgressCallback = std::function<void(int current, int total)>;
    void setProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }
    
private:
    double sampleDistribution(const ComponentVariation& var);
    void runWorker(int threadId);
    void checkConvergence();
    
    std::vector<ComponentVariation> m_variations;
    std::vector<SpecLimits> m_specs;
    MeasurementFunction m_measurementFunc;
    
    std::vector<MCRunResult> m_results;
    
    std::mt19937_64 m_rng;
    int m_numRuns;
    int m_numThreads;
    int m_currentRun;
    double m_convergenceThreshold;
    bool m_converged;
    bool m_running;
    std::atomic<bool> m_stopRequested;
    
    ProgressCallback m_progressCallback;
    mutable std::mutex m_mutex;
};

// Convenience function for simple Monte Carlo
MCStatistics monte_carlo_simulate(
    const std::vector<ComponentVariation>& variations,
    const std::vector<SpecLimits>& specs,
    MonteCarloEngine::MeasurementFunction func,
    int numRuns = 1000,
    unsigned int seed = 42
);

// C interface for FluxScript
extern "C" {
    void* flux_mc_create();
    void flux_mc_destroy(void* mc);
    void flux_mc_add_variation(void* mc, const char* name, double nominal, 
                               double tolerance, int dist_type);
    void flux_mc_add_spec(void* mc, const char* measurement, 
                          double lower, double upper, int has_lower, int has_upper);
    void flux_mc_set_runs(void* mc, int n);
    void flux_mc_run(void* mc);
    double flux_mc_get_yield(void* mc);
    double flux_mc_get_mean(void* mc, const char* measurement);
    double flux_mc_get_std(void* mc, const char* measurement);
}

} // namespace Simulation
} // namespace Flux

#endif // FLUX_MONTE_CARLO_H
