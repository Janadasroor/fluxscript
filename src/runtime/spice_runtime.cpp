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

// Get current simulation time
double flux_get_time() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    return g_sim_time;
}

// Get current time step
double flux_get_dt() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    return g_sim_dt;
}

// Get simulation temperature
double flux_get_temp() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    return g_sim_temp;
}

// Get voltage at node
double flux_get_voltage(const char* node_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_node_voltages.find(node_name);
    if (it != g_node_voltages.end()) {
        return it->second;
    }
    return 0.0;
}

// Get current through branch
double flux_get_current(const char* branch_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_branch_currents.find(branch_name);
    if (it != g_branch_currents.end()) {
        return it->second;
    }
    return 0.0;
}

// Get parameter value
double flux_get_parameter(const char* param_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    auto it = g_parameters.find(param_name);
    if (it != g_parameters.end()) {
        return it->second;
    }
    return 0.0;
}

// Set parameter value
void flux_set_parameter(const char* param_name, double value) {
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

double flux_register_bsource(const char* name, const char* pos_node, const char* neg_node, const char* type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_bsources.push_back({name, pos_node, neg_node, type});
    printf("[SPICE] Registered B-source: %s (%s) between nodes %s and %s\n", name, type, pos_node, neg_node);
    return 0.0;
}

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
    g_current_analysis = analysis_type;
    printf("[SPICE] Registered analysis: .%s\n", analysis_type);
    return 0.0;
}

double flux_register_measure(const char* name, const char* measure_type) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_measures[name] = {name, measure_type, 0.0, false};
    printf("[SPICE] Registered measure: %s (%s)\n", name, measure_type);
    return 0.0;
}

double flux_register_probe(const char* var_name, const char* output_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_probes.push_back({var_name, output_name});
    printf("[SPICE] Registered probe: %s as \"%s\"\n", var_name, output_name);
    return 0.0;
}

double flux_register_save(const char* var_name) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_save_vars.push_back(var_name);
    printf("[SPICE] Registered save: %s\n", var_name);
    return 0.0;
}

// ============================================================================
// Subcircuits and Models
// ============================================================================

double flux_register_subckt(double name_ptr_double, double pins_ptr_double) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    
    const char* name = bit_cast<const char*>(name_ptr_double);
    const char* pins = bit_cast<const char*>(pins_ptr_double);
    
    SubcktInfo info;
    info.name = name;
    
    // Parse pin list
    char* pins_copy = strdup(pins);
    char* token = strtok(pins_copy, " ");
    while (token) {
        info.pins.push_back(token);
        token = strtok(nullptr, " ");
    }
    free(pins_copy);
    
    g_subckts[name] = info;
    printf("[SPICE] Registered subcircuit: %s with %zu pins\n", name, info.pins.size());
    return 0.0;
}

double flux_register_model(double name_ptr_double, double model_type_ptr_double) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    
    const char* name = bit_cast<const char*>(name_ptr_double);
    const char* model_type = bit_cast<const char*>(model_type_ptr_double);
    
    g_models[name] = {name, model_type};
    printf("[SPICE] Registered model: %s (type: %s)\n", name, model_type);
    return 0.0;
}

double flux_register_param(const char* name, double value) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_parameters[name] = value;
    printf("[SPICE] Registered parameter: %s = %g\n", name, value);
    return 0.0;
}

double flux_register_ic(const char* node_name, double value) {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_node_voltages[node_name] = value;
    printf("[SPICE] Registered initial condition: V(%s) = %g\n", node_name, value);
    return 0.0;
}

// ============================================================================
// ngspice Callback Interface
// ============================================================================

// This function is called by ngspice during transient analysis
// It evaluates the behavioral source expression and returns the result
double flux_evaluate_bsource(const char* bsource_name, double time, double dt,
                             const char** node_names, double* node_values, int num_nodes) {
    // Update global state
    flux_update_state(time, dt, node_names, node_values, num_nodes);
    
    // The actual expression evaluation is done by the JIT-compiled update() function
    // This is just a placeholder for the callback mechanism
    printf("[SPICE] Evaluating B-source: %s at time=%g\n", bsource_name, time);
    
    return 0.0;  // Placeholder
}

// Print simulation summary
void flux_print_summary() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    
    printf("\n=== FluxScript SPICE Simulation Summary ===\n");
    printf("B-sources: %zu\n", g_bsources.size());
    printf("Subcircuits: %zu\n", g_subckts.size());
    printf("Models: %zu\n", g_models.size());
    printf("Parameters: %zu\n", g_parameters.size());
    printf("Probes: %zu\n", g_probes.size());
    printf("Save variables: %zu\n", g_save_vars.size());
    printf("Measures: %zu\n", g_measures.size());
    printf("==========================================\n\n");
}

// Reset simulation state
void flux_reset_simulation() {
    std::lock_guard<std::mutex> lock(g_sim_mutex);
    g_sim_time = 0.0;
    g_sim_dt = 1e-12;
    g_node_voltages.clear();
    g_branch_currents.clear();
    g_bsources.clear();
    g_probes.clear();
    g_save_vars.clear();
    g_measures.clear();
}

} // extern "C"
