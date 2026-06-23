/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/jit/flux_jit.h"
#include "flux/runtime/flux_runtime.h"
#include "flux/runtime/advanced_math.h"
#include <complex>

// JIT-callable wrappers (flux_runtime.cpp, C++ linkage)
double jit_register_analysis(double name_ptr);
double jit_register_measure(double name_ptr, double type_ptr);
double jit_register_probe(double name_ptr, double out_ptr);
double jit_register_save(double name_ptr);
double jit_register_param(double name_ptr, double value);
double jit_register_ic(double name_ptr, double value);

// Additional matrix functions not in flux_runtime.h
extern "C" void flux_matrix_set(void* m_ptr, int row, int col, double val);
extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s);
extern "C" void* flux_matrix_div_ms(void* m_ptr, double s);
extern "C" void* flux_matrix_div_sm(double s, void* m_ptr);
extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr);
extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr);
extern "C" void* flux_matrix_add_ms(void* m_ptr, double s);
extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s);
extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr);
extern "C" void* flux_matrix_zeros(int rows, int cols);
extern "C" void* flux_create_range_sum(double start, double step, double end);
extern "C" void* flux_matrix_ones(int rows, int cols);
extern "C" void* flux_matrix_eye(int n);
extern "C" void* flux_matrix_copy(void* m_ptr);
extern "C" void* flux_matrix_diag(void* v_ptr);
extern "C" void* flux_matrix_hcat(void* a_ptr, void* b_ptr);
extern "C" void* flux_matrix_vcat(void* a_ptr, void* b_ptr);
extern "C" double flux_matrix_trace(void* m_ptr);
extern "C" void* flux_matrix_diag_extract(void* m_ptr);
extern "C" void* flux_matrix_slice(void* m_ptr, int r0, int r1, int c0, int c1);
extern "C" double flux_matrix_sum(void* m_ptr);
extern "C" double flux_matrix_mean(void* m_ptr);
extern "C" void* flux_promote_matrix_to_complex(void* m_ptr, int rows, int cols);
extern "C" void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols);
extern "C" void* flux_complex_matrix_mul(void* a_ptr, void* b_ptr);
extern "C" void* flux_complex_matrix_add(void* a_ptr, void* b_ptr);
extern "C" void* flux_complex_matrix_sub(void* a_ptr, void* b_ptr);
extern "C" void* flux_complex_matrix_mul_ms(void* m_ptr, double re, double im);
extern "C" void* flux_complex_matrix_transpose(void* m_ptr);
extern "C" void* flux_complex_matrix_conj(void* m_ptr);
extern "C" void* flux_complex_matrix_ctranspose(void* m_ptr);
extern "C" void* flux_complex_matrix_inv(void* m_ptr);
extern "C" double flux_complex_matrix_det(void* m_ptr);
extern "C" double flux_complex_matrix_trace(void* m_ptr);
extern "C" double flux_complex_matrix_get_real(void* m_ptr, int row, int col);
extern "C" double flux_complex_matrix_get_imag(void* m_ptr, int row, int col);
extern "C" void flux_complex_matrix_set(void* m_ptr, int row, int col, double real, double imag);
extern "C" void* flux_complex_matrix_zeros(int rows, int cols);
extern "C" void* flux_complex_matrix_ones(int rows, int cols);
extern "C" void* flux_complex_matrix_eye(int n);
extern "C" int flux_complex_matrix_rows(void* m_ptr);
extern "C" int flux_complex_matrix_cols(void* m_ptr);
extern "C" void* flux_complex_matrix_ew_div(void* a_ptr, void* b_ptr);
extern "C" void* flux_complex_matrix_ew_mul(void* a_ptr, void* b_ptr);
extern "C" void* flux_complex_matrix_add_ms(void* m_ptr, double re, double im);
extern "C" void* flux_complex_matrix_sub_ms(void* m_ptr, double re, double im);
extern "C" void* flux_complex_matrix_sub_sm(double re, double im, void* m_ptr);
extern "C" void* flux_complex_matrix_div_ms(void* m_ptr, double re, double im);
extern "C" void* flux_complex_matrix_div_sm(double re, double im, void* m_ptr);
extern "C" void flux_free_matrix(void* ptr);
extern "C" void* flux_lookup_matrix(void* data_ptr);
extern "C" void* flux_register_new_matrix(int rows, int cols);
extern "C" void* flux_register_new_complex_matrix(int rows, int cols);
extern "C" void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols);
extern "C" double flux_print_complex_matrix(void* m_ptr);

// String functions not in flux_runtime.h
extern "C" double flux_string_concat(double a_dbl, double b_dbl);
extern "C" double flux_double_to_string(double val);
extern "C" double flux_regex_match(double str_dbl, double pat_dbl);
extern "C" double flux_regex_replace(double str_dbl, double pat_dbl, double repl_dbl);
extern "C" double flux_string_at(double s_ptr, double index);
extern "C" double flux_string_slice(double s_ptr, double start, double end);
extern "C" double flux_string_find(double s_ptr, double c);
extern "C" double flux_parse_number(double s_ptr);
extern "C" double flux_vec_eq(double* a_data, int a_size, double* b_data, int b_size);

// File I/O
extern "C" double flux_fopen(double filename_ptr, double mode_ptr);
extern "C" double flux_fclose(double handle);
extern "C" double flux_fflush(double handle);
extern "C" double flux_feof(double handle);
extern "C" double flux_fgets(double handle);
extern "C" double flux_fprintf(double handle, double format_ptr, double arg);
extern "C" double flux_fetch_url(double url_ptr);

// Symbolic
extern "C" double flux_sym_decl(const char* name);
extern "C" double flux_sym_number(double val);
extern "C" double flux_sym_add(double a, double b);
extern "C" double flux_sym_sub(double a, double b);
extern "C" double flux_sym_mul(double a, double b);
extern "C" double flux_sym_div(double a, double b);
extern "C" double flux_sym_pow(double a, double b);
extern "C" double flux_sym_simplify(double e);
extern "C" double flux_sym_pdiff(double e, double v_dbl, int o);
extern "C" double flux_sym_eq(double a, double b);
extern "C" double flux_sym_ne(double a, double b);
extern "C" double flux_sym_differentiate(double e, const char* var);
extern "C" double flux_sym_integrate(double e, const char* var);
extern "C" double flux_sym_laplace(double e, const char* t, const char* s);
extern "C" double flux_sym_inverse_laplace(double e, const char* s, const char* t);
extern "C" double flux_sym_expand(double e);
extern "C" double flux_sym_factor(double e);
extern "C" double flux_sym_collect(double e, const char* var);
extern "C" double flux_sym_numerator(double e);
extern "C" double flux_sym_denominator(double e);
extern "C" void* flux_sym_poles(double e);
extern "C" void* flux_sym_zeros(double e);
extern "C" void* flux_sym_jacobian(void* e_p, int ec, void* v_p, int vc);
extern "C" double flux_sym_pde_register(double eq, int vc, void* v_p);

// Analysis
extern "C" double flux_stability_run(double output_dbl);
extern "C" double flux_sensitivity_run(double output_dbl);
extern "C" double flux_register_worst_case(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl, int iterations);
extern "C" double flux_monte_carlo_analyze(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl, int iterations);
extern "C" double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl,
                                        double mins_dbl, double maxs_dbl, int num_vars, int max_iter);
extern "C" double flux_bode_analyze(double output_dbl, double freqStart, double freqEnd, int pointsPerDecade);
extern "C" double flux_plot_data(double* yData, int yRows, int yCols, double* xData, int hasX, double title_dbl);

// Threading
extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr, void* user_data);
extern "C" int64_t flux_get_num_threads();
extern "C" double flux_spawn(void* func_ptr, void* args, int64_t nargs);
extern "C" double flux_join(double handle);
extern "C" double flux_thread_self();
extern "C" double flux_chan_create();
extern "C" void flux_chan_send(double chan, double val);
extern "C" double flux_chan_recv(double chan);
extern "C" void flux_chan_close(double chan);
extern "C" void flux_chan_destroy(double chan);
extern "C" double flux_mutex_create();
extern "C" void flux_mutex_lock(double mtx);
extern "C" void flux_mutex_unlock(double mtx);
extern "C" void flux_mutex_destroy(double mtx);
extern "C" double flux_rwlock_create();
extern "C" void flux_rwlock_read_lock(double rw);
extern "C" void flux_rwlock_write_lock(double rw);
extern "C" void flux_rwlock_unlock(double rw);
extern "C" void flux_rwlock_destroy(double rw);

// Error handling
extern "C" void flux_set_error(const char* msg);
extern "C" const char* flux_get_error();
extern "C" void flux_clear_error();
extern "C" void flux_push_jmp_buf(void* buf);
extern "C" void flux_pop_jmp_buf();
extern "C" void flux_throw_error(double value, const char* msg);
extern "C" double flux_last_thrown_value();
extern "C" const char* flux_last_thrown_msg();

// Goals / optimize / GPU / allocator (flux_runtime.cpp)
extern "C" void flux_register_goal(double current, double target);
extern "C" double flux_optimize();
extern "C" double flux_hot_swap(double name_dbl, double model_ptr);
extern "C" double flux_set_diagnostic(double node_dbl, double type_dbl, double threshold);
extern "C" double flux_gpu_available();
extern "C" void flux_gpu_set_enabled(double enabled);
extern "C" double flux_print_double(double x);
extern "C" void* flux_malloc(size_t size);
extern "C" void flux_free_all_allocations();
extern "C" double flux_dyn_ptr_push(double data, double vtable);
extern "C" double flux_dyn_ptr_get_data(double idx);
extern "C" double flux_dyn_ptr_get_vtable(double idx);
extern "C" void flux_dyn_ptr_clear();
extern "C" void flux_inc_call_depth();
extern "C" void flux_dec_call_depth();

// SPICE additional
extern "C" double flux_register_subckt(double name_ptr, double pins_ptr);
extern "C" double flux_register_analysis(const char* analysis_type);
extern "C" double flux_register_measure(const char* name, const char* measure_type);
extern "C" double flux_register_probe(const char* var_name, const char* output_name);
extern "C" double flux_register_save(const char* var_name);
extern "C" double flux_register_param(const char* name, double value);
extern "C" double flux_register_ic(const char* node_name, double value);
extern "C" double flux_register_bsource(const char* name, const char* pos, const char* neg,
                                        double expr, int is_current);
extern "C" double flux_register_esource(const char* name, const char* pos, const char* neg,
                                        const char* cpos, const char* cneg, double gain);
extern "C" double flux_register_fsource(const char* name, const char* pos, const char* neg,
                                        const char* vsource, double gain);
extern "C" double flux_register_gsource(const char* name, const char* pos, const char* neg,
                                        const char* cpos, const char* cneg, double transcond);
extern "C" double flux_register_hsource(const char* name, const char* pos, const char* neg,
                                        const char* vsource, double transres);
extern "C" double flux_register_subckt_instance(const char* name, const char* subckt,
                                                double pins_ptr, double params_ptr);
extern "C" double flux_register_model(const char* name, const char* type, double params_ptr);
extern "C" double flux_register_update_func(const char* time_var, const char* inputs_var,
                                            void* body_func);
extern "C" double flux_edge_detect(double value, double edge_type);
extern "C" double flux_cross_detect(double value, double rise_fall);
extern "C" void flux_register_adevice(const char* name, int device_type, const char* input_nodes, const char* output_nodes);
extern "C" double flux_register_wavefile(const char* name, const char* pos_node,
                                         const char* neg_node, const char* file_path, int channel);

// LLVM bridge
extern "C" double flux_llvm_context_create();
extern "C" double flux_llvm_context_dispose(double ctx);
extern "C" double flux_llvm_module_create_with_name(double name);
extern "C" double flux_llvm_module_create_with_name_in_ctx(double name, double ctx);
extern "C" double flux_llvm_get_module_context(double module);
extern "C" double flux_llvm_dispose_module(double module);
extern "C" double flux_llvm_print_module(double mod);
extern "C" double flux_llvm_print_module_to_string(double module);
extern "C" double flux_llvm_dispose_message(double msg);
extern "C" double flux_llvm_verify_module(double module, double action, double out_message);
extern "C" double flux_llvm_verify_function(double fn, double action);
extern "C" double flux_llvm_add_function(double module, double name, double fn_type);
extern "C" double flux_llvm_get_named_function(double module, double name);
extern "C" double flux_llvm_create_builder_in_ctx(double ctx);
extern "C" double flux_llvm_create_builder();
extern "C" double flux_llvm_position_builder_at_end(double builder, double block);
extern "C" double flux_llvm_get_insert_block(double builder);
extern "C" double flux_llvm_dispose_builder(double builder);
extern "C" double flux_llvm_clear_insertion_position(double builder);
extern "C" double flux_llvm_get_param(double fn, double index);
extern "C" double flux_llvm_get_first_param(double fn);
extern "C" double flux_llvm_count_params(double fn);
extern "C" double flux_llvm_append_basic_block(double fn, double name);
extern "C" double flux_llvm_get_first_basic_block(double fn);
extern "C" double flux_llvm_get_basic_block_terminator(double block);
extern "C" double flux_llvm_set_value_name(double val, double name);
extern "C" double flux_llvm_get_value_name(double val);
extern "C" double flux_llvm_type_of(double val);
extern "C" double flux_llvm_global_get_value_type(double global_val);
extern "C" double flux_llvm_add_global(double module, double type, double name);
extern "C" double flux_llvm_set_initializer(double global, double const_val);
extern "C" double flux_llvm_set_linkage(double global, double linkage);
extern "C" double flux_llvm_void_type_in_ctx(double ctx);
extern "C" double flux_llvm_int1_type_in_ctx(double ctx);
extern "C" double flux_llvm_int8_type_in_ctx(double ctx);
extern "C" double flux_llvm_int32_type_in_ctx(double ctx);
extern "C" double flux_llvm_int64_type_in_ctx(double ctx);
extern "C" double flux_llvm_double_type_in_ctx(double ctx);
extern "C" double flux_llvm_float_type_in_ctx(double ctx);
extern "C" double flux_llvm_pointer_type_in_ctx(double ctx, double addr_space);
extern "C" double flux_llvm_function_type(double ret_ty, double param_count, double is_var_arg);
extern "C" double flux_llvm_struct_type_in_ctx(double ctx, double elem_count, double packed);
extern "C" double flux_llvm_struct_create_named(double ctx, double name);
extern "C" double flux_llvm_struct_set_body(double struct_ty, double elem_count, double packed);
extern "C" double flux_llvm_array_type(double elem_ty, double elem_count);
extern "C" double flux_llvm_get_element_type(double ty);
extern "C" double flux_llvm_get_type_kind(double ty);
extern "C" double flux_llvm_struct_get_type_index(double struct_ty, double i);
extern "C" double flux_llvm_const_int(double int_ty, double value, double sign_extend);
extern "C" double flux_llvm_const_real(double real_ty, double value);
extern "C" double flux_llvm_const_null(double ty);
extern "C" double flux_llvm_const_string_in_ctx(double ctx, double str, double length, double dont_null_terminate);
extern "C" double flux_llvm_const_struct_in_ctx(double ctx, double elem_count, double packed);
extern "C" double flux_llvm_get_undef(double ty);
extern "C" double flux_llvm_const_bitcast(double val, double dest_type);
extern "C" double flux_llvm_const_named_struct(double struct_ty, double elem_count);
extern "C" double flux_llvm_call_arg_push(double val);
extern "C" double flux_llvm_call_arg_reset();
extern "C" double flux_llvm_call_arg_save();
extern "C" double flux_llvm_call_arg_restore(double handle);
extern "C" double flux_llvm_type_arg_push(double ty);
extern "C" double flux_llvm_type_arg_reset();
extern "C" double flux_llvm_index_arg_push(double idx);
extern "C" double flux_llvm_index_arg_reset();
extern "C" double flux_llvm_const_arg_push(double val);
extern "C" double flux_llvm_const_arg_reset();
extern "C" double flux_llvm_build_ret_void(double builder);
extern "C" double flux_llvm_build_ret(double builder, double val);
extern "C" double flux_llvm_build_br(double builder, double dest);
extern "C" double flux_llvm_build_cond_br(double builder, double cond, double then_bb, double else_bb);
extern "C" double flux_llvm_build_unreachable(double builder);
extern "C" double flux_llvm_build_add(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_fadd(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_sub(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_fsub(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_mul(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_fmul(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_sdiv(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_fdiv(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_srem(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_frem(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_neg(double builder, double v, double name);
extern "C" double flux_llvm_build_fneg(double builder, double v, double name);
extern "C" double flux_llvm_build_and(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_or(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_xor(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_shl(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_ashr(double builder, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_alloca(double builder, double ty, double name);
extern "C" double flux_llvm_build_load2(double builder, double ty, double ptr, double name);
extern "C" double flux_llvm_build_store(double builder, double val, double ptr);
extern "C" double flux_llvm_build_gep2(double builder, double ty, double ptr, double num_indices, double name);
extern "C" double flux_llvm_build_struct_gep2(double builder, double ty, double ptr, double index, double name);
extern "C" double flux_llvm_build_global_string_ptr(double builder, double str, double name);
extern "C" double flux_llvm_build_call2(double builder, double func_ty, double fn, double num_args, double name);
extern "C" double flux_llvm_build_icmp(double builder, double op, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_fcmp(double builder, double op, double lhs, double rhs, double name);
extern "C" double flux_llvm_build_phi(double builder, double ty, double name);
extern "C" double flux_llvm_build_select(double builder, double cond, double then_v, double else_v, double name);
extern "C" double flux_llvm_add_incoming(double phi, double val, double block);
extern "C" double flux_llvm_build_extract_value(double builder, double agg, double index, double name);
extern "C" double flux_llvm_build_insert_value(double builder, double agg, double elem, double index, double name);
extern "C" double flux_llvm_build_trunc(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_zext(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_sext(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_fptosi(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_sitofp(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_uitofp(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_ptrtoint(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_inttoptr(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_build_bitcast(double builder, double val, double dest_ty, double name);
extern "C" double flux_llvm_get_target_from_triple(double triple, double out_target);
extern "C" double flux_llvm_create_target_machine(double target, double triple, double cpu, double features, double level);
extern "C" double flux_llvm_target_machine_emit_to_file(double tm, double module, double filename, double file_type);
extern "C" double flux_llvm_target_machine_emit_to_mem_buf(double tm, double module, double file_type);
extern "C" double flux_llvm_initialize_native_target();
extern "C" double flux_llvm_initialize_native_asm_printer();
extern "C" double flux_llvm_write_bitcode_to_file(double module, double path);
extern "C" double flux_llvm_get_current_debug_location(double builder);
extern "C" double flux_llvm_set_current_debug_location(double builder, double loc);
extern "C" double flux_hm_create();
extern "C" double flux_hm_destroy(double hm_id);
extern "C" double flux_hm_put(double hm_id, double key, double val);
extern "C" double flux_hm_get(double hm_id, double key);
extern "C" double flux_hm_has(double hm_id, double key);
extern "C" double flux_hm_remove(double hm_id, double key);
extern "C" double flux_hm_size(double hm_id);
extern "C" double flux_hm_keys(double hm_id);
extern "C" double flux_type_store(double name_dbl, double type_val);
extern "C" double flux_type_load(double name_dbl);
extern "C" double flux_type_has(double name_dbl);

// AI (extern "C" in surrogate.cpp, inside Flux::AI namespace)
extern "C" {
void* flux_nn_create(const int* layers, int numLayers);
void flux_nn_destroy(void* nn);
void flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inputDim, int outputDim, int epochs);
void flux_nn_predict(void* nn, const double* input, double* output, int inputDim, int outputDim);
void flux_nn_save(void* nn, const char* filename);
void* flux_nn_load(const char* filename);
void* flux_surrogate_create();
void flux_surrogate_train(void* surrogate, const char* circuit_type, const double* params, const double* results, int numSamples, int numParams);
void flux_surrogate_predict(void* surrogate, const double* params, double* result, int numParams);
} // extern "C"

// Random
extern "C" double rand_uniform(double min, double max);

// File reading
extern "C" double flux_read_file(double path_ptr);
extern "C" double flux_clock_ms();

// Additional string functions (string_runtime.cpp)
extern "C" double flux_strcmp(double a_ptr, double b_ptr);
extern "C" double flux_strlen(double s_ptr);

// Additional LLVM bridge functions (flux_llvm_bridge.cpp)
extern "C" double flux_str_len(double str_ptr);
extern "C" double flux_str_at(double str_ptr, double idx);
extern "C" double flux_str_slice(double str_ptr, double start, double end);
extern "C" double flux_str_from_char(double ch);
extern "C" double flux_str_concat(double a_ptr, double b_ptr);
extern "C" double flux_str_from_double(double val);
extern "C" double flux_str_cmp(double a_ptr, double b_ptr);

void registerRuntimeFunctions(Flux::FluxJIT& jit)
{
    using namespace Flux;
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
    jit.registerFunction("flux_nn_create", (void*)&flux_nn_create);
    jit.registerFunction("flux_nn_train", (void*)&flux_nn_train);
    jit.registerFunction("flux_nn_predict", (void*)&flux_nn_predict);
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
    jit.registerFunction("flux_fflush", (void*)&flux_fflush);
    jit.registerFunction("flux_feof", (void*)&flux_feof);
    jit.registerFunction("flux_fgets", (void*)&flux_fgets);
    jit.registerFunction("flux_fprintf", (void*)&flux_fprintf);
    jit.registerFunction("flux_fetch_url", (void*)&flux_fetch_url);
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

    // Runtime error reporting (callable from JIT-compiled FluxScript)
    jit.registerFunction("flux_set_error", (void*)&flux_set_error);
    jit.registerFunction("flux_get_error", (void*)&flux_get_error);
    jit.registerFunction("flux_clear_error", (void*)&flux_clear_error);
}
