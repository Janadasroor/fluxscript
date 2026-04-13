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

// ============================================================================
// FluxScript Cross-Simulator Validation Layer Implementation
// ============================================================================

#include "flux/verification/cross_simulator_validation.h"
#include "flux/runtime/ngspice_bridge.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <numeric>

namespace Flux {

// ============================================================================
// CrossSimulatorValidator Singleton
// ============================================================================

CrossSimulatorValidator& CrossSimulatorValidator::instance() {
    static CrossSimulatorValidator instance;
    return instance;
}

void CrossSimulatorValidator::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    
    m_reference_simulator = "ngspice";
    m_total_validations = 0;
    m_passed_validations = 0;
    m_reference_cache.clear();
    m_initialized = true;
    
    std::cout << "[CrossSimulatorValidator] Initialized" << std::endl;
}

void CrossSimulatorValidator::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reference_cache.clear();
    m_initialized = false;
    
    std::cout << "[CrossSimulatorValidator] Finalized" << std::endl;
}

void CrossSimulatorValidator::setReferenceSimulator(const std::string& simulator_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reference_simulator = simulator_name;
}

CircuitValidationResult CrossSimulatorValidator::validateCircuit(
    const ValidationConfig& config,
    const std::string& fluxscript_netlist,
    const std::string& reference_netlist
) {
    auto start_time = std::chrono::steady_clock::now();
    
    CircuitValidationResult result;
    result.circuit_name = config.circuit_name;
    result.reference_simulator = config.reference_simulator;
    result.overall_passed = true;
    result.max_absolute_error = 0.0;
    result.max_relative_error = 0.0;
    result.avg_absolute_error = 0.0;
    result.avg_relative_error = 0.0;
    
    // Run FluxScript simulation
    std::map<std::string, double> fluxscript_results;
    try {
        fluxscript_results = SimulationHelpers::runFluxScriptSimulation(
            fluxscript_netlist,
            config.simulation_time,
            config.time_step
        );
    } catch (const std::exception& e) {
        result.overall_passed = false;
        result.notes = "FluxScript simulation failed: " + std::string(e.what());
        return result;
    }
    
    // Run reference simulation (check cache first)
    std::map<std::string, double> reference_results;
    std::string cache_key = config.circuit_name + "_" + config.reference_simulator;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_reference_cache.find(cache_key);
        if (it != m_reference_cache.end()) {
            reference_results = it->second;
        }
    }
    
    if (reference_results.empty()) {
        try {
            if (config.reference_simulator == "ngspice") {
                reference_results = SimulationHelpers::runNgspiceSimulation(
                    reference_netlist,
                    config.simulation_time,
                    config.time_step
                );
            } else {
                result.notes = "Unsupported reference simulator: " + config.reference_simulator;
                result.overall_passed = false;
                return result;
            }
            
            // Cache the result
            cacheReferenceResult(cache_key, reference_results);
        } catch (const std::exception& e) {
            result.overall_passed = false;
            result.notes = "Reference simulation failed: " + std::string(e.what());
            return result;
        }
    }
    
    // Compare results
    result.comparisons = compareMeasurements(
        fluxscript_results,
        reference_results,
        config.absolute_tolerance,
        config.relative_tolerance
    );
    
    // Calculate statistics
    double sum_abs_error = 0.0;
    double sum_rel_error = 0.0;
    int count = 0;
    
    for (const auto& comp : result.comparisons) {
        if (!comp.passed) {
            result.overall_passed = false;
        }
        
        sum_abs_error += comp.absolute_error;
        sum_rel_error += comp.relative_error;
        count++;
        
        result.max_absolute_error = std::max(result.max_absolute_error, comp.absolute_error);
        result.max_relative_error = std::max(result.max_relative_error, comp.relative_error);
    }
    
    if (count > 0) {
        result.avg_absolute_error = sum_abs_error / count;
        result.avg_relative_error = sum_rel_error / count;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_total_validations++;
        if (result.overall_passed) m_passed_validations++;
    }
    
    return result;
}

std::vector<MeasurementComparison> CrossSimulatorValidator::compareMeasurements(
    const std::map<std::string, double>& fluxscript_results,
    const std::map<std::string, double>& reference_results,
    double abs_tol,
    double rel_tol
) {
    std::vector<MeasurementComparison> comparisons;
    
    // Compare all measurements present in both results
    for (const auto& [name, fs_value] : fluxscript_results) {
        auto it = reference_results.find(name);
        if (it == reference_results.end()) {
            continue;  // Skip if not in reference
        }
        
        double ref_value = it->second;
        double abs_error = std::abs(fs_value - ref_value);
        double rel_error = (std::abs(ref_value) > 1e-12) ? (abs_error / std::abs(ref_value)) : abs_error;
        
        MeasurementComparison comp;
        comp.name = name;
        comp.fluxscript_value = fs_value;
        comp.reference_value = ref_value;
        comp.absolute_error = abs_error;
        comp.relative_error = rel_error;
        
        if (abs_error < abs_tol || rel_error < rel_tol) {
            comp.passed = true;
            if (abs_error < abs_tol * 0.1 && rel_error < rel_tol * 0.1) {
                comp.status = "MATCH";
            } else {
                comp.status = "CLOSE";
            }
        } else {
            comp.passed = false;
            comp.status = "DIFFER";
        }
        
        comparisons.push_back(comp);
    }
    
    return comparisons;
}

std::string CrossSimulatorValidator::generateReport(const CircuitValidationResult& result) const {
    std::ostringstream oss;
    
    oss << "==================================================\n";
    oss << "  Cross-Simulator Validation Report\n";
    oss << "==================================================\n\n";
    
    oss << "Circuit: " << result.circuit_name << "\n";
    oss << "Reference Simulator: " << result.reference_simulator << "\n";
    oss << "Overall Result: " << (result.overall_passed ? "PASSED " : "FAILED ") << "\n";
    oss << "Execution Time: " << result.execution_time_ms << " ms\n\n";
    
    oss << "--- Measurement Comparisons ---\n";
    oss << std::left << std::setw(20) << "Name"
        << std::right << std::setw(12) << "FluxScript"
        << std::setw(12) << "Reference"
        << std::setw(12) << "Abs Error"
        << std::setw(12) << "Rel Error"
        << std::setw(10) << "Status" << "\n";
    oss << std::string(78, '-') << "\n";
    
    for (const auto& comp : result.comparisons) {
        oss << std::left << std::setw(20) << comp.name
            << std::right << std::setw(12) << std::scientific << std::setprecision(4) << comp.fluxscript_value
            << std::setw(12) << comp.reference_value
            << std::setw(12) << comp.absolute_error
            << std::setw(12) << comp.relative_error
            << std::setw(10) << comp.status << "\n";
    }
    
    oss << "\n--- Summary Statistics ---\n";
    oss << "Max Absolute Error: " << std::scientific << std::setprecision(6) << result.max_absolute_error << "\n";
    oss << "Max Relative Error: " << result.max_relative_error << "\n";
    oss << "Avg Absolute Error: " << result.avg_absolute_error << "\n";
    oss << "Avg Relative Error: " << result.avg_relative_error << "\n";
    
    if (!result.notes.empty()) {
        oss << "\nNotes: " << result.notes << "\n";
    }
    
    oss << "\n==================================================\n";
    
    return oss.str();
}

bool CrossSimulatorValidator::exportReport(const CircuitValidationResult& result, const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    file << generateReport(result);
    file.close();
    
    std::cout << "[CrossSimulatorValidator] Report exported to: " << filename << std::endl;
    return true;
}

std::vector<CircuitValidationResult> CrossSimulatorValidator::validateCircuitBatch(
    const std::vector<ValidationConfig>& configs,
    const std::vector<std::string>& fluxscript_netlists,
    const std::vector<std::string>& reference_netlists
) {
    std::vector<CircuitValidationResult> results;
    
    for (size_t i = 0; i < configs.size(); i++) {
        if (i >= fluxscript_netlists.size() || i >= reference_netlists.size()) {
            break;
        }
        
        results.push_back(validateCircuit(
            configs[i],
            fluxscript_netlists[i],
            reference_netlists[i]
        ));
    }
    
    return results;
}

double CrossSimulatorValidator::getPassRate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_total_validations == 0) return 0.0;
    return (double)m_passed_validations / m_total_validations * 100.0;
}

void CrossSimulatorValidator::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total_validations = 0;
    m_passed_validations = 0;
}

bool CrossSimulatorValidator::cacheReferenceResult(
    const std::string& circuit_key,
    const std::map<std::string, double>& result
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reference_cache[circuit_key] = result;
    return true;
}

const std::map<std::string, double>* CrossSimulatorValidator::getCachedReference(
    const std::string& circuit_key
) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_reference_cache.find(circuit_key);
    if (it == m_reference_cache.end()) return nullptr;
    return &it->second;
}

// ============================================================================
// SimulationHelpers Implementation
// ============================================================================

namespace SimulationHelpers {

std::map<std::string, double> runFluxScriptSimulation(
    const std::string& netlist,
    double sim_time,
    double time_step
) {
    std::map<std::string, double> results;
    
    // Initialize ngspice with netlist
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        throw std::runtime_error("Failed to initialize FluxScript simulation");
    }
    
    // Run transient simulation
    rc = flux_ngspice_run_transient(0.0, sim_time, time_step);
    if (rc != 0) {
        throw std::runtime_error("FluxScript transient simulation failed");
    }
    
    // Extract results
    auto vectors = flux_ngspice_get_vector_names();
    for (const auto& vec_name : vectors) {
        auto vec_data = flux_ngspice_extract_vector(vec_name.c_str());
        if (!vec_data.empty()) {
            // Use last value as measurement
            results[vec_name] = vec_data.back();
        }
    }
    
    return results;
}

std::map<std::string, double> runNgspiceSimulation(
    const std::string& netlist,
    double sim_time,
    double time_step
) {
    // For now, use the same ngspice bridge
    // In production, this would run ngspice separately and parse output
    
    std::map<std::string, double> results;
    
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        throw std::runtime_error("Failed to initialize ngspice simulation");
    }
    
    rc = flux_ngspice_run_transient(0.0, sim_time, time_step);
    if (rc != 0) {
        throw std::runtime_error("ngspice transient simulation failed");
    }
    
    auto vectors = flux_ngspice_get_vector_names();
    for (const auto& vec_name : vectors) {
        auto vec_data = flux_ngspice_extract_vector(vec_name.c_str());
        if (!vec_data.empty()) {
            results[vec_name] = vec_data.back();
        }
    }
    
    return results;
}

std::map<std::string, double> extractMeasurements(
    const std::vector<double>& time,
    const std::vector<double>& voltage,
    const std::vector<std::string>& measurement_names
) {
    std::map<std::string, double> measurements;
    
    if (time.empty() || voltage.empty()) {
        return measurements;
    }
    
    // Calculate common measurements
    double max_val = *std::max_element(voltage.begin(), voltage.end());
    double min_val = *std::min_element(voltage.begin(), voltage.end());
    double avg_val = std::accumulate(voltage.begin(), voltage.end(), 0.0) / voltage.size();
    
    double sum_sq = 0.0;
    for (double v : voltage) sum_sq += v * v;
    double rms_val = std::sqrt(sum_sq / voltage.size());
    
    measurements["max"] = max_val;
    measurements["min"] = min_val;
    measurements["avg"] = avg_val;
    measurements["rms"] = rms_val;
    measurements["peak_to_peak"] = max_val - min_val;
    measurements["final_value"] = voltage.back();
    measurements["simulation_time"] = time.back();
    
    return measurements;
}

} // namespace SimulationHelpers

} // namespace Flux
