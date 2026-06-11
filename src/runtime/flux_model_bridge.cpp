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
#include "flux/runtime/flux_runtime.h"

extern "C" {

/* ------------------------------------------------------------------ */
/*  flux_compile_model                                                */
/*                                                                     */
/*  Compile a FluxScript source string containing a user-defined       */
/*  component model, then generate a wrapper with the flat C ABI:     */
/*                                                                     */
/*    void model_fn(double* voltages, double* currents,                */
/*                  double* jacobian, int num_pins);                   */
/*                                                                     */
/*  The user's FluxScript function must have the signature:            */
/*                                                                     */
/*    def model_fn(v1: Double, v2: ..., vN: Double) -> ModelOut       */
/*                                                                     */
/*  where ModelOut is a struct with N + N*N Double fields:            */
/*    - first N fields are the pin currents                           */
/*    - remaining N*N fields are the Jacobian entries (row-major)     */
/*                                                                     */
/*  Each call creates its own JIT instance. The returned function      */
/*  pointer is valid for the lifetime of the process.                  */
/* ------------------------------------------------------------------ */

/* ---- Global storage: every model's JIT lives forever ------------ */
static std::vector<std::unique_ptr<Flux::FluxJIT>> s_jits;

void* flux_compile_model(const char* source, const char* func_name,
                         int num_pins, const char** error)
{
    if (!source || !func_name || num_pins < 1) {
        if (error)
            *error = strdup("flux_compile_model: invalid arguments");
        return nullptr;
    }

    /* ---- 1. Create an isolated JIT for this model ---- */
    auto jit = std::make_unique<Flux::FluxJIT>(Flux::OptimizationLevel::O2);

    /* ---- Register ALL runtime functions (exp, sin, matrix_*, etc.) ---- */
    registerRuntimeFunctions(*jit);

    /* ---- 2. Compile user's FluxScript source ---- */
    Flux::CompilerOptions opts;
    opts.optimizationLevel = Flux::OptimizationLevel::O2;
    opts.injectStdlib = true;

    Flux::CompilerInstance compiler(opts);
    std::string err;

    auto artifacts = compiler.compileToIR(source, &err);
    if (!artifacts) {
        if (error)
            *error = strdup(err.c_str());
        return nullptr;
    }

    /* ---- Add user's compiled module to JIT ---- */
    if (artifacts->codegenContext && artifacts->codegenContext->OwnedModule) {
        jit->addModule(std::move(artifacts->codegenContext->OwnedModule),
                       std::move(artifacts->codegenContext->OwnedContext));
    }

    /* ---- 3. Build LLVM IR wrapper module ---- */
    auto ctx  = std::make_unique<llvm::LLVMContext>();
    auto& C   = *ctx;
    auto mod  = std::make_unique<llvm::Module>("flux_model_wrapper", C);
    llvm::IRBuilder<> builder(C);

    /* ---- Struct type: N currents + N*N Jacobian entries ---- */
    int num_fields = num_pins + num_pins * num_pins;
    auto* dbl_ty = builder.getDoubleTy();
    std::vector<llvm::Type*> field_tys(num_fields, dbl_ty);
    auto* model_out_ty =
        llvm::StructType::create(C, field_tys, "ModelOut");

    /* ---- User function type ---- */
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
        user_fn_ty, llvm::Function::ExternalLinkage, func_name, mod.get());

    /* ---- Wrapper: void(double* v, double* i, double* j, int n) ---- */
    auto* dbl_ptr_ty = llvm::PointerType::get(C, 0);
    auto* i32_ty = builder.getInt32Ty();

    std::vector<llvm::Type*> wrapper_param_tys = {
        dbl_ptr_ty, dbl_ptr_ty, dbl_ptr_ty, i32_ty};
    auto* wrapper_fn_ty = llvm::FunctionType::get(
        builder.getVoidTy(), wrapper_param_tys, false);

    std::string wrapper_name = std::string(func_name) + "_wrapper";
    auto* wrapper_fn = llvm::Function::Create(
        wrapper_fn_ty, llvm::Function::ExternalLinkage, wrapper_name, mod.get());

    auto ai = wrapper_fn->arg_begin();
    auto* v_ptr = ai++;
    auto* i_ptr = ai++;
    auto* j_ptr = ai++;
    ai->setName("n");
    v_ptr->setName("v");
    i_ptr->setName("i");
    j_ptr->setName("j");

    /* ---- Wrapper body ---- */
    auto* entry = llvm::BasicBlock::Create(C, "entry", wrapper_fn);
    builder.SetInsertPoint(entry);

    std::vector<llvm::Value*> voltages;
    voltages.reserve(num_pins);
    for (int p = 0; p < num_pins; p++) {
        auto* gep = builder.CreateGEP(dbl_ty, v_ptr,
                                       builder.getInt32(p), "vgep");
        voltages.push_back(
            builder.CreateLoad(dbl_ty, gep, "v" + std::to_string(p)));
    }

    /* Call the user function */
    llvm::Value* sret_alloca = builder.CreateAlloca(model_out_ty, nullptr,
                                                     "model_out");

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

    for (int p = 0; p < num_pins; p++) {
        auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca, p, "igep");
        auto* fv = builder.CreateLoad(dbl_ty, fp, "i" + std::to_string(p));
        auto* dst = builder.CreateGEP(dbl_ty, i_ptr, builder.getInt32(p), "idst");
        builder.CreateStore(fv, dst);
    }

    for (int row = 0; row < num_pins; row++) {
        for (int col = 0; col < num_pins; col++) {
            int fi = num_pins + row * num_pins + col;
            auto* fp = builder.CreateStructGEP(model_out_ty, sret_alloca,
                                                fi, "jgep");
            auto* fv = builder.CreateLoad(dbl_ty, fp, "j" + std::to_string(fi));
            int flat = row * num_pins + col;
            auto* dst = builder.CreateGEP(dbl_ty, j_ptr,
                                           builder.getInt32(flat), "jdst");
            builder.CreateStore(fv, dst);
        }
    }

    builder.CreateRetVoid();

    /* ---- 4. Add wrapper module to JIT ---- */
    jit->addModule(std::move(mod), std::move(ctx));

    /* ---- 5. Look up wrapper function pointer ---- */
    auto* fn_ptr = jit->getPointerToFunction(wrapper_name);
    if (!fn_ptr) {
        if (error) {
            std::string msg = "flux_compile_model: wrapper not found: " + wrapper_name;
            *error = strdup(msg.c_str());
        }
        return nullptr;
    }

    /* ---- 6. Keep the JIT alive forever (global) ---- */
    s_jits.push_back(std::move(jit));
    return fn_ptr;
}

} /* extern "C" */
