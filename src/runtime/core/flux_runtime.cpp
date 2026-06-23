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
#include "flux/ai/surrogate.h"
#include "flux/analysis/advanced_analysis.h"
#include "flux/runtime/matrix_tracker.h"
#include "flux/runtime/runtime_helpers.h"
#include "flux/runtime/symbolic_helpers.h"
#ifndef FLUX_RUNTIME_STANDALONE
#include "flux/jit/flux_jit.h"
#endif
#include "flux/runtime/advanced_math.h"
#include "flux/runtime/symbolic_engine.h"

#include <Eigen/Dense>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <queue>
#include <shared_mutex>
#ifdef FLUX_HAS_CURL
#include <curl/curl.h>
#endif
#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class FluxSimulationService;
extern "C" FluxSimulationService* g_flux_sim_service;
FluxSimulationService* g_flux_sim_service = nullptr;

// Global C-API declarations
extern "C" {

// File I/O (defined in file_io_runtime.cpp)
double flux_fopen(double filename_ptr, double mode_ptr);
double flux_fclose(double handle);
double flux_fflush(double handle);
double flux_feof(double handle);
double flux_fgets(double handle);
double flux_fprintf(double handle, double format_ptr, double arg);
double flux_fetch_url(double url_ptr);

// String operations (defined in string_runtime.cpp)
double flux_strcmp(double a_ptr, double b_ptr);
double flux_vec_eq(double* a_data, int a_size, double* b_data, int b_size);
double flux_strlen(double s_ptr);
double flux_string_at(double s_ptr, double index);
double flux_string_slice(double s_ptr, double start, double end);
double flux_string_find(double s_ptr, double c);
double flux_parse_number(double s_ptr);

// String concat/regex (defined in string_runtime.cpp or flux_runtime.cpp)
double flux_string_concat(double a_dbl, double b_dbl);
double flux_double_to_string(double val);
double flux_regex_match(double str_dbl, double pat_dbl);
double flux_regex_replace(double str_dbl, double pat_dbl, double repl_dbl);

// Analysis functions (defined in analysis_runtime.cpp)
double flux_stability_run(double output_dbl);
double flux_sensitivity_run(double output_dbl);
double flux_register_worst_case(double output_dbl, double names_dbl, double nominals_dbl, double tolerances_dbl,
                                int iterations);
double flux_monte_carlo_analyze(double output_dbl, double names_dbl, double nominals_dbl, double tolerances_dbl,
                                int iterations);
double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl, double mins_dbl, double maxs_dbl,
                             int num_vars, int max_iter);
double flux_bode_analyze(double output_dbl, double freqStart, double freqEnd, int pointsPerDecade);
double flux_plot_data(double* yData, int yRows, int yCols, double* xData, int hasX, double title_dbl);

// Symbolic functions (defined in symbolic_runtime.cpp)
double flux_sym_decl(const char* name);
double flux_sym_number(double val);
double flux_sym_add(double a, double b);
double flux_sym_sub(double a, double b);
double flux_sym_mul(double a, double b);
double flux_sym_div(double a, double b);
double flux_sym_pow(double a, double b);
double flux_sym_simplify(double e);
double flux_sym_pdiff(double e, double v_dbl, int o);
double flux_sym_eq(double a, double b);
double flux_sym_ne(double a, double b);
double flux_sym_differentiate(double e, const char* var);
double flux_sym_integrate(double e, const char* var);
double flux_sym_laplace(double e, const char* t, const char* s);
double flux_sym_inverse_laplace(double e, const char* s, const char* t);
double flux_sym_expand(double e);
double flux_sym_factor(double e);
double flux_sym_collect(double e, const char* var);
double flux_sym_numerator(double e);
double flux_sym_denominator(double e);
void* flux_sym_poles(double e);
void* flux_sym_zeros(double e);
void* flux_sym_jacobian(void* e_p, int ec, void* v_p, int vc);
double flux_sym_pde_register(double eq, int vc, void* v_p);

double flux_set_diagnostic(double node_dbl, double type_dbl, double threshold);

double flux_register_subckt(double name_ptr, double pins_ptr);
void flux_register_goal(double current, double target);
void flux_hot_swap(double name_dbl, double model_ptr);
double flux_optimize();
double flux_edge_detect(double value, double edge_type);

// Matrix operations (defined in matrix_runtime.cpp)
void flux_free_matrix(void* ptr);
void* flux_lookup_matrix(void* data_ptr);
void* flux_register_new_matrix(int rows, int cols);
void* flux_register_new_complex_matrix(int rows, int cols);
void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols);
void* flux_create_matrix(double* data, int rows, int cols);
void* flux_matrix_mul(void* a_ptr, void* b_ptr);
void* flux_matrix_mul_ms(void* m_ptr, double s);
void* flux_matrix_add(void* a_ptr, void* b_ptr);
void* flux_matrix_sub(void* a_ptr, void* b_ptr);
void* flux_matrix_transpose(void* m_ptr);
int flux_matrix_rows(void* m_ptr);
int flux_matrix_cols(void* m_ptr);
double flux_matrix_det(void* m_ptr);
void flux_matrix_set(void* m_ptr, int row, int col, double val);
void* flux_matrix_zeros(int rows, int cols);
void* flux_create_range_sum(double start, double step, double end);
void* flux_matrix_ones(int rows, int cols);
void* flux_matrix_eye(int n);
void* flux_matrix_copy(void* m_ptr);
void* flux_matrix_diag(void* v_ptr);
void* flux_matrix_hcat(void* a_ptr, void* b_ptr);
void* flux_matrix_vcat(void* a_ptr, void* b_ptr);
double flux_matrix_trace(void* m_ptr);
void* flux_matrix_diag_extract(void* m_ptr);
void* flux_matrix_slice(void* m_ptr, int r0, int r1, int c0, int c1);
double flux_matrix_sum(void* m_ptr);
double flux_matrix_mean(void* m_ptr);
void* flux_matrix_inv(void* m_ptr);
void* flux_matrix_solve(void* a_ptr, void* b_ptr);
double flux_matrix_get(void* m_ptr, int row, int col);
void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols);
void* flux_matrix_mul_ms(void* m_ptr, double s);
void* flux_promote_matrix_to_complex(void* m_ptr, int rows, int cols);
int flux_complex_matrix_rows(void* m_ptr);
int flux_complex_matrix_cols(void* m_ptr);
void* flux_complex_matrix_mul(void* a_ptr, void* b_ptr);
void* flux_complex_matrix_add(void* a_ptr, void* b_ptr);
void* flux_complex_matrix_sub(void* a_ptr, void* b_ptr);
void* flux_complex_matrix_ew_div(void* a_ptr, void* b_ptr);
void* flux_complex_matrix_ew_mul(void* a_ptr, void* b_ptr);
void* flux_complex_matrix_add_ms(void* m_ptr, double re, double im);
void* flux_complex_matrix_sub_ms(void* m_ptr, double re, double im);
void* flux_complex_matrix_sub_sm(double re, double im, void* m_ptr);
void* flux_complex_matrix_mul_ms(void* m_ptr, double re, double im);
void* flux_complex_matrix_div_ms(void* m_ptr, double re, double im);
void* flux_complex_matrix_div_sm(double re, double im, void* m_ptr);
void* flux_complex_matrix_transpose(void* m_ptr);
void* flux_complex_matrix_conj(void* m_ptr);
void* flux_complex_matrix_ctranspose(void* m_ptr);
void* flux_complex_matrix_inv(void* m_ptr);
double flux_complex_matrix_det(void* m_ptr);
double flux_complex_matrix_trace(void* m_ptr);
double flux_complex_matrix_get_real(void* m_ptr, int row, int col);
double flux_complex_matrix_get_imag(void* m_ptr, int row, int col);
void flux_complex_matrix_set(void* m_ptr, int row, int col, double real, double imag);
void* flux_complex_matrix_zeros(int rows, int cols);
void* flux_complex_matrix_ones(int rows, int cols);
void* flux_complex_matrix_eye(int n);
void* flux_matrix_div_ms(void* m_ptr, double s);
void* flux_matrix_div_sm(double s, void* m_ptr);
void* flux_matrix_ew_div(void* a_ptr, void* b_ptr);
void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr);
void* flux_matrix_add_ms(void* m_ptr, double s);
void* flux_matrix_sub_ms(void* m_ptr, double s);
void* flux_matrix_sub_sm(double s, void* m_ptr);
double flux_print_matrix(void* m_ptr);
double flux_print_complex_matrix(void* m_ptr);

// SPICE simulation API (defined in spice_runtime.cpp)
double flux_register_analysis(const char* analysis_type);
double flux_register_measure(const char* name, const char* measure_type);
double flux_register_probe(const char* var_name, const char* output_name);
double flux_register_save(const char* var_name);
double flux_register_param(const char* name, double value);
double flux_register_ic(const char* node_name, double value);
double flux_register_model(const char* name, const char* model_type);
double flux_register_bsource(const char* name, const char* pos_node, const char* neg_node, const char* type);
double flux_register_esource(const char* name, const char* pos_node, const char* neg_node, const char* ctrl_pos,
                             const char* ctrl_neg, double gain);
double flux_register_fsource(const char* name, const char* pos_node, const char* neg_node, const char* vsense,
                             double gain);
double flux_register_gsource(const char* name, const char* pos_node, const char* neg_node, const char* ctrl_pos,
                             const char* ctrl_neg, double transcond);
double flux_register_hsource(const char* name, const char* pos_node, const char* neg_node, const char* vsense,
                             double transres);
void flux_register_adevice(const char* name, int device_type, const char* input_nodes, const char* output_nodes);
void flux_register_wavefile(const char* name, const char* pos_node, const char* neg_node, const char* file_path,
                            int channel);
double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl, double mins_dbl, double maxs_dbl,
                             int num_vars, int max_iter);

// LLVM C-API bridge functions (defined in flux_llvm_bridge.cpp)
double flux_llvm_context_create();
double flux_llvm_context_dispose(double ctx);
double flux_llvm_module_create_with_name(double name);
double flux_llvm_module_create_with_name_in_ctx(double name, double ctx);
double flux_llvm_get_module_context(double module);
double flux_llvm_dispose_module(double module);
double flux_llvm_print_module_to_string(double module);
double flux_llvm_dispose_message(double msg);
double flux_llvm_void_type_in_ctx(double ctx);
double flux_llvm_int1_type_in_ctx(double ctx);
double flux_llvm_int8_type_in_ctx(double ctx);
double flux_llvm_int32_type_in_ctx(double ctx);
double flux_llvm_int64_type_in_ctx(double ctx);
double flux_llvm_double_type_in_ctx(double ctx);
double flux_llvm_float_type_in_ctx(double ctx);
double flux_llvm_pointer_type_in_ctx(double ctx, double addr_space);
double flux_llvm_function_type(double ret_ty, double param_count, double is_var_arg);
double flux_llvm_struct_type_in_ctx(double ctx, double elem_count, double packed);
double flux_llvm_struct_create_named(double ctx, double name);
double flux_llvm_struct_set_body(double struct_ty, double elem_count, double packed);
double flux_llvm_array_type(double elem_ty, double elem_count);
double flux_llvm_get_element_type(double ty);
double flux_llvm_get_type_kind(double ty);
double flux_llvm_struct_get_type_index(double struct_ty, double i);
double flux_llvm_create_builder_in_ctx(double ctx);
double flux_llvm_create_builder();
double flux_llvm_position_builder_at_end(double builder, double block);
double flux_llvm_get_insert_block(double builder);
double flux_llvm_dispose_builder(double builder);
double flux_llvm_clear_insertion_position(double builder);
double flux_llvm_add_function(double module, double name, double fn_type);
double flux_llvm_get_named_function(double module, double name);
double flux_llvm_get_param(double fn, double index);
double flux_llvm_get_first_param(double fn);
double flux_llvm_count_params(double fn);
double flux_llvm_append_basic_block(double fn, double name);
double flux_llvm_get_first_basic_block(double fn);
double flux_llvm_get_basic_block_terminator(double block);
double flux_llvm_set_value_name(double val, double name);
double flux_llvm_get_value_name(double val);
double flux_llvm_type_of(double val);
double flux_llvm_global_get_value_type(double global_val);
double flux_llvm_const_int(double int_ty, double value, double sign_extend);
double flux_llvm_const_real(double real_ty, double value);
double flux_llvm_const_null(double ty);
double flux_llvm_const_string_in_ctx(double ctx, double str, double length, double dont_null_terminate);
double flux_llvm_const_struct_in_ctx(double ctx, double elem_count, double packed);
double flux_llvm_get_undef(double ty);
double flux_llvm_build_ret_void(double builder);
double flux_llvm_build_ret(double builder, double val);
double flux_llvm_build_br(double builder, double dest);
double flux_llvm_build_cond_br(double builder, double cond, double then_bb, double else_bb);
double flux_llvm_build_unreachable(double builder);
double flux_llvm_build_add(double builder, double lhs, double rhs, double name);
double flux_llvm_build_fadd(double builder, double lhs, double rhs, double name);
double flux_llvm_build_sub(double builder, double lhs, double rhs, double name);
double flux_llvm_build_fsub(double builder, double lhs, double rhs, double name);
double flux_llvm_build_mul(double builder, double lhs, double rhs, double name);
double flux_llvm_build_fmul(double builder, double lhs, double rhs, double name);
double flux_llvm_build_sdiv(double builder, double lhs, double rhs, double name);
double flux_llvm_build_fdiv(double builder, double lhs, double rhs, double name);
double flux_llvm_build_srem(double builder, double lhs, double rhs, double name);
double flux_llvm_build_frem(double builder, double lhs, double rhs, double name);
double flux_llvm_build_neg(double builder, double v, double name);
double flux_llvm_build_fneg(double builder, double v, double name);
double flux_llvm_build_and(double builder, double lhs, double rhs, double name);
double flux_llvm_build_or(double builder, double lhs, double rhs, double name);
double flux_llvm_build_xor(double builder, double lhs, double rhs, double name);
double flux_llvm_build_shl(double builder, double lhs, double rhs, double name);
double flux_llvm_build_ashr(double builder, double lhs, double rhs, double name);
double flux_llvm_build_alloca(double builder, double ty, double name);
double flux_llvm_build_load2(double builder, double ty, double ptr, double name);
double flux_llvm_build_store(double builder, double val, double ptr);
double flux_llvm_build_gep2(double builder, double ty, double ptr, double num_indices, double name);
double flux_llvm_build_struct_gep2(double builder, double ty, double ptr, double index, double name);
double flux_llvm_build_global_string_ptr(double builder, double str, double name);
double flux_llvm_build_call2(double builder, double func_ty, double fn, double num_args, double name);
double flux_llvm_build_icmp(double builder, double op, double lhs, double rhs, double name);
double flux_llvm_build_fcmp(double builder, double op, double lhs, double rhs, double name);
double flux_llvm_build_phi(double builder, double ty, double name);
double flux_llvm_build_select(double builder, double cond, double then_v, double else_v, double name);
double flux_llvm_build_extract_value(double builder, double agg, double index, double name);
double flux_llvm_build_insert_value(double builder, double agg, double elem, double index, double name);
double flux_llvm_build_trunc(double builder, double val, double dest_ty, double name);
double flux_llvm_build_zext(double builder, double val, double dest_ty, double name);
double flux_llvm_build_sext(double builder, double val, double dest_ty, double name);
double flux_llvm_build_fptosi(double builder, double val, double dest_ty, double name);
double flux_llvm_build_sitofp(double builder, double val, double dest_ty, double name);
double flux_llvm_build_uitofp(double builder, double val, double dest_ty, double name);
double flux_llvm_build_ptrtoint(double builder, double val, double dest_ty, double name);
double flux_llvm_build_inttoptr(double builder, double val, double dest_ty, double name);
double flux_llvm_build_bitcast(double builder, double val, double dest_ty, double name);
double flux_llvm_verify_module(double module, double action, double out_message);
double flux_llvm_verify_function(double fn, double action);
double flux_llvm_get_target_from_triple(double triple, double out_target);
double flux_llvm_create_target_machine(double triple, double cpu, double features, double opt_level, double reloc,
                                       double code_model);
double flux_llvm_target_machine_emit_to_file(double tm, double module, double filename, double file_type);
double flux_llvm_target_machine_emit_to_mem_buf(double tm, double module, double file_type);
double flux_llvm_initialize_native_target();
double flux_llvm_initialize_native_asm_printer();
double flux_llvm_add_incoming(double phi, double val, double block);
double flux_llvm_write_bitcode_to_file(double module, double path);
double flux_llvm_get_current_debug_location(double builder);
double flux_llvm_set_current_debug_location(double builder, double loc);
double flux_llvm_call_arg_push(double val);
double flux_llvm_call_arg_reset();
double flux_llvm_call_arg_save();
double flux_llvm_call_arg_restore(double handle);
double flux_llvm_type_arg_push(double ty);
double flux_llvm_type_arg_reset();
double flux_llvm_index_arg_push(double idx);
double flux_llvm_index_arg_reset();
double flux_llvm_const_arg_push(double val);
double flux_llvm_const_arg_reset();
double flux_hm_create();
double flux_hm_destroy(double hm_id);
double flux_hm_put(double hm_id, double key, double val);
double flux_hm_get(double hm_id, double key);
double flux_hm_has(double hm_id, double key);
double flux_hm_remove(double hm_id, double key);
double flux_hm_size(double hm_id);
double flux_hm_keys(double hm_id);

// String utilities for FluxScript lexer/parser
double flux_str_len(double str_ptr);
double flux_str_at(double str_ptr, double idx);
double flux_str_slice(double str_ptr, double start, double end);
double flux_str_from_char(double ch);
double flux_str_concat(double a_ptr, double b_ptr);
double flux_read_file(double path_dbl);
double flux_clock_ms();
}

// JIT-callable wrappers (C++ linkage, call extern "C" functions internally)
double jit_register_analysis(double name_ptr)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    return flux_register_analysis(name);
}
double jit_register_measure(double name_ptr, double type_ptr)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    const char* type = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(type_ptr)));
    return flux_register_measure(name, type);
}
double jit_register_probe(double name_ptr, double out_ptr)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    const char* out = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(out_ptr)));
    return flux_register_probe(name, out);
}
double jit_register_save(double name_ptr)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    return flux_register_save(name);
}
double jit_register_param(double name_ptr, double value)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    return flux_register_param(name, value);
}
double jit_register_ic(double name_ptr, double value)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_ptr)));
    return flux_register_ic(name, value);
}

namespace Flux {

static thread_local std::vector<void*> g_tracked_allocations;
static thread_local int g_jit_call_depth = 0;

extern "C" void* flux_malloc(size_t size)
{
    void* ptr = std::malloc(size);
    if (ptr) {
        g_tracked_allocations.push_back(ptr);
    }
    return ptr;
}

extern void flux_join_all_pending();

extern "C" void flux_free_all_allocations()
{
    flux_join_all_pending();
    for (void* ptr : g_tracked_allocations) {
        std::free(ptr);
    }
    g_tracked_allocations.clear();
}

// ---- Dyn trait fat-pointer table ----
static thread_local std::vector<std::pair<uint64_t, uint64_t>> g_dyn_ptrs;

extern "C" double flux_dyn_ptr_push(double data, double vtable)
{
    double idx = static_cast<double>(g_dyn_ptrs.size());
    g_dyn_ptrs.push_back({jit_bitcast<uint64_t>(data), jit_bitcast<uint64_t>(vtable)});
    return idx;
}

extern "C" double flux_dyn_ptr_get_data(double idx)
{
    size_t i = static_cast<size_t>(idx);
    if (i >= g_dyn_ptrs.size())
        return 0.0;
    return jit_bitcast<double>(g_dyn_ptrs[i].first);
}

extern "C" double flux_dyn_ptr_get_vtable(double idx)
{
    size_t i = static_cast<size_t>(idx);
    if (i >= g_dyn_ptrs.size())
        return 0.0;
    return jit_bitcast<double>(g_dyn_ptrs[i].second);
}

extern "C" void flux_dyn_ptr_clear()
{
    g_dyn_ptrs.clear();
}

static constexpr int FLUX_MAX_CALL_DEPTH = 10000;

extern "C" void flux_inc_call_depth()
{
    if (++g_jit_call_depth > FLUX_MAX_CALL_DEPTH) {
        flux_set_error("Maximum recursion depth exceeded");
        std::abort();
    }
}

extern "C" void flux_dec_call_depth()
{
    if (--g_jit_call_depth == 0) {
        flux_free_all_allocations();
        flux_dyn_ptr_clear();
    }
}

extern "C" double flux_print_string(double str_dbl);
extern "C" double flux_print_double(double x);

extern "C" double flux_pi()
{
    return 3.14159265358979323846;
}
extern "C" double flux_sin(double x)
{
    return std::sin(x);
}
extern "C" double flux_cos(double x)
{
    return std::cos(x);
}
extern "C" double flux_sqrt(double x)
{
    return std::sqrt(x);
}
extern "C" double flux_pow(double base, double exp)
{
    return std::pow(base, exp);
}

extern "C" double flux_gpu_available()
{
    return g_matrix_tracker.use_gpu ? 1.0 : 0.0;
}
extern "C" void flux_gpu_set_enabled(double enabled)
{
    g_matrix_tracker.use_gpu = (enabled != 0.0);
}

// Forward declarations for JIT-callable parallel runtime functions
extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr, void* user_data);
extern "C" int64_t flux_get_num_threads();

// Forward declarations for threading primitives
extern "C" double flux_spawn(void* func_ptr, void* args, int64_t nargs);
extern "C" double flux_join(double handle);
extern "C" double flux_thread_self();
extern "C" double flux_chan_create();
extern "C" void flux_chan_send(double chan, double val);
extern "C" double flux_chan_recv(double chan);
extern "C" void flux_chan_close(double chan);
extern "C" void flux_chan_destroy(double chan);

// Forward declarations for Mutex
extern "C" double flux_mutex_create();
extern "C" void flux_mutex_lock(double mtx);
extern "C" void flux_mutex_unlock(double mtx);
extern "C" void flux_mutex_destroy(double mtx);

// Forward declarations for RwLock
extern "C" double flux_rwlock_create();
extern "C" void flux_rwlock_read_lock(double rw);
extern "C" void flux_rwlock_write_lock(double rw);
extern "C" void flux_rwlock_unlock(double rw);
extern "C" void flux_rwlock_destroy(double rw);

#ifndef FLUX_RUNTIME_STANDALONE
#endif

// Weak forwarding wrappers for AOT compilation.
// These provide short names (matrix_mul, etc.) that forward to the
// flux_-prefixed implementations.  Unlike GNU asm .equ aliases,
// this pattern works on both ELF and Mach-O (macOS).
extern "C" void* matrix_mul(void* a, void* b);
extern "C" void* matrix_mul(void* a, void* b)
{
    return flux_matrix_mul(a, b);
}
extern "C" void* matrix_add(void* a, void* b);
extern "C" void* matrix_add(void* a, void* b)
{
    return flux_matrix_add(a, b);
}
extern "C" void* matrix_sub(void* a, void* b);
extern "C" void* matrix_sub(void* a, void* b)
{
    return flux_matrix_sub(a, b);
}
extern "C" void* matrix_transpose(void* m);
extern "C" void* matrix_transpose(void* m)
{
    return flux_matrix_transpose(m);
}
extern "C" void* matrix_inv(void* m);
extern "C" void* matrix_inv(void* m)
{
    return flux_matrix_inv(m);
}
extern "C" void* matrix_solve(void* a, void* b);
extern "C" void* matrix_solve(void* a, void* b)
{
    return flux_matrix_solve(a, b);
}
extern "C" double matrix_det(void* m);
extern "C" double matrix_det(void* m)
{
    return flux_matrix_det(m);
}
extern "C" double matrix_get(void* m, int row, int col);
extern "C" double matrix_get(void* m, int row, int col)
{
    return flux_matrix_get(m, row, col);
}
extern "C" int matrix_rows(void* m);
extern "C" int matrix_rows(void* m)
{
    return flux_matrix_rows(m);
}
extern "C" int matrix_cols(void* m);
extern "C" int matrix_cols(void* m)
{
    return flux_matrix_cols(m);
}
extern "C" void matrix_set(void* m, int row, int col, double val);
extern "C" void matrix_set(void* m, int row, int col, double val)
{
    return flux_matrix_set(m, row, col, val);
}
extern "C" void* matrix_zeros(int rows, int cols);
extern "C" void* matrix_zeros(int rows, int cols)
{
    return flux_matrix_zeros(rows, cols);
}
extern "C" void* matrix_ones(int rows, int cols);
extern "C" void* matrix_ones(int rows, int cols)
{
    return flux_matrix_ones(rows, cols);
}
extern "C" void* matrix_eye(int n);
extern "C" void* matrix_eye(int n)
{
    return flux_matrix_eye(n);
}
extern "C" void* matrix_copy(void* m);
extern "C" void* matrix_copy(void* m)
{
    return flux_matrix_copy(m);
}
extern "C" double matrix_sum(void* m);
extern "C" double matrix_sum(void* m)
{
    return flux_matrix_sum(m);
}
extern "C" double matrix_mean(void* m);
extern "C" double matrix_mean(void* m)
{
    return flux_matrix_mean(m);
}
extern "C" void* matrix_slice(void* m, int r0, int r1, int c0, int c1);
extern "C" void* matrix_slice(void* m, int r0, int r1, int c0, int c1)
{
    return flux_matrix_slice(m, r0, r1, c0, c1);
}
extern "C" double pi();
extern "C" double pi()
{
    return flux_pi();
}

extern "C" double flux_print_string(double str_dbl)
{
    const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(str_dbl)));
    if (str) {
        fwrite(str, 1, strlen(str), stdout);
        fflush(stdout);
    }
    return 0.0;
}
extern "C" double flux_print_double(double x)
{
    printf("%g", x);
    fflush(stdout);
    return x;
}

static thread_local std::vector<std::pair<double, double>> g_goals;
extern "C" void flux_register_goal(double current, double target)
{
    g_goals.push_back({current, target});
}
extern "C" double flux_optimize()
{
    if (g_goals.empty())
        return 0.0;
    double total_error = 0.0;
    for (const auto& goal : g_goals)
        total_error += std::abs(goal.first - goal.second);
    g_goals.clear();
    return total_error;
}

extern "C" void flux_hot_swap(double name_dbl, double model_ptr)
{
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(name_dbl)));
    std::string subckt_name = name ? name : "anonymous";
    std::cout << "[FLUX AI] Hot-swapping subcircuit '" << subckt_name << "' with NN Surrogate model at " << std::hex
              << (uintptr_t)model_ptr << std::dec << "..." << std::endl;
}

extern "C" double flux_set_diagnostic(double node_dbl, double type_dbl, double threshold)
{
    const char* node = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(node_dbl)));
    const char* diagType = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(type_dbl)));
    std::cout << "[Runtime] Diagnostic: node=" << (node ? node : "null")
              << " type=" << (diagType && diagType[0] ? diagType : "all") << " threshold=" << threshold << std::endl;
    return 1.0;
}

// Weak forwarding wrappers for functions defined in other TUs (advanced_math.cpp).
// These are needed for AOT compilation: the LLVM IR codegen emits calls to short
// names (matrix_lu, fft, etc.), but the runtime library exports flux_-prefixed
// names.  Asm .equ aliases cannot reference cross-TU symbols, so we use C++
// forwarding functions that the linker inlines to a tail call (jmp).
// The declarations are already provided by the "advanced_math.h" include above.
extern "C" void* matrix_lu(void*);
extern "C" void* matrix_lu(void* m)
{
    return flux_matrix_lu(m);
}

extern "C" void* matrix_qr(void*);
extern "C" void* matrix_qr(void* m)
{
    return flux_matrix_qr(m);
}

extern "C" void* matrix_svd(void*);
extern "C" void* matrix_svd(void* m)
{
    return flux_matrix_svd(m);
}

extern "C" void* matrix_cholesky(void*);
extern "C" void* matrix_cholesky(void* m)
{
    return flux_matrix_cholesky(m);
}

extern "C" void* matrix_eigenvalues(void*);
extern "C" void* matrix_eigenvalues(void* m)
{
    return flux_matrix_eigenvalues(m);
}

extern "C" void* matrix_eigenvectors(void*);
extern "C" void* matrix_eigenvectors(void* m)
{
    return flux_matrix_eigenvectors(m);
}

extern "C" double matrix_rank(void*);
extern "C" double matrix_rank(void* m)
{
    return flux_matrix_rank(m);
}

extern "C" double matrix_cond(void*);
extern "C" double matrix_cond(void* m)
{
    return flux_matrix_cond(m);
}

extern "C" double matrix_norm(void*, double);
extern "C" double matrix_norm(void* m, double p)
{
    return flux_matrix_norm(m, p);
}

extern "C" void* fft(void*, double);
extern "C" void* fft(void* sig, double sr)
{
    return flux_fft(sig, sr);
}

extern "C" double fft_thd(void*, double);
extern "C" double fft_thd(void* sig, double sr)
{
    return flux_fft_thd(sig, sr);
}

extern "C" double fft_snr(void*, double);
extern "C" double fft_snr(void* sig, double sr)
{
    return flux_fft_snr(sig, sr);
}

// ============================================================================
// try / catch / throw runtime support
// ----------------------------------------------------------------------------
// The codegen emits a per-try alloca for a jmp_buf, calls `setjmp` from JIT'd
// code (linked from libc), and branches on the return value. On `throw`,
// `flux_throw_error` longjmps to the topmost jmp_buf on this thread's stack,
// so nested try/catch unwinds correctly. The most recent thrown value is
// preserved in thread-local storage so the catch handler can bind it to the
// catch variable.
// ============================================================================

// IMPORTANT: thread_local std::vector and even trivial thread_local structs
// interact badly with the LLVM ORC JIT runtime (process exits with non-zero
// code at lib load). Use a regular static + TLS to back the jmp_buf stack
// so no thread_local storage is needed at all.
#if defined(_WIN32)
#include <windows.h>
static DWORD g_jmp_key;
static bool g_jmp_key_init = false;

static void jmp_key_init()
{
    g_jmp_key = TlsAlloc();
    g_jmp_key_init = true;
}

struct JmpBufStack
{
    jmp_buf* slots[256];
    int depth;
};

static JmpBufStack* get_jmp_stack()
{
    if (!g_jmp_key_init)
        jmp_key_init();
    void* p = TlsGetValue(g_jmp_key);
    if (!p) {
        p = calloc(1, sizeof(JmpBufStack));
        TlsSetValue(g_jmp_key, p);
    }
    return static_cast<JmpBufStack*>(p);
}
#else
#include <pthread.h>
static pthread_key_t g_jmp_key;
static pthread_once_t g_jmp_once = PTHREAD_ONCE_INIT;

static void jmp_key_destructor(void* p)
{
    free(p);
}

static void jmp_key_init()
{
    pthread_key_create(&g_jmp_key, jmp_key_destructor);
}

struct JmpBufStack
{
    jmp_buf* slots[256];
    int depth;
};

static JmpBufStack* get_jmp_stack()
{
    pthread_once(&g_jmp_once, jmp_key_init);
    void* p = pthread_getspecific(g_jmp_key);
    if (!p) {
        p = calloc(1, sizeof(JmpBufStack));
        pthread_setspecific(g_jmp_key, p);
    }
    return static_cast<JmpBufStack*>(p);
}
#endif

static thread_local double g_last_thrown_value = 0.0;
static thread_local const char* g_last_thrown_msg = nullptr;

extern "C" void flux_push_jmp_buf(void* buf)
{
    JmpBufStack* s = get_jmp_stack();
    if (s->depth < 256) {
        s->slots[s->depth++] = static_cast<jmp_buf*>(buf);
    } else {
        std::fprintf(stderr, "[FATAL] jmp_buf stack overflow\n");
        std::abort();
    }
}

extern "C" void flux_pop_jmp_buf()
{
    JmpBufStack* s = get_jmp_stack();
    if (s->depth > 0)
        s->depth--;
}

extern "C" void flux_throw_error(double value, const char* msg)
{
    g_last_thrown_value = value;
    g_last_thrown_msg = msg;
    JmpBufStack* s = get_jmp_stack();
    if (s->depth == 0) {
        std::fprintf(stderr, "[FATAL] uncaught Flux exception: %s\n", msg ? msg : "(no message)");
        std::abort();
    }
    longjmp(*s->slots[--s->depth], 1);
}

extern "C" double flux_last_thrown_value()
{
    return g_last_thrown_value;
}
extern "C" const char* flux_last_thrown_msg()
{
    return g_last_thrown_msg;
}

extern "C" void println_string(const char* str)
{
    printf("%s\n", str);
}

// ============================================================================
// Runtime error reporting for JIT-compiled model functions.
// Thread-local buffer so concurrent model evaluations don't interfere.
// A model calls flux_set_error("msg") to signal a runtime fault (e.g.
// division by zero, singular matrix, domain error), and the host checks
// flux_get_error() after the wrapper returns to detect failure.
// ============================================================================

#ifdef _MSC_VER
static __declspec(thread) char tls_runtime_error[1024] = {0};
static __declspec(thread) int tls_runtime_error_set = 0;
#else
static thread_local char tls_runtime_error[1024] = {0};
static thread_local int tls_runtime_error_set = 0;
#endif

extern "C" void flux_set_error(const char* msg)
{
    if (msg) {
        strncpy(tls_runtime_error, msg, sizeof(tls_runtime_error) - 1);
        tls_runtime_error[sizeof(tls_runtime_error) - 1] = '\0';
        tls_runtime_error_set = 1;
    }
}

extern "C" const char* flux_get_error()
{
    return tls_runtime_error_set ? tls_runtime_error : nullptr;
}

extern "C" void flux_clear_error()
{
    tls_runtime_error[0] = '\0';
    tls_runtime_error_set = 0;
}

} // namespace Flux
