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

#ifndef FLUX_SENSITIVITY_H
#define FLUX_SENSITIVITY_H

#include <string>
#include <vector>
#include <map>
#include <complex>
#include <functional>

namespace Flux {
namespace Sensitivity {

// ============================================================================
// Sensitivity Data Structures
// ============================================================================

struct SensitivityResult {
    std::string outputVariable;
    std::string parameter;
    double absolute;    // Output/Parameter
    double relative;    // (Output/Output)/(Parameter/Parameter)
    double normalized;  // Normalized sensitivity
    std::string unit;
    
    SensitivityResult() : absolute(0), relative(0), normalized(0) {}
};

struct SensitivityReport {
    std::string circuitName;
    std::string outputVariable;
    std::vector<SensitivityResult> results;
    
    // Sorted by magnitude
    std::vector<SensitivityResult> mostSensitive;
    std::vector<SensitivityResult> leastSensitive;
    
    // Statistics
    double meanSensitivity;
    double maxSensitivity;
    double minSensitivity;
    
    std::string toString() const;
};

struct ToleranceAnalysisResult {
    std::string outputVariable;
    double nominal;
    double min;
    double max;
    double stdDev;
    std::vector<std::pair<std::string, double>> contributors;  // Parameter -> contribution %
    
    ToleranceAnalysisResult() : nominal(0), min(0), max(0), stdDev(0) {}
};

// ============================================================================
// Sensitivity Analyzer
// ============================================================================

class SensitivityAnalyzer {
public:
    SensitivityAnalyzer();
    ~SensitivityAnalyzer();
    
    // Load circuit
    void loadCircuit(const std::string& circuitFile);
    
    // Set analysis parameters
    void setOutputVariable(const std::string& var);
    void setParameters(const std::vector<std::string>& params);
    void setParameter(const std::string& param, double nominal, double tolerance);
    
    // Sensitivity computation methods
    enum class Method {
        FiniteDifference,
        Direct,
        Adjoint,
        MonteCarlo
    };
    
    void setMethod(Method method);
    
    // Run analysis
    SensitivityReport analyze();
    SensitivityReport analyzeDC();
    SensitivityReport analyzeAC(double frequency);
    SensitivityReport analyzeTransient(double time);
    
    // Specific sensitivities
    std::map<std::string, double> dcSensitivity(const std::string& output);
    std::map<std::string, std::complex<double>> acSensitivity(const std::string& output,
                                                                double frequency);
    std::map<std::string, double> transientSensitivity(const std::string& output,
                                                        double time);
    
    // Get results
    std::vector<SensitivityResult> getResults() const;
    SensitivityResult getSensitivity(const std::string& parameter) const;
    
    // Visualization data
    std::vector<double> getSensitivityValues() const;
    std::vector<std::string> getParameterNames() const;
    
private:
    std::string m_circuitFile;
    std::string m_outputVariable;
    std::vector<std::string> m_parameters;
    std::map<std::string, double> m_nominalValues;
    std::map<std::string, double> m_tolerances;
    Method m_method;
    
    std::vector<SensitivityResult> m_results;
    
    // Computation methods
    std::vector<SensitivityResult> computeFiniteDifference();
    std::vector<SensitivityResult> computeDirect();
    std::vector<SensitivityResult> computeAdjoint();
    
    // Helper
    double perturbParameter(const std::string& param, double delta);
    void restoreParameter(const std::string& param, double original);
};

// ============================================================================
// Tolerance Analyzer
// ============================================================================

class ToleranceAnalyzer {
public:
    ToleranceAnalyzer();
    ~ToleranceAnalyzer();
    
    // Load circuit and configuration
    void loadCircuit(const std::string& circuitFile);
    void setOutputVariable(const std::string& var);
    
    // Set component tolerances
    void setTolerance(const std::string& component, double tolerance);
    void setToleranceByType(const std::string& type, double tolerance);
    // Types: "resistor", "capacitor", "inductor", "voltage_source", etc.
    
    // Analysis methods
    enum class ToleranceMethod {
        RSS,          // Root Sum Square
        MonteCarlo,
        WorstCase,
        Interval
    };
    
    void setMethod(ToleranceMethod method);
    void setNumSamples(int n);  // For Monte Carlo
    
    // Run analysis
    ToleranceAnalysisResult analyze();
    
    // Worst-case analysis
    struct WorstCaseResult {
        std::string outputVariable;
        double nominal;
        double worstMin;
        double worstMax;
        std::map<std::string, double> worstCaseValues;  // Parameter -> worst-case value
    };
    
    WorstCaseResult analyzeWorstCase();
    
    // Statistical analysis
    struct StatisticalResult {
        std::string outputVariable;
        double mean;
        double stdDev;
        double skewness;
        double kurtosis;
        double percentile_5;
        double percentile_95;
        double percentile_99;
        std::vector<double> samples;
    };
    
    StatisticalResult analyzeStatistical();
    
    // Yield estimation
    double estimateYield(double specLower, double specUpper);
    
    // Contribution analysis
    std::map<std::string, double> getContributions() const;
    std::vector<std::string> getTopContributors(int n = 5) const;
    
private:
    std::string m_circuitFile;
    std::string m_outputVariable;
    std::map<std::string, double> m_tolerances;
    ToleranceMethod m_method;
    int m_numSamples;
    
    ToleranceAnalysisResult m_result;
};

// ============================================================================
// Parameter Optimization
// ============================================================================

class ParameterOptimizer {
public:
    ParameterOptimizer();
    ~ParameterOptimizer();
    
    // Load circuit
    void loadCircuit(const std::string& circuitFile);
    
    // Set optimization goal
    enum class Goal {
        Minimize,
        Maximize,
        Target
    };
    
    void setObjective(const std::string& expression, Goal goal);
    void setTarget(double target, double tolerance);
    
    // Set constraints
    void addConstraint(const std::string& expression, 
                       const std::string& type,  // "le", "ge", "eq"
                       double value);
    
    // Set parameter bounds
    void setParameterBounds(const std::string& param, double lower, double upper);
    
    // Optimization methods
    enum class Method {
        GradientDescent,
        Newton,
        BFGS,
        Genetic,
        ParticleSwarm,
        SimulatedAnnealing
    };
    
    void setMethod(Method method);
    void setMaxIterations(int maxIter);
    void setTolerance(double tol);
    
    // Run optimization
    struct OptimizationResult {
        bool success;
        std::map<std::string, double> optimalValues;
        double objectiveValue;
        int iterations;
        std::string message;
        std::vector<double> convergenceHistory;
    };
    
    OptimizationResult optimize();
    
    // Multi-objective optimization
    struct ParetoFront {
        std::vector<std::map<std::string, double>> solutions;
        std::vector<double> objectives;
    };
    
    ParetoFront optimizeMultiObjective(const std::vector<std::string>& objectives);
    
    // Sensitivity-guided optimization
    OptimizationResult optimizeWithSensitivity();
    
private:
    std::string m_circuitFile;
    std::string m_objective;
    Goal m_goal;
    double m_target;
    std::vector<std::string> m_constraints;
    std::map<std::string, std::pair<double, double>> m_bounds;
    Method m_method;
    int m_maxIterations;
    double m_tolerance;
};

// ============================================================================
// Corner Analyzer
// ============================================================================

class CornerAnalyzer {
public:
    CornerAnalyzer();
    ~CornerAnalyzer();
    
    // Load circuit
    void loadCircuit(const std::string& circuitFile);
    
    // Define corners
    void addCorner(const std::string& name, 
                   const std::map<std::string, double>& parameters);
    
    // Standard corners
    void useStandardCorners(const std::string& type);
    // Types: "process" (tt, ff, ss, fs, sf), "temperature", "voltage", "all"
    
    // Set parameter ranges
    void setParameterRange(const std::string& param, double min, double max);
    
    // Run corner analysis
    struct CornerResult {
        std::string cornerName;
        std::map<std::string, double> parameterValues;
        std::map<std::string, double> outputs;
        bool meetsSpec;
    };
    
    std::vector<CornerResult> analyze(const std::vector<std::string>& outputs);
    
    // Get results
    std::map<std::string, double> getWorstCase(const std::string& output);
    std::string getWorstCaseCorner(const std::string& output);
    
    // Margin analysis
    double getMargin(const std::string& output, double specLower, double specUpper);
    
private:
    std::string m_circuitFile;
    std::vector<std::pair<std::string, std::map<std::string, double>>> m_corners;
    std::vector<CornerResult> m_results;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Sensitivity analyzer
    void* flux_sens_create();
    void flux_sens_destroy(void* sens);
    void flux_sens_load(void* sens, const char* circuitFile);
    void flux_sens_set_output(void* sens, const char* output);
    void flux_sens_set_parameter(void* sens, const char* param, double nominal, double tol);
    void flux_sens_analyze(void* sens);
    const char* flux_sens_get_results(void* sens);
    double flux_sens_get_value(void* sens, const char* param);
    
    // Tolerance analyzer
    void* flux_tol_create();
    void flux_tol_destroy(void* tol);
    void flux_tol_load(void* tol, const char* circuitFile);
    void flux_tol_set_tolerance(void* tol, const char* component, double tol_value);
    void flux_tol_analyze(void* tol);
    double flux_tol_get_yield(void* tol, double spec_low, double spec_high);
    
    // Parameter optimizer
    void* flux_opt_create();
    void flux_opt_destroy(void* opt);
    void flux_opt_load(void* opt, const char* circuitFile);
    void flux_opt_set_objective(void* opt, const char* expr, const char* goal);
    void flux_opt_set_bounds(void* opt, const char* param, double lower, double upper);
    int flux_opt_optimize(void* opt);
    double flux_opt_get_value(void* opt, const char* param);
    
    // Corner analyzer
    void* flux_corner_create();
    void flux_corner_destroy(void* corner);
    void flux_corner_load(void* corner, const char* circuitFile);
    void flux_corner_use_standard(void* corner, const char* type);
    const char* flux_corner_analyze(void* corner);
    const char* flux_corner_get_worst(void* corner, const char* output);
}

} // namespace Sensitivity
} // namespace Flux

#endif // FLUX_SENSITIVITY_H
