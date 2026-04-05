// ============================================================================
// FluxScript Cross-Simulator Validation Layer
// Compares FluxScript results against ngspice and other simulators
// ============================================================================

#ifndef FLUX_CROSS_SIMULATOR_VALIDATION_H
#define FLUX_CROSS_SIMULATOR_VALIDATION_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>

namespace Flux {

// Comparison result for a single measurement
struct MeasurementComparison {
    std::string name;
    double fluxscript_value;
    double reference_value;      // ngspice or other reference
    double absolute_error;
    double relative_error;
    bool passed;
    std::string status;          // "MATCH", "CLOSE", "DIFFER", "ERROR"
};

// Validation result for a complete circuit
struct CircuitValidationResult {
    std::string circuit_name;
    std::string reference_simulator;
    bool overall_passed;
    std::vector<MeasurementComparison> comparisons;
    double max_absolute_error;
    double max_relative_error;
    double avg_absolute_error;
    double avg_relative_error;
    double execution_time_ms;
    std::string notes;
};

// Validation configuration
struct ValidationConfig {
    std::string circuit_name;
    std::string reference_simulator = "ngspice";
    double absolute_tolerance = 1e-6;
    double relative_tolerance = 1e-3;
    int num_test_points = 100;
    double simulation_time = 1e-3;
    double time_step = 1e-6;
    bool generate_report = true;
    std::string report_file;
};

// Cross-simulator validator
class CrossSimulatorValidator {
public:
    static CrossSimulatorValidator& instance();
    
    // Initialize/finalize
    void initialize();
    void finalize();
    bool isInitialized() const { return m_initialized; }
    
    // Set reference simulator
    void setReferenceSimulator(const std::string& simulator_name);
    std::string getReferenceSimulator() const { return m_reference_simulator; }
    
    // Run validation
    CircuitValidationResult validateCircuit(
        const ValidationConfig& config,
        const std::string& fluxscript_netlist,
        const std::string& reference_netlist
    );
    
    // Compare specific measurements
    std::vector<MeasurementComparison> compareMeasurements(
        const std::map<std::string, double>& fluxscript_results,
        const std::map<std::string, double>& reference_results,
        double abs_tol,
        double rel_tol
    );
    
    // Generate validation report
    std::string generateReport(const CircuitValidationResult& result) const;
    bool exportReport(const CircuitValidationResult& result, const std::string& filename) const;
    
    // Batch validation
    std::vector<CircuitValidationResult> validateCircuitBatch(
        const std::vector<ValidationConfig>& configs,
        const std::vector<std::string>& fluxscript_netlists,
        const std::vector<std::string>& reference_netlists
    );
    
    // Statistics
    int getTotalValidations() const { return m_total_validations; }
    int getPassedValidations() const { return m_passed_validations; }
    double getPassRate() const;
    void resetStatistics();
    
    // Reference simulation cache
    bool cacheReferenceResult(const std::string& circuit_key, 
                            const std::map<std::string, double>& result);
    const std::map<std::string, double>* getCachedReference(const std::string& circuit_key) const;

private:
    CrossSimulatorValidator() = default;
    ~CrossSimulatorValidator() = default;
    
    std::string m_reference_simulator;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    
    // Statistics
    int m_total_validations = 0;
    int m_passed_validations = 0;
    
    // Cache for reference results
    std::map<std::string, std::map<std::string, double>> m_reference_cache;
};

// Helper functions for running simulations
namespace SimulationHelpers {
    
    // Run FluxScript simulation
    std::map<std::string, double> runFluxScriptSimulation(
        const std::string& netlist,
        double sim_time,
        double time_step
    );
    
    // Run ngspice simulation
    std::map<std::string, double> runNgspiceSimulation(
        const std::string& netlist,
        double sim_time,
        double time_step
    );
    
    // Extract measurements from simulation results
    std::map<std::string, double> extractMeasurements(
        const std::vector<double>& time,
        const std::vector<double>& voltage,
        const std::vector<std::string>& measurement_names
    );
}

} // namespace Flux

#endif // FLUX_CROSS_SIMULATOR_VALIDATION_H
