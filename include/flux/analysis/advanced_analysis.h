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

#ifndef FLUX_ADVANCED_ANALYSIS_H
#define FLUX_ADVANCED_ANALYSIS_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Flux {
namespace AdvancedAnalysis {

// ┌─────────────────────────────────────────────────────┐
// │  Stability Analysis Results                         │
// └─────────────────────────────────────────────────────┘
struct StabilityResult {
    double gainMargin;           // dB
    double phaseMargin;          // degrees
    double gainMarginFreq;       // Hz (frequency where phase = -180°)
    double phaseMarginFreq;      // Hz (frequency where gain = 0dB)
    double bandwidth;            // Hz (-3dB frequency)
    double peakGain;             // dB
    double peakGainFreq;         // Hz
    bool isStable;
    std::string verdict;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    StabilityResult() : gainMargin(0), phaseMargin(0), gainMarginFreq(0),
                        phaseMarginFreq(0), bandwidth(0), peakGain(0),
                        peakGainFreq(0), isStable(true) {}
    
    std::string toString() const;
    std::string toMarkdown() const;
};

// ┌─────────────────────────────────────────────────────┐
// │  Sensitivity Analysis Results                       │
// └─────────────────────────────────────────────────────┘
struct SensitivityResult {
    struct ComponentSensitivity {
        std::string name;
        double sensitivity;       // ∂Output/∂Component
        double percentChange;     // % change in output for 1% change in component
        std::string criticality;  // "Critical", "Moderate", "Low"
    };
    
    std::vector<ComponentSensitivity> sensitivities;
    std::string outputVariable;
    std::string mostCritical;
    std::string leastCritical;
    std::vector<std::string> recommendations;
    
    std::string toString() const;
    std::string toMarkdown() const;
};

// ┌─────────────────────────────────────────────────────┐
// │  Monte Carlo Analysis Results                       │
// └─────────────────────────────────────────────────────┘
struct MonteCarloResult {
    int iterations;
    double mean;
    double stdDev;
    double min;
    double max;
    double yield;                // % within specifications
    double worstCase;            // Worst case value
    std::string worstCaseParams;
    std::map<std::string, double> paramStats;
    std::vector<double> histogram;  // Binned results
    std::vector<std::string> recommendations;
    
    MonteCarloResult() : iterations(0), mean(0), stdDev(0), 
                         min(0), max(0), yield(0), worstCase(0) {}
    
    std::string toString() const;
    std::string toMarkdown() const;
};

// ┌─────────────────────────────────────────────────────┐
// │  Optimization Results                               │
// └─────────────────────────────────────────────────────┘
struct OptimizationResult {
    bool converged;
    int iterations;
    std::map<std::string, double> optimalValues;
    std::map<std::string, double> initialValues;
    double objectiveValue;
    double improvement;
    std::vector<std::map<std::string, double>> history;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    OptimizationResult() : converged(false), iterations(0), 
                           objectiveValue(0), improvement(0) {}
    
    std::string toString() const;
    std::string toMarkdown() const;
};

// ┌─────────────────────────────────────────────────────┐
// │  Worst-Case Analysis Results                        │
// └─────────────────────────────────────────────────────┘
struct WorstCaseResult {
    double nominal;
    double worstMin;
    double worstMax;
    std::string worstMinParams;
    std::string worstMaxParams;
    double margin;
    bool meetsSpec;
    std::vector<std::map<std::string, double>> corners;
    std::vector<std::string> violations;
    std::vector<std::string> recommendations;
    
    WorstCaseResult() : nominal(0), worstMin(0), worstMax(0), 
                        margin(0), meetsSpec(true) {}
    
    std::string toString() const;
    std::string toMarkdown() const;
};

// ┌─────────────────────────────────────────────────────┐
// │  Analysis Engines                                   │
// └─────────────────────────────────────────────────────┘

class StabilityAnalyzer {
public:
    StabilityAnalyzer();
    ~StabilityAnalyzer();
    
    // Run stability analysis on circuit
    StabilityResult analyze(const std::string& circuitFile);
    StabilityResult analyzeFromString(const std::string& netlist);
    
private:
    double findGainMargin(const std::vector<double>& freq, 
                          const std::vector<double>& phase);
    double findPhaseMargin(const std::vector<double>& freq, 
                           const std::vector<double>& gain);
    double find3dBBandwidth(const std::vector<double>& freq, 
                            const std::vector<double>& gain);
};

class SensitivityAnalyzer {
public:
    SensitivityAnalyzer();
    ~SensitivityAnalyzer();
    
    // Run sensitivity analysis
    SensitivityResult analyze(const std::string& circuitFile,
                              const std::vector<std::string>& components,
                              const std::string& outputVar);
    
private:
    double calculateSensitivity(const std::string& netlist,
                                const std::string& paramName,
                                double nominalValue,
                                double deltaPercent,
                                const std::string& outputVar);
};

class MonteCarloEngine {
public:
    MonteCarloEngine();
    ~MonteCarloEngine();
    
    // Configure analysis
    void setIterations(int n) { m_iterations = n; }
    void setDistribution(const std::string& dist);  // "gaussian", "uniform", "custom"
    void addParameter(const std::string& name, double nominal, double tolerance);
    void setOutputVariable(const std::string& var) { m_outputVar = var; }
    void setSpecLimits(double min, double max);
    
    // Run Monte Carlo
    MonteCarloResult run(const std::string& circuitFile);
    
private:
    int m_iterations;
    std::string m_distribution;
    std::vector<std::string> m_paramNames;
    std::vector<double> m_nominalValues;
    std::vector<double> m_tolerances;
    std::string m_outputVar;
    double m_specMin, m_specMax;
    
    double runSingleSimulation(const std::string& netlist,
                               const std::map<std::string, double>& params);
};

class CircuitOptimizer {
public:
    CircuitOptimizer();
    ~CircuitOptimizer();
    
    // Add optimization variable
    void addVariable(const std::string& name, double initial, 
                     double min, double max);
    
    // Add optimization target
    void addTarget(const std::string& target, double value, double weight = 1.0);
    
    // Run optimization
    OptimizationResult optimize(const std::string& circuitFile);
    
private:
    std::vector<std::string> m_varNames;
    std::vector<double> m_varInitial, m_varMin, m_varMax;
    std::vector<std::string> m_targetNames;
    std::vector<double> m_targetValues, m_targetWeights;
    
    double evaluateObjective(const std::string& netlist,
                            const std::map<std::string, double>& vars);
};

class WorstCaseAnalyzer {
public:
    WorstCaseAnalyzer();
    ~WorstCaseAnalyzer();
    
    // Add component with tolerance
    void addComponent(const std::string& name, double nominal, double tolerance);
    void setOutputVariable(const std::string& var) { m_outputVar = var; }
    void setSpecLimits(double min, double max);
    
    // Run worst-case analysis
    WorstCaseResult analyze(const std::string& circuitFile);
    
private:
    std::vector<std::string> m_compNames;
    std::vector<double> m_nominalValues, m_tolerances;
    std::string m_outputVar;
    double m_specMin, m_specMax;
    
    double runCorner(const std::string& netlist,
                     const std::map<std::string, double>& values);
};

// ┌─────────────────────────────────────────────────────┐
// │  C Interface for FluxScript JIT                     │
// └─────────────────────────────────────────────────────┘
extern "C" {
    // Stability
    void* flux_stability_create();
    void flux_stability_destroy(void* analyzer);
    const char* flux_stability_analyze(void* analyzer, const char* circuit);
    
    // Sensitivity
    void* flux_sensitivity_create();
    void flux_sensitivity_destroy(void* analyzer);
    void flux_sensitivity_add_component(void* analyzer, const char* name);
    const char* flux_sensitivity_analyze(void* analyzer, const char* circuit, const char* output);
    
    // Monte Carlo
    void* flux_monte_carlo_create();
    void flux_monte_carlo_destroy(void* mc);
    void flux_monte_carlo_set_iterations(void* mc, int n);
    void flux_monte_carlo_add_param(void* mc, const char* name, double nominal, double tol);
    const char* flux_monte_carlo_run(void* mc, const char* circuit);
    
    // Optimizer
    void* flux_optimizer_create();
    void flux_optimizer_destroy(void* opt);
    void flux_optimizer_add_variable(void* opt, const char* name, double init, double min, double max);
    const char* flux_optimizer_run(void* opt, const char* circuit);
    
    // Worst-Case
    void* flux_worst_case_create();
    void flux_worst_case_destroy(void* wc);
    void flux_worst_case_add_component(void* wc, const char* name, double nominal, double tol);
    const char* flux_worst_case_analyze(void* wc, const char* circuit);
}

} // namespace AdvancedAnalysis
} // namespace Flux

#endif // FLUX_ADVANCED_ANALYSIS_H
