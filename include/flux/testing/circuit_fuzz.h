#ifndef FLUX_CIRCUIT_FUZZ_H
#define FLUX_CIRCUIT_FUZZ_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Flux {
namespace FuzzTesting {

// Fuzz Test Result
enum class FuzzStatus {
    Passed,
    Failed,
    Crashed,
    Unstable,
    Timeout
};

struct FuzzTestCase {
    std::map<std::string, double> inputs;
    std::map<std::string, double> expectedOutputs;
    std::map<std::string, double> actualOutputs;
    FuzzStatus status;
    std::string errorMessage;
    double executionTimeMs;
    int iterationId;
    
    FuzzTestCase() : status(FuzzStatus::Passed), executionTimeMs(0), iterationId(0) {}
};

// Fuzz Configuration
struct FuzzConfig {
    int iterations;
    double mutationRate;       // Probability of modifying a parameter (0.0 - 1.0)
    double mutationMagnitude;  // How much to modify parameters
    bool randomizeOrder;       // Randomize test execution order
    int timeoutMs;             // Max time per test case
    std::map<std::string, std::pair<double, double>> paramBounds; // Min/Max for each param
    std::function<bool(const std::map<std::string, double>&, std::map<std::string, double>&)> simulate;
    
    FuzzConfig() : iterations(100), mutationRate(0.5), mutationMagnitude(0.2),
                   randomizeOrder(false), timeoutMs(5000) {}
};

// Fuzz Test Summary
struct FuzzSummary {
    int totalTests;
    int passed;
    int failed;
    int crashed;
    int unstable;
    double passRate;
    double avgExecutionTime;
    double worstCaseTime;
    std::vector<std::string> failingInputs;
    std::vector<std::string> recommendations;
    
    FuzzSummary() : totalTests(0), passed(0), failed(0), crashed(0), unstable(0),
                    passRate(0), avgExecutionTime(0), worstCaseTime(0) {}
    
    std::string toString() const;
};

// Circuit Fuzzer
class CircuitFuzzer {
public:
    CircuitFuzzer();
    ~CircuitFuzzer();
    
    // Set configuration
    void setConfig(const FuzzConfig& config);
    
    // Run fuzz test on a circuit
    FuzzSummary runFuzzTest(const std::string& circuitId);
    
    // Get specific test case details
    const FuzzTestCase& getTestCase(int index) const;
    std::vector<FuzzTestCase> getFailedCases() const;
    
    // Helper: Generate random input within bounds
    std::map<std::string, double> generateRandomInput() const;
    
    // Helper: Mutate existing inputs
    std::map<std::string, double> mutateInputs(const std::map<std::string, double>& inputs) const;
    
private:
    FuzzConfig m_config;
    std::vector<FuzzTestCase> m_testCases;
    
    // Internal methods
    FuzzTestCase runSingleTest(int iteration, const std::map<std::string, double>& inputs);
    FuzzStatus checkStability(const std::map<std::string, double>& outputs) const;
};

// Convenience function
FuzzSummary circuit_fuzz_test(const std::string& circuitId, int iterations = 1000);

// C Interface for FluxScript JIT
extern "C" {
    void* flux_fuzz_create();
    void flux_fuzz_destroy(void* fuzz);
    void flux_fuzz_set_iterations(void* fuzz, int iterations);
    void flux_fuzz_set_bounds(void* fuzz, const char* paramName, double min, double max);
    const char* flux_fuzz_run(void* fuzz, const char* circuitId);
}

} // namespace FuzzTesting
} // namespace Flux

#endif // FLUX_CIRCUIT_FUZZ_H
