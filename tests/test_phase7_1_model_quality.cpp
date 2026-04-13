// FluxScript Phase 7.1: Model Quality & Verification Tests
// Tests for Property-Based Testing and Cross-Simulator Validation

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <cassert>
#include <map>

#include "flux/verification/property_testing.h"
#include "flux/verification/cross_simulator_validation.h"

using namespace Flux;

// ============================================================================
// Test #1: Property-Based Randomized Testing
// ============================================================================

void test_property_based_testing() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #1: Property-Based Randomized Testing             \n";
    std::cout << "\n";

    auto& tester = PropertyBasedTester::instance();
    tester.initialize();

    // Test 1.1: Register common properties
    std::cout << "\nTest 1.1: Property Registration\n";
    std::cout << "\n";

    assert(tester.registerProperty(CommonProperties::createPassivityProperty()) && "Failed to register passivity!");
    assert(tester.registerProperty(CommonProperties::createCausalityProperty()) && "Failed to register causality!");
    assert(tester.registerProperty(CommonProperties::createPositiveRealProperty()) && "Failed to register positive real!");
    
    auto props = tester.getRegisteredProperties();
    std::cout << "  Registered properties: " << props.size() << "\n";
    for (const auto& name : props) {
        std::cout << "    - " << name << "\n";
    }
    
    assert(props.size() >= 3 && "Should have at least 3 properties!");
    
    std::cout << "\n Test 1.1 PASSED\n";

    // Test 1.2: Run property tests with mock simulation
    std::cout << "\n\nTest 1.2: Property Test Execution\n";
    std::cout << "\n";

    PropertyTestConfig config;
    config.test_name = "mock_test";
    config.num_test_cases = 50;
    config.seed = 12345;
    
    // Mock simulation function
    auto mock_simulate = [](const std::map<std::string, double>& params) {
        std::map<std::string, double> result;
        result["power"] = 1.0;  // Positive power
        result["delay"] = 1e-6; // Positive delay
        result["Z_real"] = 50.0; // Positive real impedance
        return result;
    };
    
    auto result = tester.testProperty("passivity", config, mock_simulate);
    
    std::cout << "  Property: " << result.property_name << "\n";
    std::cout << "  Status: " << (result.passed ? "PASSED" : "FAILED") << "\n";
    std::cout << "  Test cases: " << result.test_cases_evaluated << "\n";
    std::cout << "  Passed: " << result.test_cases_passed << "\n";
    std::cout << "  Failed: " << result.test_cases_failed << "\n";
    std::cout << "  Execution time: " << std::setprecision(2) << result.execution_time_ms << " ms\n";
    
    assert(result.passed && "Passivity test should pass!");
    assert(result.test_cases_evaluated == 50 && "Should evaluate 50 test cases!");
    assert(result.test_cases_passed == 50 && "All should pass!");
    
    std::cout << "\n Test 1.2 PASSED\n";

    // Test 1.3: Test failure detection
    std::cout << "\n\nTest 1.3: Failure Detection\n";
    std::cout << "\n";

    auto mock_simulate_fail = [](const std::map<std::string, double>& params) {
        std::map<std::string, double> result;
        result["power"] = -1.0;  // Negative power (should fail passivity)
        return result;
    };
    
    auto fail_result = tester.testProperty("passivity", config, mock_simulate_fail);
    
    std::cout << "  Property: " << fail_result.property_name << "\n";
    std::cout << "  Status: " << (fail_result.passed ? "PASSED" : "FAILED (expected)") << "\n";
    std::cout << "  Test cases: " << fail_result.test_cases_evaluated << "\n";
    std::cout << "  Failed: " << fail_result.test_cases_failed << "\n";
    
    assert(!fail_result.passed && "Should detect passivity violation!");
    assert(fail_result.test_cases_failed > 0 && "Should have failures!");
    
    std::cout << "\n Test 1.3 PASSED\n";

    // Test 1.4: Statistics
    std::cout << "\n\nTest 1.4: Statistics\n";
    std::cout << "\n";

    std::cout << "  Total tests run: " << tester.getTotalTestsRun() << "\n";
    std::cout << "  Total failures: " << tester.getTotalFailures() << "\n";
    std::cout << "  Total execution time: " << std::setprecision(2) << tester.getTotalExecutionTime() << " ms\n";
    
    assert(tester.getTotalTestsRun() >= 2 && "Should have run at least 2 tests!");
    
    std::cout << "\n Test 1.4 PASSED\n";

    tester.finalize();

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #1: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Test #2: Cross-Simulator Validation
// ============================================================================

void test_cross_simulator_validation() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #2: Cross-Simulator Validation                    \n";
    std::cout << "\n";

    auto& validator = CrossSimulatorValidator::instance();
    validator.initialize();

    // Test 2.1: Measurement comparison
    std::cout << "\nTest 2.1: Measurement Comparison\n";
    std::cout << "\n";

    std::map<std::string, double> fluxscript_results = {
        {"V(out)", 3.298},
        {"I(R1)", 0.001234},
        {"power", 0.004067},
        {"gain", 3.298}
    };
    
    std::map<std::string, double> reference_results = {
        {"V(out)", 3.300},
        {"I(R1)", 0.001235},
        {"power", 0.004068},
        {"gain", 3.300}
    };
    
    auto comparisons = validator.compareMeasurements(
        fluxscript_results,
        reference_results,
        1e-3,  // Absolute tolerance
        1e-3   // Relative tolerance
    );
    
    std::cout << "  Comparisons: " << comparisons.size() << "\n";
    
    for (const auto& comp : comparisons) {
        std::cout << "  " << comp.name << ": ";
        std::cout << "FS=" << std::setprecision(4) << comp.fluxscript_value << ", ";
        std::cout << "Ref=" << comp.reference_value << ", ";
        std::cout << "RelErr=" << std::setprecision(6) << comp.relative_error << " ";
        std::cout << "[" << comp.status << "]\n";
    }
    
    assert(comparisons.size() == 4 && "Should have 4 comparisons!");
    
    // Check that most comparisons passed
    int passed_count = 0;
    for (const auto& comp : comparisons) {
        if (comp.passed) passed_count++;
    }
    
    std::cout << "  Passed: " << passed_count << "/" << comparisons.size() << "\n";
    assert(passed_count >= 3 && "Most comparisons should pass!");
    
    std::cout << "\n Test 2.1 PASSED\n";

    // Test 2.2: Report generation
    std::cout << "\n\nTest 2.2: Report Generation\n";
    std::cout << "\n";

    CircuitValidationResult validation_result;
    validation_result.circuit_name = "RC Low-Pass Filter";
    validation_result.reference_simulator = "ngspice";
    validation_result.overall_passed = true;
    validation_result.comparisons = comparisons;
    validation_result.max_absolute_error = 0.002;
    validation_result.max_relative_error = 0.0006;
    validation_result.avg_absolute_error = 0.001;
    validation_result.avg_relative_error = 0.0003;
    validation_result.execution_time_ms = 125.5;
    
    std::string report = validator.generateReport(validation_result);
    
    std::cout << "  Report generated: " << report.length() << " bytes\n";
    std::cout << "  First line: " << report.substr(0, report.find('\n')) << "\n";
    
    assert(report.length() > 100 && "Report should be substantial!");
    assert(report.find("RC Low-Pass Filter") != std::string::npos && "Report should contain circuit name!");
    assert(report.find("PASSED") != std::string::npos && "Report should show pass status!");
    
    std::cout << "\n Test 2.2 PASSED\n";

    // Test 2.3: Reference caching
    std::cout << "\n\nTest 2.3: Reference Caching\n";
    std::cout << "\n";

    std::map<std::string, double> cache_data = {
        {"V(out)", 3.3},
        {"I(R1)", 0.001}
    };
    
    assert(validator.cacheReferenceResult("test_circuit_ngspice", cache_data) && "Caching failed!");
    
    const auto* cached = validator.getCachedReference("test_circuit_ngspice");
    assert(cached != nullptr && "Cache retrieval failed!");
    assert(cached->count("V(out)") > 0 && "Cached data missing V(out)!");
    assert(std::abs(cached->at("V(out)") - 3.3) < 0.001 && "Cached value incorrect!");
    
    std::cout << "  Cached circuit: test_circuit_ngspice\n";
    std::cout << "  Cached measurements: " << cached->size() << "\n";
    std::cout << "  V(out) = " << cached->at("V(out)") << "\n";
    
    std::cout << "\n Test 2.3 PASSED\n";

    // Test 2.4: Statistics
    std::cout << "\n\nTest 2.4: Validator Statistics\n";
    std::cout << "\n";

    std::cout << "  Total validations: " << validator.getTotalValidations() << "\n";
    std::cout << "  Pass rate: " << std::setprecision(1) << validator.getPassRate() << "%\n";
    std::cout << "  Reference simulator: " << validator.getReferenceSimulator() << "\n";
    
    validator.setReferenceSimulator("xyce");
    std::cout << "  Changed to: " << validator.getReferenceSimulator() << "\n";
    
    assert(validator.getReferenceSimulator() == "xyce" && "Simulator change failed!");
    
    std::cout << "\n Test 2.4 PASSED\n";

    validator.finalize();

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #2: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "    Phase 7.1: Model Quality & Verification Tests        \n";
    std::cout << "\n";

    try {
        // Test property-based randomized testing
        test_property_based_testing();

        // Test cross-simulator validation
        test_cross_simulator_validation();

        std::cout << "\n";
        std::cout << "\n";
        std::cout << "    ALL PHASE 7.1 MODEL QUALITY TESTS PASSED            \n";
        std::cout << "\n";
        std::cout << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
