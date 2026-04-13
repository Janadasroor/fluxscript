// Advanced Analysis Tools Tests
#include <iostream>
#include <cassert>
#include "flux/analysis/advanced_analysis.h"

using namespace Flux::AdvancedAnalysis;

// 
//  Test 1: Stability Analyzer                          
// 
void test_stability_analyzer() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 1: Stability Analyzer                              \n";
    std::cout << "\n";
    std::cout << "\n";
    
    StabilityAnalyzer analyzer;
    auto result = analyzer.analyzeFromString("test");
    
    std::cout << result.toString();
    
    // Verify basic structure
    assert(result.phaseMargin >= 0 && "Phase margin should be non-negative");
    assert(result.gainMargin >= 0 && "Gain margin should be non-negative");
    assert(result.bandwidth >= 0 && "Bandwidth should be non-negative");
    
    // Test markdown output
    std::string md = result.toMarkdown();
    assert(md.find("# Stability") != std::string::npos && "Should have markdown header");
    
    std::cout << "\n Test 1 PASSED\n";
}

// 
//  Test 2: Sensitivity Analyzer                        
// 
void test_sensitivity_analyzer() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 2: Sensitivity Analyzer                            \n";
    std::cout << "\n";
    std::cout << "\n";
    
    SensitivityAnalyzer analyzer;
    std::vector<std::string> components = {"R1", "R2", "C1"};
    auto result = analyzer.analyze("test", components, "Vout");
    
    std::cout << result.toString();
    
    // Verify structure
    assert(result.sensitivities.size() == 3 && "Should have 3 components");
    assert(!result.outputVariable.empty() && "Should have output variable");
    assert(!result.mostCritical.empty() && "Should identify most critical");
    assert(!result.leastCritical.empty() && "Should identify least critical");
    
    // Test markdown output
    std::string md = result.toMarkdown();
    assert(md.find("# Sensitivity") != std::string::npos && "Should have markdown header");
    
    std::cout << "\n Test 2 PASSED\n";
}

// 
//  Test 3: Monte Carlo Engine                          
// 
void test_monte_carlo() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 3: Monte Carlo Engine                              \n";
    std::cout << "\n";
    std::cout << "\n";
    
    MonteCarloEngine mc;
    mc.setIterations(100);
    mc.setDistribution("gaussian");
    mc.addParameter("R1", 10000.0, 100.0);  // 10k  1%
    mc.addParameter("C1", 1e-9, 0.2e-9);     // 1nF  20%
    mc.setOutputVariable("Vout");
    mc.setSpecLimits(4.5, 5.5);
    
    auto result = mc.run("test");
    
    std::cout << result.toString();
    
    // Verify structure
    assert(result.iterations == 100 && "Should have 100 iterations");
    assert(result.yield >= 0 && result.yield <= 100 && "Yield should be 0-100%");
    
    // Test markdown output
    std::string md = result.toMarkdown();
    assert(md.find("# Monte Carlo") != std::string::npos && "Should have markdown header");
    
    std::cout << "\n Test 3 PASSED\n";
}

// 
//  Test 4: Circuit Optimizer                           
// 
void test_circuit_optimizer() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 4: Circuit Optimizer                               \n";
    std::cout << "\n";
    std::cout << "\n";
    
    CircuitOptimizer optimizer;
    optimizer.addVariable("Rval", 10000.0, 1000.0, 100000.0);
    optimizer.addVariable("Cval", 1e-9, 1e-12, 1e-6);
    optimizer.addTarget("cutoff", 1000.0, 1.0);
    
    auto result = optimizer.optimize("test");
    
    std::cout << result.toString();
    
    // Verify structure
    assert(result.iterations > 0 && "Should have iterations");
    assert(result.optimalValues.size() == 2 && "Should have 2 optimized values");
    assert(result.initialValues.size() == 2 && "Should have 2 initial values");
    
    // Test markdown output
    std::string md = result.toMarkdown();
    assert(md.find("# Optimization") != std::string::npos && "Should have markdown header");
    
    std::cout << "\n Test 4 PASSED\n";
}

// 
//  Test 5: Worst-Case Analyzer                         
// 
void test_worst_case() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 5: Worst-Case Analyzer                             \n";
    std::cout << "\n";
    std::cout << "\n";
    
    WorstCaseAnalyzer wc;
    wc.addComponent("R1", 10000.0, 100.0);  // 10k  1%
    wc.addComponent("R2", 20000.0, 200.0);  // 20k  1%
    wc.addComponent("C1", 1e-9, 0.2e-9);     // 1nF  20%
    wc.setOutputVariable("Vout");
    wc.setSpecLimits(4.5, 5.5);
    
    auto result = wc.analyze("test");
    
    std::cout << result.toString();
    
    // Verify structure
    assert(result.nominal == 5.0 && "Should have nominal value");
    assert(result.corners.size() == 8 && "Should test all 8 corners (2^3)");
    
    // Test markdown output
    std::string md = result.toMarkdown();
    assert(md.find("# Worst-Case") != std::string::npos && "Should have markdown header");
    
    std::cout << "\n Test 5 PASSED\n";
}

// 
//  Test 6: C Interface                                 
// 
void test_c_interface() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 6: C Interface for JIT                             \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test stability C interface
    void* stab = flux_stability_create();
    assert(stab != nullptr && "Should create stability analyzer");
    const char* stab_result = flux_stability_analyze(stab, "test");
    assert(stab_result != nullptr && "Should return result");
    flux_stability_destroy(stab);
    
    // Test sensitivity C interface
    void* sens = flux_sensitivity_create();
    assert(sens != nullptr && "Should create sensitivity analyzer");
    flux_sensitivity_destroy(sens);
    
    // Test Monte Carlo C interface
    void* mc = flux_monte_carlo_create();
    assert(mc != nullptr && "Should create Monte Carlo engine");
    flux_monte_carlo_set_iterations(mc, 100);
    flux_monte_carlo_add_param(mc, "R1", 10000.0, 100.0);
    const char* mc_result = flux_monte_carlo_run(mc, "test");
    assert(mc_result != nullptr && "Should return Monte Carlo result");
    flux_monte_carlo_destroy(mc);
    
    // Test optimizer C interface
    void* opt = flux_optimizer_create();
    assert(opt != nullptr && "Should create optimizer");
    flux_optimizer_add_variable(opt, "Rval", 10000.0, 1000.0, 100000.0);
    const char* opt_result = flux_optimizer_run(opt, "test");
    assert(opt_result != nullptr && "Should return optimization result");
    flux_optimizer_destroy(opt);
    
    // Test worst-case C interface
    void* wc = flux_worst_case_create();
    assert(wc != nullptr && "Should create worst-case analyzer");
    flux_worst_case_add_component(wc, "R1", 10000.0, 100.0);
    const char* wc_result = flux_worst_case_analyze(wc, "test");
    assert(wc_result != nullptr && "Should return worst-case result");
    flux_worst_case_destroy(wc);
    
    std::cout << "   All C interface functions work correctly\n";
    std::cout << "\n Test 6 PASSED\n";
}

// 
//  Main Test Runner                                    
// 
int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "       Advanced Analysis Tools - Test Suite               \n";
    std::cout << "\n";
    
    try {
        test_stability_analyzer();
        test_sensitivity_analyzer();
        test_monte_carlo();
        test_circuit_optimizer();
        test_worst_case();
        test_c_interface();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL ADVANCED ANALYSIS TESTS PASSED            \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
