// Circuit Fuzz Testing Tests
#include <iostream>
#include <cassert>
#include <cmath>
#include "flux/testing/circuit_fuzz.h"

using namespace Flux::FuzzTesting;

void test_fuzz_basic() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #2: Circuit Fuzz Testing - Basic Tests          \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: Simple Fuzz Test
    std::cout << "Test 1: Simple Fuzz Test\n";
    std::cout << "\n";
    
    CircuitFuzzer fuzzer;
    FuzzConfig config;
    config.iterations = 10;
    config.paramBounds["Vin"] = {0.0, 5.0};
    config.paramBounds["R1"] = {1000.0, 10000.0};
    
    fuzzer.setConfig(config);
    
    auto summary = fuzzer.runFuzzTest("test_circuit");
    
    std::cout << "  Total tests: " << summary.totalTests << "\n";
    std::cout << "  Passed: " << summary.passed << "\n";
    std::cout << "  Pass rate: " << (summary.passRate * 100) << "%\n";
    
    assert(summary.totalTests == 10 && "Should run 10 iterations!");
    assert(summary.passed == 10 && "All should pass with default stub!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Random Input Generation
    std::cout << "\n\nTest 2: Random Input Generation\n";
    std::cout << "\n";
    
    auto inputs1 = fuzzer.generateRandomInput();
    auto inputs2 = fuzzer.generateRandomInput();
    
    std::cout << "  Generated input set 1:\n";
    for (const auto& pair : inputs1) {
        std::cout << "    " << pair.first << ": " << pair.second << "\n";
    }
    
    // Values should be different (with high probability)
    assert(inputs1["Vin"] >= 0.0 && inputs1["Vin"] <= 5.0 && "Vin should be in bounds!");
    assert(inputs1["R1"] >= 1000.0 && inputs1["R1"] <= 10000.0 && "R1 should be in bounds!");
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Mutation
    std::cout << "\n\nTest 3: Input Mutation\n";
    std::cout << "\n";
    
    std::map<std::string, double> original = {{"Vin", 2.5}, {"R1", 5000.0}};
    auto mutated = fuzzer.mutateInputs(original);
    
    std::cout << "  Original: Vin=" << original["Vin"] << ", R1=" << original["R1"] << "\n";
    std::cout << "  Mutated:  Vin=" << mutated["Vin"] << ", R1=" << mutated["R1"] << "\n";
    
    // Mutated values should be somewhat close to original (depending on magnitude)
    // This is a probabilistic test, but with fixed seed it should be deterministic
    assert(mutated.count("Vin") && "Should have Vin!");
    assert(mutated.count("R1") && "Should have R1!");
    
    std::cout << "\n Test 3 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Fuzz Testing: BASIC TESTS PASSED                   \n";
    std::cout << "\n";
}

void test_fuzz_crash_detection() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #2: Circuit Fuzz Testing - Crash Detection      \n";
    std::cout << "\n";
    std::cout << "\n";
    
    std::cout << "Test 1: Exception Handling\n";
    std::cout << "\n";
    
    CircuitFuzzer fuzzer;
    FuzzConfig config;
    config.iterations = 100;  // Increased iterations
    config.paramBounds["Vin"] = {0.0, 10.0};
    
    // Set a simulate function that throws
    config.simulate = [](const std::map<std::string, double>& inputs, 
                         std::map<std::string, double>& outputs) -> bool {
        // Lower threshold to ensure we hit it
        if (inputs.count("Vin") && inputs.at("Vin") > 5.0) {
            throw std::runtime_error("Voltage too high!");
        }
        outputs["Vout"] = inputs.at("Vin") * 0.5;
        return true;
    };
    
    fuzzer.setConfig(config);
    auto summary = fuzzer.runFuzzTest("crash_test");
    
    std::cout << "  Total: " << summary.totalTests << "\n";
    std::cout << "  Passed: " << summary.passed << "\n";
    std::cout << "  Crashed: " << summary.crashed << "\n";
    std::cout << "  Failed: " << summary.failed << "\n";
    
    assert(summary.crashed > 0 && "Should detect crashes!");
    assert(!summary.failingInputs.empty() && "Should record failing inputs!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Fuzz Testing: CRASH TESTS PASSED                   \n";
    std::cout << "\n";
}

void test_fuzz_stability() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #2: Circuit Fuzz Testing - Stability Checks     \n";
    std::cout << "\n";
    std::cout << "\n";
    
    std::cout << "Test 1: NaN/Inf Detection\n";
    std::cout << "\n";
    
    CircuitFuzzer fuzzer;
    FuzzConfig config;
    config.iterations = 100;  // Increased iterations
    config.paramBounds["Vin"] = {0.0, 10.0};
    
    // Set a simulate function that produces NaN
    config.simulate = [](const std::map<std::string, double>& inputs, 
                         std::map<std::string, double>& outputs) -> bool {
        // Lower threshold to ensure we hit it
        if (inputs.count("Vin") && inputs.at("Vin") > 5.0) {
            outputs["Vout"] = std::nan("");
            return true;
        }
        outputs["Vout"] = inputs.at("Vin") * 0.5;
        return true;
    };
    
    fuzzer.setConfig(config);
    auto summary = fuzzer.runFuzzTest("stability_test");
    
    std::cout << "  Total: " << summary.totalTests << "\n";
    std::cout << "  Passed: " << summary.passed << "\n";
    std::cout << "  Unstable: " << summary.unstable << "\n";
    
    assert(summary.unstable > 0 && "Should detect unstable outputs (NaN)!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Fuzz Testing: STABILITY TESTS PASSED               \n";
    std::cout << "\n";
}

void test_fuzz_report() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #2: Circuit Fuzz Testing - Reports              \n";
    std::cout << "\n";
    std::cout << "\n";
    
    std::cout << "Test 1: Report Generation\n";
    std::cout << "\n";
    
    auto summary = circuit_fuzz_test("my_circuit", 50);
    
    std::string report = summary.toString();
    std::cout << report;
    
    assert(report.find("Fuzz Testing Summary") != std::string::npos && "Should have header!");
    assert(report.find("Total Tests:") != std::string::npos && "Should have stats!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Fuzz Testing: REPORT TESTS PASSED                  \n";
    std::cout << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          Circuit Fuzz Testing - Test Suite               \n";
    std::cout << "\n";
    
    try {
        test_fuzz_basic();
        test_fuzz_crash_detection();
        test_fuzz_stability();
        test_fuzz_report();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL FUZZ TESTS PASSED                         \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
