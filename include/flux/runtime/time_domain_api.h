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

#ifndef FLUX_TIME_DOMAIN_API_H
#define FLUX_TIME_DOMAIN_API_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace Flux {

// ============================================================================
// Time-Domain Simulation API
// ============================================================================

// Simulation state passed to update() functions
struct SimulationState {
    double time;                  // Current simulation time
    double timestep;              // Current time step (dt)
    double temperature;           // Simulation temperature (Kelvin)
    int iteration;                // Current iteration count
    bool converged;               // Whether simulation has converged
};

// Node voltage container
struct NodeVoltages {
    std::map<std::string, double> voltages;  // node_name -> voltage
    std::map<std::string, double> currents;  // branch_name -> current
    
    double get(const std::string& node) const {
        auto it = voltages.find(node);
        return (it != voltages.end()) ? it->second : 0.0;
    }
    
    void set(const std::string& node, double value) {
        voltages[node] = value;
    }
};

// Behavioral source output
struct BSourceOutput {
    double voltage;        // Output voltage
    double current;        // Output current (if applicable)
    bool valid;            // Whether output is valid
    
    BSourceOutput() : voltage(0.0), current(0.0), valid(false) {}
};

// Update function signature: update(time, inputs) -> outputs
using UpdateFunction = std::function<BSourceOutput(double time, const NodeVoltages& inputs)>;

// Time-domain simulation engine
class TimeDomainEngine {
public:
    static TimeDomainEngine& instance();
    
    // Register a behavioral source
    bool registerBSource(const std::string& name, UpdateFunction updateFn);
    
    // Unregister a behavioral source
    void unregisterBSource(const std::string& name);
    
    // Evaluate a behavioral source at current time
    BSourceOutput evaluateBSource(const std::string& name, double time, const NodeVoltages& inputs);
    
    // Get all registered sources
    std::vector<std::string> getRegisteredSources() const;
    
    // Simulation control
    void setSimulationTime(double time);
    void setTimestep(double dt);
    void setTemperature(double temp);
    
    double getSimulationTime() const { return m_simTime; }
    double getTimestep() const { return m_timestep; }
    double getTemperature() const { return m_temperature; }
    
    // Get built-in variable: time, dt, temp
    double getBuiltinVariable(const std::string& name) const;

    // State management
    double stateGet(const std::string& name, double defaultValue) const;
    void stateSet(const std::string& name, double value);

    // Initial conditions
    void setInitialCondition(const std::string& node, double voltage);
    double getInitialCondition(const std::string& node) const;
    std::map<std::string, double> getAllInitialConditions() const;

    // Output management
    void setOutput(const std::string& name, double value);
    double getOutput(const std::string& name) const;
    std::map<std::string, double> getAllOutputs() const;

private:
    TimeDomainEngine() : m_simTime(0.0), m_timestep(1e-9), m_temperature(300.0) {}

    double m_simTime;
    double m_timestep;
    double m_temperature;
    std::map<std::string, UpdateFunction> m_bsources;
    std::map<std::string, double> m_stateVariables;
    std::map<std::string, double> m_initialConditions;
    std::map<std::string, double> m_outputs;
};

// ============================================================================
// C API for ngspice callback integration
// ============================================================================

extern "C" {
    // Register behavioral source from C
    int flux_register_bsource(const char* name, void* userData);
    
    // Evaluate behavioral source
    double flux_evaluate_bsource(const char* name, double time, double timestep,
                                const double* nodeVoltages, int numNodes);
    
    // Get built-in variables
    double flux_get_time();
    double flux_get_dt();
    double flux_get_temp();
    
    // State variables
    double flux_state_get(double name_ptr, double defaultValue);
    void flux_state_set(double name_ptr, double value);
    
    // Set simulation state
    void flux_set_simtime(double time);
    void flux_set_timestep(double dt);

    // Initial conditions
    void flux_set_initial_condition(double node_ptr, double voltage);
    double flux_get_initial_condition(double node_ptr);

    // Outputs
    void flux_set_output(double name_ptr, double value);
    double flux_get_output(double name_ptr);
}

} // namespace Flux

#endif // FLUX_TIME_DOMAIN_API_H
