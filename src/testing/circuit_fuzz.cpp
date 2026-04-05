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

// Circuit Fuzz Testing Implementation
#include "flux/testing/circuit_fuzz.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace Flux {
namespace FuzzTesting {

// ============================================================================
// FuzzSummary Report Generation
// ============================================================================

std::string FuzzSummary::toString() const {
    std::ostringstream oss;
    oss << "Fuzz Testing Summary\n";
    oss << "====================\n\n";
    oss << "Total Tests: " << totalTests << "\n";
    oss << "Passed:      " << passed << " (" << (passRate * 100) << "%)\n";
    oss << "Failed:      " << failed << "\n";
    oss << "Crashed:     " << crashed << "\n";
    oss << "Unstable:    " << unstable << "\n\n";
    oss << "Avg Time: " << avgExecutionTime << "ms\n";
    oss << "Worst Time: " << worstCaseTime << "ms\n";
    
    if (!failingInputs.empty()) {
        oss << "\nFailing Inputs:\n";
        for (const auto& input : failingInputs) {
            oss << "  - " << input << "\n";
        }
    }
    
    if (!recommendations.empty()) {
        oss << "\nRecommendations:\n";
        for (const auto& rec : recommendations) {
            oss << "  • " << rec << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// CircuitFuzzer Implementation
// ============================================================================

CircuitFuzzer::CircuitFuzzer() = default;
CircuitFuzzer::~CircuitFuzzer() = default;

void CircuitFuzzer::setConfig(const FuzzConfig& config) {
    m_config = config;
}

FuzzSummary CircuitFuzzer::runFuzzTest(const std::string& circuitId) {
    FuzzSummary summary;
    summary.totalTests = m_config.iterations;
    m_testCases.clear();
    
    std::mt19937 rng(42); // Fixed seed for reproducibility
    
    for (int i = 0; i < m_config.iterations; ++i) {
        // Generate random input
        auto inputs = generateRandomInput();
        
        // Run test
        FuzzTestCase testCase = runSingleTest(i, inputs);
        m_testCases.push_back(testCase);
        
        // Update summary
        summary.avgExecutionTime += testCase.executionTimeMs;
        summary.worstCaseTime = std::max(summary.worstCaseTime, testCase.executionTimeMs);
        
        switch (testCase.status) {
            case FuzzStatus::Passed:
                summary.passed++;
                break;
            case FuzzStatus::Failed:
                summary.failed++;
                summary.failingInputs.push_back(testCase.errorMessage);
                break;
            case FuzzStatus::Crashed:
                summary.crashed++;
                summary.failingInputs.push_back(testCase.errorMessage);
                break;
            case FuzzStatus::Unstable:
                summary.unstable++;
                summary.failingInputs.push_back(testCase.errorMessage);
                break;
            default:
                break;
        }
    }
    
    // Calculate averages
    summary.avgExecutionTime /= m_config.iterations;
    summary.passRate = (summary.totalTests > 0) ? 
        static_cast<double>(summary.passed) / summary.totalTests : 0.0;
    
    // Generate recommendations
    if (summary.passRate < 0.9) {
        summary.recommendations.push_back("Circuit has high failure rate - review stability margins");
    }
    if (summary.crashed > 0) {
        summary.recommendations.push_back("Circuit crashed during simulation - check for singularities");
    }
    if (summary.unstable > 0) {
        summary.recommendations.push_back("Circuit showed instability - check feedback loops");
    }
    
    return summary;
}

const FuzzTestCase& CircuitFuzzer::getTestCase(int index) const {
    return m_testCases[index];
}

std::vector<FuzzTestCase> CircuitFuzzer::getFailedCases() const {
    std::vector<FuzzTestCase> failed;
    for (const auto& tc : m_testCases) {
        if (tc.status != FuzzStatus::Passed) {
            failed.push_back(tc);
        }
    }
    return failed;
}

std::map<std::string, double> CircuitFuzzer::generateRandomInput() const {
    std::map<std::string, double> inputs;
    std::mt19937 rng(42 + m_testCases.size()); // Vary seed slightly
    
    for (const auto& bound : m_config.paramBounds) {
        const std::string& param = bound.first;
        double minVal = bound.second.first;
        double maxVal = bound.second.second;
        
        std::uniform_real_distribution<> dist(minVal, maxVal);
        inputs[param] = dist(rng);
    }
    
    return inputs;
}

std::map<std::string, double> CircuitFuzzer::mutateInputs(
    const std::map<std::string, double>& inputs) const {
    
    std::map<std::string, double> mutated = inputs;
    std::mt19937 rng(42);
    
    for (auto& pair : mutated) {
        std::uniform_real_distribution<> dist(0.0, 1.0);
        if (dist(rng) < m_config.mutationRate) {
            // Mutate this parameter
            double currentValue = pair.second;
            double magnitude = m_config.mutationMagnitude * currentValue;
            std::normal_distribution<> mutation(0.0, magnitude);
            pair.second += mutation(rng);
            
            // Clamp to bounds
            auto it = m_config.paramBounds.find(pair.first);
            if (it != m_config.paramBounds.end()) {
                pair.second = std::max(it->second.first, std::min(it->second.second, pair.second));
            }
        }
    }
    
    return mutated;
}

FuzzTestCase CircuitFuzzer::runSingleTest(int iteration, const std::map<std::string, double>& inputs) {
    FuzzTestCase testCase;
    testCase.iterationId = iteration;
    testCase.inputs = inputs;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Check timeout
        // (Simplified - real implementation would use timers)
        
        // Call simulation function
        if (m_config.simulate) {
            std::map<std::string, double> outputs;
            bool success = m_config.simulate(inputs, outputs);
            
            if (!success) {
                testCase.status = FuzzStatus::Failed;
                testCase.errorMessage = "Simulation function returned failure";
            } else {
                testCase.actualOutputs = outputs;
                testCase.status = checkStability(outputs);
            }
        } else {
            // Stub simulation
            testCase.status = FuzzStatus::Passed;
            testCase.actualOutputs = {{"Vout", inputs.count("Vin") ? inputs.at("Vin") * 0.5 : 0}};
        }
    } catch (const std::exception& e) {
        testCase.status = FuzzStatus::Crashed;
        testCase.errorMessage = std::string("Exception: ") + e.what();
    } catch (...) {
        testCase.status = FuzzStatus::Crashed;
        testCase.errorMessage = "Unknown exception";
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    testCase.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    return testCase;
}

FuzzStatus CircuitFuzzer::checkStability(const std::map<std::string, double>& outputs) const {
    // Check for NaN or Inf
    for (const auto& pair : outputs) {
        if (std::isnan(pair.second) || std::isinf(pair.second)) {
            return FuzzStatus::Unstable;
        }
    }
    return FuzzStatus::Passed;
}

// ============================================================================
// Convenience Functions
// ============================================================================

FuzzSummary circuit_fuzz_test(const std::string& circuitId, int iterations) {
    CircuitFuzzer fuzzer;
    FuzzConfig config;
    config.iterations = iterations;
    // Example bounds
    config.paramBounds["Vin"] = {0.0, 20.0};
    config.paramBounds["R1"] = {100.0, 100000.0};
    config.paramBounds["C1"] = {1e-12, 1e-3};
    
    fuzzer.setConfig(config);
    return fuzzer.runFuzzTest(circuitId);
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" {

void* flux_fuzz_create() {
    return new CircuitFuzzer();
}

void flux_fuzz_destroy(void* fuzz) {
    delete static_cast<CircuitFuzzer*>(fuzz);
}

void flux_fuzz_set_iterations(void* fuzz, int iterations) {
    auto* fuzzer = static_cast<CircuitFuzzer*>(fuzz);
    FuzzConfig config;
    config.iterations = iterations;
    fuzzer->setConfig(config);
}

void flux_fuzz_set_bounds(void* fuzz, const char* paramName, double min, double max) {
    auto* fuzzer = static_cast<CircuitFuzzer*>(fuzz);
    FuzzConfig config;
    // Note: This is a simplified setter that overwrites config
    // In production, you'd merge configs properly
    config.paramBounds[paramName ? paramName : ""] = {min, max};
    fuzzer->setConfig(config);
}

const char* flux_fuzz_run(void* fuzz, const char* circuitId) {
    static std::string result;
    auto* fuzzer = static_cast<CircuitFuzzer*>(fuzz);
    auto summary = fuzzer->runFuzzTest(circuitId ? circuitId : "unknown");
    result = summary.toString();
    return result.c_str();
}

}

} // namespace FuzzTesting
} // namespace Flux
