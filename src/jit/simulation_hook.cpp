// ============================================================================
// FluxScript Simulation Hook Implementation - Ngspice Integration
// ============================================================================

#include "flux/jit/simulation_hook.h"
#include "flux/jit/jit_manager.h"
#include "flux/runtime/ngspice_bridge.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace Flux {

// ============================================================================
// NgspiceSimulationHook Singleton
// ============================================================================

NgspiceSimulationHook& NgspiceSimulationHook::instance() {
    static NgspiceSimulationHook instance;
    return instance;
}

void NgspiceSimulationHook::initialize(void* ngspice_context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ngspice_context = ngspice_context;
    m_initialized = true;
    std::cout << "[NgspiceHook] Initialized with ngspice context" << std::endl;
}

void NgspiceSimulationHook::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bound_nodes.clear();
    m_component_inputs.clear();
    m_component_outputs.clear();
    m_initialized = false;
    m_simulation_active = false;
    std::cout << "[NgspiceHook] Finalized" << std::endl;
}

bool NgspiceSimulationHook::onSimulationInit() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        std::cerr << "[NgspiceHook] Not initialized" << std::endl;
        return false;
    }
    std::cout << "[NgspiceHook] Simulation init callback" << std::endl;
    return true;
}

bool NgspiceSimulationHook::onSimulationStart() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_simulation_active = true;
    std::cout << "[NgspiceHook] Simulation started" << std::endl;
    return true;
}

bool NgspiceSimulationHook::onTimeStep(double time, double dt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_simulation_active) {
        return false;
    }

    // Update all JIT components with current time step
    auto& jit_manager = JITManager::instance();
    
    for (const auto& [comp_name, input_nodes] : m_component_inputs) {
        // Read input voltages from ngspice nodes
        std::vector<double> inputs;
        for (const auto& node : input_nodes) {
            inputs.push_back(getNodeVoltage(node));
        }

        // Evaluate component
        std::vector<double> outputs(m_component_outputs[comp_name].size(), 0.0);
        jit_manager.evaluateComponent(comp_name, time, dt, inputs.data(), outputs.data());

        // Write outputs to ngspice nodes
        const auto& output_nodes = m_component_outputs[comp_name];
        for (size_t i = 0; i < output_nodes.size() && i < outputs.size(); i++) {
            setNodeVoltage(output_nodes[i], outputs[i]);
        }
    }

    return true;
}

bool NgspiceSimulationHook::onSimulationEnd() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_simulation_active = false;
    std::cout << "[NgspiceHook] Simulation ended" << std::endl;
    return true;
}

double NgspiceSimulationHook::getNodeVoltage(const std::string& node_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bound_nodes.find(node_name);
    if (it == m_bound_nodes.end()) {
        return 0.0;
    }
    if (it->second.voltage_ptr) {
        return *it->second.voltage_ptr;
    }
    return 0.0;
}

double NgspiceSimulationHook::getBranchCurrent(const std::string& branch_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bound_nodes.find(branch_name);
    if (it == m_bound_nodes.end()) {
        return 0.0;
    }
    if (it->second.current_ptr) {
        return *it->second.current_ptr;
    }
    return 0.0;
}

void NgspiceSimulationHook::setNodeVoltage(const std::string& node_name, double voltage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bound_nodes.find(node_name);
    if (it != m_bound_nodes.end() && it->second.voltage_ptr) {
        *it->second.voltage_ptr = voltage;
    }
}

void NgspiceSimulationHook::setBranchCurrent(const std::string& branch_name, double current) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bound_nodes.find(branch_name);
    if (it != m_bound_nodes.end() && it->second.current_ptr) {
        *it->second.current_ptr = current;
    }
}

bool NgspiceSimulationHook::bindNode(const std::string& node_name, double* voltage_ptr, double* current_ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    NgspiceNodeData data;
    data.node_name = node_name;  // Copy the string
    data.voltage_ptr = voltage_ptr;
    data.current_ptr = current_ptr;
    data.node_index = -1;
    m_bound_nodes[node_name] = data;
    return true;
}

bool NgspiceSimulationHook::unbindNode(const std::string& node_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bound_nodes.erase(node_name);
    return true;
}

bool NgspiceSimulationHook::isNodeBound(const std::string& node_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bound_nodes.count(node_name) > 0;
}

bool NgspiceSimulationHook::integrateJITComponent(const std::string& component_name,
                                                  const std::vector<std::string>& input_nodes,
                                                  const std::vector<std::string>& output_nodes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_component_inputs[component_name] = input_nodes;
    m_component_outputs[component_name] = output_nodes;
    std::cout << "[NgspiceHook] Integrated JIT component: " << component_name 
              << " (inputs: " << input_nodes.size() << ", outputs: " << output_nodes.size() << ")" << std::endl;
    return true;
}

bool NgspiceSimulationHook::onTransientTimestep(const TimestepData& timestep_data) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Update all JIT components with current time step
    auto& jit_manager = JITManager::instance();

    for (const auto& [comp_name, input_nodes] : m_component_inputs) {
        // Read input voltages from ngspice nodes
        std::vector<double> inputs;
        inputs.reserve(input_nodes.size());
        for (const auto& node : input_nodes) {
            inputs.push_back(getNodeVoltage(node));
        }

        // Evaluate component
        std::vector<double> outputs(m_component_outputs[comp_name].size(), 0.0);
        bool success = jit_manager.evaluateComponent(comp_name, 
                                                     timestep_data.current_time, 
                                                     timestep_data.dt, 
                                                     inputs.data(), 
                                                     outputs.data());
        
        if (!success) {
            std::cerr << "[NgspiceHook] Failed to evaluate component: " << comp_name << std::endl;
            return false;
        }

        // Write outputs to ngspice nodes
        const auto& output_nodes = m_component_outputs[comp_name];
        for (size_t i = 0; i < output_nodes.size() && i < outputs.size(); i++) {
            setNodeVoltage(output_nodes[i], outputs[i]);
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double eval_time = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    m_total_eval_time += eval_time;
    m_timestep_count++;

    return true;
}

bool NgspiceSimulationHook::bindNgspiceVector(const std::string& node_name, int vector_index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bound_nodes.find(node_name);
    if (it == m_bound_nodes.end()) {
        return false;
    }
    it->second.node_index = vector_index;
    return true;
}

double NgspiceSimulationHook::readFromVector(int vector_index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (vector_index < 0 || vector_index >= static_cast<int>(m_ngspice_voltages.size())) {
        return 0.0;
    }
    return m_ngspice_voltages[vector_index];
}

void NgspiceSimulationHook::writeToVector(int vector_index, double value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (vector_index >= 0 && vector_index < static_cast<int>(m_ngspice_voltages.size())) {
        m_ngspice_voltages[vector_index] = value;
    }
}

std::vector<double> NgspiceSimulationHook::getAllInputVoltages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> voltages;
    for (const auto& [name, node_data] : m_bound_nodes) {
        if (node_data.voltage_ptr) {
            voltages.push_back(*node_data.voltage_ptr);
        } else {
            voltages.push_back(0.0);
        }
    }
    return voltages;
}

void NgspiceSimulationHook::setAllOutputVoltages(const std::vector<double>& voltages) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t idx = 0;
    for (auto& [name, node_data] : m_bound_nodes) {
        if (idx < voltages.size() && node_data.voltage_ptr) {
            *node_data.voltage_ptr = voltages[idx];
        }
        idx++;
    }
}

void NgspiceSimulationHook::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timestep_count = 0;
    m_total_eval_time = 0.0;
}

// ============================================================================
// BehavioralSourceManager Singleton
// ============================================================================

BehavioralSourceManager& BehavioralSourceManager::instance() {
    static BehavioralSourceManager instance;
    return instance;
}

bool BehavioralSourceManager::registerSource(const BehavioralSource& source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sources.count(source.name)) {
        return false;
    }
    m_sources[source.name] = source;
    return true;
}

bool BehavioralSourceManager::unregisterSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sources.erase(name);
    return true;
}

BehavioralSource* BehavioralSourceManager::getSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sources.find(name);
    if (it == m_sources.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> BehavioralSourceManager::getSourceNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, source] : m_sources) {
        names.push_back(name);
    }
    return names;
}

bool BehavioralSourceManager::compileSource(const std::string& name, std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sources.find(name);
    if (it == m_sources.end()) {
        if (error) *error = "Source not found: " + name;
        return false;
    }

    // Compile the expression using JIT
    auto& jit_manager = JITManager::instance();
    return jit_manager.registerComponent(name, it->second.expression,
                                        it->second.nodes.size(), 1, error) &&
           jit_manager.compileComponent(name, error);
}

bool BehavioralSourceManager::compileAllSources(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool all_success = true;
    for (auto& [name, source] : m_sources) {
        if (!compileSource(name, error)) {
            all_success = false;
        }
    }
    return all_success;
}

double BehavioralSourceManager::evaluateSource(const std::string& name, double time, double dt,
                                               const double* node_voltages, int num_nodes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sources.find(name);
    if (it == m_sources.end()) {
        return 0.0;
    }

    auto& jit_manager = JITManager::instance();
    std::vector<double> outputs(1, 0.0);
    
    if (jit_manager.evaluateComponent(name, time, dt, node_voltages, outputs.data())) {
        return outputs[0];
    }
    return 0.0;
}

std::string BehavioralSourceManager::generateNetlist() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "* FluxScript Behavioral Sources Netlist\n";
    oss << "* Generated automatically\n\n";
    
    for (const auto& [name, source] : m_sources) {
        oss << generateSourceNetlist(name) << "\n";
    }
    
    return oss.str();
}

std::string BehavioralSourceManager::generateSourceNetlist(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sources.find(name);
    if (it == m_sources.end()) {
        return "";
    }

    const auto& source = it->second;
    std::ostringstream oss;
    
    if (source.type == "B") {
        // B-source: Bxxx n+ n- V=<expression>
        oss << "B" << name << " " << source.nodes[0] << " " << source.nodes[1] 
            << " V=" << source.expression << "\n";
    } else if (source.type == "E") {
        // E-source: Exxx n+ n- nc+ nc- <gain>
        oss << "E" << name << " " << source.nodes[0] << " " << source.nodes[1] 
            << " " << source.nodes[2] << " " << source.nodes[3]
            << " " << source.expression << "\n";
    } else if (source.type == "F") {
        // F-source: Fxxx n+ n- <vsource> <gain>
        oss << "F" << name << " " << source.nodes[0] << " " << source.nodes[1] 
            << " " << source.nodes[2] << " " << source.expression << "\n";
    } else if (source.type == "G") {
        // G-source: Gxxx n+ n- nc+ nc- <transconductance>
        oss << "G" << name << " " << source.nodes[0] << " " << source.nodes[1] 
            << " " << source.nodes[2] << " " << source.nodes[3]
            << " " << source.expression << "\n";
    } else if (source.type == "H") {
        // H-source: Hxxx n+ n- <vsource> <transresistance>
        oss << "H" << name << " " << source.nodes[0] << " " << source.nodes[1] 
            << " " << source.nodes[2] << " " << source.expression << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// SimulationController Singleton
// ============================================================================

SimulationController& SimulationController::instance() {
    static SimulationController instance;
    return instance;
}

void SimulationController::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hook = &NgspiceSimulationHook::instance();
    m_jit_manager = &JITManager::instance();
    m_source_manager = &BehavioralSourceManager::instance();
    m_running = false;
    m_paused = false;
    m_current_time = 0.0;
    m_end_time = 0.0;
    std::cout << "[SimController] Initialized" << std::endl;
}

void SimulationController::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    stopSimulation();
    std::cout << "[SimController] Finalized" << std::endl;
}

bool SimulationController::startSimulation(const std::string& netlist) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        return false;
    }

    // Initialize JIT components
    m_jit_manager->initializeSimulation();

    // Compile all behavioral sources
    std::string error;
    if (!m_source_manager->compileAllSources(&error)) {
        if (m_error_callback) {
            m_error_callback("Failed to compile sources: " + error);
        }
        return false;
    }

    // Initialize ngspice with netlist
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        if (m_error_callback) {
            m_error_callback("Failed to initialize ngspice");
        }
        return false;
    }

    // Start simulation
    m_running = true;
    m_paused = false;
    m_current_time = 0.0;

    std::cout << "[SimController] Simulation started" << std::endl;

    if (m_progress_callback) {
        m_progress_callback(0.0, 0.0);
    }

    return true;
}

bool SimulationController::stopSimulation() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        return false;
    }

    m_running = false;
    m_paused = false;

    std::cout << "[SimController] Simulation stopped" << std::endl;
    
    if (m_completion_callback) {
        m_completion_callback(true);
    }

    return true;
}

bool SimulationController::pauseSimulation() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || m_paused) {
        return false;
    }
    m_paused = true;
    std::cout << "[SimController] Simulation paused" << std::endl;
    return true;
}

bool SimulationController::resumeSimulation() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || !m_paused) {
        return false;
    }
    m_paused = false;
    std::cout << "[SimController] Simulation resumed" << std::endl;
    return true;
}

bool SimulationController::stepSimulation(double time_step) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || m_paused) {
        return false;
    }

    m_current_time += time_step;

    // Prepare timestep data for JIT components
    TimestepData timestep_data;
    timestep_data.current_time = m_current_time;
    timestep_data.dt = time_step;
    timestep_data.iteration = static_cast<int>(m_current_time / time_step);
    timestep_data.converged = true;

    // Update ngspice voltages from JIT components
    if (m_hook) {
        m_hook->onTransientTimestep(timestep_data);
    }

    // Update JIT components
    m_jit_manager->stepSimulation(m_current_time, time_step);

    // Report progress
    if (m_progress_callback && m_end_time > 0) {
        double percent = (m_current_time / m_end_time) * 100.0;
        m_progress_callback(m_current_time, percent);
    }

    return true;
}

bool SimulationController::runTransientSimulation(double tstart, double tstop, double tstep) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || m_paused) {
        return false;
    }

    std::cout << "[SimController] Running transient: " << tstart << " to " << tstop 
              << " (step: " << tstep << ")" << std::endl;

    // Start ngspice transient simulation
    int rc = flux_ngspice_run_transient(tstart, tstop, tstep);
    if (rc != 0) {
        if (m_error_callback) {
            m_error_callback("ngspice transient simulation failed");
        }
        return false;
    }

    // Run simulation loop, calling JIT components at each timestep
    for (double t = tstart; t <= tstop && m_running && !m_paused; t += tstep) {
        m_current_time = t;

        TimestepData timestep_data;
        timestep_data.current_time = t;
        timestep_data.dt = tstep;
        timestep_data.iteration = static_cast<int>((t - tstart) / tstep);
        timestep_data.converged = true;

        // Call JIT components through simulation hook
        if (m_hook) {
            m_hook->onTransientTimestep(timestep_data);
        }

        // Update statistics in JIT manager
        m_jit_manager->stepSimulation(t, tstep);

        // Report progress
        if (m_progress_callback && m_end_time > 0) {
            double percent = (t / m_end_time) * 100.0;
            m_progress_callback(t, percent);
        }
    }

    std::cout << "[SimController] Transient simulation completed at t=" << m_current_time << std::endl;
    return true;
}

bool SimulationController::isSimulationRunning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_running;
}

bool SimulationController::isSimulationPaused() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_paused;
}

double SimulationController::getCurrentTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_time;
}

double SimulationController::getEndTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_end_time;
}

void SimulationController::setProgressCallback(std::function<void(double time, double percent)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress_callback = callback;
}

void SimulationController::setCompletionCallback(std::function<void(bool success)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completion_callback = callback;
}

void SimulationController::setErrorCallback(std::function<void(const std::string& error)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_callback = callback;
}

} // namespace Flux
