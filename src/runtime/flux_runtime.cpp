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

#include "flux/runtime/flux_runtime.h"

#include "flux/jit/flux_jit.h"
#include "flux/runtime/symbolic_engine.h"
#include "flux/runtime/mixed_signal_runtime.h"

#include <Eigen/Dense>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef FLUX_HAS_QT
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPixmap>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>
#endif


#include "flux/runtime/flux_sim_service.h"
#include "flux/runtime/mixed_signal_runtime.h"
#include "flux/runtime/time_domain_api.h"

// Global service pointer definition
extern "C" FluxSimulationService* g_flux_sim_service = nullptr;

// Symbolic function declarations (must be before registerRuntimeFunctions)
extern "C" double flux_sym_decl(const char* name);
extern "C" double flux_sym_number(double val);
extern "C" double flux_sym_add(double a_ptr, double b_ptr);
extern "C" double flux_sym_sub(double a_ptr, double b_ptr);
extern "C" double flux_sym_mul(double a_ptr, double b_ptr);
extern "C" double flux_sym_div(double a_ptr, double b_ptr);
extern "C" double flux_sym_pow(double a_ptr, double b_ptr);
extern "C" double flux_sym_simplify(double expr_ptr);
extern "C" double flux_sym_differentiate(double expr_ptr, const char* var);
extern "C" double flux_sym_substitute(double expr_ptr, double var_count, void* var_names_ptr, void* var_values_ptr);
extern "C" double flux_sym_solve(double lhs_ptr, double rhs_ptr, const char* var);
extern "C" double flux_sym_evaluate(double expr_ptr, double var_count, void* var_names_ptr, void* var_values_ptr);
extern "C" void* flux_sym_jacobian(void* exprs_ptr, int expr_count, void* vars_ptr, int var_count);
extern "C" double flux_sym_pde_register(double eq_ptr, int var_count, void* var_names_ptr);
extern "C" double flux_sym_pdiff(double expr_ptr, const char* var, int order);
extern "C" double flux_sym_expand(double expr_ptr);
extern "C" double flux_sym_factor(double expr_ptr);
extern "C" double flux_sym_laplace(double expr_ptr, const char* t_var, const char* s_var);
extern "C" double flux_sym_inverse_laplace(double expr_ptr, const char* s_var, const char* t_var);
extern "C" double flux_sym_poles(double expr_ptr);
extern "C" double flux_sym_zeros(double expr_ptr);
extern "C" double flux_sym_to_string(double expr_ptr);
extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr);
extern "C" double flux_print_double(double x);
extern "C" double flux_matrix_det(void* m_ptr);
extern "C" void* flux_matrix_inv(void* m_ptr);
extern "C" void* flux_matrix_eig(void* m_ptr);
extern "C" double flux_vector_dot(double* v1_ptr, int len1, double* v2_ptr, int len2);
extern "C" void* flux_vector_cross(double* v1_ptr, int len1, double* v2_ptr, int len2);
extern "C" double flux_vector_norm(double* v_ptr, int len);
extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr);
extern "C" void* flux_matrix_pow(void* m_ptr, double p);
extern "C" void* flux_matrix_transpose(void* m_ptr);
extern "C" int flux_matrix_rows(void* m_ptr);
extern "C" int flux_matrix_cols(void* m_ptr);
extern "C" double flux_register_analysis(const char* type);
extern "C" double flux_register_measure(const char* name, const char* type);
extern "C" double flux_register_subckt(double name_ptr, double pins_ptr);
extern "C" double flux_register_model(double name_ptr, double type_ptr);
extern "C" double flux_register_param(const char* name, double value);

extern "C" const char* flux_string_concat_double(const char* s, double d);
extern "C" const char* flux_string_concat_matrix(const char* s, double* data, int rows, int cols);
extern "C" const char* flux_string_concat_string(const char* s1, const char* s2);

// Forward declaration of exception handler
extern "C" void flux_throw_error(double error_code, const char* message);
extern "C" void* flux_get_current_jmp_buf();
extern "C" void flux_push_jmp_buf(void* buf);
extern "C" void flux_pop_jmp_buf();

namespace Flux {

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

void registerRuntimeFunctions(FluxJIT& jit) {
    jit.registerFunction("flux_create_vector_sum", reinterpret_cast<void*>(&flux_create_vector_sum));
    jit.registerFunction("flux_matrix_mul", reinterpret_cast<void*>(&flux_matrix_mul));
    jit.registerFunction("flux_matrix_mul_ms", reinterpret_cast<void*>(&flux_matrix_mul_ms));
    jit.registerFunction("flux_matrix_add", reinterpret_cast<void*>(&flux_matrix_add));
    jit.registerFunction("flux_matrix_sub", reinterpret_cast<void*>(&flux_matrix_sub));
    jit.registerFunction("flux_matrix_ew_mul", reinterpret_cast<void*>(&flux_matrix_ew_mul));
    jit.registerFunction("flux_matrix_ew_div", reinterpret_cast<void*>(&flux_matrix_ew_div));
    jit.registerFunction("flux_matrix_add_ms", reinterpret_cast<void*>(&flux_matrix_add_ms));
    jit.registerFunction("flux_matrix_sub_ms", reinterpret_cast<void*>(&flux_matrix_sub_ms));
    jit.registerFunction("flux_matrix_sub_sm", reinterpret_cast<void*>(&flux_matrix_sub_sm));
    jit.registerFunction("flux_matrix_transpose", reinterpret_cast<void*>(&flux_matrix_transpose));
    jit.registerFunction("flux_matrix_rows", reinterpret_cast<void*>(&flux_matrix_rows));
    jit.registerFunction("flux_matrix_cols", reinterpret_cast<void*>(&flux_matrix_cols));
    jit.registerFunction("flux_matrix_get", reinterpret_cast<void*>(&flux_matrix_get));
    jit.registerFunction("flux_create_matrix", reinterpret_cast<void*>(&flux_create_matrix));
    jit.registerFunction("print_matrix", reinterpret_cast<void*>(&flux_print_matrix));
    jit.registerFunction("print_string", reinterpret_cast<void*>(&flux_print_string));
    jit.registerFunction("flux_string_concat_double", reinterpret_cast<void*>(&flux_string_concat_double));
    jit.registerFunction("flux_string_concat_matrix", reinterpret_cast<void*>(&flux_string_concat_matrix));
    jit.registerFunction("flux_string_concat_string", reinterpret_cast<void*>(&flux_string_concat_string));

    jit.registerFunction("flux_print_double", reinterpret_cast<void*>(&flux_print_double));

    jit.registerFunction("flux_get_voltage", reinterpret_cast<void*>(&flux_get_voltage));
    jit.registerFunction("flux_get_current", reinterpret_cast<void*>(&flux_get_current));
    jit.registerFunction("net", reinterpret_cast<void*>(&flux_get_voltage));
    jit.registerFunction("branch", reinterpret_cast<void*>(&flux_get_current));
    jit.registerFunction("flux_get_parameter", reinterpret_cast<void*>(&flux_get_parameter));
    jit.registerFunction("p", reinterpret_cast<void*>(&flux_get_parameter));
    jit.registerFunction("flux_set_parameter", reinterpret_cast<void*>(&flux_set_parameter));
    jit.registerFunction("sim_run", reinterpret_cast<void*>(&flux_sim_run));
    jit.registerFunction("sim_stop", reinterpret_cast<void*>(&flux_sim_stop));
    jit.registerFunction("sim_pause", reinterpret_cast<void*>(&flux_sim_pause));
    jit.registerFunction("erc_check", reinterpret_cast<void*>(&flux_erc_check));

    jit.registerFunction("uramp", reinterpret_cast<void*>(&flux_uramp));
    jit.registerFunction("limit", reinterpret_cast<void*>(&flux_limit));
    jit.registerFunction("buf", reinterpret_cast<void*>(&flux_buf));
    jit.registerFunction("logic_not", reinterpret_cast<void*>(&flux_inv));

    jit.registerFunction("sin", reinterpret_cast<void*>(&flux_sin));
    jit.registerFunction("cos", reinterpret_cast<void*>(&flux_cos));
    jit.registerFunction("tan", reinterpret_cast<void*>(&flux_tan));
    jit.registerFunction("asin", reinterpret_cast<void*>(&flux_asin));
    jit.registerFunction("acos", reinterpret_cast<void*>(&flux_acos));
    jit.registerFunction("atan", reinterpret_cast<void*>(&flux_atan));
    jit.registerFunction("atan2", reinterpret_cast<void*>(&flux_atan2));
    jit.registerFunction("sqrt", reinterpret_cast<void*>(&flux_sqrt));
    jit.registerFunction("exp", reinterpret_cast<void*>(&flux_exp));
    jit.registerFunction("log", reinterpret_cast<void*>(&flux_log));
    jit.registerFunction("log10", reinterpret_cast<void*>(&flux_log10));
    jit.registerFunction("abs", reinterpret_cast<void*>(&flux_abs));
    jit.registerFunction("floor", reinterpret_cast<void*>(&flux_floor));
    jit.registerFunction("ceil", reinterpret_cast<void*>(&flux_ceil));
    jit.registerFunction("round", reinterpret_cast<void*>(&flux_round));
    jit.registerFunction("pow", reinterpret_cast<void*>(&flux_pow));
    jit.registerFunction("sinh", reinterpret_cast<void*>(&flux_sinh));
    jit.registerFunction("cosh", reinterpret_cast<void*>(&flux_cosh));
    jit.registerFunction("tanh", reinterpret_cast<void*>(&flux_tanh));

    jit.registerFunction("pi", reinterpret_cast<void*>(&flux_pi));
    jit.registerFunction("e", reinterpret_cast<void*>(&flux_e));
    jit.registerFunction("flux_throw_error", reinterpret_cast<void*>(&flux_throw_error));
    jit.registerFunction("flux_get_current_jmp_buf", reinterpret_cast<void*>(&flux_get_current_jmp_buf));
    jit.registerFunction("flux_push_jmp_buf", reinterpret_cast<void*>(&flux_push_jmp_buf));
    jit.registerFunction("flux_pop_jmp_buf", reinterpret_cast<void*>(&flux_pop_jmp_buf));

    // Matrix/Vector math
    jit.registerFunction("flux_matrix_det", reinterpret_cast<void*>(&flux_matrix_det));
    jit.registerFunction("flux_matrix_inv", reinterpret_cast<void*>(&flux_matrix_inv));
    jit.registerFunction("flux_matrix_eig", reinterpret_cast<void*>(&flux_matrix_eig));
    jit.registerFunction("det", reinterpret_cast<void*>(&flux_matrix_det));
    jit.registerFunction("inv", reinterpret_cast<void*>(&flux_matrix_inv));
    jit.registerFunction("solve", reinterpret_cast<void*>(&flux_matrix_solve));
    jit.registerFunction("matpow", reinterpret_cast<void*>(&flux_matrix_pow));
    jit.registerFunction("eig", reinterpret_cast<void*>(&flux_matrix_eig));
    jit.registerFunction("dot", reinterpret_cast<void*>(&flux_vector_dot));
    jit.registerFunction("cross", reinterpret_cast<void*>(&flux_vector_cross));
    jit.registerFunction("norm", reinterpret_cast<void*>(&flux_vector_norm));

    // Security functions
    jit.registerFunction("flux_bounds_check_row", reinterpret_cast<void*>(&flux_bounds_check_row));
    jit.registerFunction("flux_bounds_check_col", reinterpret_cast<void*>(&flux_bounds_check_col));
    jit.registerFunction("flux_stack_enter", reinterpret_cast<void*>(&flux_stack_enter));
    jit.registerFunction("flux_stack_leave", reinterpret_cast<void*>(&flux_stack_leave));

    // Statistical functions
    jit.registerFunction("mean", reinterpret_cast<void*>(&flux_mean));
    jit.registerFunction("std", reinterpret_cast<void*>(&flux_std));
    jit.registerFunction("median", reinterpret_cast<void*>(&flux_median));
    jit.registerFunction("min", reinterpret_cast<void*>(&flux_min));
    jit.registerFunction("max", reinterpret_cast<void*>(&flux_max));

    // Signal generation functions
    jit.registerFunction("sawtooth", reinterpret_cast<void*>(&flux_sawtooth));
    jit.registerFunction("square", reinterpret_cast<void*>(&flux_square));
    jit.registerFunction("pulse", reinterpret_cast<void*>(&flux_pulse));

    // Interpolation function
    jit.registerFunction("interp1", reinterpret_cast<void*>(&flux_interp1));

    // Numerical integration
    jit.registerFunction("trapz", reinterpret_cast<void*>(&flux_trapz));

    // Plotting and Visualization functions
    jit.registerFunction("plot", reinterpret_cast<void*>(&flux_plot));
    jit.registerFunction("semilogx", reinterpret_cast<void*>(&flux_semilogx));
    jit.registerFunction("semilogy", reinterpret_cast<void*>(&flux_semilogy));
    jit.registerFunction("subplot", reinterpret_cast<void*>(&flux_subplot));
    jit.registerFunction("plot_title", reinterpret_cast<void*>(&flux_plot_title));
    jit.registerFunction("plot_xlabel", reinterpret_cast<void*>(&flux_plot_xlabel));
    jit.registerFunction("plot_ylabel", reinterpret_cast<void*>(&flux_plot_ylabel));
    jit.registerFunction("plot_grid", reinterpret_cast<void*>(&flux_plot_grid));
    jit.registerFunction("plot_legend", reinterpret_cast<void*>(&flux_plot_legend));
    jit.registerFunction("plot_save", reinterpret_cast<void*>(&flux_plot_save));
    jit.registerFunction("plot_clear", reinterpret_cast<void*>(&flux_plot_clear));
    jit.registerFunction("plot_theme", reinterpret_cast<void*>(&flux_plot_theme));

    // CSV/HDF5 Export functions
    jit.registerFunction("export_csv", reinterpret_cast<void*>(&flux_export_csv));
    jit.registerFunction("export_hdf5", reinterpret_cast<void*>(&flux_export_hdf5));

    // Image Processing functions
    jit.registerFunction("array_to_image", reinterpret_cast<void*>(&flux_array_to_image));
    jit.registerFunction("image_save", reinterpret_cast<void*>(&flux_image_save));
    jit.registerFunction("image_display", reinterpret_cast<void*>(&flux_image_display));

    // Mixed-signal and modeling extensions
    jit.registerFunction("flux_cross_detect", reinterpret_cast<void*>(&flux_cross_detect));
    jit.registerFunction("flux_above_detect", reinterpret_cast<void*>(&flux_above_detect));
    jit.registerFunction("flux_timer_get", reinterpret_cast<void*>(&flux_timer_get));
    jit.registerFunction("flux_fsm_create", reinterpret_cast<void*>(&flux_fsm_create));
    jit.registerFunction("flux_fsm_add_transition", reinterpret_cast<void*>(&flux_fsm_add_transition));
    jit.registerFunction("flux_edge_detect", reinterpret_cast<void*>(&flux_edge_detect));
    jit.registerFunction("flux_noise_generate", reinterpret_cast<void*>(&flux_noise_generate));
    jit.registerFunction("flux_white_noise", reinterpret_cast<void*>(&flux_white_noise));
    jit.registerFunction("flux_flicker_noise", reinterpret_cast<void*>(&flux_flicker_noise));
    jit.registerFunction("flux_thermal_noise", reinterpret_cast<void*>(&flux_thermal_noise));
    jit.registerFunction("flux_piecewise_create", reinterpret_cast<void*>(&flux_piecewise_create));
    jit.registerFunction("flux_piecewise_add_point", reinterpret_cast<void*>(&flux_piecewise_add_point));
    jit.registerFunction("flux_piecewise_eval", reinterpret_cast<void*>(&flux_piecewise_eval));
    jit.registerFunction("flux_table_create", reinterpret_cast<void*>(&flux_table_create));
    jit.registerFunction("flux_table_add_entry", reinterpret_cast<void*>(&flux_table_add_entry));
    jit.registerFunction("flux_table_set_default", reinterpret_cast<void*>(&flux_table_set_default));
    jit.registerFunction("flux_table_lookup", reinterpret_cast<void*>(&flux_table_lookup));
    jit.registerFunction("flux_csv_import", reinterpret_cast<void*>(&flux_csv_import));
    jit.registerFunction("flux_unit_create", reinterpret_cast<void*>(&flux_unit_create));
    jit.registerFunction("flux_dimension", reinterpret_cast<void*>(&flux_dimension));
    jit.registerFunction("flux_convert", reinterpret_cast<void*>(&flux_convert));
    jit.registerFunction("flux_has_unit", reinterpret_cast<void*>(&flux_has_unit));

    // Symbolic math functions
    jit.registerFunction("flux_sym_decl", reinterpret_cast<void*>(&flux_sym_decl));
    jit.registerFunction("flux_sym_number", reinterpret_cast<void*>(&flux_sym_number));
    jit.registerFunction("flux_sym_add", reinterpret_cast<void*>(&flux_sym_add));
    jit.registerFunction("flux_sym_sub", reinterpret_cast<void*>(&flux_sym_sub));
    jit.registerFunction("flux_sym_mul", reinterpret_cast<void*>(&flux_sym_mul));
    jit.registerFunction("flux_sym_div", reinterpret_cast<void*>(&flux_sym_div));
    jit.registerFunction("flux_sym_pow", reinterpret_cast<void*>(&flux_sym_pow));
    jit.registerFunction("flux_sym_simplify", reinterpret_cast<void*>(&flux_sym_simplify));
    jit.registerFunction("flux_sym_differentiate", reinterpret_cast<void*>(&flux_sym_differentiate));
    jit.registerFunction("flux_sym_substitute", reinterpret_cast<void*>(&flux_sym_substitute));
    jit.registerFunction("flux_sym_solve", reinterpret_cast<void*>(&flux_sym_solve));
    jit.registerFunction("flux_sym_evaluate", reinterpret_cast<void*>(&flux_sym_evaluate));
    jit.registerFunction("flux_sym_jacobian", reinterpret_cast<void*>(&flux_sym_jacobian));
    jit.registerFunction("flux_sym_pde_register", reinterpret_cast<void*>(&flux_sym_pde_register));
    jit.registerFunction("flux_sym_pdiff", reinterpret_cast<void*>(&flux_sym_pdiff));
    jit.registerFunction("flux_sym_expand", reinterpret_cast<void*>(&flux_sym_expand));
    jit.registerFunction("flux_sym_factor", reinterpret_cast<void*>(&flux_sym_factor));
    jit.registerFunction("flux_sym_laplace", reinterpret_cast<void*>(&flux_sym_laplace));
    jit.registerFunction("flux_sym_inverse_laplace", reinterpret_cast<void*>(&flux_sym_inverse_laplace));
    jit.registerFunction("flux_sym_poles", reinterpret_cast<void*>(&flux_sym_poles));
    jit.registerFunction("flux_sym_zeros", reinterpret_cast<void*>(&flux_sym_zeros));
    jit.registerFunction("flux_register_analysis", reinterpret_cast<void*>(&flux_register_analysis));
    jit.registerFunction("flux_register_measure", reinterpret_cast<void*>(&flux_register_measure));
    jit.registerFunction("flux_register_subckt", reinterpret_cast<void*>(&flux_register_subckt));
    jit.registerFunction("flux_register_model", reinterpret_cast<void*>(&flux_register_model));
    jit.registerFunction("flux_register_param", reinterpret_cast<void*>(&flux_register_param));
    jit.registerFunction("flux_sym_to_string", reinterpret_cast<void*>(&flux_sym_to_string));

    // Parallel runtime function
    jit.registerFunction("flux_parallel_for", reinterpret_cast<void*>(&flux_parallel_for));

    // State management functions
    jit.registerFunction("state_get", reinterpret_cast<void*>(&flux_state_get));
    jit.registerFunction("state_set", reinterpret_cast<void*>(&flux_state_set));

    // Initial conditions
    jit.registerFunction("flux_set_initial_condition", reinterpret_cast<void*>(&flux_set_initial_condition));
    jit.registerFunction("flux_get_initial_condition", reinterpret_cast<void*>(&flux_get_initial_condition));

    // Outputs
    jit.registerFunction("flux_set_output", reinterpret_cast<void*>(&flux_set_output));
    jit.registerFunction("flux_get_output", reinterpret_cast<void*>(&flux_get_output));
}

// extern "C" runtime functions

extern "C" double flux_print_string(const char* str) {
    std::printf("%s", str);
    std::fflush(stdout);
    return 0.0;
}

extern "C" double flux_print_double(double x) {
    std::printf("%g", x);
    std::fflush(stdout);
    return x;
}

extern "C" double flux_get_voltage(double node_ptr_double) {
    const char* node = reinterpret_cast<const char*>(bit_cast<uint64_t>(node_ptr_double));
    if (g_flux_sim_service && g_flux_sim_service->get_voltage)
        return g_flux_sim_service->get_voltage(node);
    if (std::string(node) == "0")
        return 0.0;
    return 0.0; // Default if no solver
}

extern "C" double flux_get_current(double branch_ptr_double) {
    const char* branch = reinterpret_cast<const char*>(bit_cast<uint64_t>(branch_ptr_double));
    if (g_flux_sim_service && g_flux_sim_service->get_current)
        return g_flux_sim_service->get_current(branch);
    return 0.0;
}

extern "C" double flux_get_parameter(double name_ptr_double) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr_double));
    if (g_flux_sim_service && g_flux_sim_service->get_parameter)
        return g_flux_sim_service->get_parameter(name);
    return 0.0;
}

extern "C" double flux_set_parameter(double name_ptr_double, double value) {
    const char* name = reinterpret_cast<const char*>(bit_cast<uint64_t>(name_ptr_double));
    if (g_flux_sim_service && g_flux_sim_service->set_parameter) {
        g_flux_sim_service->set_parameter(name, value);
        return value;
    }
    std::printf("FluxScript (No Solver): Setting parameter %s to %f\n", name, value);
    return value;
}

extern "C" double flux_sim_run() {
    if (g_flux_sim_service && g_flux_sim_service->sim_run) {
        g_flux_sim_service->sim_run();
        return 1.0;
    }
    std::printf("FluxScript: Simulation RUN requested (No Solver Connected).\n");
    return 0.0;
}

extern "C" double flux_sim_stop() {
    if (g_flux_sim_service && g_flux_sim_service->sim_stop) {
        g_flux_sim_service->sim_stop();
        return 1.0;
    }
    return 0.0;
}

extern "C" double flux_sim_pause() {
    if (g_flux_sim_service && g_flux_sim_service->sim_pause) {
        g_flux_sim_service->sim_pause(1);
        return 1.0;
    }
    return 0.0;
}

extern "C" double flux_erc_check() {
    if (g_flux_sim_service && g_flux_sim_service->log_message) {
        g_flux_sim_service->log_message("FluxScript: Running ERC check...");
    }
    return 1.0; 
}


extern "C" double flux_uramp(double x) { return x > 0.0 ? x : 0.0; }
extern "C" double flux_limit(double x, double low, double high) {
    if (x < low) return low;
    if (x > high) return high;
    return x;
}
extern "C" double flux_buf(double x) { return x > 0.5 ? 1.0 : 0.0; }
extern "C" double flux_inv(double x) { return x > 0.5 ? 0.0 : 1.0; }

extern "C" double flux_sin(double x) { return std::sin(x); }
extern "C" double flux_cos(double x) { return std::cos(x); }
extern "C" double flux_tan(double x) { return std::tan(x); }
extern "C" double flux_asin(double x) { return std::asin(x); }
extern "C" double flux_acos(double x) { return std::acos(x); }
extern "C" double flux_atan(double x) { return std::atan(x); }
extern "C" double flux_atan2(double y, double x) { return std::atan2(y, x); }
extern "C" double flux_sqrt(double x) { return std::sqrt(x); }
extern "C" double flux_exp(double x) { return std::exp(x); }
extern "C" double flux_log(double x) { return std::log(x); }
extern "C" double flux_log10(double x) { return std::log10(x); }
extern "C" double flux_abs(double x) { return std::fabs(x); }
extern "C" double flux_floor(double x) { return std::floor(x); }
extern "C" double flux_ceil(double x) { return std::ceil(x); }
extern "C" double flux_round(double x) { return std::round(x); }
extern "C" double flux_pow(double base, double exp) { return std::pow(base, exp); }
extern "C" double flux_sinh(double x) { return std::sinh(x); }
extern "C" double flux_cosh(double x) { return std::cosh(x); }
extern "C" double flux_tanh(double x) { return std::tanh(x); }

extern "C" double flux_pi() { return 3.14159265358979323846; }
extern "C" double flux_e() { return 2.71828182845904523536; }

// ============ Statistical Functions ============
extern "C" double flux_mean(double* data, int size) {
    if (size <= 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < size; ++i) sum += data[i];
    return sum / static_cast<double>(size);
}

extern "C" double flux_std(double* data, int size) {
    if (size <= 1) return 0.0;
    double m = flux_mean(data, size);
    double sum_sq = 0.0;
    for (int i = 0; i < size; ++i) {
        double diff = data[i] - m;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(size - 1));
}

extern "C" double flux_median(double* data, int size) {
    if (size <= 0) return 0.0;
    std::vector<double> temp(data, data + size);
    std::sort(temp.begin(), temp.end());
    int mid = size / 2;
    if (size % 2 == 0) {
        return (temp[mid - 1] + temp[mid]) / 2.0;
    } else {
        return temp[mid];
    }
}

extern "C" double flux_min(double* data, int size) {
    if (size <= 0) return 0.0;
    double min_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] < min_val) min_val = data[i];
    }
    return min_val;
}

extern "C" double flux_max(double* data, int size) {
    if (size <= 0) return 0.0;
    double max_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > max_val) max_val = data[i];
    }
    return max_val;
}

// ============ Signal Generation Functions ============
extern "C" void* flux_sawtooth(double freq, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = 2.0 * phase - 1.0; // Range: -1 to 1
    }
    return result;
}

extern "C" void* flux_square(double freq, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = (phase < 0.5) ? 1.0 : -1.0;
    }
    return result;
}

extern "C" void* flux_pulse(double freq, double duty, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    duty = std::max(0.0, std::min(1.0, duty)); // Clamp duty cycle
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = (phase < duty) ? 1.0 : 0.0;
    }
    return result;
}

// ============ Interpolation Function ============
extern "C" double flux_interp1(double x, double* x_data, double* y_data, int size) {
    if (size <= 0) return 0.0;
    if (size == 1) return y_data[0];
    
    // Check bounds
    if (x <= x_data[0]) return y_data[0];
    if (x >= x_data[size - 1]) return y_data[size - 1];
    
    // Find interval
    for (int i = 0; i < size - 1; ++i) {
        if (x >= x_data[i] && x <= x_data[i + 1]) {
            double t = (x - x_data[i]) / (x_data[i + 1] - x_data[i]);
            return y_data[i] + t * (y_data[i + 1] - y_data[i]);
        }
    }
    return y_data[size - 1];
}

// ============ Numerical Integration ============
extern "C" double flux_trapz(double* y_data, double* x_data, int size) {
    if (size <= 1) return 0.0;
    double integral = 0.0;
    for (int i = 0; i < size - 1; ++i) {
        double dx = x_data[i + 1] - x_data[i];
        double avg_height = (y_data[i] + y_data[i + 1]) / 2.0;
        integral += dx * avg_height;
    }
    return integral;
}

#include <unordered_map>
#include <mutex>

// Central tracker for matrix memory management
struct MatrixTracker {
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXd>> matrices;
    std::mutex mutex;

    void* register_matrix(std::unique_ptr<Eigen::MatrixXd> mat) {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat.get());
        matrices[ptr] = std::move(mat);
        return ptr;
    }
};

static MatrixTracker g_matrix_tracker;

extern "C" void* flux_create_matrix(double* data, int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    if (data) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c)
                (*mat)(r, c) = data[r * cols + c];
        }
    } else {
        mat->setZero();
    }
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) * (*B)));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) * s));
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);

    if (A->rows() == B->rows() && A->cols() == B->cols())
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));

    // Broadcasting support
    if (A->rows() == B->rows() && B->cols() == 1)
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->colwise() + B->col(0)));
    if (A->cols() == B->cols() && B->rows() == 1)
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->rowwise() + B->row(0)));

    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);

    if (A->rows() == B->rows() && A->cols() == B->cols())
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));

    if (A->rows() == B->rows() && B->cols() == 1)
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->colwise() - B->col(0)));

    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);

    if (A->rows() == B->rows() && A->cols() == B->cols())
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));

    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);

    if (A->rows() == B->rows() && A->cols() == B->cols())
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));

    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() + s));
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() - s));
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s - M->array()));
}

extern "C" void* flux_matrix_div_ms(void* m_ptr, double s) {
    if (!m_ptr || s == 0) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(*M / s));
}

extern "C" void* flux_matrix_div_sm(double s, void* m_ptr) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s / M->array()));
}

extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr) {
    if (!a_ptr || !b_ptr) return nullptr;
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    if (A->rows() != A->cols()) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->partialPivLu().solve(*B)));
}

extern "C" void* flux_matrix_pow(void* m_ptr, double p) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (M->rows() == M->cols() && std::floor(p) == p && p >= 0) {
        int ip = (int)p;
        if (ip == 0) return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(Eigen::MatrixXd::Identity(M->rows(), M->cols())));
        Eigen::MatrixXd res = *M;
        for (int i = 1; i < ip; i++) res *= *M;
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(res));
    }
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array().pow(p)));
}

extern "C" void* flux_matrix_transpose(void* m_ptr) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->transpose()));
}

extern "C" int flux_matrix_rows(void* m_ptr) {
    if (!m_ptr) return 0;
    return (int)static_cast<Eigen::MatrixXd*>(m_ptr)->rows();
}

extern "C" int flux_matrix_cols(void* m_ptr) {
    if (!m_ptr) return 0;
    return (int)static_cast<Eigen::MatrixXd*>(m_ptr)->cols();
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col) {
    if (!m_ptr) return 0.0;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) return 0.0;
    return (*M)(row, col);
}
extern "C" double flux_create_vector_sum(double* data, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i)
        sum += data[i];
    return sum;
}

extern "C" double flux_print_matrix(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

extern "C" double flux_matrix_det(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return 0.0;
    return M->determinant();
}

extern "C" void* flux_matrix_inv(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->inverse()));
}

extern "C" void* flux_matrix_eig(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return nullptr;
    Eigen::EigenSolver<Eigen::MatrixXd> es(*M);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(es.eigenvalues().real()));
}

extern "C" double flux_vector_dot(double* v1_ptr, int len1, double* v2_ptr, int len2) {
    if (!v1_ptr || !v2_ptr) return 0.0;
    Eigen::Map<Eigen::VectorXd> V1(v1_ptr, len1);
    Eigen::Map<Eigen::VectorXd> V2(v2_ptr, len2);
    return V1.dot(V2);
}

extern "C" void* flux_vector_cross(double* v1_ptr, int len1, double* v2_ptr, int len2) {
    if (!v1_ptr || !v2_ptr || len1 < 3 || len2 < 3) return nullptr;
    Eigen::Map<Eigen::Vector3d> a(v1_ptr);
    Eigen::Map<Eigen::Vector3d> b(v2_ptr);
    return new Eigen::MatrixXd(a.cross(b));
}

extern "C" double flux_vector_norm(double* v_ptr, int len) {
    if (!v_ptr) return 0.0;
    Eigen::Map<Eigen::VectorXd> V(v_ptr, len);
    return V.norm();
}

// ============================================================================
// Plotting and Visualization Implementation
// ============================================================================

// Global plot state (simplified for CLI - stores data for later GUI display)
namespace {
    struct PlotData {
        std::vector<double> xData;
        std::vector<double> yData;
        std::string title;
        std::string options;
        bool logX = false;
        bool logY = false;
        int subplotRow = 1;
        int subplotCol = 1;
        int subplotIndex = 1;
    };
    
    PlotData g_currentPlot;
    std::string g_currentTheme = "light";
    bool g_gridEnabled = false;
    std::string g_plotTitle = "";
    std::string g_xLabel = "";
    std::string g_yLabel = "";
}

extern "C" double flux_plot(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = false;
    g_currentPlot.logY = false;
    
    // Print plot data summary (for CLI mode)
    std::cout << "[PLOT] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    std::cout << "  X range: [" << g_currentPlot.xData.front() << ", " << g_currentPlot.xData.back() << "]" << std::endl;
    std::cout << "  Y range: [" << g_currentPlot.yData.front() << ", " << g_currentPlot.yData.back() << "]" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would trigger QCustomPlot update
    // For now, we just store the data for later retrieval
#endif
    
    return 1.0;
}

extern "C" double flux_semilogx(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = true;
    g_currentPlot.logY = false;
    
    std::cout << "[SEMILOGX] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    
    return 1.0;
}

extern "C" double flux_semilogy(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = false;
    g_currentPlot.logY = true;
    
    std::cout << "[SEMILOGY] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    
    return 1.0;
}

extern "C" double flux_subplot(int rows, int cols, int index) {
    g_currentPlot.subplotRow = rows;
    g_currentPlot.subplotCol = cols;
    g_currentPlot.subplotIndex = index;
    
    std::cout << "[SUBPLOT] Grid: " << rows << "x" << cols << ", Active: " << index << std::endl;
    
    return 1.0;
}

extern "C" double flux_plot_title(const char* title) {
    g_plotTitle = title ? title : "";
    std::cout << "[PLOT_TITLE] " << g_plotTitle << std::endl;
    return 1.0;
}

extern "C" double flux_plot_xlabel(const char* label) {
    g_xLabel = label ? label : "";
    std::cout << "[PLOT_XLABEL] " << g_xLabel << std::endl;
    return 1.0;
}

extern "C" double flux_plot_ylabel(const char* label) {
    g_yLabel = label ? label : "";
    std::cout << "[PLOT_YLABEL] " << g_yLabel << std::endl;
    return 1.0;
}

extern "C" double flux_plot_grid(double enable) {
    g_gridEnabled = (enable != 0.0);
    std::cout << "[PLOT_GRID] " << (g_gridEnabled ? "enabled" : "disabled") << std::endl;
    return 1.0;
}

extern "C" double flux_plot_legend(const char* location) {
    std::string loc = location ? location : "best";
    std::cout << "[PLOT_LEGEND] Location: " << loc << std::endl;
    return 1.0;
}

extern "C" double flux_plot_save(const char* filename, int width, int height) {
    std::cout << "[PLOT_SAVE] " << filename << " (" << width << "x" << height << ")" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would save the current plot to file
    // Supported formats: PNG, JPG, PDF, SVG
#endif
    
    return 1.0;
}

extern "C" double flux_plot_clear() {
    g_currentPlot.xData.clear();
    g_currentPlot.yData.clear();
    g_currentPlot.title = "";
    g_currentPlot.options = "";
    g_plotTitle = "";
    g_xLabel = "";
    g_yLabel = "";
    g_gridEnabled = false;
    
    std::cout << "[PLOT_CLEAR]" << std::endl;
    
    return 1.0;
}

extern "C" double flux_plot_theme(const char* theme) {
    g_currentTheme = theme ? theme : "light";
    std::cout << "[PLOT_THEME] " << g_currentTheme << std::endl;
    return 1.0;
}

// ============================================================================
// CSV/HDF5 Export Implementation
// ============================================================================

extern "C" double flux_export_csv(double* data, int rows, int cols, const char* filename, const char* delimiter) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[EXPORT_CSV] Error: Cannot open file " << filename << std::endl;
        return 0.0;
    }
    
    std::string delim = delimiter ? delimiter : ",";
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            file << std::setprecision(15) << data[i * cols + j];
            if (j < cols - 1) file << delim;
        }
        file << "\n";
    }
    
    file.close();
    std::cout << "[EXPORT_CSV] Saved " << rows << "x" << cols << " to " << filename << std::endl;
    
    return 1.0;
}

extern "C" double flux_export_hdf5(double* data, int rows, int cols, const char* filename, const char* dataset_name) {
    // HDF5 export requires the HDF5 library
    // For now, provide a stub that indicates the feature
    std::cout << "[EXPORT_HDF5] HDF5 export requested (requires HDF5 library)" << std::endl;
    std::cout << "  Dataset: " << (dataset_name ? dataset_name : "data") << std::endl;
    std::cout << "  Size: " << rows << "x" << cols << std::endl;
    std::cout << "  File: " << filename << std::endl;
    
    // TODO: Implement HDF5 export when HDF5 library is available
    // h5create_file, h5open_file, h5write_dataset, etc.
    
    return 1.0;
}

// ============================================================================
// Image Processing Implementation
// ============================================================================

extern "C" void* flux_array_to_image(double* data, int rows, int cols, const char* colormap) {
    // Create an image buffer from 2D array data
    // Colormap options: "gray", "jet", "hot", "cool", "viridis"
    
    std::string cmap = colormap ? colormap : "gray";
    std::cout << "[ARRAY_TO_IMAGE] " << rows << "x" << cols << " with colormap: " << cmap << std::endl;
    
    // Allocate image buffer (RGBA format)
    // In a full implementation, this would use Qt's QImage or similar
    unsigned char* imageBuffer = new unsigned char[rows * cols * 4];
    
    // Simple grayscale conversion for now
    double minVal = data[0], maxVal = data[0];
    for (int i = 1; i < rows * cols; ++i) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    double range = maxVal - minVal;
    if (range == 0) range = 1.0;
    
    for (int i = 0; i < rows * cols; ++i) {
        unsigned char intensity = static_cast<unsigned char>((data[i] - minVal) / range * 255.0);
        
        if (cmap == "gray") {
            imageBuffer[i * 4 + 0] = intensity; // R
            imageBuffer[i * 4 + 1] = intensity; // G
            imageBuffer[i * 4 + 2] = intensity; // B
        } else if (cmap == "jet") {
            // Simple jet colormap approximation
            double t = (data[i] - minVal) / range;
            if (t < 0.25) {
                imageBuffer[i * 4 + 0] = 0;
                imageBuffer[i * 4 + 1] = static_cast<unsigned char>(t * 4 * 255);
                imageBuffer[i * 4 + 2] = 255;
            } else if (t < 0.5) {
                imageBuffer[i * 4 + 0] = 0;
                imageBuffer[i * 4 + 1] = 255;
                imageBuffer[i * 4 + 2] = static_cast<unsigned char>((0.5 - t) * 4 * 255);
            } else if (t < 0.75) {
                imageBuffer[i * 4 + 0] = static_cast<unsigned char>((t - 0.5) * 4 * 255);
                imageBuffer[i * 4 + 1] = 255;
                imageBuffer[i * 4 + 2] = 0;
            } else {
                imageBuffer[i * 4 + 0] = 255;
                imageBuffer[i * 4 + 1] = static_cast<unsigned char>((1.0 - t) * 4 * 255);
                imageBuffer[i * 4 + 2] = 0;
            }
        } else {
            // Default to grayscale
            imageBuffer[i * 4 + 0] = intensity;
            imageBuffer[i * 4 + 1] = intensity;
            imageBuffer[i * 4 + 2] = intensity;
        }
        imageBuffer[i * 4 + 3] = 255; // Alpha
    }
    
    return imageBuffer;
}

extern "C" double flux_image_save(void* image_ptr, const char* filename, int quality) {
    if (!image_ptr) {
        std::cerr << "[IMAGE_SAVE] Error: Null image pointer" << std::endl;
        return 0.0;
    }
    
    unsigned char* buffer = static_cast<unsigned char*>(image_ptr);
    std::cout << "[IMAGE_SAVE] Saving to " << filename << " (quality: " << quality << ")" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would save the image using QImage
    // QImage image(buffer, width, height, QImage::Format_RGBA8888);
    // image.save(filename, nullptr, quality);
#endif
    
    // For CLI mode, we could save as raw PPM format
    std::string fname = filename ? filename : "output.ppm";
    if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".ppm") {
        fname += ".ppm";
    }
    
    std::ofstream file(fname, std::ios::binary);
    if (file.is_open()) {
        // Write simple PPM header (assuming 256x256 for demo)
        file << "P6\n256 256\n255\n";
        file.write(reinterpret_cast<char*>(buffer), 256 * 256 * 4);
        file.close();
        std::cout << "[IMAGE_SAVE] Saved as PPM: " << fname << std::endl;
        return 1.0;
    }
    
    return 0.0;
}

extern "C" double flux_image_display(void* image_ptr, const char* title) {
    if (!image_ptr) {
        std::cerr << "[IMAGE_DISPLAY] Error: Null image pointer" << std::endl;
        return 0.0;
    }

    std::string titleStr = title ? title : "Image";
    std::cout << "[IMAGE_DISPLAY] Displaying: " << titleStr << std::endl;

#ifdef FLUX_HAS_QT
    // In GUI mode, this would display the image in a QLabel or QDialog
    // For CLI mode, just acknowledge the request
#endif

    return 1.0;
}


// ============================================================================
// Security Functions Implementation
// ============================================================================

extern "C" void flux_bounds_check_row(int row, int max_rows, void* matrix) {
    if (row < 0 || row >= max_rows) {
        std::cerr << "[BOUNDS ERROR] Row index " << row << " out of bounds [0, " 
                  << max_rows << ")" << std::endl;
        throw std::out_of_range("Matrix row index out of bounds");
    }
}

extern "C" void flux_bounds_check_col(int col, int max_cols, void* matrix) {
    if (col < 0 || col >= max_cols) {
        std::cerr << "[BOUNDS ERROR] Column index " << col << " out of bounds [0, " 
                  << max_cols << ")" << std::endl;
        throw std::out_of_range("Matrix column index out of bounds");
    }
}

// Thread-local stack depth counter
static thread_local int g_stackDepth = 0;
static const int MAX_STACK_DEPTH = 1000;

extern "C" void flux_stack_enter() {
    ++g_stackDepth;
    if (g_stackDepth > MAX_STACK_DEPTH) {
        std::cerr << "[STACK ERROR] Stack depth " << g_stackDepth 
                  << " exceeds maximum " << MAX_STACK_DEPTH << std::endl;
        throw std::overflow_error("Stack overflow detected");
    }
}

extern "C" void flux_stack_leave() {
    if (g_stackDepth > 0) {
        --g_stackDepth;
    }
}

// ============================================================================
// Symbolic Math Runtime Functions
// ============================================================================

// Internal helper to get shared_ptr without taking ownership from raw pointer
static std::shared_ptr<SymbolicExpr> get_sym_ptr(double ptr) {
    if (ptr == 0) return nullptr;
    // We assume the pointer is already managed by the engine's registry
    // We create a shared_ptr with a no-op deleter to avoid double-free
    return std::shared_ptr<SymbolicExpr>((SymbolicExpr*)(uintptr_t)ptr, [](SymbolicExpr*){});
}

// Internal helper to register and return a raw pointer from a shared_ptr
static double make_sym_ptr(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.registerExpr(expr).get();
}

extern "C" double flux_register_analysis(const char* type) {
    std::cerr << "[RUNTIME] Registering analysis: " << type << std::endl;
    return 1.0;
}

extern "C" double flux_register_measure(const char* name, const char* type) {
    std::cerr << "[RUNTIME] Registering measure: " << name << " (" << type << ")" << std::endl;
    return 1.0;
}

extern "C" double flux_register_subckt(double name_ptr, double pins_ptr) {
    const char* name = bit_cast<const char*>(name_ptr);
    const char* pins = bit_cast<const char*>(pins_ptr);
    std::cerr << "[RUNTIME] Registering subckt: " << name << " pins: " << pins << std::endl;
    return 1.0;
}

extern "C" double flux_register_model(double name_ptr, double type_ptr) {
    const char* name = bit_cast<const char*>(name_ptr);
    const char* type = bit_cast<const char*>(type_ptr);
    std::cerr << "[RUNTIME] Registering model: " << name << " type: " << type << std::endl;
    return 1.0;
}

extern "C" double flux_register_param(const char* name, double value) {
    std::cerr << "[RUNTIME] Registering param: " << name << " = " << value << std::endl;
    return 1.0;
}

extern "C" double flux_sym_decl(const char* name) {
    std::cerr << "[RUNTIME] flux_sym_decl: " << name << std::endl;
    auto& engine = SymbolicEngine::instance();
    auto sym = engine.sym(std::string(name));
    return (double)(uintptr_t)sym.get();
}

extern "C" double flux_sym_number(double val) {
    std::cerr << "[RUNTIME] flux_sym_number: " << val << std::endl;
    auto& engine = SymbolicEngine::instance();
    auto num = engine.registerExpr(SymbolicExpr::makeNumber(val));
    return (double)(uintptr_t)num.get();
}

extern "C" double flux_sym_add(double a_ptr, double b_ptr) {
    std::cerr << "[RUNTIME] flux_sym_add: " << a_ptr << ", " << b_ptr << std::endl;
    auto a = get_sym_ptr(a_ptr);
    auto b = get_sym_ptr(b_ptr);
    if (!a || !b) return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.add(a, b).get();
}

extern "C" double flux_sym_sub(double a_ptr, double b_ptr) {
    std::cerr << "[RUNTIME] flux_sym_sub: " << a_ptr << ", " << b_ptr << std::endl;
    auto a = get_sym_ptr(a_ptr);
    auto b = get_sym_ptr(b_ptr);
    if (!a || !b) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto neg_one = engine.registerExpr(SymbolicExpr::makeNumber(-1.0));
    auto neg_b = engine.mul(neg_one, b);
    return (double)(uintptr_t)engine.add(a, neg_b).get();
}

extern "C" double flux_sym_mul(double a_ptr, double b_ptr) {
    std::cerr << "[RUNTIME] flux_sym_mul: " << a_ptr << ", " << b_ptr << std::endl;
    auto a = get_sym_ptr(a_ptr);
    auto b = get_sym_ptr(b_ptr);
    if (!a || !b) return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.mul(a, b).get();
}

extern "C" double flux_sym_div(double a_ptr, double b_ptr) {
    std::cerr << "[RUNTIME] flux_sym_div: " << a_ptr << ", " << b_ptr << std::endl;
    auto a = get_sym_ptr(a_ptr);
    auto b = get_sym_ptr(b_ptr);
    if (!a || !b) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto neg_one = engine.registerExpr(SymbolicExpr::makeNumber(-1.0));
    auto inv_b = engine.power(b, neg_one);
    return (double)(uintptr_t)engine.mul(a, inv_b).get();
}

extern "C" double flux_sym_pow(double a_ptr, double b_ptr) {
    std::cerr << "[RUNTIME] flux_sym_pow: " << a_ptr << ", " << b_ptr << std::endl;
    auto a = get_sym_ptr(a_ptr);
    auto b = get_sym_ptr(b_ptr);
    if (!a || !b) return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.power(a, b).get();
}

extern "C" double flux_sym_simplify(double expr_ptr) {
    std::cerr << "[RUNTIME] flux_sym_simplify: " << expr_ptr << std::endl;
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.simplify(expr);
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_differentiate(double expr_ptr, const char* var) {
    std::cerr << "[RUNTIME] flux_sym_differentiate: " << expr_ptr << " w.r.t " << var << std::endl;
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.differentiate(expr, std::string(var));
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_substitute(double expr_ptr, double var_count, void* var_names_ptr, void* var_values_ptr) {
    std::cerr << "[RUNTIME] flux_sym_substitute: " << expr_ptr << std::endl;
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();

    int count = (int)var_count;
    auto* names = static_cast<const char**>(var_names_ptr);
    auto* values = static_cast<double*>(var_values_ptr);

    std::map<std::string, double> vars;
    for (int i = 0; i < count; i++) {
        vars[std::string(names[i])] = values[i];
    }

    auto result = engine.substitute(expr, vars);
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_solve(double lhs_ptr, double rhs_ptr, const char* var) {
    auto lhs = get_sym_ptr(lhs_ptr);
    auto rhs = get_sym_ptr(rhs_ptr);
    if (!lhs || !rhs) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto solutions = engine.solve(lhs, rhs, std::string(var));
    return solutions.empty() ? 0.0 : solutions[0];
}

extern "C" double flux_sym_evaluate(double expr_ptr, double var_count, void* var_names_ptr, void* var_values_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();

    int count = (int)var_count;
    auto* names = static_cast<const char**>(var_names_ptr);
    auto* values = static_cast<double*>(var_values_ptr);

    std::map<std::string, double> vars;
    for (int i = 0; i < count; i++) {
        vars[std::string(names[i])] = values[i];
    }

    return engine.evaluate(expr, vars);
}

extern "C" double flux_sym_expand(double expr_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.expand(expr);
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_factor(double expr_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.factor(expr);
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_laplace(double expr_ptr, const char* t_var, const char* s_var) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.laplace(expr, std::string(t_var), std::string(s_var));
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_inverse_laplace(double expr_ptr, const char* s_var, const char* t_var) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto result = engine.inverseLaplace(expr, std::string(s_var), std::string(t_var));
    return (double)(uintptr_t)result.get();
}

extern "C" double flux_sym_poles(double expr_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto poles = engine.poles(expr);
    return (double)poles.size();
}

extern "C" double flux_sym_zeros(double expr_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    auto zeros = engine.zeros(expr);
    return (double)zeros.size();
}

extern "C" double flux_sym_to_string(double expr_ptr) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    std::cout << expr->toString() << std::endl;
    return 1.0;
}
// ============================================================================
// Parallel Runtime Function
// ============================================================================

// Function pointer type for parallel body: void body(int index, void* user_data)
typedef void (*ParallelBodyFunc)(int64_t, void*);

extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr) {
    if (start >= end || !body_func_ptr) return;
    
    auto body_func = reinterpret_cast<ParallelBodyFunc>(body_func_ptr);
    int64_t num_threads = std::thread::hardware_concurrency();
    if (num_threads < 2) num_threads = 2;
    
    // Calculate actual chunk size
    int64_t range = end - start;
    int64_t actual_chunk = chunk_size > 0 ? chunk_size : (range + num_threads - 1) / num_threads;
    if (actual_chunk < 1) actual_chunk = 1;
    
    // Create thread pool
    std::vector<std::thread> threads;
    std::atomic<int64_t> current(start);
    
    for (int64_t t = 0; t < num_threads; t++) {
        threads.emplace_back([&current, end, actual_chunk, body_func]() {
            while (true) {
                int64_t chunk_start = current.fetch_add(actual_chunk);
                if (chunk_start >= end) break;
                
                int64_t chunk_end = std::min(chunk_start + actual_chunk, end);
                for (int64_t i = chunk_start; i < chunk_end; i++) {
                    body_func(i, nullptr);
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
}

// ============================================================================
// Exception Handling Runtime
// ============================================================================

#include <setjmp.h>
#include <vector>

// Global stack of jump buffers for nested try blocks
static std::vector<jmp_buf*> g_jmp_bufs;

extern "C" void* flux_get_current_jmp_buf() {
    if (g_jmp_bufs.empty()) return nullptr;
    return g_jmp_bufs.back();
}

extern "C" void flux_push_jmp_buf(void* buf) {
    g_jmp_bufs.push_back(static_cast<jmp_buf*>(buf));
}

extern "C" void flux_pop_jmp_buf() {
    if (!g_jmp_bufs.empty()) g_jmp_bufs.pop_back();
}

extern "C" void flux_throw_error(double error_code, const char* message) {
    std::cerr << "[FLUX ERROR] " << message << " (code: " << error_code << ")" << std::endl;
    
    void* current_buf = flux_get_current_jmp_buf();
    if (current_buf) {
        longjmp(*static_cast<jmp_buf*>(current_buf), (int)error_code ?: 1);
    }
    
    std::cerr << "[FLUX ERROR] Uncaught exception. Aborting." << std::endl;
    std::abort();
}

extern "C" const char* flux_string_concat_double(const char* s, double d) {
    std::string res = std::string(s) + std::to_string(d);
    return strdup(res.c_str());
}

extern "C" const char* flux_string_concat_matrix(const char* s, double* data, int rows, int cols) {
    if (!s) s = "";
    
    std::string res = s;
    if (!data) {
        res += "[]";
    } else {
        res += "[";
        for (int i = 0; i < rows; ++i) {
            if (i > 0) res += "; ";
            for (int j = 0; j < cols; ++j) {
                if (j > 0) res += ", ";
                res += std::to_string(data[i * cols + j]);
            }
        }
        res += "]";
    }
    return strdup(res.c_str());
}

extern "C" const char* flux_string_concat_string(const char* s1, const char* s2) {
    std::string res = std::string(s1) + std::string(s2);
    return strdup(res.c_str());
}

extern "C" void print_string(const char* s) {
    if (s) {
        printf("%s", s);
    }
    fflush(stdout);
}

extern "C" void print_double(double d) {
    printf("%g", d);
    fflush(stdout);
}

extern "C" void print_matrix(double* data, int rows, int cols) {
    if (!data) {
        printf("[]");
        return;
    }
    
    printf("[");
    for (int i = 0; i < rows; ++i) {
        if (i > 0) printf("; ");
        for (int j = 0; j < cols; ++j) {
            if (j > 0) printf(", ");
            printf("%g", data[i * cols + j]);
        }
    }
    printf("]");
    fflush(stdout);
}

extern "C" void* flux_sym_jacobian(void* exprs_ptr, int expr_count, void* vars_ptr, int var_count) {
    auto* expr_ptrs = static_cast<double*>(exprs_ptr);
    auto** var_names = static_cast<const char**>(vars_ptr);
    
    std::vector<std::shared_ptr<SymbolicExpr>> exprs;
    for(int i=0; i<expr_count; i++) exprs.push_back(get_sym_ptr(expr_ptrs[i]));
    
    std::vector<std::string> vars;
    for(int i=0; i<var_count; i++) vars.push_back(var_names[i]);
    
    auto& engine = SymbolicEngine::instance();
    auto J = engine.jacobian(exprs, vars);
    
    int rows = (int)J.size();
    int cols = (int)vars.size();
    
    auto result = std::make_unique<Eigen::MatrixXd>(rows, cols);
    for(int r=0; r<rows; r++) {
        for(int c=0; c<cols; c++) {
            (*result)(r, c) = make_sym_ptr(J[r][c]);
        }
    }
    
    return g_matrix_tracker.register_matrix(std::move(result));
}

} // namespace Flux

// End of file

extern "C" double flux_sym_pde_register(double eq_ptr, int var_count, void* var_names_ptr) {
    auto eq = get_sym_ptr(eq_ptr);
    if (!eq) return 0.0;
    
    auto** names = static_cast<const char**>(var_names_ptr);
    std::vector<std::string> vars;
    for(int i=0; i<var_count; i++) vars.push_back(names[i]);
    
    auto& engine = SymbolicEngine::instance();
    auto res = engine.pde_register(eq, vars);
    return make_sym_ptr(res);
}

extern "C" double flux_sym_pdiff(double expr_ptr, const char* var, int order) {
    auto expr = get_sym_ptr(expr_ptr);
    if (!expr) return 0.0;
    
    auto& engine = SymbolicEngine::instance();
    auto res = engine.partial_differentiate(expr, var, order);
    return make_sym_ptr(res);
}
} // namespace Flux

// End of file
