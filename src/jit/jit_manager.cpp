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
// FluxScript JIT Manager Implementation
// Manages multiple JIT-compiled behavioral components for SPICE simulation
// ============================================================================

#include "flux/jit/jit_manager.h"
#include "flux/jit/flux_jit.h"
#include "flux/runtime/flux_runtime.h"
#include "flux/compiler/compiler_instance.h"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace Flux {

namespace {

double defaultComponentUpdate(double /*time*/, double /*dt*/, const double* inputs, double* outputs, void* statePtr) {
    auto* state = reinterpret_cast<ComponentState*>(statePtr);
    if (!state) return 0.0;

    if (inputs) {
        for (size_t i = 0; i < state->inputs.size(); ++i) {
            state->inputs[i] = inputs[i];
        }
    }
    if (outputs) {
        for (size_t i = 0; i < state->outputs.size(); ++i) {
            const double value = (i < state->inputs.size()) ? state->inputs[i] : 0.0;
            state->outputs[i] = value;
            outputs[i] = value;
        }
    }
    return state->outputs.empty() ? 0.0 : state->outputs.front();
}

void defaultComponentInit(void* statePtr) {
    auto* state = reinterpret_cast<ComponentState*>(statePtr);
    if (!state) return;
    std::fill(state->outputs.begin(), state->outputs.end(), 0.0);
    state->is_valid = true;
}

} // namespace

// ============================================================================
// JITManager Singleton
// ============================================================================

JITManager& JITManager::instance() {
    static JITManager instance;
    return instance;
}

// ============================================================================
// Initialization
// ============================================================================

void JITManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return;
    }

    m_components.clear();
    m_probe_data.clear();
    m_input_buffers.clear();
    m_output_buffers.clear();
    m_stats = SimulationStats();
    m_simulation_active = false;
    m_initialized = true;

    std::cout << "[JITManager] Initialized successfully" << std::endl;
}

void JITManager::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_components.clear();
    m_probe_data.clear();
    m_input_buffers.clear();
    m_output_buffers.clear();
    m_initialized = false;
    m_simulation_active = false;

    std::cout << "[JITManager] Finalized" << std::endl;
}

// ============================================================================
// Component Registration
// ============================================================================

bool JITManager::registerComponent(const std::string& name, const std::string& source_code,
                                   int num_inputs, int num_outputs, std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        if (error) *error = "JITManager not initialized";
        return false;
    }

    if (m_components.count(name)) {
        if (error) *error = "Component already exists: " + name;
        return false;
    }

    JITComponent comp;
    comp.name = name;
    comp.source_code = source_code;
    comp.num_inputs = num_inputs;
    comp.num_outputs = num_outputs;
    comp.state.name = name;
    comp.state.inputs.resize(num_inputs, 0.0);
    comp.state.outputs.resize(num_outputs, 0.0);
    comp.is_hot_reloadable = true;

    m_components[name] = std::move(comp);
    m_stats.total_components++;

    std::cout << "[JITManager] Registered component: " << name 
              << " (inputs: " << num_inputs << ", outputs: " << num_outputs << ")" << std::endl;

    return true;
}

bool JITManager::unregisterComponent(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }

    m_probe_data.erase(name);
    m_input_buffers.erase(name);
    m_output_buffers.erase(name);
    m_components.erase(it);
    m_stats.total_components--;

    std::cout << "[JITManager] Unregistered component: " << name << std::endl;
    return true;
}

bool JITManager::hasComponent(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_components.count(name) > 0;
}

JITComponent* JITManager::getComponent(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return nullptr;
    }
    return &it->second;
}

const JITComponent* JITManager::getComponent(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> JITManager::getComponentNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, comp] : m_components) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Compilation
// ============================================================================

bool JITManager::compileComponent(const std::string& name, std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_components.find(name);
    if (it == m_components.end()) {
        if (error) *error = "Component not found: " + name;
        return false;
    }

    return compileSource(it->second.source_code, it->second, error);
}

bool JITManager::recompileComponent(const std::string& name, const std::string& new_source,
                                    std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_components.find(name);
    if (it == m_components.end()) {
        if (error) *error = "Component not found: " + name;
        return false;
    }

    it->second.source_code = new_source;
    return compileSource(new_source, it->second, error);
}

bool JITManager::hotReloadComponent(const std::string& name, const std::string& new_source,
                                    std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_components.find(name);
    if (it == m_components.end()) {
        if (error) *error = "Component not found: " + name;
        return false;
    }

    if (!it->second.is_hot_reloadable) {
        if (error) *error = "Component is not hot-reloadable: " + name;
        return false;
    }

    // Save current state
    ComponentState saved_state = it->second.state;

    // Compile new source
    it->second.source_code = new_source;
    bool success = compileSource(new_source, it->second, error);

    if (success) {
        // Restore state
        it->second.state = saved_state;
        m_stats.hot_reload_count++;
        std::cout << "[JITManager] Hot-reloaded component: " << name << std::endl;
    }

    return success;
}

bool JITManager::compileSource(const std::string& source, JITComponent& comp, std::string* error) {
    if (source.empty()) {
        if (error) *error = "Component source is empty";
        comp.jit.reset();
        comp.update_func = nullptr;
        comp.init_func = nullptr;
        comp.eval_func = nullptr;
        comp.state.is_valid = false;
        return false;
    }

    try {
        // Create a new JIT instance for this component
        comp.jit = std::make_unique<FluxJIT>();

        // Use the real FluxScript compiler pipeline
        Flux::CompilerOptions opts;
        opts.injectStdlib = true;
        opts.optimizationLevel = Flux::OptimizationLevel::O0;  // O0 for debugging
        opts.inputName = comp.name;
        opts.moduleName = "component_" + comp.name;

        std::cout << "[JITManager] Creating compiler for: " << comp.name << std::endl;
        Flux::CompilerInstance compiler(opts);

        auto artifacts = compiler.compileToIR(source, error);

        if (!artifacts || !artifacts->codegenContext || !artifacts->codegenContext->TheModule) {
            comp.state.is_valid = false;
            if (!error || error->empty()) {
                if (error) *error = "Compilation produced no module";
            }
            return false;
        }

        // Move the compiled module into the JIT
        // Debug: print module functions before adding
        std::cout << "[JITManager] Functions in module before JIT add:" << std::endl;
        for (auto& F : *artifacts->codegenContext->TheModule) {
            std::cout << "  " << F.getName().str() << " (linkage: " << F.getLinkage() << ")" << std::endl;
        }

        comp.jit->addModule(
            std::move(artifacts->codegenContext->OwnedModule),
            std::move(artifacts->codegenContext->OwnedContext)
        );

        // Register runtime functions (sin, cos, etc.) so the JIT can resolve them
        registerRuntimeFunctions(*comp.jit);

        // Get function pointers from the JIT
        // Find the function name from the source (first "def name" pattern)
        std::string func_name = "update";
        size_t def_pos = source.find("def ");
        if (def_pos != std::string::npos) {
            size_t name_start = def_pos + 4;
            size_t name_end = source.find_first_of("( \t\n", name_start);
            if (name_end != std::string::npos && name_end > name_start) {
                func_name = source.substr(name_start, name_end - name_start);
            }
        }

        comp.update_func = comp.jit->getPointerToFunction(func_name);
        comp.eval_func = comp.jit->getPointerToFunction("eval");

        // If no function was found, that's an error
        if (!comp.update_func) {
            if (error) *error = "No function '" + func_name + "' defined in component source";
            comp.state.is_valid = false;
            return false;
        }

        comp.state.is_valid = true;
        comp.compile_count++;
        comp.last_compile_time = std::chrono::steady_clock::now();
        m_stats.active_components++;

        std::cout << "[JITManager] Compiled component: " << comp.name
                  << " (update: " << (comp.update_func ? "OK" : "FAIL")
                  << ", eval: " << (comp.eval_func ? "OK" : "FAIL")
                  << ")" << std::endl;
        return true;
    } catch (const std::exception& e) {
        if (error) *error = std::string("Compilation error: ") + e.what();
        comp.state.is_valid = false;
        return false;
    }
}

bool JITManager::validateComponent(const JITComponent& comp, std::string* error) const {
    if (!comp.state.is_valid) {
        if (error) *error = "Component not compiled: " + comp.name;
        return false;
    }

    if (!comp.update_func) {
        if (error) *error = "Component missing update function: " + comp.name;
        return false;
    }

    return true;
}

// ============================================================================
// Simulation Control
// ============================================================================

bool JITManager::initializeSimulation() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_simulation_active = true;
    m_stats.total_sim_time = 0.0;
    m_stats.total_evaluations = 0;

    // Initialize all components
    for (auto& [name, comp] : m_components) {
        if (comp.init_func) {
            // Call init function
            typedef void (*InitFunc)(void*);
            auto init = reinterpret_cast<InitFunc>(comp.init_func);
            init(&comp.state);
        }

        comp.state.current_time = 0.0;
        comp.state.dt = 1e-12;
    }

    std::cout << "[JITManager] Simulation initialized with " 
              << m_components.size() << " components" << std::endl;
    return true;
}

bool JITManager::stepSimulation(double time, double dt) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_simulation_active) {
        return false;
    }

    m_stats.total_sim_time = time;
    bool all_success = true;

    for (auto& [name, comp] : m_components) {
        comp.state.current_time = time;
        comp.state.dt = dt;

        // Evaluate component
        if (!evaluateComponent(name, time, dt, 
                              comp.state.inputs.data(), 
                              comp.state.outputs.data())) {
            all_success = false;
            std::cerr << "[JITManager] Failed to evaluate component: " << name << std::endl;
        }
    }

    return all_success;
}

bool JITManager::evaluateComponent(const std::string& name, double time, double dt,
                                   const double* inputs, double* outputs) {
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }

    auto& comp = it->second;
    if (!comp.update_func) {
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();

    // Detect function signature from the JIT module
    // The compiled function could be:
    //   double update(double, double)        2 scalar args (def update(t, x))
    //   double update(double, double*)       scalar + pointer (def update(t, inputs))
    double result = 0.0;

    // Try 2-scalar-arg signature first (most common for simple components)
    typedef double (*UpdateFunc2SS)(double, double);
    auto update2ss = reinterpret_cast<UpdateFunc2SS>(comp.update_func);
    result = update2ss(time, inputs ? inputs[0] : 0.0);
    if (outputs) outputs[0] = result;

    auto end_time = std::chrono::steady_clock::now();
    double eval_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();

    // Update statistics
    m_stats.total_evaluations++;
    m_stats.avg_eval_time_us = (m_stats.avg_eval_time_us * (m_stats.total_evaluations - 1) +
                                eval_time_us) / m_stats.total_evaluations;

    // Record probe data if enabled
    if (m_probe_data.count(name)) {
        for (auto& [signal_name, data] : m_probe_data[name]) {
            if (signal_name == "output") {
                for (int i = 0; i < comp.num_outputs; i++) {
                    data.push_back(outputs[i]);
                }
            }
        }
    }

    return true;
}

bool JITManager::evaluateAllComponents(double time, double dt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool all_success = true;

    for (auto& [name, comp] : m_components) {
        if (!evaluateComponent(name, time, dt,
                              comp.state.inputs.data(),
                              comp.state.outputs.data())) {
            all_success = false;
        }
    }

    return all_success;
}

// ============================================================================
// State Management
// ============================================================================

bool JITManager::setComponentState(const std::string& name, const ComponentState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }
    it->second.state = state;
    return true;
}

bool JITManager::getComponentState(const std::string& name, ComponentState& state) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }
    state = it->second.state;
    return true;
}

bool JITManager::resetComponentState(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }
    it->second.state = ComponentState();
    it->second.state.name = name;
    it->second.state.inputs.resize(it->second.num_inputs, 0.0);
    it->second.state.outputs.resize(it->second.num_outputs, 0.0);
    return true;
}

bool JITManager::resetAllStates() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, comp] : m_components) {
        comp.state = ComponentState();
        comp.state.name = name;
        comp.state.inputs.resize(comp.num_inputs, 0.0);
        comp.state.outputs.resize(comp.num_outputs, 0.0);
    }
    return true;
}

// ============================================================================
// Zero-Copy Data Interface
// ============================================================================

bool JITManager::bindInputBuffer(const std::string& name, double* buffer, int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }
    m_input_buffers[name] = buffer;
    it->second.state.inputs.assign(buffer, buffer + size);
    return true;
}

bool JITManager::bindOutputBuffer(const std::string& name, double* buffer, int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        return false;
    }
    m_output_buffers[name] = buffer;
    return true;
}

double* JITManager::getInputBuffer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_input_buffers.find(name);
    if (it == m_input_buffers.end()) {
        return nullptr;
    }
    return it->second;
}

double* JITManager::getOutputBuffer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_output_buffers.find(name);
    if (it == m_output_buffers.end()) {
        return nullptr;
    }
    return it->second;
}

// ============================================================================
// Signal Generation Support
// ============================================================================

bool JITManager::registerSignalGenerator(const std::string& name, const std::string& source_code,
                                         std::string* error) {
    // Register as a component with 0 inputs and 1 output (signal generator)
    bool result = registerComponent(name, source_code, 0, 1, error);
    if (result) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_components.find(name);
        if (it != m_components.end()) {
            it->second.is_signal_generator = true;
            it->second.component_type = "signal";
        }
    }
    return result;
}

double JITManager::evaluateSignal(const std::string& name, double time) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end() || !it->second.eval_func) {
        return 0.0;
    }

    // Call eval function: eval(time) -> value
    typedef double (*EvalFunc)(double);
    auto eval = reinterpret_cast<EvalFunc>(it->second.eval_func);
    return eval(time);
}

std::vector<double> JITManager::generateWaveform(const std::string& name,
                                                 double t_start, double t_stop, double sample_rate,
                                                 std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_components.find(name);
    if (it == m_components.end()) {
        if (error) *error = "Component not found: " + name;
        return {};
    }

    if (!it->second.eval_func) {
        if (error) *error = "Component has no eval function: " + name;
        return {};
    }

    double dt = 1.0 / sample_rate;
    int num_points = static_cast<int>((t_stop - t_start) / dt) + 1;
    
    std::vector<double> values;
    values.reserve(num_points);

    typedef double (*EvalFunc)(double);
    auto eval = reinterpret_cast<EvalFunc>(it->second.eval_func);

    for (int i = 0; i < num_points; i++) {
        double t = t_start + i * dt;
        values.push_back(eval(t));
    }

    return values;
}

// ============================================================================
// Probing
// ============================================================================

bool JITManager::enableProbe(const std::string& component_name, const std::string& signal_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_probe_data[component_name][signal_name] = std::vector<double>();
    return true;
}

bool JITManager::disableProbe(const std::string& component_name, const std::string& signal_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_probe_data.find(component_name);
    if (it == m_probe_data.end()) {
        return false;
    }
    it->second.erase(signal_name);
    return true;
}

std::vector<double> JITManager::getProbeData(const std::string& component_name,
                                             const std::string& signal_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_probe_data.find(component_name);
    if (it == m_probe_data.end()) {
        return {};
    }
    auto sit = it->second.find(signal_name);
    if (sit == it->second.end()) {
        return {};
    }
    return sit->second;
}

bool JITManager::clearProbeData(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_probe_data.erase(component_name);
    return true;
}

// ============================================================================
// Statistics
// ============================================================================

SimulationStats JITManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

double JITManager::getComponentEvalTime(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Return average evaluation time for component
    return m_stats.avg_eval_time_us;
}

void JITManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_sim_time = 0.0;
    m_stats.avg_eval_time_us = 0.0;
    m_stats.total_evaluations = 0;
    m_stats.hot_reload_count = 0;
}

// ============================================================================
// JIT Utilities
// ============================================================================

namespace JITUtils {

std::string generateComponentTemplate(const std::string& name, int num_inputs, int num_outputs) {
    std::ostringstream oss;
    oss << "// " << name << " - JIT Component Template\n";
    oss << "// Inputs: " << num_inputs << ", Outputs: " << num_outputs << "\n\n";
    oss << "update(t, inputs) {\n";
    oss << "    // t: current simulation time\n";
    oss << "    // inputs: array of input voltages\n\n";
    
    for (int i = 0; i < num_inputs; i++) {
        oss << "    var vin" << i << " = inputs[" << i << "];\n";
    }
    
    oss << "\n    // TODO: Implement component behavior\n";
    oss << "    var vout = 0.0;\n\n";
    
    oss << "    return vout;\n";
    oss << "}\n";
    
    return oss.str();
}

std::string generateTestbench(const std::string& component_name) {
    std::ostringstream oss;
    oss << "// Testbench for " << component_name << "\n\n";
    oss << "import \"signal\"\n";
    oss << "import \"measurement\"\n\n";
    oss << "// Setup test stimulus\n";
    oss << "sine_wave(\"vin\", \"in\", 0, 1.0, 1e3);\n\n";
    oss << "// Setup analysis\n";
    oss << "analysis tran {\n";
    oss << "    tstop = 5e-3,\n";
    oss << "    tstep = 1e-6\n";
    oss << "}\n\n";
    oss << "// Measure output\n";
    oss << "measure vout_rms RMS {\n";
    oss << "    V(out),\n";
    oss << "    from = 2e-3,\n";
    oss << "    to = 5e-3\n";
    oss << "}\n";
    
    return oss.str();
}

} // namespace JITUtils

} // namespace Flux
