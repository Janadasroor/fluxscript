#include "flux/runtime/time_domain_api.h"
#include <iostream>
#include <algorithm>

namespace Flux {

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

void flux_set_simtime(double time) {
    TimeDomainEngine::instance().setSimulationTime(time);
}

void flux_set_timestep(double dt) {
    TimeDomainEngine::instance().setTimestep(dt);
}

} // extern "C"

} // namespace Flux
