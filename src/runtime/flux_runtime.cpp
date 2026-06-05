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
#include <cstdio>
#include <csetjmp>
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
double flux_register_subckt(double name_ptr, double pins_ptr);
void flux_register_goal(double current, double target);
void flux_hot_swap(double name_dbl, double model_ptr);
double flux_optimize();
double flux_edge_detect(double value, double edge_type);

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
void flux_register_wavefile(const char* name, const char* pos_node, const char* neg_node,
                            const char* file_path, int channel);
double flux_sensitivity_run(double output_dbl);
double flux_monte_carlo_analyze(double output_dbl, double names_dbl, double nominals_dbl, double tolerances_dbl, int iterations);
double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl, double mins_dbl, double maxs_dbl);

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
double flux_llvm_create_target_machine(double triple, double cpu, double features, double opt_level, double reloc, double code_model);
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
template <typename To, typename From> inline To jit_bitcast(const From& src) noexcept
{
    static_assert(sizeof(To) == sizeof(From), "bitcast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}
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

// File I/O and string utilities for FluxScript
#include <cstdio>
#include <deque>
#include <regex>
static thread_local std::string g_fileio_buffer;
static thread_local std::deque<std::string> g_fileio_pool;

// File I/O wrappers (flux_ prefix avoids symbol conflicts with libc)
extern "C" double flux_fopen(double filename_ptr, double mode_ptr)
{
    auto* fn = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(filename_ptr));
    auto* md = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(mode_ptr));
    auto* fp = std::fopen(fn ? fn : "", md ? md : "r");
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(fp));
}

extern "C" double flux_fclose(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp)
        return 0.0;
    return std::fclose(fp) == 0 ? 1.0 : 0.0;
}

extern "C" double flux_feof(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp)
        return 1.0;
    return std::feof(fp) ? 1.0 : 0.0;
}

extern "C" double flux_fgets(double handle)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    if (!fp || std::feof(fp))
        return 0.0;
    g_fileio_buffer.clear();
    g_fileio_buffer.resize(4096);
    if (!std::fgets(&g_fileio_buffer[0], g_fileio_buffer.size(), fp))
        return 0.0;
    g_fileio_buffer.resize(std::strlen(g_fileio_buffer.c_str()));
    while (!g_fileio_buffer.empty() && (g_fileio_buffer.back() == '\n' || g_fileio_buffer.back() == '\r'))
        g_fileio_buffer.pop_back();
    g_fileio_pool.push_back(g_fileio_buffer);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(g_fileio_pool.back().c_str()));
}

extern "C" double flux_fprintf(double handle, double format_ptr, double arg)
{
    auto* fp = reinterpret_cast<std::FILE*>(jit_bitcast<uintptr_t>(handle));
    auto* fmt = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(format_ptr));
    if (!fp || !fmt)
        return 0.0;
    return std::fprintf(fp, fmt, arg);
}

extern "C" double flux_strcmp(double a_ptr, double b_ptr)
{
    auto* a = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(a_ptr));
    auto* b = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(b_ptr));
    if (!a || !b)
        return (a == b) ? 0.0 : 1.0;
    return static_cast<double>(std::strcmp(a, b));
}

extern "C" double flux_vec_eq(double* a_data, int a_size, double* b_data, int b_size)
{
    if (a_size != b_size) return 0.0;
    for (int i = 0; i < a_size; ++i) {
        if (a_data[i] != b_data[i]) return 0.0;
    }
    return 1.0;
}

extern "C" double flux_strlen(double s_ptr)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    return s ? static_cast<double>(std::strlen(s)) : 0.0;
}

extern "C" double flux_string_at(double s_ptr, double index)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return 0.0;
    int i = static_cast<int>(index);
    if (i < 0 || i >= static_cast<int>(std::strlen(s)))
        return 0.0;
    return static_cast<double>(static_cast<unsigned char>(s[i]));
}

extern "C" double flux_string_slice(double s_ptr, double start, double end)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return 0.0;
    int lo = std::max(0, static_cast<int>(start));
    int hi = std::min(static_cast<int>(std::strlen(s)), static_cast<int>(end));
    if (lo >= hi)
        return 0.0;
    g_fileio_pool.push_back(std::string(s + lo, s + hi));
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(g_fileio_pool.back().c_str()));
}

extern "C" double flux_string_find(double s_ptr, double c)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s)
        return -1.0;
    char ch = static_cast<char>(static_cast<int>(c));
    const char* p = std::strchr(s, ch);
    if (!p)
        return -1.0;
    return static_cast<double>(p - s);
}

extern "C" double flux_parse_number(double s_ptr)
{
    auto* s = reinterpret_cast<const char*>(jit_bitcast<uintptr_t>(s_ptr));
    if (!s || !*s)
        return 0.0;
    char* end = nullptr;
    double val = std::strtod(s, &end);
    if (end && *end) {
        switch (*end) {
        case 'k':
        case 'K':
            val *= 1e3;
            break;
        case 'M':
        case 'm':
            if (*(end + 1) == 'e' || *(end + 1) == 'E') {
                val *= 1e6;
                break;
            }
            if (end > s && (*(end - 1) == 'm')) { /* already handled */
                break;
            }
            val *= 1e-3;
            break;
        case 'u':
        case 'U':
            val *= 1e-6;
            break;
        case 'n':
        case 'N':
            val *= 1e-9;
            break;
        case 'p':
        case 'P':
            val *= 1e-12;
            break;
        case 'f':
        case 'F':
            val *= 1e-15;
            break;
        case 'R':
        case 'r':
            break; // Ohm suffix
        case 'e':
        case 'E':
            break; // Already handled by strtod
        }
    }
    return val;
}

namespace Flux {

template <typename To, typename From> inline To bit_cast(const From& src) noexcept
{
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

struct MatrixTracker
{
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXd>> matrices;
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXcd>> complex_matrices;
    std::mutex mutex;
    bool use_gpu = false;

    MatrixTracker()
    {
        const char* gpu_env = std::getenv("FLUX_USE_GPU");
        if (gpu_env && std::string(gpu_env) == "1")
            use_gpu = true;
    }

    void* register_matrix(std::unique_ptr<Eigen::MatrixXd> mat)
    {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat->data());
        matrices[ptr] = std::move(mat);
        return ptr;
    }

    void* register_complex_matrix(std::unique_ptr<Eigen::MatrixXcd> mat)
    {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat->data());
        complex_matrices[ptr] = std::move(mat);
        return ptr;
    }

    Eigen::MatrixXd* get_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = matrices.find(ptr);
        return it != matrices.end() ? it->second.get() : nullptr;
    }

    Eigen::MatrixXcd* get_complex_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = complex_matrices.find(ptr);
        return it != complex_matrices.end() ? it->second.get() : nullptr;
    }

    void unregister_matrix(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        matrices.erase(ptr);
        complex_matrices.erase(ptr);
    }
};
static MatrixTracker g_matrix_tracker;

static std::shared_ptr<SymbolicExpr> get_sym_ptr(double ptr)
{
    if (ptr == 0)
        return nullptr;
    return std::shared_ptr<SymbolicExpr>((SymbolicExpr*)(uintptr_t)ptr, [](SymbolicExpr*) {});
}
static double make_sym_ptr(std::shared_ptr<SymbolicExpr> expr)
{
    if (!expr)
        return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.registerExpr(expr).get();
}

#include <mutex>
#include <vector>
#include <atomic>

static std::vector<void*> g_tracked_allocations;
static std::mutex g_alloc_mutex;
static std::atomic<int> g_jit_call_depth{0};

extern "C" void* flux_malloc(size_t size)
{
    void* ptr = std::malloc(size);
    if (ptr) {
        std::lock_guard<std::mutex> lock(g_alloc_mutex);
        g_tracked_allocations.push_back(ptr);
    }
    return ptr;
}

extern "C" void flux_free_all_allocations()
{
    std::lock_guard<std::mutex> lock(g_alloc_mutex);
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
    return jit_bitcast<double>(g_dyn_ptrs[static_cast<size_t>(idx)].first);
}

extern "C" double flux_dyn_ptr_get_vtable(double idx)
{
    return jit_bitcast<double>(g_dyn_ptrs[static_cast<size_t>(idx)].second);
}

extern "C" void flux_dyn_ptr_clear()
{
    g_dyn_ptrs.clear();
}

extern "C" void flux_inc_call_depth()
{
    g_jit_call_depth++;
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

extern "C" void flux_free_matrix(void* ptr)
{
    g_matrix_tracker.unregister_matrix(ptr);
}

// Access matrix from data pointer (for advanced_math.cpp)
extern "C" void* flux_lookup_matrix(void* data_ptr)
{
    return g_matrix_tracker.get_matrix(data_ptr);
}

// Register a newly allocated matrix and return its data pointer
extern "C" void* flux_register_new_matrix(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_register_new_complex_matrix(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

// Copy data into a tracked matrix
extern "C" void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols)
{
    auto* M = g_matrix_tracker.get_matrix(data_ptr);
    if (!M || M->rows() != rows || M->cols() != cols)
        return;
    std::memcpy(M->data(), data, rows * cols * sizeof(double));
}

extern "C" void* flux_create_matrix(double* data, int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    if (data) {
        // Use Eigen::Map for fast copy if possible, or just loop
        std::memcpy(mat->data(), data, rows * cols * sizeof(double));
    } else
        mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) * (*B)));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) * s));
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));
}

extern "C" void* flux_matrix_transpose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->transpose()));
}

extern "C" int flux_matrix_rows(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    int r = M ? (int)M->rows() : 0;
    return r;
}

extern "C" int flux_matrix_cols(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? (int)M->cols() : 0;
}

extern "C" double flux_matrix_det(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->determinant() : 0.0;
}

extern "C" void flux_matrix_set(void* m_ptr, int row, int col, double val)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) {
        return;
    }
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) {
        return;
    }
    (*M)(row, col) = val;
}

extern "C" void* flux_matrix_zeros(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    mat->setZero();
    void* ptr = g_matrix_tracker.register_matrix(std::move(mat));
    return ptr;
}

extern "C" void* flux_create_range_sum(double start, double step, double end)
{
    if (step == 0.0) {
        auto mat = std::make_unique<Eigen::MatrixXd>(1, 1);
        mat->setZero();
        return g_matrix_tracker.register_matrix(std::move(mat));
    }
    int n = (int)std::floor((end - start) / step) + 1;
    if (n < 1) n = 1;
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 1);
    for (int i = 0; i < n; ++i)
        (*mat)(i, 0) = start + i * step;
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_ones(int rows, int cols)
{
    auto* M = new Eigen::MatrixXd(rows, cols);
    M->setOnes();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_eye(int n)
{
    auto* M = new Eigen::MatrixXd(n, n);
    M->setIdentity();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_copy(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return flux_create_matrix(M->data(), M->rows(), M->cols());
}

extern "C" void* flux_matrix_diag(void* v_ptr)
{
    auto* V = g_matrix_tracker.get_matrix(v_ptr);
    if (!V || V->cols() != 1)
        return nullptr;
    int n = V->rows();
    auto* M = new Eigen::MatrixXd(n, n);
    M->setZero();
    for (int i = 0; i < n; i++)
        (*M)(i, i) = (*V)(i, 0);
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_hcat(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    Eigen::MatrixXd C(A->rows(), A->cols() + B->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" void* flux_matrix_vcat(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    Eigen::MatrixXd C(A->rows() + B->rows(), A->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" double flux_matrix_trace(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->trace() : 0.0;
}

extern "C" void* flux_matrix_diag_extract(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    int n = std::min(M->rows(), M->cols());
    auto* V = new Eigen::MatrixXd(n, 1);
    for (int i = 0; i < n; i++)
        (*V)(i, 0) = (*M)(i, i);
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(V));
}

extern "C" void* flux_matrix_slice(void* m_ptr, int r0, int r1, int c0, int c1)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    if (r0 < 0)
        r0 = 0;
    if (r1 > M->rows())
        r1 = M->rows();
    if (c0 < 0)
        c0 = 0;
    if (c1 > M->cols())
        c1 = M->cols();
    if (r0 >= r1 || c0 >= c1)
        return flux_matrix_zeros(0, 0);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->block(r0, c0, r1 - r0, c1 - c0)));
}

extern "C" double flux_matrix_sum(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->sum() : 0.0;
}

extern "C" double flux_matrix_mean(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->mean() : 0.0;
}

extern "C" void* flux_matrix_inv(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->inverse()));
}

extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->partialPivLu().solve(*B)));
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) {
        return 0.0;
    }
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) {
        return 0.0;
    }
    double val = (*M)(row, col);
    return val;
}

extern "C" void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    if (data) {
        std::memcpy(mat->data(), data, rows * cols * sizeof(std::complex<double>));
    } else
        mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_mul_decomposed(void* a_ptr, int a_rows, int a_cols, void* b_ptr, int b_rows, int b_cols)
{
    Eigen::Map<Eigen::MatrixXcd> A(static_cast<std::complex<double>*>(a_ptr), a_rows, a_cols);
    Eigen::Map<Eigen::MatrixXcd> B(static_cast<std::complex<double>*>(b_ptr), b_rows, b_cols);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(A * B));
}

extern "C" void* flux_promote_matrix_to_complex(void* m_ptr, int rows, int cols)
{
    if (!m_ptr)
        return nullptr;
    Eigen::Map<Eigen::MatrixXd> M(static_cast<double*>(m_ptr), rows, cols);
    auto res = std::make_unique<Eigen::MatrixXcd>(M.cast<std::complex<double>>());
    return g_matrix_tracker.register_complex_matrix(std::move(res));
}

// Complex matrix utility functions (binary operator path)
extern "C" int flux_complex_matrix_rows(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    return M ? (int)M->rows() : 0;
}

extern "C" int flux_complex_matrix_cols(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    return M ? (int)M->cols() : 0;
}

extern "C" void* flux_complex_matrix_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) * (*B)));
}

extern "C" void* flux_complex_matrix_add(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) + (*B)));
}

extern "C" void* flux_complex_matrix_sub(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) - (*B)));
}

extern "C" void* flux_complex_matrix_ew_div(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(A->array() / B->array()));
}

extern "C" void* flux_complex_matrix_ew_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(A->array() * B->array()));
}

extern "C" void* flux_complex_matrix_add_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->array() + s));
}

extern "C" void* flux_complex_matrix_sub_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->array() - s));
}

extern "C" void* flux_complex_matrix_sub_sm(double re, double im, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(s - M->array()));
}

extern "C" void* flux_complex_matrix_mul_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>((*M) * s));
}

extern "C" void* flux_complex_matrix_div_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>((*M) / s));
}

extern "C" void* flux_complex_matrix_div_sm(double re, double im, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(s / M->array()));
}

extern "C" void* flux_complex_matrix_transpose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->transpose()));
}

extern "C" void* flux_complex_matrix_conj(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->conjugate()));
}

extern "C" void* flux_complex_matrix_ctranspose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->adjoint()));
}

extern "C" void* flux_complex_matrix_inv(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->inverse()));
}

extern "C" double flux_complex_matrix_det(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return 0.0;
    return std::abs(M->determinant());
}

extern "C" double flux_complex_matrix_trace(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return 0.0;
    return M->trace().real();
}

extern "C" double flux_complex_matrix_get_real(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M || row < 0 || row >= M->rows() || col < 0 || col >= M->cols())
        return 0.0;
    return (*M)(row, col).real();
}

extern "C" double flux_complex_matrix_get_imag(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M || row < 0 || row >= M->rows() || col < 0 || col >= M->cols())
        return 0.0;
    return (*M)(row, col).imag();
}

extern "C" void flux_complex_matrix_set(void* m_ptr, int row, int col, double real, double imag)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (M && row >= 0 && row < M->rows() && col >= 0 && col < M->cols())
        (*M)(row, col) = std::complex<double>(real, imag);
}

extern "C" void* flux_complex_matrix_zeros(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_ones(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setOnes();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_eye(int n)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(n, n);
    mat->setIdentity();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

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

extern "C" double flux_print_matrix(void* m_ptr)
{
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M)
        return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

extern "C" double flux_print_complex_matrix(void* m_ptr)
{
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    if (!M)
        return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

extern "C" double flux_sym_decl(const char* name)
{
    return make_sym_ptr(SymbolicEngine::instance().sym(name ? name : "x"));
}
extern "C" double flux_sym_number(double val)
{
    return make_sym_ptr(SymbolicExpr::makeNumber(val));
}
extern "C" double flux_sym_add(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().add(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_sub(double a, double b)
{
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().add(sa, SymbolicEngine::instance().mul(neg_one, sb)));
}
extern "C" double flux_sym_mul(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().mul(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_div(double a, double b)
{
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().mul(sa, SymbolicEngine::instance().power(sb, neg_one)));
}
extern "C" double flux_sym_pow(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().power(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_simplify(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().simplify(get_sym_ptr(e)));
}
extern "C" double flux_sym_pdiff(double e, double v_dbl, int o)
{
    const char* v = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(v_dbl)));
    return make_sym_ptr(SymbolicEngine::instance().partial_differentiate(get_sym_ptr(e), v, o));
}
extern "C" double flux_sym_eq(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().eq(get_sym_ptr(a), get_sym_ptr(b)));
}
extern "C" double flux_sym_ne(double a, double b)
{
    return make_sym_ptr(SymbolicEngine::instance().ne(get_sym_ptr(a), get_sym_ptr(b)));
}

// Symbolic math operations (missing wrappers)
extern "C" double flux_sym_differentiate(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().differentiate(get_sym_ptr(e), var));
}

extern "C" double flux_sym_integrate(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().integrate(get_sym_ptr(e), var));
}

extern "C" double flux_sym_laplace(double e, const char* t, const char* s)
{
    return make_sym_ptr(SymbolicEngine::instance().laplace(get_sym_ptr(e), t, s));
}

extern "C" double flux_sym_inverse_laplace(double e, const char* s, const char* t)
{
    return make_sym_ptr(SymbolicEngine::instance().inverseLaplace(get_sym_ptr(e), s, t));
}

// Expand, factor, collect, numerator, denominator
extern "C" double flux_sym_expand(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().expand(get_sym_ptr(e)));
}

extern "C" double flux_sym_factor(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().factor(get_sym_ptr(e)));
}

extern "C" double flux_sym_collect(double e, const char* var)
{
    return make_sym_ptr(SymbolicEngine::instance().collect(get_sym_ptr(e), var));
}

extern "C" double flux_sym_numerator(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().numerator(get_sym_ptr(e)));
}

extern "C" double flux_sym_denominator(double e)
{
    return make_sym_ptr(SymbolicEngine::instance().denominator(get_sym_ptr(e)));
}

extern "C" void* flux_sym_poles(double e)
{
    auto poles = SymbolicEngine::instance().poles(get_sym_ptr(e));
    size_t n = poles.size();
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 2);
    for (size_t i = 0; i < n; ++i) {
        (*mat)(i, 0) = poles[i].real();
        (*mat)(i, 1) = poles[i].imag();
    }
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_sym_zeros(double e)
{
    auto zeros = SymbolicEngine::instance().zeros(get_sym_ptr(e));
    size_t n = zeros.size();
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 2);
    for (size_t i = 0; i < n; ++i) {
        (*mat)(i, 0) = zeros[i].real();
        (*mat)(i, 1) = zeros[i].imag();
    }
    return g_matrix_tracker.register_matrix(std::move(mat));
}

// Stability analysis
extern "C" double flux_stability_run(double output_dbl)
{
    using namespace Flux::AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    auto analyzer = StabilityAnalyzer();
    auto result = analyzer.analyze(output ? output : "");
    printf("=== Stability Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Gain Margin: %.2f dB (at %.2f Hz)\n", result.gainMargin, result.gainMarginFreq);
    printf("Phase Margin: %.2f deg (at %.2f Hz)\n", result.phaseMargin, result.phaseMarginFreq);
    printf("Bandwidth: %.2f Hz\n", result.bandwidth);
    printf("Peak Gain: %.2f dB (at %.2f Hz)\n", result.peakGain, result.peakGainFreq);
    printf("Stable: %s\n", result.isStable ? "YES" : "NO");
    printf("Verdict: %s\n", result.verdict.c_str());
    for (const auto& w : result.warnings)
        printf("  Warning: %s\n", w.c_str());
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.isStable ? 1.0 : 0.0;
}

// Sensitivity analysis
extern "C" double flux_sensitivity_run(double output_dbl)
{
    using namespace Flux::AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    SensitivityAnalyzer analyzer;
    std::vector<std::string> components;
    auto result = analyzer.analyze(output ? output : "", components, output ? output : "");
    printf("=== Sensitivity Analysis ===\n");
    printf("Output: %s\n", output ? output : "(null)");
    for (const auto& comp : result.sensitivities)
        printf("  %s: %.4f (%s)\n", comp.name.c_str(), comp.sensitivity, comp.criticality.c_str());
    printf("Most Critical: %s\n", result.mostCritical.c_str());
    printf("Least Critical: %s\n", result.leastCritical.c_str());
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.sensitivities.empty() ? 0.0 : 1.0;
}

// Worst-case analysis
extern "C" double flux_register_worst_case(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl)
{
    using namespace Flux::AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* nominalsStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(nominals_dbl)));
    const char* tolerancesStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(tolerances_dbl)));

    auto analyzer = WorstCaseAnalyzer();
    analyzer.setOutputVariable(output ? output : "");

    // Parse comma-separated component data
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    std::string names(namesStr ? namesStr : "");
    std::string nominals(nominalsStr ? nominalsStr : "");
    std::string tolerances(tolerancesStr ? tolerancesStr : "");

    auto nameParts = split(names);
    auto nominalParts = split(nominals);
    auto toleranceParts = split(tolerances);

    for (size_t i = 0; i < nameParts.size(); i++) {
        double nom = (i < nominalParts.size()) ? std::stod(nominalParts[i]) : 0.0;
        double tol = (i < toleranceParts.size()) ? std::stod(toleranceParts[i]) : 0.0;
        analyzer.addComponent(nameParts[i], nom, tol);
    }

    auto result = analyzer.analyze("");
    printf("=== Worst-Case Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Nominal: %.4f\n", result.nominal);
    printf("Worst Min: %.4f\n", result.worstMin);
    printf("Worst Max: %.4f\n", result.worstMax);
    printf("Margin: %.4f\n", result.margin);
    printf("Meets Spec: %s\n", result.meetsSpec ? "YES" : "NO");
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.meetsSpec ? 1.0 : 0.0;
}

// Monte Carlo analysis
extern "C" double flux_monte_carlo_analyze(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl, int iterations)
{
    using namespace Flux::AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* nominalsStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(nominals_dbl)));
    const char* tolerancesStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(tolerances_dbl)));

    auto engine = MonteCarloEngine();
    engine.setOutputVariable(output ? output : "");
    engine.setIterations(iterations > 0 ? iterations : 100);

    // Parse comma-separated component data
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    auto nameParts = split(namesStr ? namesStr : "");
    auto nominalParts = split(nominalsStr ? nominalsStr : "");
    auto toleranceParts = split(tolerancesStr ? tolerancesStr : "");

    for (size_t i = 0; i < nameParts.size(); i++) {
        double nom = (i < nominalParts.size()) ? std::stod(nominalParts[i]) : 0.0;
        double tol = (i < toleranceParts.size()) ? std::stod(toleranceParts[i]) : 0.01;
        engine.addParameter(nameParts[i], nom, tol);
    }

    auto result = engine.run("");
    printf("=== Monte Carlo Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Iterations: %d\n", result.iterations);
    printf("Mean: %.4f\n", result.mean);
    printf("Std Dev: %.4f\n", result.stdDev);
    printf("Min: %.4f\n", result.min);
    printf("Max: %.4f\n", result.max);
    printf("Yield: %.1f%%\n", result.yield);
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.yield >= 50.0 ? 1.0 : 0.0;
}

// Optimization
extern "C" double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl,
                                        double mins_dbl, double maxs_dbl)
{
    using namespace Flux::AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* initsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(inits_dbl)));
    const char* minsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(mins_dbl)));
    const char* maxsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(maxs_dbl)));

    auto optimizer = CircuitOptimizer();
    optimizer.addTarget(output ? output : "", 0.0, 1.0);

    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    auto nameParts = split(namesStr ? namesStr : "");
    auto initParts = split(initsStr ? initsStr : "");
    auto minParts = split(minsStr ? minsStr : "");
    auto maxParts = split(maxsStr ? maxsStr : "");

    for (size_t i = 0; i < nameParts.size(); i++) {
        double init = (i < initParts.size()) ? std::stod(initParts[i]) : 1000.0;
        double minv = (i < minParts.size()) ? std::stod(minParts[i]) : 0.0;
        double maxv = (i < maxParts.size()) ? std::stod(maxParts[i]) : 1e6;
        optimizer.addVariable(nameParts[i], init, minv, maxv);
    }

    auto result = optimizer.optimize("");
    printf("=== Optimization ===\n");
    printf("Output: %s\n", output);
    printf("Converged: %s\n", result.converged ? "YES" : "NO");
    printf("Iterations: %d\n", result.iterations);
    printf("Improvement: %.2fx\n", result.improvement);
    printf("Optimal Values:\n");
    for (const auto& v : result.optimalValues)
        printf("  %s = %g\n", v.first.c_str(), v.second);
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.converged ? 1.0 : 0.0;
}

extern "C" double flux_bode_analyze(double output_dbl, double freqStart, double freqEnd, int pointsPerDecade)
{
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));

    if (freqStart <= 0) freqStart = 10;
    if (freqEnd <= freqStart) freqEnd = freqStart * 1000;
    if (pointsPerDecade < 5) pointsPerDecade = 20;

    int numDecades = static_cast<int>(std::ceil(std::log10(freqEnd / freqStart)));
    int numPoints = numDecades * pointsPerDecade;

    printf("=== Bode Plot: %s ===\n", output ? output : "(null)");
    printf("Frequency range: %.1f Hz to %.1f Hz\n", freqStart, freqEnd);
    printf("Points: %d\n\n", numPoints);

    // Generate frequency response data (simulated RC low-pass)
    std::vector<double> freqs, magDB, phaseDeg;
    double fc = 1590.0; // 1/(2*pi*10k*10nF)
    for (int i = 0; i < numPoints; i++) {
        double f = freqStart * std::pow(10.0, static_cast<double>(i) / pointsPerDecade);
        double h = 1.0 / std::sqrt(1.0 + (f / fc) * (f / fc));
        double mag = 20.0 * std::log10(h);
        double phase = -std::atan2(f / fc, 1.0) * 180.0 / 3.14159;
        freqs.push_back(f);
        magDB.push_back(mag);
        phaseDeg.push_back(phase);
    }

    // Print magnitude table
    printf("  Freq (Hz)  |  Mag (dB)  | Phase (deg)\n");
    printf("  -----------+-----------+-------------\n");
    for (size_t i = 0; i < freqs.size(); i += std::max(1, numPoints / 20)) {
        printf("  %9.2f  |  %+8.2f  |  %+8.2f\n", freqs[i], magDB[i], phaseDeg[i]);
    }

    // ASCII magnitude plot
    printf("\n  Magnitude (dB):\n");
    double magMin = *std::min_element(magDB.begin(), magDB.end());
    double magMax = *std::max_element(magDB.begin(), magDB.end());
    double magRange = magMax - magMin;
    if (magRange < 1) magRange = 1;
    for (int row = 0; row < 10; row++) {
        double level = magMax - (row + 0.5) * magRange / 10.0;
        printf("  %+6.1f |", level);
        for (size_t i = 0; i < freqs.size(); i++) {
            if (i % std::max(1, numPoints / 60) != 0) continue;
            if (magDB[i] >= level)
                printf("*");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("  --------+");
    for (size_t i = 0; i < freqs.size(); i += std::max(1, numPoints / 60))
        printf("-");
    printf("\n");

    // ASCII phase plot
    printf("\n  Phase (deg):\n");
    double phaseMin = *std::min_element(phaseDeg.begin(), phaseDeg.end());
    double phaseMax = *std::max_element(phaseDeg.begin(), phaseDeg.end());
    double phaseRange = phaseMax - phaseMin;
    if (phaseRange < 1) phaseRange = 1;
    for (int row = 0; row < 10; row++) {
        double level = phaseMax - (row + 0.5) * phaseRange / 10.0;
        printf("  %+6.1f |", level);
        for (size_t i = 0; i < freqs.size(); i++) {
            if (i % std::max(1, numPoints / 60) != 0) continue;
            if (phaseDeg[i] >= level)
                printf("*");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("  --------+");
    for (size_t i = 0; i < freqs.size(); i += std::max(1, numPoints / 60))
        printf("-");
    printf("\n\n");

    return 1.0;
}

extern "C" double flux_plot_data(double* yData, int yRows, int yCols, double* xData, int hasX, double title_dbl)
{
    const char* title = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(title_dbl)));

    int n = yRows * yCols;
    if (n == 0) {
        printf(" No data to plot\n");
        return 0.0;
    }

    std::vector<double> y(yData, yData + n);
    std::vector<double> x;
    if (hasX && xData) {
        x.assign(xData, xData + n);
    } else {
        for (int i = 0; i < n; i++)
            x.push_back(static_cast<double>(i));
    }

    printf("\n=== Plot: %s ===\n", title && title[0] ? title : "(unnamed)");

    double min_y = *std::min_element(y.begin(), y.end());
    double max_y = *std::max_element(y.begin(), y.end());
    double min_x = *std::min_element(x.begin(), x.end());
    double max_x = *std::max_element(x.begin(), x.end());

    int width = 60;
    int height = 15;

    // Build character buffer
    std::vector<std::vector<char>> plot(height, std::vector<char>(width, ' '));

    for (int i = 0; i < n; i++) {
        int px = static_cast<int>((x[i] - min_x) / (max_x - min_x) * (width - 1));
        int py = static_cast<int>((1.0 - (y[i] - min_y) / (max_y - min_y)) * (height - 1));
        px = std::max(0, std::min(width - 1, px));
        py = std::max(0, std::min(height - 1, py));
        plot[py][px] = '*';
    }

    // Print plot
    for (int r = 0; r < height; r++) {
        if (r == 0 || r == height - 1 || r == height / 2) {
            double val = max_y - static_cast<double>(r) / (height - 1) * (max_y - min_y);
            printf("%8.3f |", val);
        } else {
            printf("         |");
        }
        for (int c = 0; c < width; c++)
            printf("%c", plot[r][c]);
        printf("\n");
    }
    printf("         +");
    for (int c = 0; c < width; c++)
        printf("-");
    printf("\n");
    printf("     %8.3f %26s %8.3f\n", min_x, "", max_x);

    if (title && title[0])
        printf("  Title: %s\n", title);
    printf("  Points: %d   Y range: [%.3f, %.3f]\n\n", n, min_y, max_y);

    return 1.0;
}

extern "C" void* flux_sym_jacobian(void* e_p, int ec, void* v_p, int vc)
{
    auto* ptrs = static_cast<double*>(e_p);
    auto** names = static_cast<const char**>(v_p);
    std::vector<std::shared_ptr<SymbolicExpr>> exprs;
    for (int i = 0; i < ec; i++)
        exprs.push_back(get_sym_ptr(ptrs[i]));
    std::vector<std::string> vars;
    for (int i = 0; i < vc; i++)
        vars.push_back(names[i]);
    auto J = SymbolicEngine::instance().jacobian(exprs, vars);
    auto res = std::make_unique<Eigen::MatrixXd>(J.size(), vars.size());
    for (int r = 0; r < (int)J.size(); r++)
        for (int c = 0; c < (int)vars.size(); c++)
            (*res)(r, c) = make_sym_ptr(J[r][c]);
    return g_matrix_tracker.register_matrix(std::move(res));
}

extern "C" double flux_sym_pde_register(double eq, int vc, void* v_p)
{
    auto** names = static_cast<const char**>(v_p);
    std::vector<std::string> vars;
    for (int i = 0; i < vc; i++)
        vars.push_back(names[i]);
    return make_sym_ptr(SymbolicEngine::instance().pde_register(get_sym_ptr(eq), vars));
}

extern "C" void* flux_matrix_div_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) / s));
}

extern "C" void* flux_matrix_div_sm(double s, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s / M->array()));
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() + s));
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() - s));
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s - M->array()));
}

extern "C" double flux_string_concat(double a_dbl, double b_dbl);
extern "C" double flux_double_to_string(double val);
extern "C" double flux_regex_match(double str_dbl, double pat_dbl);
extern "C" double flux_regex_replace(double str_dbl, double pat_dbl, double repl_dbl);
extern "C" double flux_set_diagnostic(double node_dbl, double type_dbl, double threshold);

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
void registerRuntimeFunctions(FluxJIT& jit)
{
    jit.registerFunction("flux_create_matrix", (void*)&flux_create_matrix);
    jit.registerFunction("flux_matrix_mul", (void*)&flux_matrix_mul);
    jit.registerFunction("flux_matrix_mul_ms", (void*)&flux_matrix_mul_ms);
    jit.registerFunction("flux_matrix_div_ms", (void*)&flux_matrix_div_ms);
    jit.registerFunction("flux_matrix_div_sm", (void*)&flux_matrix_div_sm);
    jit.registerFunction("flux_matrix_ew_div", (void*)&flux_matrix_ew_div);
    jit.registerFunction("flux_matrix_ew_mul", (void*)&flux_matrix_ew_mul);
    jit.registerFunction("flux_matrix_add_ms", (void*)&flux_matrix_add_ms);
    jit.registerFunction("flux_matrix_sub_ms", (void*)&flux_matrix_sub_ms);
    jit.registerFunction("flux_matrix_sub_sm", (void*)&flux_matrix_sub_sm);
    jit.registerFunction("flux_matrix_add", (void*)&flux_matrix_add);
    jit.registerFunction("flux_matrix_sub", (void*)&flux_matrix_sub);
    jit.registerFunction("flux_matrix_transpose", (void*)&flux_matrix_transpose);
    jit.registerFunction("flux_matrix_rows", (void*)&flux_matrix_rows);
    jit.registerFunction("flux_matrix_cols", (void*)&flux_matrix_cols);
    jit.registerFunction("flux_matrix_get", (void*)&flux_matrix_get);
    jit.registerFunction("flux_matrix_det", (void*)&flux_matrix_det);
    jit.registerFunction("flux_matrix_inv", (void*)&flux_matrix_inv);
    jit.registerFunction("flux_matrix_solve", (void*)&flux_matrix_solve);
    jit.registerFunction("print_matrix", (void*)&flux_print_matrix);
    jit.registerFunction("flux_create_complex_matrix", (void*)&flux_create_complex_matrix);
    jit.registerFunction("flux_register_new_complex_matrix", (void*)&flux_register_new_complex_matrix);
    jit.registerFunction("flux_complex_matrix_mul", (void*)&flux_complex_matrix_mul);
    jit.registerFunction("print_complex_matrix", (void*)&flux_print_complex_matrix);
    jit.registerFunction("flux_promote_matrix_to_complex", (void*)&flux_promote_matrix_to_complex);
    jit.registerFunction("flux_complex_matrix_rows", (void*)&flux_complex_matrix_rows);
    jit.registerFunction("flux_complex_matrix_cols", (void*)&flux_complex_matrix_cols);
    jit.registerFunction("flux_complex_matrix_add", (void*)&flux_complex_matrix_add);
    jit.registerFunction("flux_complex_matrix_sub", (void*)&flux_complex_matrix_sub);
    jit.registerFunction("flux_complex_matrix_ew_div", (void*)&flux_complex_matrix_ew_div);
    jit.registerFunction("flux_complex_matrix_ew_mul", (void*)&flux_complex_matrix_ew_mul);
    jit.registerFunction("flux_complex_matrix_add_ms", (void*)&flux_complex_matrix_add_ms);
    jit.registerFunction("flux_complex_matrix_sub_ms", (void*)&flux_complex_matrix_sub_ms);
    jit.registerFunction("flux_complex_matrix_sub_sm", (void*)&flux_complex_matrix_sub_sm);
    jit.registerFunction("flux_complex_matrix_mul_ms", (void*)&flux_complex_matrix_mul_ms);
    jit.registerFunction("flux_complex_matrix_div_ms", (void*)&flux_complex_matrix_div_ms);
    jit.registerFunction("flux_complex_matrix_div_sm", (void*)&flux_complex_matrix_div_sm);
    jit.registerFunction("flux_complex_matrix_transpose", (void*)&flux_complex_matrix_transpose);
    jit.registerFunction("flux_complex_matrix_conj", (void*)&flux_complex_matrix_conj);
    jit.registerFunction("flux_complex_matrix_ctranspose", (void*)&flux_complex_matrix_ctranspose);
    jit.registerFunction("flux_complex_matrix_inv", (void*)&flux_complex_matrix_inv);
    jit.registerFunction("flux_complex_matrix_det", (void*)&flux_complex_matrix_det);
    jit.registerFunction("flux_complex_matrix_trace", (void*)&flux_complex_matrix_trace);
    jit.registerFunction("flux_complex_matrix_get_real", (void*)&flux_complex_matrix_get_real);
    jit.registerFunction("flux_complex_matrix_get_imag", (void*)&flux_complex_matrix_get_imag);
    jit.registerFunction("flux_complex_matrix_set", (void*)&flux_complex_matrix_set);
    jit.registerFunction("flux_complex_matrix_zeros", (void*)&flux_complex_matrix_zeros);
    jit.registerFunction("flux_complex_matrix_ones", (void*)&flux_complex_matrix_ones);
    jit.registerFunction("flux_complex_matrix_eye", (void*)&flux_complex_matrix_eye);

    // Clean user-facing matrix API
    jit.registerFunction("flux_create_range_sum", (void*)&flux_create_range_sum);
    jit.registerFunction("matrix_create", (void*)&flux_matrix_zeros);
    jit.registerFunction("matrix_zeros", (void*)&flux_matrix_zeros);
    jit.registerFunction("matrix_ones", (void*)&flux_matrix_ones);
    jit.registerFunction("matrix_eye", (void*)&flux_matrix_eye);
    jit.registerFunction("matrix_copy", (void*)&flux_matrix_copy);
    jit.registerFunction("matrix_diag", (void*)&flux_matrix_diag);
    jit.registerFunction("matrix_hcat", (void*)&flux_matrix_hcat);
    jit.registerFunction("matrix_vcat", (void*)&flux_matrix_vcat);
    jit.registerFunction("matrix_sum", (void*)&flux_matrix_sum);
    jit.registerFunction("matrix_mean", (void*)&flux_matrix_mean);
    jit.registerFunction("matrix_mul", (void*)&flux_matrix_mul);
    jit.registerFunction("matrix_add", (void*)&flux_matrix_add);
    jit.registerFunction("matrix_sub", (void*)&flux_matrix_sub);
    jit.registerFunction("matrix_transpose", (void*)&flux_matrix_transpose);
    jit.registerFunction("matrix_inv", (void*)&flux_matrix_inv);
    jit.registerFunction("matrix_det", (void*)&flux_matrix_det);
    jit.registerFunction("matrix_solve", (void*)&flux_matrix_solve);
    jit.registerFunction("matrix_get", (void*)&flux_matrix_get);
    jit.registerFunction("matrix_set", (void*)&flux_matrix_set);
    jit.registerFunction("flux_matrix_set", (void*)&flux_matrix_set);
    jit.registerFunction("matrix_slice", (void*)&flux_matrix_slice);
    jit.registerFunction("flux_matrix_slice", (void*)&flux_matrix_slice);
    jit.registerFunction("matrix_trace", (void*)&flux_matrix_trace);
    jit.registerFunction("matrix_diag", (void*)&flux_matrix_diag_extract);
    jit.registerFunction("matrix_rows", (void*)&flux_matrix_rows);
    jit.registerFunction("matrix_cols", (void*)&flux_matrix_cols);

    jit.registerFunction("gpu_available", (void*)&flux_gpu_available);
    jit.registerFunction("gpu_enable", (void*)&flux_gpu_set_enabled);
    jit.registerFunction("pi", (void*)&flux_pi);
    jit.registerFunction("sin", (void*)&flux_sin);
    jit.registerFunction("cos", (void*)&flux_cos);
    jit.registerFunction("sqrt", (void*)&flux_sqrt);
    jit.registerFunction("pow", (void*)&flux_pow);
    jit.registerFunction("flux_print_string", (void*)&flux_print_string);
    jit.registerFunction("flux_print_double", (void*)&flux_print_double);
    jit.registerFunction("flux_malloc", (void*)&flux_malloc);
    jit.registerFunction("flux_free_all_allocations", (void*)&flux_free_all_allocations);
    jit.registerFunction("flux_dyn_ptr_push", (void*)&flux_dyn_ptr_push);
    jit.registerFunction("flux_dyn_ptr_get_data", (void*)&flux_dyn_ptr_get_data);
    jit.registerFunction("flux_dyn_ptr_get_vtable", (void*)&flux_dyn_ptr_get_vtable);
    jit.registerFunction("flux_dyn_ptr_clear", (void*)&flux_dyn_ptr_clear);
    jit.registerFunction("flux_nn_create", (void*)&AI::flux_nn_create);
    jit.registerFunction("flux_nn_train", (void*)&AI::flux_nn_train);
    jit.registerFunction("flux_nn_predict", (void*)&AI::flux_nn_predict);
    // SPICE runtime functions
    jit.registerFunction("flux_register_subckt", (void*)&flux_register_subckt);
    jit.registerFunction("flux_register_model", (void*)&flux_register_model);
    jit.registerFunction("flux_register_bsource", (void*)&flux_register_bsource);
    jit.registerFunction("flux_register_esource", (void*)&flux_register_esource);
    jit.registerFunction("flux_register_fsource", (void*)&flux_register_fsource);
    jit.registerFunction("flux_register_gsource", (void*)&flux_register_gsource);
    jit.registerFunction("flux_register_hsource", (void*)&flux_register_hsource);
    jit.registerFunction("flux_register_analysis", (void*)&flux_register_analysis);
    jit.registerFunction("flux_register_measure", (void*)&flux_register_measure);
    jit.registerFunction("flux_register_probe", (void*)&flux_register_probe);
    jit.registerFunction("flux_register_save", (void*)&flux_register_save);
    jit.registerFunction("flux_register_param", (void*)&flux_register_param);
    jit.registerFunction("flux_register_ic", (void*)&flux_register_ic);
    jit.registerFunction("flux_register_goal", (void*)&flux_register_goal);
    jit.registerFunction("flux_stability_run", (void*)&flux_stability_run);
    jit.registerFunction("flux_sensitivity_run", (void*)&flux_sensitivity_run);
    jit.registerFunction("flux_monte_carlo_analyze", (void*)&flux_monte_carlo_analyze);
    jit.registerFunction("flux_optimize_analyze", (void*)&flux_optimize_analyze);
    jit.registerFunction("flux_register_worst_case", (void*)&flux_register_worst_case);
    jit.registerFunction("flux_register_adevice", (void*)&flux_register_adevice);
    jit.registerFunction("flux_register_wavefile", (void*)&flux_register_wavefile);
    jit.registerFunction("optimize", (void*)&flux_optimize);
    jit.registerFunction("flux_hot_swap", (void*)&flux_hot_swap);
    jit.registerFunction("edge_detect", (void*)&flux_edge_detect);
    jit.registerFunction("flux_sym_decl", (void*)&flux_sym_decl);
    jit.registerFunction("flux_sym_number", (void*)&flux_sym_number);
    jit.registerFunction("flux_sym_add", (void*)&flux_sym_add);
    jit.registerFunction("flux_sym_sub", (void*)&flux_sym_sub);
    jit.registerFunction("flux_sym_mul", (void*)&flux_sym_mul);
    jit.registerFunction("flux_sym_div", (void*)&flux_sym_div);
    jit.registerFunction("flux_sym_pow", (void*)&flux_sym_pow);
    jit.registerFunction("flux_sym_simplify", (void*)&flux_sym_simplify);
    jit.registerFunction("flux_sym_jacobian", (void*)&flux_sym_jacobian);
    jit.registerFunction("flux_sym_pde_register", (void*)&flux_sym_pde_register);
    jit.registerFunction("flux_sym_pdiff", (void*)&flux_sym_pdiff);
    jit.registerFunction("flux_sym_eq", (void*)&flux_sym_eq);
    jit.registerFunction("flux_sym_ne", (void*)&flux_sym_ne);
    jit.registerFunction("flux_sym_differentiate", (void*)&flux_sym_differentiate);
    jit.registerFunction("flux_sym_integrate", (void*)&flux_sym_integrate);
    jit.registerFunction("flux_sym_laplace", (void*)&flux_sym_laplace);
    jit.registerFunction("flux_sym_inverse_laplace", (void*)&flux_sym_inverse_laplace);
    jit.registerFunction("flux_sym_expand", (void*)&flux_sym_expand);
    jit.registerFunction("flux_sym_factor", (void*)&flux_sym_factor);
    jit.registerFunction("flux_sym_collect", (void*)&flux_sym_collect);
    jit.registerFunction("flux_sym_numerator", (void*)&flux_sym_numerator);
    jit.registerFunction("flux_sym_denominator", (void*)&flux_sym_denominator);
    jit.registerFunction("flux_sym_poles", (void*)&flux_sym_poles);
    jit.registerFunction("flux_sym_zeros", (void*)&flux_sym_zeros);

    // Advanced math: linear algebra
    jit.registerFunction("matrix_lu", (void*)&flux_matrix_lu);
    jit.registerFunction("matrix_qr", (void*)&flux_matrix_qr);
    jit.registerFunction("matrix_svd", (void*)&flux_matrix_svd);
    jit.registerFunction("matrix_cholesky", (void*)&flux_matrix_cholesky);
    jit.registerFunction("matrix_eigenvalues", (void*)&flux_matrix_eigenvalues);
    jit.registerFunction("matrix_eigenvectors", (void*)&flux_matrix_eigenvectors);
    jit.registerFunction("matrix_rank", (void*)&flux_matrix_rank);
    jit.registerFunction("matrix_cond", (void*)&flux_matrix_cond);
    jit.registerFunction("matrix_norm", (void*)&flux_matrix_norm);

    // Advanced math: statistics
    jit.registerFunction("randn", (void*)&flux_randn);
    jit.registerFunction("rand_uniform", (void*)&flux_rand_uniform);
    jit.registerFunction("normal_pdf", (void*)&flux_normal_pdf);
    jit.registerFunction("normal_cdf", (void*)&flux_normal_cdf);
    jit.registerFunction("uniform_pdf", (void*)&flux_uniform_pdf);
    jit.registerFunction("exponential_pdf", (void*)&flux_exponential_pdf);
    jit.registerFunction("covariance", (void*)&flux_covariance);
    jit.registerFunction("correlation", (void*)&flux_correlation);
    jit.registerFunction("percentile", (void*)&flux_percentile);
    jit.registerFunction("skewness", (void*)&flux_skewness);
    jit.registerFunction("kurtosis", (void*)&flux_kurtosis);
    jit.registerFunction("median_filter", (void*)&flux_median_filter);

    // Advanced math: numerical methods
    jit.registerFunction("ode_rk4", (void*)&flux_ode_rk4);
    jit.registerFunction("ode_euler", (void*)&flux_ode_euler);
    jit.registerFunction("root_bisection", (void*)&flux_root_bisection);
    jit.registerFunction("root_newton", (void*)&flux_root_newton);
    jit.registerFunction("integrate_adaptive", (void*)&flux_integrate_adaptive);
    jit.registerFunction("interp1_linear", (void*)&flux_interp1_linear);
    jit.registerFunction("interp1_spline", (void*)&flux_interp1_spline);

    // Advanced math: special functions
    jit.registerFunction("erf", (void*)&flux_erf);
    jit.registerFunction("erfc", (void*)&flux_erfc);
    jit.registerFunction("gamma", (void*)&flux_gamma);
    jit.registerFunction("lgamma", (void*)&flux_lgamma);
    jit.registerFunction("bessel_j0", (void*)&flux_bessel_j0);
    jit.registerFunction("bessel_j1", (void*)&flux_bessel_j1);
    jit.registerFunction("bessel_y0", (void*)&flux_bessel_y0);
    jit.registerFunction("bessel_y1", (void*)&flux_bessel_y1);

    // Advanced math: signal processing
    jit.registerFunction("conv", (void*)&flux_conv);
    jit.registerFunction("corr", (void*)&flux_corr);
    jit.registerFunction("lowpass", (void*)&flux_lowpass);
    jit.registerFunction("highpass", (void*)&flux_highpass);

    // Advanced math: polynomials
    jit.registerFunction("polyval", (void*)&flux_polyval);
    jit.registerFunction("polyfit", (void*)&flux_polyfit);
    jit.registerFunction("polyroots", (void*)&flux_polyroots);

    // SPICE Simulation API (JIT-callable wrappers)
    jit.registerFunction("register_analysis", (void*)&jit_register_analysis);
    jit.registerFunction("register_measure", (void*)&jit_register_measure);
    jit.registerFunction("register_probe", (void*)&jit_register_probe);
    jit.registerFunction("register_save", (void*)&jit_register_save);
    jit.registerFunction("register_param", (void*)&jit_register_param);
    jit.registerFunction("register_ic", (void*)&jit_register_ic);

    // File I/O and string utilities (flux_ prefix avoids libc symbol conflicts)
    jit.registerFunction("flux_fopen", (void*)&flux_fopen);
    jit.registerFunction("flux_fclose", (void*)&flux_fclose);
    jit.registerFunction("flux_feof", (void*)&flux_feof);
    jit.registerFunction("flux_fgets", (void*)&flux_fgets);
    jit.registerFunction("flux_fprintf", (void*)&flux_fprintf);
    jit.registerFunction("flux_strcmp", (void*)&flux_strcmp);
    jit.registerFunction("flux_strlen", (void*)&flux_strlen);
    jit.registerFunction("flux_string_at", (void*)&flux_string_at);
    jit.registerFunction("flux_string_slice", (void*)&flux_string_slice);
    jit.registerFunction("flux_string_find", (void*)&flux_string_find);
    jit.registerFunction("flux_parse_number", (void*)&flux_parse_number);
    jit.registerFunction("flux_string_concat", (void*)&flux_string_concat);
    jit.registerFunction("flux_double_to_string", (void*)&flux_double_to_string);
    jit.registerFunction("flux_regex_match", (void*)&flux_regex_match);
    jit.registerFunction("flux_regex_replace", (void*)&flux_regex_replace);

    // FFT
    jit.registerFunction("fft", (void*)&flux_fft);
    jit.registerFunction("fft_thd", (void*)&flux_fft_thd);
    jit.registerFunction("fft_snr", (void*)&flux_fft_snr);

    // Advanced math: optimization
    jit.registerFunction("least_squares", (void*)&flux_least_squares);
    jit.registerFunction("gradient_descent", (void*)&flux_gradient_descent);
    jit.registerFunction("flux_set_diagnostic", (void*)&flux_set_diagnostic);

    // Parallel for
    jit.registerFunction("flux_parallel_for", (void*)&flux_parallel_for);
    jit.registerFunction("flux_get_num_threads", (void*)&flux_get_num_threads);

    // LLVM C-API bridge (from flux_llvm_bridge.cpp)
    jit.registerFunction("flux_llvm_context_create", (void*)&flux_llvm_context_create);
    jit.registerFunction("flux_llvm_context_dispose", (void*)&flux_llvm_context_dispose);
    jit.registerFunction("flux_llvm_module_create_with_name", (void*)&flux_llvm_module_create_with_name);
    jit.registerFunction("flux_llvm_module_create_with_name_in_ctx", (void*)&flux_llvm_module_create_with_name_in_ctx);
    jit.registerFunction("flux_llvm_get_module_context", (void*)&flux_llvm_get_module_context);
    jit.registerFunction("flux_llvm_dispose_module", (void*)&flux_llvm_dispose_module);
    jit.registerFunction("flux_llvm_print_module_to_string", (void*)&flux_llvm_print_module_to_string);
    jit.registerFunction("flux_llvm_dispose_message", (void*)&flux_llvm_dispose_message);
    jit.registerFunction("flux_llvm_void_type_in_ctx", (void*)&flux_llvm_void_type_in_ctx);
    jit.registerFunction("flux_llvm_int1_type_in_ctx", (void*)&flux_llvm_int1_type_in_ctx);
    jit.registerFunction("flux_llvm_int8_type_in_ctx", (void*)&flux_llvm_int8_type_in_ctx);
    jit.registerFunction("flux_llvm_int32_type_in_ctx", (void*)&flux_llvm_int32_type_in_ctx);
    jit.registerFunction("flux_llvm_int64_type_in_ctx", (void*)&flux_llvm_int64_type_in_ctx);
    jit.registerFunction("flux_llvm_double_type_in_ctx", (void*)&flux_llvm_double_type_in_ctx);
    jit.registerFunction("flux_llvm_float_type_in_ctx", (void*)&flux_llvm_float_type_in_ctx);
    jit.registerFunction("flux_llvm_pointer_type_in_ctx", (void*)&flux_llvm_pointer_type_in_ctx);
    jit.registerFunction("flux_llvm_function_type", (void*)&flux_llvm_function_type);
    jit.registerFunction("flux_llvm_struct_type_in_ctx", (void*)&flux_llvm_struct_type_in_ctx);
    jit.registerFunction("flux_llvm_struct_create_named", (void*)&flux_llvm_struct_create_named);
    jit.registerFunction("flux_llvm_struct_set_body", (void*)&flux_llvm_struct_set_body);
    jit.registerFunction("flux_llvm_array_type", (void*)&flux_llvm_array_type);
    jit.registerFunction("flux_llvm_get_element_type", (void*)&flux_llvm_get_element_type);
    jit.registerFunction("flux_llvm_get_type_kind", (void*)&flux_llvm_get_type_kind);
    jit.registerFunction("flux_llvm_struct_get_type_index", (void*)&flux_llvm_struct_get_type_index);
    jit.registerFunction("flux_llvm_create_builder_in_ctx", (void*)&flux_llvm_create_builder_in_ctx);
    jit.registerFunction("flux_llvm_create_builder", (void*)&flux_llvm_create_builder);
    jit.registerFunction("flux_llvm_position_builder_at_end", (void*)&flux_llvm_position_builder_at_end);
    jit.registerFunction("flux_llvm_get_insert_block", (void*)&flux_llvm_get_insert_block);
    jit.registerFunction("flux_llvm_dispose_builder", (void*)&flux_llvm_dispose_builder);
    jit.registerFunction("flux_llvm_clear_insertion_position", (void*)&flux_llvm_clear_insertion_position);
    jit.registerFunction("flux_llvm_add_function", (void*)&flux_llvm_add_function);
    jit.registerFunction("flux_llvm_get_named_function", (void*)&flux_llvm_get_named_function);
    jit.registerFunction("flux_llvm_get_param", (void*)&flux_llvm_get_param);
    jit.registerFunction("flux_llvm_get_first_param", (void*)&flux_llvm_get_first_param);
    jit.registerFunction("flux_llvm_count_params", (void*)&flux_llvm_count_params);
    jit.registerFunction("flux_llvm_append_basic_block", (void*)&flux_llvm_append_basic_block);
    jit.registerFunction("flux_llvm_get_first_basic_block", (void*)&flux_llvm_get_first_basic_block);
    jit.registerFunction("flux_llvm_get_basic_block_terminator", (void*)&flux_llvm_get_basic_block_terminator);
    jit.registerFunction("flux_llvm_set_value_name", (void*)&flux_llvm_set_value_name);
    jit.registerFunction("flux_llvm_get_value_name", (void*)&flux_llvm_get_value_name);
    jit.registerFunction("flux_llvm_type_of", (void*)&flux_llvm_type_of);
    jit.registerFunction("flux_llvm_global_get_value_type", (void*)&flux_llvm_global_get_value_type);
    jit.registerFunction("flux_llvm_const_int", (void*)&flux_llvm_const_int);
    jit.registerFunction("flux_llvm_const_real", (void*)&flux_llvm_const_real);
    jit.registerFunction("flux_llvm_const_null", (void*)&flux_llvm_const_null);
    jit.registerFunction("flux_llvm_const_string_in_ctx", (void*)&flux_llvm_const_string_in_ctx);
    jit.registerFunction("flux_llvm_const_struct_in_ctx", (void*)&flux_llvm_const_struct_in_ctx);
    jit.registerFunction("flux_llvm_get_undef", (void*)&flux_llvm_get_undef);
    jit.registerFunction("flux_llvm_build_ret_void", (void*)&flux_llvm_build_ret_void);
    jit.registerFunction("flux_llvm_build_ret", (void*)&flux_llvm_build_ret);
    jit.registerFunction("flux_llvm_build_br", (void*)&flux_llvm_build_br);
    jit.registerFunction("flux_llvm_build_cond_br", (void*)&flux_llvm_build_cond_br);
    jit.registerFunction("flux_llvm_build_unreachable", (void*)&flux_llvm_build_unreachable);
    jit.registerFunction("flux_llvm_build_add", (void*)&flux_llvm_build_add);
    jit.registerFunction("flux_llvm_build_fadd", (void*)&flux_llvm_build_fadd);
    jit.registerFunction("flux_llvm_build_sub", (void*)&flux_llvm_build_sub);
    jit.registerFunction("flux_llvm_build_fsub", (void*)&flux_llvm_build_fsub);
    jit.registerFunction("flux_llvm_build_mul", (void*)&flux_llvm_build_mul);
    jit.registerFunction("flux_llvm_build_fmul", (void*)&flux_llvm_build_fmul);
    jit.registerFunction("flux_llvm_build_sdiv", (void*)&flux_llvm_build_sdiv);
    jit.registerFunction("flux_llvm_build_fdiv", (void*)&flux_llvm_build_fdiv);
    jit.registerFunction("flux_llvm_build_srem", (void*)&flux_llvm_build_srem);
    jit.registerFunction("flux_llvm_build_frem", (void*)&flux_llvm_build_frem);
    jit.registerFunction("flux_llvm_build_neg", (void*)&flux_llvm_build_neg);
    jit.registerFunction("flux_llvm_build_fneg", (void*)&flux_llvm_build_fneg);
    jit.registerFunction("flux_llvm_build_and", (void*)&flux_llvm_build_and);
    jit.registerFunction("flux_llvm_build_or", (void*)&flux_llvm_build_or);
    jit.registerFunction("flux_llvm_build_xor", (void*)&flux_llvm_build_xor);
    jit.registerFunction("flux_llvm_build_shl", (void*)&flux_llvm_build_shl);
    jit.registerFunction("flux_llvm_build_ashr", (void*)&flux_llvm_build_ashr);
    jit.registerFunction("flux_llvm_build_alloca", (void*)&flux_llvm_build_alloca);
    jit.registerFunction("flux_llvm_build_load2", (void*)&flux_llvm_build_load2);
    jit.registerFunction("flux_llvm_build_store", (void*)&flux_llvm_build_store);
    jit.registerFunction("flux_llvm_build_gep2", (void*)&flux_llvm_build_gep2);
    jit.registerFunction("flux_llvm_build_struct_gep2", (void*)&flux_llvm_build_struct_gep2);
    jit.registerFunction("flux_llvm_build_global_string_ptr", (void*)&flux_llvm_build_global_string_ptr);
    jit.registerFunction("flux_llvm_build_call2", (void*)&flux_llvm_build_call2);
    jit.registerFunction("flux_llvm_build_icmp", (void*)&flux_llvm_build_icmp);
    jit.registerFunction("flux_llvm_build_fcmp", (void*)&flux_llvm_build_fcmp);
    jit.registerFunction("flux_llvm_build_phi", (void*)&flux_llvm_build_phi);
    jit.registerFunction("flux_llvm_build_select", (void*)&flux_llvm_build_select);
    jit.registerFunction("flux_llvm_build_extract_value", (void*)&flux_llvm_build_extract_value);
    jit.registerFunction("flux_llvm_build_insert_value", (void*)&flux_llvm_build_insert_value);
    jit.registerFunction("flux_llvm_build_trunc", (void*)&flux_llvm_build_trunc);
    jit.registerFunction("flux_llvm_build_zext", (void*)&flux_llvm_build_zext);
    jit.registerFunction("flux_llvm_build_sext", (void*)&flux_llvm_build_sext);
    jit.registerFunction("flux_llvm_build_fptosi", (void*)&flux_llvm_build_fptosi);
    jit.registerFunction("flux_llvm_build_sitofp", (void*)&flux_llvm_build_sitofp);
    jit.registerFunction("flux_llvm_build_uitofp", (void*)&flux_llvm_build_uitofp);
    jit.registerFunction("flux_llvm_build_ptrtoint", (void*)&flux_llvm_build_ptrtoint);
    jit.registerFunction("flux_llvm_build_inttoptr", (void*)&flux_llvm_build_inttoptr);
    jit.registerFunction("flux_llvm_build_bitcast", (void*)&flux_llvm_build_bitcast);
    jit.registerFunction("flux_llvm_verify_module", (void*)&flux_llvm_verify_module);
    jit.registerFunction("flux_llvm_verify_function", (void*)&flux_llvm_verify_function);
    jit.registerFunction("flux_llvm_get_target_from_triple", (void*)&flux_llvm_get_target_from_triple);
    jit.registerFunction("flux_llvm_create_target_machine", (void*)&flux_llvm_create_target_machine);
    jit.registerFunction("flux_llvm_target_machine_emit_to_file", (void*)&flux_llvm_target_machine_emit_to_file);
    jit.registerFunction("flux_llvm_target_machine_emit_to_mem_buf", (void*)&flux_llvm_target_machine_emit_to_mem_buf);
    jit.registerFunction("flux_llvm_initialize_native_target", (void*)&flux_llvm_initialize_native_target);
    jit.registerFunction("flux_llvm_initialize_native_asm_printer", (void*)&flux_llvm_initialize_native_asm_printer);
    jit.registerFunction("flux_llvm_add_incoming", (void*)&flux_llvm_add_incoming);
    jit.registerFunction("flux_llvm_write_bitcode_to_file", (void*)&flux_llvm_write_bitcode_to_file);
    jit.registerFunction("flux_llvm_get_current_debug_location", (void*)&flux_llvm_get_current_debug_location);
    jit.registerFunction("flux_llvm_set_current_debug_location", (void*)&flux_llvm_set_current_debug_location);
    // Collectors
    jit.registerFunction("flux_llvm_call_arg_push", (void*)&flux_llvm_call_arg_push);
    jit.registerFunction("flux_llvm_call_arg_reset", (void*)&flux_llvm_call_arg_reset);
    jit.registerFunction("flux_llvm_call_arg_save", (void*)&flux_llvm_call_arg_save);
    jit.registerFunction("flux_llvm_call_arg_restore", (void*)&flux_llvm_call_arg_restore);
    jit.registerFunction("flux_llvm_type_arg_push", (void*)&flux_llvm_type_arg_push);
    jit.registerFunction("flux_llvm_type_arg_reset", (void*)&flux_llvm_type_arg_reset);
    jit.registerFunction("flux_llvm_index_arg_push", (void*)&flux_llvm_index_arg_push);
    jit.registerFunction("flux_llvm_index_arg_reset", (void*)&flux_llvm_index_arg_reset);
    jit.registerFunction("flux_llvm_const_arg_push", (void*)&flux_llvm_const_arg_push);
    jit.registerFunction("flux_llvm_const_arg_reset", (void*)&flux_llvm_const_arg_reset);
    // HashMap (symbol table)
    jit.registerFunction("flux_hm_create", (void*)&flux_hm_create);
    jit.registerFunction("flux_hm_destroy", (void*)&flux_hm_destroy);
    jit.registerFunction("flux_hm_put", (void*)&flux_hm_put);
    jit.registerFunction("flux_hm_get", (void*)&flux_hm_get);
    jit.registerFunction("flux_hm_has", (void*)&flux_hm_has);
    jit.registerFunction("flux_hm_remove", (void*)&flux_hm_remove);
    jit.registerFunction("flux_hm_size", (void*)&flux_hm_size);
    jit.registerFunction("flux_hm_keys", (void*)&flux_hm_keys);
    // String utilities
    jit.registerFunction("flux_str_len", (void*)&flux_str_len);
    jit.registerFunction("flux_str_at", (void*)&flux_str_at);
    jit.registerFunction("flux_str_slice", (void*)&flux_str_slice);
    jit.registerFunction("flux_str_from_char", (void*)&flux_str_from_char);
    jit.registerFunction("flux_str_concat", (void*)&flux_str_concat);
    jit.registerFunction("flux_read_file", (void*)&flux_read_file);
    jit.registerFunction("flux_clock_ms", (void*)&flux_clock_ms);

    // Threading primitives
    jit.registerFunction("flux_spawn", (void*)&flux_spawn);
    jit.registerFunction("flux_join", (void*)&flux_join);
    jit.registerFunction("flux_thread_self", (void*)&flux_thread_self);
    jit.registerFunction("flux_chan_create", (void*)&flux_chan_create);
    jit.registerFunction("flux_chan_send", (void*)&flux_chan_send);
    jit.registerFunction("flux_chan_recv", (void*)&flux_chan_recv);
    jit.registerFunction("flux_chan_close", (void*)&flux_chan_close);
    jit.registerFunction("flux_chan_destroy", (void*)&flux_chan_destroy);

    // Mutex
    jit.registerFunction("flux_mutex_create", (void*)&flux_mutex_create);
    jit.registerFunction("flux_mutex_lock", (void*)&flux_mutex_lock);
    jit.registerFunction("flux_mutex_unlock", (void*)&flux_mutex_unlock);
    jit.registerFunction("flux_mutex_destroy", (void*)&flux_mutex_destroy);

    // RwLock
    jit.registerFunction("flux_rwlock_create", (void*)&flux_rwlock_create);
    jit.registerFunction("flux_rwlock_read_lock", (void*)&flux_rwlock_read_lock);
    jit.registerFunction("flux_rwlock_write_lock", (void*)&flux_rwlock_write_lock);
    jit.registerFunction("flux_rwlock_unlock", (void*)&flux_rwlock_unlock);
    jit.registerFunction("flux_rwlock_destroy", (void*)&flux_rwlock_destroy);
}
#endif

// Fixed-size worker thread pool with work-stealing for parallel for loops.
// Threads are created once and reused across all parallel_for calls.
class ParallelWorkerPool
{
    int m_numThreads;
    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;

    std::atomic<int64_t> m_current;
    int64_t m_end = 0;
    void (*m_body)(int64_t, void*) = nullptr;
    void* m_userData = nullptr;
    int m_doneCount = 0;
    bool m_hasWork = false;

    void workerLoop()
    {
        while (true) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_hasWork || m_stop; });
            if (m_stop)
                return;
            lock.unlock();

            // Work-stealing: atomically claim next iteration
            while (true) {
                int64_t i = m_current.fetch_add(1);
                if (i >= m_end)
                    break;
                m_body(i, m_userData);
            }

            // Signal completion
            lock.lock();
            m_doneCount++;
            if (m_doneCount >= m_numThreads) {
                m_hasWork = false;
                lock.unlock();
                m_cv.notify_all();
            }
        }
    }

public:
    ParallelWorkerPool() : m_numThreads(std::thread::hardware_concurrency())
    {
        for (int i = 0; i < m_numThreads; i++)
            m_workers.emplace_back(&ParallelWorkerPool::workerLoop, this);
    }

    ~ParallelWorkerPool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& w : m_workers)
            w.join();
    }

    int numThreads() const { return m_numThreads; }

    void dispatch(int64_t start, int64_t end, void (*body)(int64_t, void*), void* userData)
    {
        if (start >= end || !body)
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_current.store(start);
            m_end = end;
            m_body = body;
            m_userData = userData;
            m_doneCount = 0;
            m_hasWork = true;
        }
        m_cv.notify_all();

        // Main thread participates via work-stealing
        while (true) {
            int64_t i = m_current.fetch_add(1);
            if (i >= end)
                break;
            body(i, userData);
        }

        // Wait for all workers
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_hasWork; });
        }
    }
};

static ParallelWorkerPool& parallelPool()
{
    static ParallelWorkerPool pool;
    return pool;
}

typedef void (*ParallelBodyFunc)(int64_t, void*);

extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr, void* user_data)
{
    (void)chunk_size;
    auto body_func = reinterpret_cast<ParallelBodyFunc>(body_func_ptr);
    parallelPool().dispatch(start, end, body_func, user_data);
}

extern "C" int64_t flux_get_num_threads()
{
    return parallelPool().numThreads();
}

// --- Threading Primitives: spawn/join + channels ---

struct ThreadContext {
    std::thread thread;
    double result;
    std::atomic<bool> done{false};
};

static std::mutex g_thread_map_mutex;
static std::unordered_map<double, ThreadContext*> g_thread_map;
static double g_next_thread_id = 1.0;

extern "C" double flux_spawn(void* func_ptr, void* args, int64_t nargs)
{
    // Copy args array so it outlives the caller
    double* args_copy = new double[static_cast<size_t>(nargs)];
    std::memcpy(args_copy, args, static_cast<size_t>(nargs) * sizeof(double));

    auto ctx = std::make_unique<ThreadContext>();
    double* args_owned = args_copy;
    ctx->thread = std::thread([func_ptr, args_owned, nargs, ctx = ctx.get()]() {
        typedef double (*Fn0)();
        typedef double (*Fn1)(double);
        typedef double (*Fn2)(double, double);
        typedef double (*Fn3)(double, double, double);
        typedef double (*Fn4)(double, double, double, double);
        typedef double (*Fn5)(double, double, double, double, double);
        double result = 0.0;
        switch (nargs) {
            case 0: result = reinterpret_cast<Fn0>(func_ptr)(); break;
            case 1: result = reinterpret_cast<Fn1>(func_ptr)(args_owned[0]); break;
            case 2: result = reinterpret_cast<Fn2>(func_ptr)(args_owned[0], args_owned[1]); break;
            case 3: result = reinterpret_cast<Fn3>(func_ptr)(args_owned[0], args_owned[1], args_owned[2]); break;
            case 4: result = reinterpret_cast<Fn4>(func_ptr)(args_owned[0], args_owned[1], args_owned[2], args_owned[3]); break;
            case 5: result = reinterpret_cast<Fn5>(func_ptr)(args_owned[0], args_owned[1], args_owned[2], args_owned[3], args_owned[4]); break;
        }
        delete[] args_owned;
        ctx->result = result;
        ctx->done.store(true, std::memory_order_release);
    });

    // Assign ID and store in map
    double id;
    {
        std::lock_guard<std::mutex> lock(g_thread_map_mutex);
        id = g_next_thread_id++;
        g_thread_map[id] = ctx.release();
    }
    return id;
}

extern "C" double flux_join(double handle)
{
    ThreadContext* ctx = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_thread_map_mutex);
        auto it = g_thread_map.find(handle);
        if (it == g_thread_map.end())
            return 0.0;
        ctx = it->second;
        g_thread_map.erase(it);
    }
    if (ctx->thread.joinable())
        ctx->thread.join();
    double result = ctx->result;
    delete ctx;
    return result;
}

extern "C" double flux_thread_self()
{
    // Return a unique double per thread — use std::hash on thread::id
    std::hash<std::thread::id> hasher;
    double id = static_cast<double>(hasher(std::this_thread::get_id()));
    return id;
}

// --- Channels (MPSC: multi-producer, single-consumer) ---

struct Channel {
    std::queue<double> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;
};

extern "C" double flux_chan_create()
{
    auto* ch = new Channel;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(ch));
}

extern "C" void flux_chan_send(double chan, double val)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        if (ch->closed) return;
        ch->queue.push(val);
    }
    ch->cv.notify_one();
}

extern "C" double flux_chan_recv(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    std::unique_lock<std::mutex> lock(ch->mtx);
    ch->cv.wait(lock, [ch]() { return !ch->queue.empty() || ch->closed; });
    if (ch->queue.empty())
        return 0.0;
    double val = ch->queue.front();
    ch->queue.pop();
    return val;
}

extern "C" void flux_chan_close(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        ch->closed = true;
    }
    ch->cv.notify_all();
}

extern "C" void flux_chan_destroy(double chan)
{
    auto* ch = reinterpret_cast<Channel*>(jit_bitcast<uintptr_t>(chan));
    {
        std::lock_guard<std::mutex> lock(ch->mtx);
        ch->closed = true;
    }
    ch->cv.notify_all();
    delete ch;
}

// --- Mutex ---

extern "C" double flux_mutex_create()
{
    auto* m = new std::mutex;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(m));
}

extern "C" void flux_mutex_lock(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    m->lock();
}

extern "C" void flux_mutex_unlock(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    m->unlock();
}

extern "C" void flux_mutex_destroy(double mtx)
{
    auto* m = reinterpret_cast<std::mutex*>(jit_bitcast<uintptr_t>(mtx));
    delete m;
}

// --- RwLock ---

extern "C" double flux_rwlock_create()
{
    auto* rw = new std::shared_mutex;
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(rw));
}

extern "C" void flux_rwlock_read_lock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->lock_shared();
}

extern "C" void flux_rwlock_write_lock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->lock();
}

extern "C" void flux_rwlock_unlock(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    m->unlock();
}

extern "C" void flux_rwlock_destroy(double rw)
{
    auto* m = reinterpret_cast<std::shared_mutex*>(jit_bitcast<uintptr_t>(rw));
    delete m;
}

} // namespace Flux

// Weak forwarding wrappers for AOT compilation.
// These provide short names (matrix_mul, etc.) that forward to the
// flux_-prefixed implementations.  Unlike GNU asm .equ aliases,
// this pattern works on both ELF and Mach-O (macOS).
extern "C" void* matrix_mul(void* a, void* b);
extern "C" void* matrix_mul(void* a, void* b) { return Flux::flux_matrix_mul(a, b); }
extern "C" void* matrix_add(void* a, void* b);
extern "C" void* matrix_add(void* a, void* b) { return Flux::flux_matrix_add(a, b); }
extern "C" void* matrix_sub(void* a, void* b);
extern "C" void* matrix_sub(void* a, void* b) { return Flux::flux_matrix_sub(a, b); }
extern "C" void* matrix_transpose(void* m);
extern "C" void* matrix_transpose(void* m) { return Flux::flux_matrix_transpose(m); }
extern "C" void* matrix_inv(void* m);
extern "C" void* matrix_inv(void* m) { return Flux::flux_matrix_inv(m); }
extern "C" void* matrix_solve(void* a, void* b);
extern "C" void* matrix_solve(void* a, void* b) { return Flux::flux_matrix_solve(a, b); }
extern "C" double matrix_det(void* m);
extern "C" double matrix_det(void* m) { return Flux::flux_matrix_det(m); }
extern "C" double matrix_get(void* m, int row, int col);
extern "C" double matrix_get(void* m, int row, int col) { return Flux::flux_matrix_get(m, row, col); }
extern "C" int matrix_rows(void* m);
extern "C" int matrix_rows(void* m) { return Flux::flux_matrix_rows(m); }
extern "C" int matrix_cols(void* m);
extern "C" int matrix_cols(void* m) { return Flux::flux_matrix_cols(m); }
extern "C" void matrix_set(void* m, int row, int col, double val);
extern "C" void matrix_set(void* m, int row, int col, double val) { return Flux::flux_matrix_set(m, row, col, val); }
extern "C" void* matrix_zeros(int rows, int cols);
extern "C" void* matrix_zeros(int rows, int cols) { return Flux::flux_matrix_zeros(rows, cols); }
extern "C" void* matrix_ones(int rows, int cols);
extern "C" void* matrix_ones(int rows, int cols) { return Flux::flux_matrix_ones(rows, cols); }
extern "C" void* matrix_eye(int n);
extern "C" void* matrix_eye(int n) { return Flux::flux_matrix_eye(n); }
extern "C" void* matrix_copy(void* m);
extern "C" void* matrix_copy(void* m) { return Flux::flux_matrix_copy(m); }
extern "C" double matrix_sum(void* m);
extern "C" double matrix_sum(void* m) { return Flux::flux_matrix_sum(m); }
extern "C" double matrix_mean(void* m);
extern "C" double matrix_mean(void* m) { return Flux::flux_matrix_mean(m); }
extern "C" void* matrix_slice(void* m, int r0, int r1, int c0, int c1);
extern "C" void* matrix_slice(void* m, int r0, int r1, int c0, int c1) { return Flux::flux_matrix_slice(m, r0, r1, c0, c1); }
extern "C" double pi();
extern "C" double pi() { return Flux::flux_pi(); }

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

extern "C" double flux_string_concat(double a_dbl, double b_dbl)
{
    const char* a = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(a_dbl)));
    const char* b = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(b_dbl)));
    std::string aStr = a ? std::string(a) : "";
    std::string bStr = b ? std::string(b) : "";
    auto& pool = g_fileio_pool;
    pool.emplace_back(aStr + bStr);
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
}

extern "C" double flux_double_to_string(double val)
{
    auto& pool = g_fileio_pool;
    pool.push_back(std::to_string(val));
    return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
}

extern "C" double flux_regex_match(double str_dbl, double pat_dbl)
{
    const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(str_dbl)));
    const char* pat = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(pat_dbl)));
    if (!str || !pat)
        return 0.0;
    try {
        return std::regex_search(str, std::regex(pat)) ? 1.0 : 0.0;
    } catch (...) {
        return 0.0;
    }
}

extern "C" double flux_regex_replace(double str_dbl, double pat_dbl, double repl_dbl)
{
    const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(str_dbl)));
    const char* pat = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(pat_dbl)));
    const char* repl = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(repl_dbl)));
    if (!str || !pat || !repl)
        return 0.0;
    try {
        auto& pool = g_fileio_pool;
        pool.push_back(std::regex_replace(std::string(str), std::regex(pat), std::string(repl)));
        return jit_bitcast<double>(reinterpret_cast<uintptr_t>(pool.back().c_str()));
    } catch (...) {
        return 0.0;
    }
}

static std::vector<std::pair<double, double>> g_goals;
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
              << " type=" << (diagType && diagType[0] ? diagType : "all")
              << " threshold=" << threshold << std::endl;
    return 1.0;
}

// Weak forwarding wrappers for functions defined in other TUs (advanced_math.cpp).
// These are needed for AOT compilation: the LLVM IR codegen emits calls to short
// names (matrix_lu, fft, etc.), but the runtime library exports flux_-prefixed
// names.  Asm .equ aliases cannot reference cross-TU symbols, so we use C++
// forwarding functions that the linker inlines to a tail call (jmp).
// The declarations are already provided by the "advanced_math.h" include above.
extern "C" void* matrix_lu(void*);
extern "C" void* matrix_lu(void* m) { return flux_matrix_lu(m); }

extern "C" void* matrix_qr(void*);
extern "C" void* matrix_qr(void* m) { return flux_matrix_qr(m); }

extern "C" void* matrix_svd(void*);
extern "C" void* matrix_svd(void* m) { return flux_matrix_svd(m); }

extern "C" void* matrix_cholesky(void*);
extern "C" void* matrix_cholesky(void* m) { return flux_matrix_cholesky(m); }

extern "C" void* matrix_eigenvalues(void*);
extern "C" void* matrix_eigenvalues(void* m) { return flux_matrix_eigenvalues(m); }

extern "C" void* matrix_eigenvectors(void*);
extern "C" void* matrix_eigenvectors(void* m) { return flux_matrix_eigenvectors(m); }

extern "C" double matrix_rank(void*);
extern "C" double matrix_rank(void* m) { return flux_matrix_rank(m); }

extern "C" double matrix_cond(void*);
extern "C" double matrix_cond(void* m) { return flux_matrix_cond(m); }

extern "C" double matrix_norm(void*, double);
extern "C" double matrix_norm(void* m, double p) { return flux_matrix_norm(m, p); }

extern "C" void* fft(void*, double);
extern "C" void* fft(void* sig, double sr) { return flux_fft(sig, sr); }

extern "C" double fft_thd(void*, double);
extern "C" double fft_thd(void* sig, double sr) { return flux_fft_thd(sig, sr); }

extern "C" double fft_snr(void*, double);
extern "C" double fft_snr(void* sig, double sr) { return flux_fft_snr(sig, sr); }

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
// code at lib load). Use a regular static + pthread_getspecific to back the
// jmp_buf stack so no thread_local storage is needed at all.
#include <pthread.h>

static pthread_key_t g_jmp_key;
static pthread_once_t g_jmp_once = PTHREAD_ONCE_INIT;

static void jmp_key_destructor(void* p) { free(p); }

static void jmp_key_init() {
    pthread_key_create(&g_jmp_key, jmp_key_destructor);
}

struct JmpBufStack {
    jmp_buf* slots[256];
    int depth;
};

static JmpBufStack* get_jmp_stack() {
    pthread_once(&g_jmp_once, jmp_key_init);
    void* p = pthread_getspecific(g_jmp_key);
    if (!p) {
        p = calloc(1, sizeof(JmpBufStack));
        pthread_setspecific(g_jmp_key, p);
    }
    return static_cast<JmpBufStack*>(p);
}

static double g_last_thrown_value = 0.0;
static const char* g_last_thrown_msg = nullptr;

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
    if (s->depth > 0) s->depth--;
}

extern "C" void flux_throw_error(double value, const char* msg)
{
    g_last_thrown_value = value;
    g_last_thrown_msg = msg;
    JmpBufStack* s = get_jmp_stack();
    if (s->depth == 0) {
        std::fprintf(stderr, "[FATAL] uncaught Flux exception: %s\n",
                     msg ? msg : "(no message)");
        std::abort();
    }
    longjmp(*s->slots[--s->depth], 1);
}

extern "C" double flux_last_thrown_value() { return g_last_thrown_value; }
extern "C" const char* flux_last_thrown_msg() { return g_last_thrown_msg; }
