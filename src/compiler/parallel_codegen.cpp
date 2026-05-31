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

// Parallel for loop codegen - True multi-threaded JIT execution
#include "flux/compiler/ast.h"
#include "flux/runtime/parallel_runtime.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>

namespace Flux {

TypedValue ParallelForExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // 1. Generate start and end values
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    if (!StartTV.Val || !EndTV.Val)
        return TypedValue();

    auto ensureDouble = [&](TypedValue& TV) {
        if (TV.Type.Kind == TypeKind::Int)
            TV = TypedValue(context.Builder.CreateSIToFP(TV.Val, DoubleTy, "int2double"), TypeKind::Double);
    };
    ensureDouble(StartTV);
    ensureDouble(EndTV);

    llvm::Value* StartIdx = context.Builder.CreateFPToSI(StartTV.Val, Int64Ty, "start_idx");
    llvm::Value* EndIdx = context.Builder.CreateFPToSI(EndTV.Val, Int64Ty, "end_idx");

    // 2. Capture all visible variables (except loop var)
    // NamedValues contains either AllocaInst* (pointer to stored value for let vars)
    // or the value itself (for computed expressions). We need the actual value type.
    auto getCapturedValue = [&](llvm::Value* v) -> std::pair<llvm::Value*, llvm::Type*> {
        if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(v))
            return {context.Builder.CreateLoad(alloca->getAllocatedType(), v), alloca->getAllocatedType()};
        return {v, v->getType()};
    };
    std::vector<std::string> CapturedNames;
    std::vector<llvm::Value*> CapturedVals;
    std::vector<llvm::Type*> CapturedTypes;
    for (auto const& [name, val] : context.NamedValues) {
        if (name != VarName) {
            auto [capturedVal, capturedTy] = getCapturedValue(val);
            CapturedNames.push_back(name);
            CapturedVals.push_back(capturedVal);
            CapturedTypes.push_back(capturedTy);
        }
    }

    llvm::StructType* CaptureStructTy = !CapturedNames.empty()
        ? llvm::StructType::get(Ctx, CapturedTypes)
        : nullptr;
    llvm::Value* CaptureStruct = nullptr;
    if (CaptureStructTy) {
        CaptureStruct = context.Builder.CreateAlloca(CaptureStructTy, nullptr, "par_capture");
        for (size_t i = 0; i < CapturedNames.size(); i++) {
            llvm::Value* MemberPtr = context.Builder.CreateStructGEP(CaptureStructTy, CaptureStruct, i);
            context.Builder.CreateStore(CapturedVals[i], MemberPtr);
        }
    }

    // 3. Create the micro-task callback function
    // Signature: void par_body(int64_t index, void* user_data)
    llvm::FunctionType* BodyFuncTy = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {Int64Ty, VoidPtrTy}, false);
    llvm::Function* BodyFunc =
        llvm::Function::Create(BodyFuncTy, llvm::Function::InternalLinkage, "par_body", TheModule);

    // Save outer builder state
    llvm::BasicBlock* SavedBB = context.Builder.GetInsertBlock();
    auto SavedDebugLoc = context.Builder.getCurrentDebugLocation();

    // Build body function
    llvm::BasicBlock* BodyEntry = llvm::BasicBlock::Create(Ctx, "entry", BodyFunc);
    llvm::IRBuilder<> BodyBuilder(BodyEntry);

    // Loop index as double
    llvm::Value* RawIdx = BodyFunc->arg_begin();
    BodyBuilder.CreateSIToFP(RawIdx, DoubleTy, VarName.c_str());

    // Extract captured variables
    CodegenContext BodyCtx(Ctx, TheModule);
    BodyCtx.Builder.SetInsertPoint(BodyEntry);
    BodyCtx.Builder.SetCurrentDebugLocation(llvm::DebugLoc());

    llvm::Value* LoopVar = BodyBuilder.CreateSIToFP(RawIdx, DoubleTy, VarName.c_str());
    BodyCtx.NamedValues[VarName] = LoopVar;

    if (CaptureStructTy && CaptureStruct) {
        llvm::Value* UserDataPtr = BodyFunc->arg_begin() + 1;
        llvm::Value* CastData = BodyBuilder.CreateBitCast(UserDataPtr,
            llvm::PointerType::get(CaptureStructTy, 0), "capture_ptr");
        for (size_t i = 0; i < CapturedNames.size(); i++) {
            llvm::Value* MemberPtr = BodyBuilder.CreateStructGEP(CaptureStructTy, CastData, i);
            BodyCtx.NamedValues[CapturedNames[i]] = BodyBuilder.CreateLoad(CapturedTypes[i], MemberPtr);
        }
    }

    // Codegen body expression into the micro-task function
    TypedValue BodyResult = Body->codegen(BodyCtx);
    BodyBuilder.CreateRetVoid();

    // Finalize body function
    if (llvm::verifyFunction(*BodyFunc, &llvm::errs())) {
        std::cerr << "[FLUX ERROR] Parallel body function verification failed" << std::endl;
        BodyFunc->eraseFromParent();
        context.Builder.SetInsertPoint(SavedBB);
        context.Builder.SetCurrentDebugLocation(SavedDebugLoc);
        return TypedValue();
    }

    // 4. Call flux_parallel_for in the parent function
    llvm::Function* ParForFunc = TheModule->getFunction("flux_parallel_for");
    if (!ParForFunc) {
        llvm::FunctionType* ParForTy = llvm::FunctionType::get(
            llvm::Type::getVoidTy(Ctx), {Int64Ty, Int64Ty, Int64Ty, VoidPtrTy, VoidPtrTy}, false);
        ParForFunc = llvm::Function::Create(ParForTy, llvm::Function::ExternalLinkage,
                                             "flux_parallel_for", TheModule);
    }

    context.Builder.SetInsertPoint(SavedBB);
    context.Builder.SetCurrentDebugLocation(SavedDebugLoc);

    llvm::Value* ChunkSizeV = llvm::ConstantInt::get(Int64Ty, ChunkSize);
    llvm::Value* BodyFuncPtr = context.Builder.CreateBitCast(BodyFunc, VoidPtrTy, "par_body_ptr");
    llvm::Value* CapturePtr = CaptureStruct
        ? context.Builder.CreateBitCast(CaptureStruct, VoidPtrTy, "par_capture_ptr")
        : llvm::Constant::getNullValue(VoidPtrTy);

    context.Builder.CreateCall(ParForFunc, {StartIdx, EndIdx, ChunkSizeV, BodyFuncPtr, CapturePtr});

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux