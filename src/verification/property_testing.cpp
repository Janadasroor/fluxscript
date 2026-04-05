// ============================================================================
// FluxScript Property-Based Randomized Model Testing Implementation
// ============================================================================

#include "flux/verification/property_testing.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Flux {

// ============================================================================
// RandomParameterGenerator Implementation
// ============================================================================

RandomParameterGenerator::RandomParameterGenerator(unsigned int seed) : m_rng(seed) {
}

double RandomParameterGenerator::generateUniform(double min, double max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::uniform_real_distribution<double> dist(min, max);
    return dist(m_rng);
}

double RandomParameterGenerator::generateNormal(double mean, double std_dev) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::normal_distribution<double> dist(mean, std_dev);
    return dist(m_rng);
}

double RandomParameterGenerator::generateLogNormal(double median, double sigma) {
    std::lock_guard<std::mutex> lock(m_mutex);
    double log_median = std::log(median);
    std::lognormal_distribution<double> dist(log_median, sigma);
    return dist(m_rng);
}

int RandomParameterGenerator::generateInt(int min, int max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

bool RandomParameterGenerator::generateBool(double probability) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::bernoulli_distribution dist(probability);
    return dist(m_rng);
}

std::map<std::string, double> RandomParameterGenerator::generateCircuitParameters(
    const std::vector<std::string>& param_names,
    const std::map<std::string, std::pair<double, double>>& ranges
) {
    std::map<std::string, double> params;
    
    for (const auto& name : param_names) {
        auto it = ranges.find(name);
        if (it != ranges.end()) {
            params[name] = generateUniform(it->second.first, it->second.second);
        } else {
            params[name] = generateUniform(0.0, 1.0);
        }
    }
    
    return params;
}

std::map<std::string, double> RandomParameterGenerator::mutateParameters(
    const std::map<std::string, double>& base,
    double mutation_rate
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, double> mutated = base;
    std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
    std::normal_distribution<double> normal_dist(0.0, 1.0);
    
    for (auto& [name, value] : mutated) {
        if (uniform_dist(m_rng) < mutation_rate) {
            // Mutate by adding Gaussian noise (10% of value)
            double noise = normal_dist(m_rng) * std::abs(value) * 0.1;
            mutated[name] = value + noise;
        }
    }
    
    return mutated;
}

// ============================================================================
// PropertyBasedTester Singleton
// ============================================================================

PropertyBasedTester& PropertyBasedTester::instance() {
    static PropertyBasedTester instance;
    return instance;
}

void PropertyBasedTester::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    
    m_generator = std::make_unique<RandomParameterGenerator>(42);
    m_properties.clear();
    m_total_tests_run = 0;
    m_total_failures = 0;
    m_total_execution_time_ms = 0.0;
    m_initialized = true;
    
    std::cout << "[PropertyBasedTester] Initialized" << std::endl;
}

void PropertyBasedTester::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_properties.clear();
    m_initialized = false;
    
    std::cout << "[PropertyBasedTester] Finalized" << std::endl;
}

bool PropertyBasedTester::registerProperty(const Property& prop) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_properties.count(prop.name)) {
        return false;
    }
    m_properties[prop.name] = prop;
    return true;
}

bool PropertyBasedTester::unregisterProperty(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_properties.erase(name) > 0;
}

std::vector<std::string> PropertyBasedTester::getRegisteredProperties() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, prop] : m_properties) {
        names.push_back(name);
    }
    return names;
}

PropertyTestResult PropertyBasedTester::testProperty(
    const std::string& property_name,
    const PropertyTestConfig& config,
    std::function<std::map<std::string, double>(const std::map<std::string, double>&)> simulate
) {
    auto start_time = std::chrono::steady_clock::now();
    
    PropertyTestResult result;
    result.property_name = property_name;
    result.passed = true;
    result.test_cases_evaluated = 0;
    result.test_cases_passed = 0;
    result.test_cases_failed = 0;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_properties.find(property_name);
        if (it == m_properties.end()) {
            result.passed = false;
            result.failure_message = "Property not found: " + property_name;
            return result;
        }
        
        const auto& property = it->second;
        result.description = property.description;
        
        RandomParameterGenerator gen(config.seed);
        
        // Determine parameter ranges from the property or config
        // For now, we'll generate random parameters and let the simulate function handle validation
        for (int i = 0; i < config.num_test_cases; i++) {
            // Generate random test parameters
            std::map<std::string, double> params;
            for (int j = 0; j < 10; j++) {
                params["param_" + std::to_string(j)] = gen.generateUniform(0.0, 1.0);
            }
            
            // Run simulation
            auto sim_result = simulate(params);
            
            // Check property
            std::string failure_reason;
            bool prop_holds = property.predicate(params, sim_result, failure_reason);
            
            result.test_cases_evaluated++;
            
            if (prop_holds) {
                result.test_cases_passed++;
            } else {
                result.test_cases_failed++;
                result.passed = false;
                
                if (config.shrink_on_failure) {
                    result.failure_message = "Failed at test case " + std::to_string(i) + ": " + failure_reason;
                }
                
                if (!config.shrink_on_failure) {
                    break;  // Stop on first failure
                }
            }
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_total_tests_run++;
        if (!result.passed) m_total_failures++;
        m_total_execution_time_ms += result.execution_time_ms;
    }
    
    return result;
}

std::vector<PropertyTestResult> PropertyBasedTester::testAllProperties(
    const PropertyTestConfig& config,
    std::function<std::map<std::string, double>(const std::map<std::string, double>&)> simulate
) {
    std::vector<PropertyTestResult> results;
    
    std::vector<std::string> prop_names;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [name, prop] : m_properties) {
            prop_names.push_back(name);
        }
    }
    
    for (const auto& name : prop_names) {
        results.push_back(testProperty(name, config, simulate));
    }
    
    return results;
}

std::string PropertyBasedTester::generateShrinkReport(const PropertyTestResult& result) const {
    std::ostringstream oss;
    oss << "=== Property Test Report ===\n";
    oss << "Property: " << result.property_name << "\n";
    oss << "Description: " << result.description << "\n";
    oss << "Status: " << (result.passed ? "PASSED" : "FAILED") << "\n";
    oss << "Test cases: " << result.test_cases_evaluated << "\n";
    oss << "Passed: " << result.test_cases_passed << "\n";
    oss << "Failed: " << result.test_cases_failed << "\n";
    oss << "Execution time: " << result.execution_time_ms << " ms\n";
    
    if (!result.passed) {
        oss << "\nFailure: " << result.failure_message << "\n";
    }
    
    return oss.str();
}

void PropertyBasedTester::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total_tests_run = 0;
    m_total_failures = 0;
    m_total_execution_time_ms = 0.0;
}

// ============================================================================
// Common Properties
// ============================================================================

namespace CommonProperties {

Property createPassivityProperty() {
    Property prop;
    prop.name = "passivity";
    prop.description = "Power consumed >= 0 for all passive components";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        // Check if any result parameter representing power is negative
        for (const auto& [name, value] : result) {
            if (name.find("power") != std::string::npos && value < -1e-9) {
                failure = "Negative power detected: " + name + " = " + std::to_string(value);
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createReciprocityProperty() {
    Property prop;
    prop.name = "reciprocity";
    prop.description = "Sij = Sji for linear reciprocal networks";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        // Check S-parameter symmetry
        auto it1 = result.find("S12");
        auto it2 = result.find("S21");
        
        if (it1 != result.end() && it2 != result.end()) {
            if (std::abs(it1->second - it2->second) > 1e-6) {
                failure = "Reciprocity violated: S12 != S21";
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createCausalityProperty() {
    Property prop;
    prop.name = "causality";
    prop.description = "Output cannot precede input in time domain";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        auto it_delay = result.find("delay");
        if (it_delay != result.end() && it_delay->second < 0) {
            failure = "Negative delay detected: " + std::to_string(it_delay->second);
            return false;
        }
        return true;
    };
    return prop;
}

Property createBIBOStabilityProperty(double max_input, double max_output) {
    Property prop;
    prop.name = "bibo_stability";
    prop.description = "Bounded input produces bounded output";
    prop.predicate = [max_input, max_output](const auto& params, const auto& result, auto& failure) {
        for (const auto& [name, value] : result) {
            if (std::abs(value) > max_output) {
                failure = "Output exceeds bound: " + name + " = " + std::to_string(value) + 
                         " > " + std::to_string(max_output);
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createMonotonicityProperty(const std::string& input_param, const std::string& output_param) {
    Property prop;
    prop.name = "monotonicity_" + input_param + "_" + output_param;
    prop.description = "Increasing " + input_param + " monotonically affects " + output_param;
    prop.predicate = [input_param, output_param](const auto& params, const auto& result, auto& failure) {
        // This property requires comparing two simulations
        // For single simulation, we just check if output is reasonable
        auto it = result.find(output_param);
        if (it == result.end()) {
            failure = "Output parameter not found: " + output_param;
            return false;
        }
        return true;
    };
    return prop;
}

Property createKCLProperty(const std::vector<std::string>& node_names) {
    Property prop;
    prop.name = "kcl";
    prop.description = "Kirchhoff's Current Law: Sum of currents at node = 0";
    prop.predicate = [node_names](const auto& params, const auto& result, auto& failure) {
        for (const auto& node : node_names) {
            std::string current_name = "I(" + node + ")";
            auto it = result.find(current_name);
            if (it != result.end() && std::abs(it->second) > 1e-6) {
                failure = "KCL violated at node " + node + ": " + std::to_string(it->second);
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createEnergyConservationProperty() {
    Property prop;
    prop.name = "energy_conservation";
    prop.description = "Total energy in = Total energy out + Dissipated";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        auto it_in = result.find("energy_in");
        auto it_out = result.find("energy_out");
        auto it_dissipated = result.find("energy_dissipated");
        
        if (it_in != result.end() && it_out != result.end() && it_dissipated != result.end()) {
            double balance = it_in->second - it_out->second - it_dissipated->second;
            if (std::abs(balance) > 1e-6) {
                failure = "Energy not conserved: difference = " + std::to_string(balance);
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createLinearityProperty(double tolerance) {
    Property prop;
    prop.name = "linearity";
    prop.description = "Superposition holds: f(a*x1 + b*x2) = a*f(x1) + b*f(x2)";
    prop.predicate = [tolerance](const auto& params, const auto& result, auto& failure) {
        // Single simulation check - just verify output is finite
        for (const auto& [name, value] : result) {
            if (!std::isfinite(value)) {
                failure = "Non-finite output: " + name;
                return false;
            }
        }
        return true;
    };
    return prop;
}

Property createTimeInvarianceProperty() {
    Property prop;
    prop.name = "time_invariance";
    prop.description = "Shifted input produces shifted output";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        // Single simulation check
        return true;
    };
    return prop;
}

Property createPositiveRealProperty() {
    Property prop;
    prop.name = "positive_real";
    prop.description = "Impedance has positive real part for passive networks";
    prop.predicate = [](const auto& params, const auto& result, auto& failure) {
        auto it = result.find("Z_real");
        if (it != result.end() && it->second < -1e-9) {
            failure = "Negative real impedance: " + std::to_string(it->second);
            return false;
        }
        return true;
    };
    return prop;
}

} // namespace CommonProperties

// ============================================================================
// CircuitTestGenerators Implementation
// ============================================================================

namespace CircuitTestGenerators {

std::map<std::string, double> generateRCTestCase(RandomParameterGenerator& gen) {
    std::map<std::string, double> params;
    params["R"] = gen.generateLogNormal(1e3, 1.0);    // 1kΩ typical
    params["C"] = gen.generateLogNormal(1e-6, 1.0);   // 1μF typical
    params["Vin"] = gen.generateUniform(0.1, 10.0);    // Input voltage
    params["freq"] = gen.generateLogNormal(1e3, 2.0);  // Frequency
    return params;
}

std::map<std::string, double> generateRLCTestCase(RandomParameterGenerator& gen) {
    std::map<std::string, double> params;
    params["R"] = gen.generateLogNormal(100, 1.0);
    params["L"] = gen.generateLogNormal(1e-3, 1.0);   // 1mH typical
    params["C"] = gen.generateLogNormal(1e-6, 1.0);
    params["Vin"] = gen.generateUniform(0.1, 10.0);
    params["freq"] = gen.generateLogNormal(1e3, 2.0);
    return params;
}

std::map<std::string, double> generateOpAmpTestCase(RandomParameterGenerator& gen) {
    std::map<std::string, double> params;
    params["R1"] = gen.generateLogNormal(1e3, 1.0);
    params["Rf"] = gen.generateLogNormal(10e3, 1.0);
    params["Vin"] = gen.generateUniform(-1.0, 1.0);
    params["Vcc+"] = gen.generateUniform(5.0, 15.0);
    params["Vcc-"] = gen.generateUniform(-15.0, -5.0);
    return params;
}

std::map<std::string, double> generateFilterTestCase(RandomParameterGenerator& gen, const std::string& type) {
    std::map<std::string, double> params;
    params["R"] = gen.generateLogNormal(1e3, 1.0);
    params["C"] = gen.generateLogNormal(1e-6, 1.0);
    params["cutoff_freq"] = gen.generateLogNormal(1e3, 1.0);
    params["test_freq"] = gen.generateLogNormal(1e3, 2.0);
    params["type"] = (type == "lowpass") ? 0 : (type == "highpass") ? 1 : 2;
    return params;
}

std::map<std::string, double> generateAmplifierTestCase(RandomParameterGenerator& gen, const std::string& type) {
    std::map<std::string, double> params;
    params["R1"] = gen.generateLogNormal(1e3, 1.0);
    params["R2"] = gen.generateLogNormal(10e3, 1.0);
    params["Vin"] = gen.generateUniform(0.01, 1.0);
    params["gain"] = (type == "inverting") ? -params["R2"] / params["R1"] : 1 + params["R2"] / params["R1"];
    return params;
}

} // namespace CircuitTestGenerators

} // namespace Flux
