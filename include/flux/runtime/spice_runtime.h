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

// SPICE Runtime Library Header
// This file declares the runtime functions used by JIT-compiled SPICE simulation code.

#ifndef FLUX_RUNTIME_SPICE_RUNTIME_H
#define FLUX_RUNTIME_SPICE_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Time-Domain Simulation API
// ============================================================================

// Get current simulation time
double flux_get_time(void);

// Get current time step
double flux_get_dt(void);

// Get simulation temperature
double flux_get_temp(void);

// Get voltage at node
double flux_get_voltage(const char* node_name);

// Get current through branch
double flux_get_current(const char* branch_name);

// Get parameter value
double flux_get_parameter(const char* param_name);

// Set parameter value
void flux_set_parameter(const char* param_name, double value);

// Update simulation state (called by ngspice each timestep)
void flux_update_state(double time, double dt, const char** node_names, double* node_values, int num_nodes);

// ============================================================================
// Behavioral Source Registration
// ============================================================================

double flux_register_bsource(const char* name, const char* pos_node, const char* neg_node, const char* type);
double flux_register_esource(const char* name, const char* pos_node, const char* neg_node, const char* ctrl_pos,
                             const char* ctrl_neg, double gain);
double flux_register_fsource(const char* name, const char* pos_node, const char* neg_node, const char* vsense,
                             double gain);
double flux_register_gsource(const char* name, const char* pos_node, const char* neg_node, const char* ctrl_pos,
                             const char* ctrl_neg, double transcond);
double flux_register_hsource(const char* name, const char* pos_node, const char* neg_node, const char* vsense,
                             double transres);

// A-Device digital gates
void flux_register_adevice(const char* name, int device_type, const char* input_nodes, const char* output_nodes);

// WAVEFILE source
void flux_register_wavefile(const char* name, const char* pos_node, const char* neg_node,
                            const char* file_path, int channel);

// ============================================================================
// Analysis Control
// ============================================================================

double flux_register_analysis(const char* analysis_type);
double flux_register_measure(const char* name, const char* measure_type);
double flux_register_probe(const char* var_name, const char* output_name);
double flux_register_save(const char* var_name);

// ============================================================================
// Subcircuits and Models
// ============================================================================

double flux_register_subckt(double name_ptr_double, double pins_ptr_double);
double flux_register_model(double name_ptr_double, double model_type_ptr_double);
double flux_register_param(const char* name, double value);
double flux_register_ic(const char* node_name, double value);

// ============================================================================
// ngspice Callback Interface
// ============================================================================

// This function is called by ngspice during transient analysis
double flux_evaluate_bsource(const char* bsource_name, double time, double dt, const char** node_names,
                             double* node_values, int num_nodes);

// Print simulation summary
void flux_print_summary(void);

// Reset simulation state
void flux_reset_simulation(void);

#ifdef __cplusplus
}
#endif

#endif // FLUX_RUNTIME_SPICE_RUNTIME_H
