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
/*    3. An optional include_path can be provided for import           */
/*       resolution — when compiling from a source string, the         */
/*       ModuleLoader uses this path to find imported modules.         */
/*       Pass NULL or "" to use the current working directory.         */
/*                                                                     */
/*  PARAMETER SEMANTICS:                                               */
/*  - Params are positional in a flat double* array. The caller       */
/*    passes params[0] = p1, params[1] = p2, ..., params[M-1] = pM.  */
/*  - num_params declares how many leading Double arguments the       */
/*    FluxScript function expects before the pin-voltage arguments.   */
/*    Pass num_params=0 when there are no model parameters (the       */
/*    params pointer may be set to NULL in that case).                */
/*  - The .model card in the netlist (e.g. .model RLOAD fluxmod(     */
/*    pins=2 r=100)) does NOT automatically map to num_params —       */
/*    model parameters are obtained from the SPICE .param values      */
/*    and passed at runtime by the simulator. The host simulator      */
/*    is responsible for extracting parameter values and filling      */
/*    the params[] array before each evaluate() call.                 */
/*  - Param names are declared in the .flux source file as the       */
/*    first M parameters of the model function. The C ABI knows       */
/*    only the count, not the names. The host simulator must          */
/*    maintain its own mapping from parameter names to array          */
/*    indices (e.g. via the .model card's PARAM=value pairs).        */
/*                                                                     */
/*  MINIMAL EXAMPLE (FluxScript source with two params):              */
/*    struct ModelOut { i1: Double, j11: Double }                     */
/*    def model_fn(r: Double, temp: Double,                           */
/*                 v1: Double, v2: Double) -> ModelOut {              */
/*      let g = 1.0 / r * (1.0 + 0.0039 * (temp - 27.0))            */
/*      let vd = v1 - v2                                              */
/*      let i = vd * g                                                */
/*      ModelOut { i1: i, j11: g }                                    */
/*    }                                                               */
/*                                                                     */
/*  User function signature (with params):                             */
/*    def model_fn(p1: Double, ..., pM: Double,                        */
/*                 v1: Double, ..., vN: Double) -> ModelOut           */
/*                                                                     */
/*  Wrapper ABI (returns int: 0 = success, 1 = runtime error):        */
/*    int model_fn(double* params, int num_params,                     */
/*                  double* v, double* i, double* j, int num_pins);    */
/*                                                                     */
/*  ModelOut layout (num_pins + num_pins*num_pins doubles):           */
/*    indices 0..N-1:          branch currents i[0]..i[N-1]           */
/*    indices N..N+N*N-1:      Jacobian in row-major order            */
/*                              J(0,0), J(0,1), ..., J(N-1,N-1)       */
/* ------------------------------------------------------------------ */
typedef struct {
    void*  fn_ptr;       // NULL on error
    int    error_line;   // 0 if unknown
    int    error_col;    // 0 if unknown
    char*  error_msg;    // strdup'd — caller must free with flux_free_error
} FluxCompileResult;

FluxCompileResult flux_compile_model_v2(
    const char* source, const char* func_name,
    int num_pins, int num_params,
    const char* include_path);

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

/* ------------------------------------------------------------------ */
/*  Runtime error reporting                                            */
/*                                                                     */
/*  A JIT-compiled FluxScript model can call flux_set_error("msg")     */
/*  to signal a runtime fault (e.g. division by zero, domain error).   */
/*  The wrapper now returns an int: 0 = success, 1 = error.            */
/*  After a failed call, the host calls flux_get_error() to retrieve   */
/*  the error message (or NULL if no error). Use flux_clear_error()    */
/*  before each evaluation to reset the state.                          */
/*                                                                     */
/*  Thread-safe: each thread has its own error state.                  */
/* ------------------------------------------------------------------ */
const char* flux_get_error(void);
void flux_clear_error(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLUX_RUNTIME_FLUX_MODEL_BRIDGE_H */
