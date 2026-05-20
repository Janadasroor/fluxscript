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

// SPICE Runtime Library - ngspice callback system
// This file provides the runtime functions that are called by JIT-compiled code
// to interface with the ngspice simulation engine.

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

// ============================================================================
// Global Simulation State
// ============================================================================

namespace {
    // Current simulation time
    double g_sim_time = 0.0;
    
    // Time step
    double g_sim_dt = 1e-12;
    
    // Temperature (Celsius)
    double g_sim_temp = 27.0;
    
    // Node voltages (updated by ngspice each timestep)
    std::map<std::string, double> g_node_voltages;
    
    // Branch currents
    std::map<std::string, double> g_branch_currents;
    
    // Parameters
    std::map<std::string, double> g_parameters;
    
    // Registered behavioral sources
    struct BSourceInfo {
        std::string name;
        std::string pos_node;
        std::string neg_node;
        std::string type;  // "V" or "I"
    };
    std::vector<BSourceInfo> g_bsources;
    
    // Registered analysis
    std::string g_current_analysis;
    
    // Measurements
    struct MeasureInfo {
        std::string name;
        std::string type;
        double result;
        bool computed;
    };
    std::map<std::string, MeasureInfo> g_measures;
    
    // Probes
    std::vector<std::pair<std::string, std::string>> g_probes;  // (varname, outputname)
    
    // Save list
    std::vector<std::string> g_save_vars;
    
    // Subcircuits
    struct SubcktInfo {
        std::string name;
        std::vector<std::string> pins;
    };
    std::map<std::string, SubcktInfo> g_subckts;
    
    // Models
    struct ModelInfo {
        std::string name;
        std::string type;
    };
    std::map<std::string, ModelInfo> g_models;
    
    // Mutex for thread safety
    std::mutex g_sim_mutex;
}

// ============================================================================
// Time-Domain Simulation API
// ============================================================================

extern "C" {

// Get voltage at node
double flux_get_voltage(double node_dbl) {
    const char* node_name = bit_cast<const char*>(node_dbl);
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_node_voltages.find(node_name);
    if (it != g_node_voltages.end()) {
        return it->second;
    }
    return 0.0;
}

// Get current through branch
double flux_get_current(double branch_dbl) {
    const char* branch_name = bit_cast<const char*>(branch_dbl);
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_branch_currents.find(branch_name);
    if (it != g_branch_currents.end()) {
        return it->second;
    }
    return 0.0;
}

// Get parameter value
double flux_get_parameter(double param_dbl) {
    const char* param_name = bit_cast<const char*>(param_dbl);
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_parameters.find(param_name);
    if (it != g_parameters.end()) {
        return it->second;
    }
    return 0.0;
}

// Set parameter value
void flux_set_parameter(double param_dbl, double value) {
    const char* param_name = bit_cast<const char*>(param_dbl);
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_parameters[param_name] = value;
}

// Update simulation state (called by ngspice each timestep)
void flux_update_state(double time, double dt, const char** node_names, double* node_values, int num_nodes) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_sim_time = time;
    g_sim_dt = dt;
    
    for (int i = 0; i < num_nodes; ++i) {
        g_node_voltages[node_names[i]] = node_values[i];
    }
}

// ============================================================================
// Behavioral Source Registration
// ============================================================================

double flux_register_esource(const char* name, const char* pos_node, const char* neg_node,
                             const char* ctrl_pos, const char* ctrl_neg, double gain) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    printf("[SPICE] Registered E-source (VCVS): %s from V(%s,%s) * %g\n", 
           name, ctrl_pos, ctrl_neg, gain);
    return 0.0;
}

double flux_register_fsource(const char* name, const char* pos_node, const char* neg_node,
                             const char* vsense, double gain) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    printf("[SPICE] Registered F-source (CCCS): %s controlled by current through %s * %g\n",
           name, vsense, gain);
    return 0.0;
}

double flux_register_gsource(const char* name, const char* pos_node, const char* neg_node,
                             const char* ctrl_pos, const char* ctrl_neg, double transcond) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    printf("[SPICE] Registered G-source (VCCS): %s from V(%s,%s) * %g S\n",
           name, ctrl_pos, ctrl_neg, transcond);
    return 0.0;
}

double flux_register_hsource(const char* name, const char* pos_node, const char* neg_node,
                             const char* vsense, double transres) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    printf("[SPICE] Registered H-source (CCVS): %s controlled by current through %s * %g Ohm\n",
           name, vsense, transres);
    return 0.0;
}

// ============================================================================
// Analysis Control
// ============================================================================

double flux_register_analysis(const char* analysis_type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_current_analysis = analysis_type ? analysis_type : "";
    printf("[SPICE] Registered analysis: .%s\n", analysis_type ? analysis_type : "?");
    return 0.0;
}

double flux_register_measure(const char* name, const char* measure_type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    std::string n = name ? name : "";
    std::string t = measure_type ? measure_type : "";
    g_measures[n] = {n, t, 0.0, false};
    printf("[SPICE] Registered measure: %s (%s)\n", n.c_str(), t.c_str());
    return 0.0;
}

double flux_register_probe(const char* var_name, const char* output_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_probes.push_back({var_name ? var_name : "", output_name ? output_name : ""});
    printf("[SPICE] Registered probe: %s as \"%s\"\n", var_name ? var_name : "?", output_name ? output_name : "?");
    return 0.0;
}

double flux_register_save(const char* var_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_save_vars.push_back(var_name ? var_name : "");
    printf("[SPICE] Registered save: %s\n", var_name ? var_name : "?");
    return 0.0;
}

// ============================================================================
// Subcircuits and Models
// ============================================================================

double flux_register_model(const char* name, const char* model_type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    
    g_models[name ? name : ""] = {name ? name : "", model_type ? model_type : ""};
    printf("[SPICE] Registered model: %s (type: %s)\n", name ? name : "?", model_type ? model_type : "?");
    return 0.0;
}

double flux_register_bsource(const char* name, const char* pos_node, const char* neg_node, const char* type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    BSourceInfo info;
    info.name = name ? name : "";
    info.pos_node = pos_node ? pos_node : "";
    info.neg_node = neg_node ? neg_node : "";
    info.type = type ? type : "V";
    g_bsources.push_back(info);
    printf("[SPICE] Registered B-source: %s %s %s type=%s\n",
           info.name.c_str(), info.pos_node.c_str(), info.neg_node.c_str(), info.type.c_str());
    return 0.0;
}

double flux_register_subckt(double name_ptr_double, double pins_ptr_double) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    const char* name = bit_cast<const char*>(name_ptr_double);
    const char* pins_str = bit_cast<const char*>(pins_ptr_double);
    SubcktInfo info;
    info.name = name ? name : "";
    // Parse space-separated pins
    if (pins_str) {
        std::string pins(pins_str);
        size_t start = 0;
        while (true) {
            size_t end = pins.find(' ', start);
            if (end == std::string::npos) {
                std::string pin = pins.substr(start);
                if (!pin.empty()) info.pins.push_back(pin);
                break;
            }
            std::string pin = pins.substr(start, end - start);
            if (!pin.empty()) info.pins.push_back(pin);
            start = end + 1;
        }
    }
    g_subckts[name ? name : ""] = info;
    printf("[SPICE] Registered subcircuit: %s with %zu pins\n",
           info.name.c_str(), info.pins.size());
    return 0.0;
}

double flux_register_param(const char* name, double value) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_parameters[name ? name : ""] = value;
    printf("[SPICE] Registered parameter: %s = %g\n", name ? name : "?", value);
    return 0.0;
}

double flux_register_ic(const char* node_name, double value) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_node_voltages[node_name ? node_name : ""] = value;
    printf("[SPICE] Registered initial condition: V(%s) = %g\n", node_name ? node_name : "?", value);
    return 0.0;
}

// ============================================================================
// VioMATRIXC Integration: A-Device and WAVEFILE Storage
// ============================================================================

struct ADeviceInfo {
    std::string instanceName;
    int deviceType;
    std::vector<std::string> inputNodes;
    std::vector<std::string> outputNodes;
};
static std::vector<ADeviceInfo> g_adevices;

struct WaveFileInfo {
    std::string instanceName;
    std::string positiveNode;
    std::string negativeNode;
    std::string filePath;
    int channel;
    bool isCurrent;
};
static std::vector<WaveFileInfo> g_wavefiles;

// ============================================================================
// Reset simulated state
// ============================================================================
void flux_reset_simulation() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_sim_time = 0.0;
    g_sim_dt = 1e-12;
    g_node_voltages.clear();
    g_branch_currents.clear();
    g_bsources.clear();
    g_adevices.clear();
    g_wavefiles.clear();
    g_probes.clear();
    g_save_vars.clear();
    g_measures.clear();
}

// ============================================================================
// VioMATRIXC Integration: A-Device Registration
// ============================================================================

void flux_register_adevice(const char* name, int deviceType,
                           const char* inputNodesStr, const char* outputNodesStr) {
    ADeviceInfo info;
    info.instanceName = name ? name : "";
    info.deviceType = deviceType;

    // Parse comma-separated input nodes
    if (inputNodesStr && inputNodesStr[0]) {
        std::string inputs(inputNodesStr);
        size_t start = 0;
        while (true) {
            size_t end = inputs.find(',', start);
            if (end == std::string::npos) {
                std::string node = inputs.substr(start);
                if (!node.empty()) info.inputNodes.push_back(node);
                break;
            }
            std::string node = inputs.substr(start, end - start);
            if (!node.empty()) info.inputNodes.push_back(node);
            start = end + 1;
        }
    }

    // Parse comma-separated output nodes
    if (outputNodesStr && outputNodesStr[0]) {
        std::string outputs(outputNodesStr);
        size_t start = 0;
        while (true) {
            size_t end = outputs.find(',', start);
            if (end == std::string::npos) {
                std::string node = outputs.substr(start);
                if (!node.empty()) info.outputNodes.push_back(node);
                break;
            }
            std::string node = outputs.substr(start, end - start);
            if (!node.empty()) info.outputNodes.push_back(node);
            start = end + 1;
        }
    }

    g_adevices.push_back(info);
    printf("[SPICE] Registered A-device: A%s type=%d inputs=[%s] outputs=[%s]\n",
           name, deviceType, inputNodesStr, outputNodesStr);
}

// ============================================================================
// VioMATRIXC Integration: WAVEFILE Source Registration
// ============================================================================

// WaveFileInfo and g_wavefiles defined above at line 312

void flux_register_wavefile(const char* name, const char* posNode, const char* negNode,
                            const char* filePath, int channel) {
    WaveFileInfo info;
    info.instanceName = name ? name : "";
    info.positiveNode = posNode ? posNode : "";
    info.negativeNode = negNode ? negNode : "";
    info.filePath = filePath ? filePath : "";
    info.channel = channel;
    info.isCurrent = (name && name[0] == 'I');  // 'I' prefix = current source

    g_wavefiles.push_back(info);
    char type = info.isCurrent ? 'I' : 'V';
    printf("[SPICE] Registered WAVEFILE: %c%s nodes=%s,%s file=\"%s\" chan=%d\n",
           type, name, posNode, negNode, filePath, channel);
}

// ngspice Callback Interface
// ============================================================================

// Print simulation summary
void flux_print_summary() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);

    printf("\n=== FluxScript SPICE Simulation Summary ===\n");
    printf("B-sources: %zu\n", g_bsources.size());
    printf("A-devices: %zu\n", g_adevices.size());
    printf("WAVEFILEs: %zu\n", g_wavefiles.size());
    printf("Subcircuits: %zu\n", g_subckts.size());
    printf("Models: %zu\n", g_models.size());
    printf("Parameters: %zu\n", g_parameters.size());
    printf("Probes: %zu\n", g_probes.size());
    printf("Save variables: %zu\n", g_save_vars.size());
    printf("Measures: %zu\n", g_measures.size());
    printf("==========================================\n\n");
}

} // extern "C"
