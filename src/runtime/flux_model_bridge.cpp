/* Copyright 2026 Janada Sroor
   SPDX-License-Identifier: Apache-2.0
   C API: compile a FluxScript function into a flat C-ABI model callback
   usable by external simulators (FerroMNA, etc.).

   Each call to flux_compile_model creates its own FluxJIT instance so
   that multiple model compilations do not conflict on duplicate stdlib
   symbols.  The compiled function pointer is valid for the lifetime of
   the process.                                                        */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include "flux/compiler/compiler_instance.h"
#include "flux/jit/flux_jit.h"
#include "flux/runtime/flux_model_bridge.h"
#include "flux/runtime/flux_runtime.h"

/* ---- Global storage: every model's JIT lives forever ------------ */
static std::mutex s_jits_mutex;
static std::vector<std::unique_ptr<Flux::FluxJIT>> s_jits;

/* ---- Debug callback (thread-local, set before evaluate) --------- */
static thread_local void (*tls_debug_callback)(const char*) = nullptr;

extern "C" {

/* ------------------------------------------------------------------ */
/*  Helper: parse "Error at line N, column M: message" format          */
/* ------------------------------------------------------------------ */

static bool extractErrorLocation(const std::string& msg,
                                  int* out_line, int* out_col,
                                  std::string* out_message)
{
    // Expected format: "Error at line 42, column 10: some message"
    const char* prefix = "Error at line ";
    if (msg.find(prefix) != 0)
        return false;

    size_t pos = strlen(prefix);
    size_t end = pos;
    while (end < msg.size() && msg[end] >= '0' && msg[end] <= '9')
        end++;
    if (end == pos) return false;
    *out_line = std::stoi(msg.substr(pos, end - pos));

    const char* col_prefix = ", column ";
    if (msg.find(col_prefix, end) != end)
        return false;
    pos = end + strlen(col_prefix);
    end = pos;
    while (end < msg.size() && msg[end] >= '0' && msg[end] <= '9')
        end++;
    if (end == pos) return false;
    *out_col = std::stoi(msg.substr(pos, end - pos));

    const char* sep = ": ";
    if (msg.find(sep, end) != end)
        return false;
    *out_message = msg.substr(end + strlen(sep));
    return true;
}

/* ------------------------------------------------------------------ */
/*  Helper: build the wrapper LLVM module (v1 — 4-arg ABI)            */
/*  Wrapper ABI: void(double* v, double* i, double* j, int n)        */
/* ------------------------------------------------------------------ */

static bool buildWrapperV1(llvm::Module* mod, llvm::IRBuilder<>& builder,
                            const std::string& func_name,
                            int num_pins, std::string& error)
{
    auto& C = mod->getContext();
    auto* dbl_ty = builder.getDoubleTy();
    auto* i32_ty = builder.getInt32Ty();

    // Declare runtime error helpers
    auto* clear_err_fn = llvm::Function::Create(
        llvm::FunctionType::get(builder.getVoidTy(), false),
        llvm::Function::ExternalLinkage, "flux_clear_error", mod);
    auto* get_err_fn = llvm::Function::Create(
        llvm::FunctionType::get(llvm::PointerType::get(C, 0), false),
        llvm::Function::ExternalLinkage, "flux_get_error", mod);

    int num_fields = num_pins + num_pins * num_pins;
    std::vector<llvm::Type*> field_tys(num_fields, dbl_ty);
    auto* model_out_ty = llvm::StructType::create(C, field_tys, "ModelOut");

    std::vector<llvm::Type*> pin_tys(num_pins, dbl_ty);
    bool use_sret = static_cast<size_t>(num_fields) * 8 > 16;

    std::vector<llvm::Type*> user_param_tys;
    if (use_sret)
        user_param_tys.push_back(llvm::PointerType::get(C, 0));
    user_param_tys.insert(user_param_tys.end(), pin_tys.begin(), pin_tys.end());

    auto* user_fn_ty = llvm::FunctionType::get(
        use_sret ? builder.getVoidTy() : llvm::Type::getDoubleTy(C),
        user_param_tys, false);
    auto* user_fn = llvm::Function::Create(
        user_fn_ty, llvm::Function::ExternalLinkage, func_name, mod);
    if (!user_fn) { error = "failed to create user function declaration"; return false; }

    auto* dbl_ptr_ty = llvm::PointerType::get(C, 0);

    std::vector<llvm::Type*> wrapper_param_tys = {dbl_ptr_ty, dbl_ptr_ty, dbl_ptr_ty, i32_ty};
    auto* wrapper_fn_ty = llvm::FunctionType::get(i32_ty, wrapper_param_tys, false);

    std::string wrapper_name = std::string(func_name) + "_wrapper";
    auto* wrapper_fn = llvm::Function::Create(
        wrapper_fn_ty, llvm::Function::ExternalLinkage, wrapper_name, mod);

    auto ai = wrapper_fn->arg_begin();
    auto* v_ptr = ai++; auto* i_ptr = ai++; auto* j_ptr = ai++; ai++;
    v_ptr->setName("v"); i_ptr->setName("i"); j_ptr->setName("j");

    auto* entry  = llvm::BasicBlock::Create(C, "entry", wrapper_fn);
    auto* ok_bb  = llvm::BasicBlock::Create(C, "store_outputs", wrapper_fn);
    auto* err_bb = llvm::BasicBlock::Create(C, "error_return", wrapper_fn);
    builder.SetInsertPoint(entry);

    builder.CreateCall(clear_err_fn);

    std::vector<llvm::Value*> voltages;
    voltages.reserve(num_pins);
    for (int p = 0; p < num_pins; p++) {
        auto* gep = builder.CreateGEP(dbl_ty, v_ptr, builder.getInt32(p), "vgep");
        voltages.push_back(builder.CreateLoad(dbl_ty, gep, "v" + std::to_string(p)));
    }

    llvm::Value* sret_alloca = builder.CreateAlloca(model_out_ty, nullptr, "model_out");

    if (use_sret) {
        std::vector<llvm::Value*> call_args;
        call_args.reserve(1 + num_pins);
        call_args.push_back(sret_alloca);
        call_args.insert(call_args.end(), voltages.begin(), voltages.end());
        builder.CreateCall(user_fn_ty, user_fn, call_args);
    } else {
        auto* result = builder.CreateCall(user_fn_ty, user_fn, voltages, "result");
        builder.CreateStore(result, sret_alloca);
    }

    auto* err_ptr = builder.CreateCall(get_err_fn, {}, "err_ptr");
    auto* has_err = builder.CreateIsNotNull(err_ptr, "has_error");
    builder.CreateCondBr(has_err, err_bb, ok_bb);

    builder.SetInsertPoint(err_bb);
    builder.CreateRet(builder.getInt32(1));

    builder.SetInsertPoint(ok_bb);
    for (int p = 0; p < num_pins; p++) {
        auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca, p, "igep");
        auto* fv = builder.CreateLoad(dbl_ty, fp, "i" + std::to_string(p));
        auto* dst = builder.CreateGEP(dbl_ty, i_ptr, builder.getInt32(p), "idst");
        builder.CreateStore(fv, dst);
    }

    for (int row = 0; row < num_pins; row++) {
        for (int col = 0; col < num_pins; col++) {
            int fi = num_pins + row * num_pins + col;
            auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca, fi, "jgep");
            auto* fv = builder.CreateLoad(dbl_ty, fp, "j" + std::to_string(fi));
            auto* dst = builder.CreateGEP(dbl_ty, j_ptr, builder.getInt32(row * num_pins + col), "jdst");
            builder.CreateStore(fv, dst);
        }
    }

    builder.CreateRet(builder.getInt32(0));
    return true;
}

/* ------------------------------------------------------------------ */
/*  Helper: build the wrapper LLVM module (v2 — 6-arg ABI)            */
/*  Wrapper ABI: void(double* params, int num_params, double* v,      */
/*                    double* i, double* j, int num_pins)              */
/* ------------------------------------------------------------------ */

static bool buildWrapperV2(llvm::Module* mod, llvm::IRBuilder<>& builder,
                            const std::string& func_name,
                            int num_pins, int num_params,
                            std::string& error)
{
    auto& C = mod->getContext();
    auto* dbl_ty = builder.getDoubleTy();
    auto* i32_ty = builder.getInt32Ty();
    auto* i8_ptr_ty = llvm::PointerType::get(C, 0);

    // Declare runtime error helpers (linked through libFluxScript.so)
    auto* clear_err_fn = llvm::Function::Create(
        llvm::FunctionType::get(builder.getVoidTy(), false),
        llvm::Function::ExternalLinkage, "flux_clear_error", mod);
    auto* get_err_fn = llvm::Function::Create(
        llvm::FunctionType::get(i8_ptr_ty, false),
        llvm::Function::ExternalLinkage, "flux_get_error", mod);

    int num_fields = num_pins + num_pins * num_pins;
    std::vector<llvm::Type*> field_tys(num_fields, dbl_ty);
    auto* model_out_ty = llvm::StructType::create(C, field_tys, "ModelOut");

    int user_num_args = num_params + num_pins;
    std::vector<llvm::Type*> pin_tys(user_num_args, dbl_ty);
    bool use_sret = static_cast<size_t>(num_fields) * 8 > 16;

    std::vector<llvm::Type*> user_param_tys;
    if (use_sret)
        user_param_tys.push_back(llvm::PointerType::get(C, 0));
    user_param_tys.insert(user_param_tys.end(), pin_tys.begin(), pin_tys.end());

    auto* user_fn_ty = llvm::FunctionType::get(
        use_sret ? builder.getVoidTy() : llvm::Type::getDoubleTy(C),
        user_param_tys, false);
    auto* user_fn = llvm::Function::Create(
        user_fn_ty, llvm::Function::ExternalLinkage, func_name, mod);
    if (!user_fn) { error = "failed to create user function declaration"; return false; }

    auto* dbl_ptr_ty = llvm::PointerType::get(C, 0);

    std::vector<llvm::Type*> wrapper_param_tys = {
        dbl_ptr_ty, i32_ty, dbl_ptr_ty, dbl_ptr_ty, dbl_ptr_ty, i32_ty};
    // Return i32: 0 = success, 1 = runtime error
    auto* wrapper_fn_ty = llvm::FunctionType::get(i32_ty, wrapper_param_tys, false);

    std::string wrapper_name = std::string(func_name) + "_wrapper";
    auto* wrapper_fn = llvm::Function::Create(
        wrapper_fn_ty, llvm::Function::ExternalLinkage, wrapper_name, mod);

    auto ai = wrapper_fn->arg_begin();
    auto* p_ptr  = ai++; auto* np_val = ai++;
    auto* v_ptr  = ai++; auto* i_ptr  = ai++; auto* j_ptr = ai++; auto* n_val = ai++;
    p_ptr->setName("p");  np_val->setName("np");
    v_ptr->setName("v");  i_ptr->setName("i");  j_ptr->setName("j");  n_val->setName("n");

    auto* entry = llvm::BasicBlock::Create(C, "entry", wrapper_fn);
    auto* ok_bb  = llvm::BasicBlock::Create(C, "store_outputs", wrapper_fn);
    auto* err_bb = llvm::BasicBlock::Create(C, "error_return", wrapper_fn);
    builder.SetInsertPoint(entry);

    // Clear any previous runtime error before calling the user function
    builder.CreateCall(clear_err_fn);

    std::vector<llvm::Value*> user_args;
    user_args.reserve(num_params + num_pins);
    for (int p = 0; p < num_params; p++) {
        auto* gep = builder.CreateGEP(dbl_ty, p_ptr, builder.getInt32(p), "pgep");
        user_args.push_back(builder.CreateLoad(dbl_ty, gep, "p" + std::to_string(p)));
    }
    for (int p = 0; p < num_pins; p++) {
        auto* gep = builder.CreateGEP(dbl_ty, v_ptr, builder.getInt32(p), "vgep");
        user_args.push_back(builder.CreateLoad(dbl_ty, gep, "v" + std::to_string(p)));
    }

    llvm::Value* sret_alloca = builder.CreateAlloca(model_out_ty, nullptr, "model_out");

    if (use_sret) {
        std::vector<llvm::Value*> call_args;
        call_args.reserve(1 + user_args.size());
        call_args.push_back(sret_alloca);
        call_args.insert(call_args.end(), user_args.begin(), user_args.end());
        builder.CreateCall(user_fn_ty, user_fn, call_args);
    } else {
        auto* result = builder.CreateCall(user_fn_ty, user_fn, user_args, "result");
        builder.CreateStore(result, sret_alloca);
    }

    // Check if a runtime error was set during the call
    auto* err_ptr = builder.CreateCall(get_err_fn, {}, "err_ptr");
    auto* has_err = builder.CreateIsNotNull(err_ptr, "has_error");
    builder.CreateCondBr(has_err, err_bb, ok_bb);

    // Error path: return 1 without storing outputs (results are garbage)
    builder.SetInsertPoint(err_bb);
    builder.CreateRet(builder.getInt32(1));

    // Success path: store outputs and return 0
    builder.SetInsertPoint(ok_bb);
    for (int p = 0; p < num_pins; p++) {
        auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca, p, "igep");
        auto* fv = builder.CreateLoad(dbl_ty, fp, "i" + std::to_string(p));
        auto* dst = builder.CreateGEP(dbl_ty, i_ptr, builder.getInt32(p), "idst");
        builder.CreateStore(fv, dst);
    }

    for (int row = 0; row < num_pins; row++) {
        for (int col = 0; col < num_pins; col++) {
            int fi = num_pins + row * num_pins + col;
            auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca, fi, "jgep");
            auto* fv = builder.CreateLoad(dbl_ty, fp, "j" + std::to_string(fi));
            auto* dst = builder.CreateGEP(dbl_ty, j_ptr, builder.getInt32(row * num_pins + col), "jdst");
            builder.CreateStore(fv, dst);
        }
    }

    builder.CreateRet(builder.getInt32(0));
    return true;
}

/* ------------------------------------------------------------------ */
/*  Core compile function used by both v1 and v2 APIs                  */
/* ------------------------------------------------------------------ */

static void* compileModelInternal(const char* source, const char* func_name,
                                   int num_pins, int num_params,
                                   bool v2_abi,
                                   char** error_out,
                                   int* out_line, int* out_col,
                                   const char* include_path)
{
    if (out_line) *out_line = 0;
    if (out_col)  *out_col  = 0;
    if (error_out) *error_out = nullptr;

    if (!source || !func_name || num_pins < 1) {
        if (error_out)
            *error_out = strdup("flux_compile_model: invalid arguments");
        return nullptr;
    }

    /* ---- 1. Create an isolated JIT for this model ---- */
    auto jit = std::make_unique<Flux::FluxJIT>(Flux::OptimizationLevel::O2);

    /* ---- Register ALL runtime functions (exp, sin, matrix_*, etc.) ---- */
    registerRuntimeFunctions(*jit);

    /* ---- Register debug print for this JIT (routes to callback) ---- */
    jit->registerFunction("flux_model_bridge_debug_print",
                          (void*)&flux_model_bridge_debug_print);

    /* ---- 2. Compile user's FluxScript source ---- */
    Flux::CompilerOptions opts;
    opts.optimizationLevel = Flux::OptimizationLevel::O2;
    opts.injectStdlib = true;
    opts.debugInfo = false;
    if (include_path)
        opts.includePath = include_path;

    Flux::CompilerInstance compiler(opts);
    std::string err;

    auto artifacts = compiler.compileToIR(source, &err);
    if (!artifacts) {
        /* Extract source-mapped error from formatted error string */
        std::string msg;
        if (!err.empty()) {
            if (extractErrorLocation(err, out_line, out_col, &msg)) {
                if (error_out)
                    *error_out = strdup(msg.c_str());
            } else {
                if (error_out)
                    *error_out = strdup(err.c_str());
            }
        } else {
            if (error_out)
                *error_out = strdup("flux_compile_model: unknown compilation error");
        }
        return nullptr;
    }

    /* ---- 3. Add user's compiled module to JIT ---- */
    if (artifacts->codegenContext && artifacts->codegenContext->OwnedModule) {
        jit->addModule(std::move(artifacts->codegenContext->OwnedModule),
                       std::move(artifacts->codegenContext->OwnedContext));
    }

    /* ---- 4. Build LLVM IR wrapper module ---- */
    auto ctx  = std::make_unique<llvm::LLVMContext>();
    auto mod = std::make_unique<llvm::Module>("flux_model_wrapper", *ctx);
    llvm::IRBuilder<> builder(*ctx);

    std::string wrapperError;
    bool ok;
    if (v2_abi) {
        ok = buildWrapperV2(mod.get(), builder, func_name,
                            num_pins, num_params, wrapperError);
    } else {
        ok = buildWrapperV1(mod.get(), builder, func_name,
                            num_pins, wrapperError);
    }
    if (!ok) {
        if (error_out)
            *error_out = strdup(wrapperError.c_str());
        return nullptr;
    }

    /* ---- 5. Add wrapper module to JIT ---- */
    jit->addModule(std::move(mod), std::move(ctx));

    /* ---- 6. Look up wrapper function pointer ---- */
    std::string wrapper_name = std::string(func_name) + "_wrapper";
    auto* fn_ptr = jit->getPointerToFunction(wrapper_name);
    if (!fn_ptr) {
        if (error_out) {
            std::string msg = "flux_compile_model: wrapper not found: " + wrapper_name;
            *error_out = strdup(msg.c_str());
        }
        return nullptr;
    }

    /* ---- 7. Keep the JIT alive forever (global) ---- */
    {
        std::lock_guard<std::mutex> lock(s_jits_mutex);
        s_jits.push_back(std::move(jit));
    }
    return fn_ptr;
}

/* ------------------------------------------------------------------ */
/*  flux_compile_model (v1) — original ABI                            */
/*                                                                     */
/*  void* flux_compile_model(source, func_name, num_pins, &error);     */
/*  Returns a function pointer with:                                   */
/*    void model_fn(double* v, double* i, double* j, int n);          */
/* ------------------------------------------------------------------ */

void* flux_compile_model(const char* source, const char* func_name,
                         int num_pins, const char** error)
{
    char* err_msg = nullptr;
    void* fn = compileModelInternal(source, func_name, num_pins,
                                     0 /* num_params */,
                                     false /* v1 ABI */,
                                     &err_msg, nullptr, nullptr,
                                     nullptr /* include_path */);
    if (error) {
        if (err_msg)
            *error = err_msg;
        else
            *error = nullptr;
    } else {
        free(err_msg);
    }
    return fn;
}

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
/*  The error_msg in the result is strdup'd — caller must free with   */
/*  flux_free_error().                                                  */
/* ------------------------------------------------------------------ */

FluxCompileResult flux_compile_model_v2(
    const char* source, const char* func_name,
    int num_pins, int num_params,
    const char* include_path)
{
    if (num_params < 0) num_params = 0;

    FluxCompileResult result;
    result.fn_ptr     = nullptr;
    result.error_line = 0;
    result.error_col  = 0;
    result.error_msg  = nullptr;

    char* err_msg = nullptr;
    int line = 0, col = 0;

    result.fn_ptr = compileModelInternal(
        source, func_name, num_pins, num_params,
        true /* v2 ABI */,
        &err_msg, &line, &col,
        include_path);

    result.error_line = line;
    result.error_col  = col;
    result.error_msg  = err_msg; // caller must free with flux_free_error

    return result;
}

/* ------------------------------------------------------------------ */
/*  flux_free_error — free the error_msg from flux_compile_model_v2   */
/* ------------------------------------------------------------------ */

void flux_free_error(char* error_msg)
{
    free(error_msg);
}

/* ------------------------------------------------------------------ */
/*  Debug callback support                                             */
/*                                                                     */
/*  Set a thread-local callback before calling the model wrapper.      */
/*  FluxScript models call flux_model_bridge_debug_print("msg") to    */
/*  route debug output through this callback back to the simulator.    */
/* ------------------------------------------------------------------ */

void flux_model_bridge_debug_print(const char* msg)
{
    if (tls_debug_callback)
        tls_debug_callback(msg);
}

void flux_model_set_debug_callback(void (*cb)(const char*))
{
    tls_debug_callback = cb;
}

void* flux_model_get_debug_callback(void)
{
    return (void*)tls_debug_callback;
}

} /* extern "C" */
