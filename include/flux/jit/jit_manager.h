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
// FluxScript JIT Manager - VioSpice Integration
// Manages multiple JIT-compiled behavioral components for SPICE simulation
// ============================================================================

#ifndef FLUX_JIT_MANAGER_H
#define FLUX_JIT_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>

#include "flux/jit/flux_jit.h"
#include "flux/compiler/ast.h"

namespace Flux {

// Component state for time-domain simulation
struct ComponentState {
    std::string name;
    double current_time = 0.0;
    double dt = 1e-12;
    std::vector<double> inputs;      // Input voltages
    std::vector<double> outputs;     // Output values
    std::map<std::string, double> internal_state;  // Internal state variables
    bool is_valid = false;
};

// JIT-compiled component
struct JITComponent {
    std::string name;
    std::string source_code;
    std::unique_ptr<FluxJIT> jit;
    void* update_func = nullptr;           // update(t, inputs, outputs, state)
    void* init_func = nullptr;             // init(state)
    void* eval_func = nullptr;             // eval(t) -> single value (for signal generation)
    ComponentState state;
    int num_inputs = 0;
    int num_outputs = 0;
    std::chrono::steady_clock::time_point last_compile_time;
    int compile_count = 0;
    bool is_hot_reloadable = false;
    bool is_signal_generator = false;      // True if component generates waveforms
    std::string component_type = "generic"; // "generic", "signal", "behavioral_source"
};

// Simulation statistics
struct SimulationStats {
    int total_components = 0;
    int active_components = 0;
    double total_sim_time = 0.0;
    double avg_eval_time_us = 0.0;
    int total_evaluations = 0;
    int hot_reload_count = 0;
};

// JIT Manager class
class JITManager {
public:
    static JITManager& instance();

    // Initialize/finalize
    void initialize();
    void finalize();
    bool isInitialized() const { return m_initialized; }

    // Component management
    bool registerComponent(const std::string& name, const std::string& source_code, 
                          int num_inputs, int num_outputs, std::string* error = nullptr);
    bool unregisterComponent(const std::string& name);
    bool hasComponent(const std::string& name) const;
    JITComponent* getComponent(const std::string& name);
    const JITComponent* getComponent(const std::string& name) const;
    std::vector<std::string> getComponentNames() const;

    // Compilation
    bool compileComponent(const std::string& name, std::string* error = nullptr);
    bool recompileComponent(const std::string& name, const std::string& new_source, 
                           std::string* error = nullptr);
    bool hotReloadComponent(const std::string& name, const std::string& new_source,
                           std::string* error = nullptr);

    // Simulation control
    bool initializeSimulation();
    bool stepSimulation(double time, double dt);
    bool evaluateComponent(const std::string& name, double time, double dt,
                          const double* inputs, double* outputs);
    bool evaluateAllComponents(double time, double dt);

    // State management
    bool setComponentState(const std::string& name, const ComponentState& state);
    bool getComponentState(const std::string& name, ComponentState& state) const;
    bool resetComponentState(const std::string& name);
    bool resetAllStates();

    // Zero-copy data interface
    bool bindInputBuffer(const std::string& name, double* buffer, int size);
    bool bindOutputBuffer(const std::string& name, double* buffer, int size);
    double* getInputBuffer(const std::string& name);
    double* getOutputBuffer(const std::string& name);

    // Signal generation support
    bool registerSignalGenerator(const std::string& name, const std::string& source_code,
                                std::string* error = nullptr);
    double evaluateSignal(const std::string& name, double time);
    std::vector<double> generateWaveform(const std::string& name, 
                                        double t_start, double t_stop, double sample_rate,
                                        std::string* error = nullptr);

    // Probing
    bool enableProbe(const std::string& component_name, const std::string& signal_name);
    bool disableProbe(const std::string& component_name, const std::string& signal_name);
    std::vector<double> getProbeData(const std::string& component_name, 
                                     const std::string& signal_name) const;
    bool clearProbeData(const std::string& component_name);

    // Statistics
    SimulationStats getStatistics() const;
    double getComponentEvalTime(const std::string& name) const;
    void resetStatistics();

    // Timeout protection
    void setEvalTimeoutMs(int timeout_ms) { m_eval_timeout_ms = timeout_ms; }
    int getEvalTimeoutMs() const { return m_eval_timeout_ms; }

private:
    JITManager() = default;
    ~JITManager() = default;

    bool compileSource(const std::string& source, JITComponent& comp, std::string* error);
    bool validateComponent(const JITComponent& comp, std::string* error) const;

    std::map<std::string, JITComponent> m_components;
    std::map<std::string, std::map<std::string, std::vector<double>>> m_probe_data;
    std::map<std::string, double*> m_input_buffers;
    std::map<std::string, double*> m_output_buffers;
    
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_simulation_active = false;
    int m_eval_timeout_ms = 1000;  // 1 second default timeout
    SimulationStats m_stats;
};

// Helper functions
namespace JITUtils {
    std::string generateComponentTemplate(const std::string& name, 
                                         int num_inputs, int num_outputs);
    std::string generateTestbench(const std::string& component_name);
}

} // namespace Flux

#endif // FLUX_JIT_MANAGER_H
