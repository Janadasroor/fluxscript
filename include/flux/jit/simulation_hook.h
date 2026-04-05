// ============================================================================
// FluxScript Simulation Hook - Ngspice Integration
// Connects JIT-compiled components to Ngspice simulation loop
// ============================================================================

#ifndef FLUX_SIMULATION_HOOK_H
#define FLUX_SIMULATION_HOOK_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>

#include "flux/jit/jit_manager.h"

namespace Flux {

// Ngspice node data (zero-copy interface)
struct NgspiceNodeData {
    std::string node_name;
    double* voltage_ptr;      // Pointer to ngspice node voltage (zero-copy)
    double* current_ptr;      // Pointer to ngspice branch current (zero-copy)
    int node_index = -1;      // Index in ngspice vector for direct access
};

// Timestep callback data
struct TimestepData {
    double current_time;
    double dt;
    int iteration;
    bool converged;
};

// Simulation hook interface
class SimulationHook {
public:
    virtual ~SimulationHook() = default;

    // Lifecycle callbacks
    virtual bool onSimulationInit() = 0;
    virtual bool onSimulationStart() = 0;
    virtual bool onTimeStep(double time, double dt) = 0;
    virtual bool onSimulationEnd() = 0;

    // Data access
    virtual double getNodeVoltage(const std::string& node_name) = 0;
    virtual double getBranchCurrent(const std::string& branch_name) = 0;
    virtual void setNodeVoltage(const std::string& node_name, double voltage) = 0;
    virtual void setBranchCurrent(const std::string& branch_name, double current) = 0;
};

// Ngspice simulation hook implementation
class NgspiceSimulationHook : public SimulationHook {
public:
    static NgspiceSimulationHook& instance();

    // Initialize with ngspice context
    void initialize(void* ngspice_context);
    void finalize();

    // SimulationHook interface
    bool onSimulationInit() override;
    bool onSimulationStart() override;
    bool onTimeStep(double time, double dt) override;
    bool onSimulationEnd() override;

    double getNodeVoltage(const std::string& node_name) override;
    double getBranchCurrent(const std::string& branch_name) override;
    void setNodeVoltage(const std::string& node_name, double voltage) override;
    void setBranchCurrent(const std::string& branch_name, double current) override;

    // Node binding
    bool bindNode(const std::string& node_name, double* voltage_ptr, double* current_ptr);
    bool unbindNode(const std::string& node_name);
    bool isNodeBound(const std::string& node_name) const;

    // Component integration
    bool integrateJITComponent(const std::string& component_name,
                              const std::vector<std::string>& input_nodes,
                              const std::vector<std::string>& output_nodes);

    // Transient loop integration - called at each timestep by ngspice
    bool onTransientTimestep(const TimestepData& timestep_data);
    
    // Direct ngspice vector access (for zero-copy operation)
    bool bindNgspiceVector(const std::string& node_name, int vector_index);
    double readFromVector(int vector_index);
    void writeToVector(int vector_index, double value);
    
    // Get all bound node voltages as array (for bulk operations)
    std::vector<double> getAllInputVoltages() const;
    void setAllOutputVoltages(const std::vector<double>& voltages);

    // Statistics
    int getTimestepCount() const { return m_timestep_count; }
    double getTotalEvalTime() const { return m_total_eval_time; }
    void resetStatistics();

private:
    NgspiceSimulationHook() = default;
    ~NgspiceSimulationHook() = default;

    void* m_ngspice_context = nullptr;
    std::map<std::string, NgspiceNodeData> m_bound_nodes;
    std::map<std::string, std::vector<std::string>> m_component_inputs;
    std::map<std::string, std::vector<std::string>> m_component_outputs;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_simulation_active = false;
    
    // ngspice vector storage (for zero-copy access)
    std::vector<double> m_ngspice_voltages;    // Cached node voltages
    std::vector<double> m_ngspice_currents;    // Cached branch currents
    
    // Statistics
    int m_timestep_count = 0;
    double m_total_eval_time = 0.0;
};

// Behavioral source interface for ngspice
struct BehavioralSource {
    std::string name;
    std::string type;  // "B", "E", "F", "G", "H"
    std::vector<std::string> nodes;
    std::string expression;  // FluxScript expression
    void* jit_function = nullptr;  // Compiled JIT function
};

// Behavioral source manager
class BehavioralSourceManager {
public:
    static BehavioralSourceManager& instance();

    // Source registration
    bool registerSource(const BehavioralSource& source);
    bool unregisterSource(const std::string& name);
    BehavioralSource* getSource(const std::string& name);
    std::vector<std::string> getSourceNames() const;

    // Compilation
    bool compileSource(const std::string& name, std::string* error = nullptr);
    bool compileAllSources(std::string* error = nullptr);

    // Evaluation (called by ngspice)
    double evaluateSource(const std::string& name, double time, double dt,
                         const double* node_voltages, int num_nodes);

    // Netlist generation
    std::string generateNetlist() const;
    std::string generateSourceNetlist(const std::string& name) const;

private:
    BehavioralSourceManager() = default;
    ~BehavioralSourceManager() = default;

    std::map<std::string, BehavioralSource> m_sources;
    mutable std::mutex m_mutex;
};

// Simulation controller
class SimulationController {
public:
    static SimulationController& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // Simulation control
    bool startSimulation(const std::string& netlist);
    bool stopSimulation();
    bool pauseSimulation();
    bool resumeSimulation();
    bool stepSimulation(double time_step);
    bool runTransientSimulation(double tstart, double tstop, double tstep);

    // Status
    bool isSimulationRunning() const;
    bool isSimulationPaused() const;
    double getCurrentTime() const;
    double getEndTime() const;

    // Callbacks
    void setProgressCallback(std::function<void(double time, double percent)> callback);
    void setCompletionCallback(std::function<void(bool success)> callback);
    void setErrorCallback(std::function<void(const std::string& error)> callback);

private:
    SimulationController() = default;
    ~SimulationController() = default;

    NgspiceSimulationHook* m_hook;
    JITManager* m_jit_manager;
    BehavioralSourceManager* m_source_manager;

    std::function<void(double, double)> m_progress_callback;
    std::function<void(bool)> m_completion_callback;
    std::function<void(const std::string&)> m_error_callback;

    mutable std::mutex m_mutex;
    bool m_running = false;
    bool m_paused = false;
    double m_current_time = 0.0;
    double m_end_time = 0.0;
};

} // namespace Flux

#endif // FLUX_SIMULATION_HOOK_H
