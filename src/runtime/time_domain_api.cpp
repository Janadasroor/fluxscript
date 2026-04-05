#include "flux/runtime/time_domain_api.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace Flux {

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

// ============================================================================
// TimeDomainEngine Implementation
// ============================================================================

TimeDomainEngine& TimeDomainEngine::instance() {
    static TimeDomainEngine engine;
    return engine;
}

bool TimeDomainEngine::registerBSource(const std::string& name, UpdateFunction updateFn) {
    if (m_bsources.count(name)) {
        std::cerr << "[TimeDomain] Warning: B-source '" << name << "' already registered" << std::endl;
        return false;
    }
    
    m_bsources[name] = std::move(updateFn);
    std::cout << "[TimeDomain] Registered B-source: " << name << std::endl;
    return true;
}

void TimeDomainEngine::unregisterBSource(const std::string& name) {
    m_bsources.erase(name);
}

BSourceOutput TimeDomainEngine::evaluateBSource(const std::string& name, double time, 
                                                const NodeVoltages& inputs) {
    auto it = m_bsources.find(name);
    if (it == m_bsources.end()) {
        std::cerr << "[TimeDomain] Error: B-source '" << name << "' not found" << std::endl;
        return BSourceOutput();
    }
    
    return it->second(time, inputs);
}

std::vector<std::string> TimeDomainEngine::getRegisteredSources() const {
    std::vector<std::string> names;
    for (const auto& [name, fn] : m_bsources) {
        names.push_back(name);
    }
    return names;
}

void TimeDomainEngine::setSimulationTime(double time) {
    m_simTime = time;
}

void TimeDomainEngine::setTimestep(double dt) {
    m_timestep = dt;
}

void TimeDomainEngine::setTemperature(double temp) {
    m_temperature = temp;
}

double TimeDomainEngine::getBuiltinVariable(const std::string& name) const {
    if (name == "time") return m_simTime;
    if (name == "dt" || name == "timestep") return m_timestep;
    if (name == "temp" || name == "temperature") return m_temperature;
    return 0.0;
}

double TimeDomainEngine::stateGet(const std::string& name, double defaultValue) const {
    auto it = m_stateVariables.find(name);
    if (it == m_stateVariables.end()) {
        return defaultValue;
    }
    return it->second;
}

void TimeDomainEngine::stateSet(const std::string& name, double value) {
    m_stateVariables[name] = value;
}

// Initial conditions
void TimeDomainEngine::setInitialCondition(const std::string& node, double voltage) {
    m_initialConditions[node] = voltage;
    std::cout << "[TimeDomain] Initial condition: " << node << " = " << voltage << "V" << std::endl;
}

double TimeDomainEngine::getInitialCondition(const std::string& node) const {
    auto it = m_initialConditions.find(node);
    return (it != m_initialConditions.end()) ? it->second : 0.0;
}

std::map<std::string, double> TimeDomainEngine::getAllInitialConditions() const {
    return m_initialConditions;
}

// Output management
void TimeDomainEngine::setOutput(const std::string& name, double value) {
    m_outputs[name] = value;
}

double TimeDomainEngine::getOutput(const std::string& name) const {
    auto it = m_outputs.find(name);
    return (it != m_outputs.end()) ? it->second : 0.0;
}

std::map<std::string, double> TimeDomainEngine::getAllOutputs() const {
    return m_outputs;
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

int flux_register_bsource(const char* name, void* userData) {
    if (!name) return -1;
    
    // userData would point to a compiled FluxScript function
    // For now, we'll register a stub
    std::cout << "[C-API] Registering B-source: " << name << std::endl;
    
    return 0;
}

double flux_evaluate_bsource(const char* name, double time, double timestep,
                            const double* nodeVoltages, int numNodes) {
    if (!name) return 0.0;
    
    auto& engine = TimeDomainEngine::instance();
    
    // Build node voltages map
    NodeVoltages inputs;
    if (nodeVoltages && numNodes > 0) {
        for (int i = 0; i < numNodes; ++i) {
            inputs.voltages["node" + std::to_string(i)] = nodeVoltages[i];
        }
    }
    
    // Evaluate
    auto output = engine.evaluateBSource(name, time, inputs);
    
    return output.voltage;
}

double flux_get_time() {
    return TimeDomainEngine::instance().getSimulationTime();
}

double flux_get_dt() {
    return TimeDomainEngine::instance().getTimestep();
}

double flux_get_temp() {
    return TimeDomainEngine::instance().getTemperature();
}

double flux_state_get(double name_ptr, double defaultValue) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr));
    if (!name) return defaultValue;
    return TimeDomainEngine::instance().stateGet(name, defaultValue);
}

void flux_state_set(double name_ptr, double value) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr));
    if (name) {
        TimeDomainEngine::instance().stateSet(name, value);
    }
}

void flux_set_simtime(double time) {
    TimeDomainEngine::instance().setSimulationTime(time);
}

void flux_set_timestep(double dt) {
    TimeDomainEngine::instance().setTimestep(dt);
}

// Initial conditions
void flux_set_initial_condition(double node_ptr, double voltage) {
    const char* node = reinterpret_cast<const char*>(bit_cast<uint64_t>(node_ptr));
    if (node) {
        TimeDomainEngine::instance().setInitialCondition(node, voltage);
    }
}

double flux_get_initial_condition(double node_ptr) {
    const char* node = reinterpret_cast<const char*>(bit_cast<uint64_t>(node_ptr));
    if (node) {
        return TimeDomainEngine::instance().getInitialCondition(node);
    }
    return 0.0;
}

// Outputs
void flux_set_output(double name_ptr, double value) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr));
    if (name) {
        TimeDomainEngine::instance().setOutput(name, value);
    }
}

double flux_get_output(double name_ptr) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr));
    if (name) {
        return TimeDomainEngine::instance().getOutput(name);
    }
    return 0.0;
}

} // extern "C"

} // namespace Flux
