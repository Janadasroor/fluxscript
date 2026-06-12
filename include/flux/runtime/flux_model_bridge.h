/* Copyright 2026 Janada Sroor
   SPDX-License-Identifier: Apache-2.0
   C API: compile a FluxScript function into a flat C-ABI model callback
   usable by external simulators (FerroMNA, etc.).

   Two API versions:
   - flux_compile_model:      original ABI, returns void(*)(double*,double*,double*,int)
   - flux_compile_model_v2:   extended ABI, returns structured result with source-mapped
                              error info and optional model instance parameters.
*/

#ifndef FLUX_RUNTIME_FLUX_MODEL_BRIDGE_H
#define FLUX_RUNTIME_FLUX_MODEL_BRIDGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  flux_compile_model (v1) — original, kept for backward compat      */
/*                                                                     */
/*    void* flux_compile_model(source, func_name, num_pins, &error);   */
/*    Returns a function pointer with this ABI:                        */
/*      void model_fn(double* voltages, double* currents,              */
/*                    double* jacobian, int num_pins);                 */
/* ------------------------------------------------------------------ */
void* flux_compile_model(const char* source, const char* func_name,
                         int num_pins, const char** error);

/* ------------------------------------------------------------------ */
/*  flux_compile_model_v2 — extended API                               */
/*                                                                     */
/*  Differences from v1:                                               */
/*    1. Returns structured FluxCompileResult with source-mapped       */
/*       error info (line, column, message).                           */
/*    2. Supports model instance parameters via num_params — the       */
/*       wrapper loads params[0..num_params-1] and passes them as     */
/*       the first arguments to the user function.                     */
/*                                                                     */
/*  User function signature (with params):                             */
/*    def model_fn(p1: Double, ..., pM: Double,                        */
/*                 v1: Double, ..., vN: Double) -> ModelOut           */
/*                                                                     */
/*  Wrapper ABI:                                                       */
/*    void model_fn(double* params, int num_params,                    */
/*                  double* v, double* i, double* j, int num_pins);    */
/*                                                                     */
/*  Pass num_params=0 for no params (same as v1 but with params ptr   */
/*  set to NULL).                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    void*  fn_ptr;       // NULL on error
    int    error_line;   // 0 if unknown
    int    error_col;    // 0 if unknown
    char*  error_msg;    // strdup'd — caller must free with flux_free_error
} FluxCompileResult;

FluxCompileResult flux_compile_model_v2(
    const char* source, const char* func_name,
    int num_pins, int num_params);

/* Free the error_msg string returned by flux_compile_model_v2 */
void flux_free_error(char* error_msg);

/* ------------------------------------------------------------------ */
/*  Debug callback support                                             */
/*                                                                     */
/*  Set a callback before calling the model wrapper.  FluxScript       */
/*  models can call flux_model_bridge_debug_print("msg") which will   */
/*  route the message through this callback back to the simulator.     */
/*                                                                     */
/*  Typical usage from a simulator:                                    */
/*    // Before the first evaluate() call for a model:                 */
/*    flux_model_set_debug_callback(my_print_fn);                      */
/*                                                                     */
/*    // In the model's FluxScript source:                             */
/*    flux_model_bridge_debug_print("iteration " + string(n));        */
/*                                                                     */
/*  The callback is thread-local, so multiple threads can each have    */
/*  their own callback.                                                */
/* ------------------------------------------------------------------ */

/* Debug print function registered in each model's JIT */
void flux_model_bridge_debug_print(const char* msg);

/* Set the thread-local debug callback (set before calling wrapper) */
void flux_model_set_debug_callback(void (*callback)(const char*));

/* Get the current thread-local debug callback */
void* flux_model_get_debug_callback(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLUX_RUNTIME_FLUX_MODEL_BRIDGE_H */
