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

// Parallel for loop codegen - Actual parallel execution support
#include "flux/compiler/ast.h"
#include "flux/runtime/parallel_runtime.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>

namespace Flux {

TypedValue ParallelForExprAST::codegen(CodegenContext& context) {
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // 1. Generate start and end values in the current function
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    if (!StartTV.Val || !EndTV.Val) return TypedValue();

    // Promote int literals to double if needed
    auto ensureDouble = [&](TypedValue& TV) {
        if (TV.Type.Kind == TypeKind::Int)
            TV = TypedValue(context.Builder.CreateSIToFP(TV.Val, DoubleTy, "int2double"), TypeKind::Double);
    };
    ensureDouble(StartTV);
    ensureDouble(EndTV);

    // Convert start/end to int64
    llvm::Value* StartIdx = context.Builder.CreateFPToSI(StartTV.Val, Int64Ty, "start_idx");
    llvm::Value* EndIdx = context.Builder.CreateFPToSI(EndTV.Val, Int64Ty, "end_idx");

    // 2. Identify captured variables (all current NamedValues)
    std::vector<std::string> CapturedNames;
    std::vector<llvm::Type*> CapturedTypes;
    for (auto const& [name, val] : context.NamedValues) {
        if (name != VarName) {
            CapturedNames.push_back(name);
            CapturedTypes.push_back(val->getType());
        }
    }

    // Create a struct to hold captured variables
    llvm::StructType* CaptureStructTy = llvm::StructType::get(Ctx, CapturedTypes);
    llvm::Value* CaptureStruct = context.Builder.CreateAlloca(CaptureStructTy, nullptr, "par_capture");

    for (size_t i = 0; i < CapturedNames.size(); i++) {
        llvm::Value* MemberPtr = context.Builder.CreateStructGEP(CaptureStructTy, CaptureStruct, i);
        context.Builder.CreateStore(context.NamedValues[CapturedNames[i]], MemberPtr);
    }

    // 3. Create the loop body function
    // Signature: void body(int64_t index, void* user_data)
    llvm::FunctionType* BodyFuncTy = llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {Int64Ty, VoidPtrTy}, false);
    llvm::Function* BodyFunc = llvm::Function::Create(BodyFuncTy, llvm::Function::InternalLinkage, "par_body", TheModule);

    // Create entry block in body function
    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(Ctx, "entry", BodyFunc);
    llvm::IRBuilder<> BodyBuilder(EntryBB);

    // Setup body context
    CodegenContext BodyContext(context.TheContext, context.TheModule);
    BodyContext.Builder.SetInsertPoint(EntryBB);

    // Extract loop index
    llvm::Value* RawIdx = BodyFunc->arg_begin();
    llvm::Value* LoopVar = BodyBuilder.CreateSIToFP(RawIdx, DoubleTy, VarName.c_str());
    BodyContext.NamedValues[VarName] = LoopVar;

    // Extract captured variables
    llvm::Value* UserDataPtr = (BodyFunc->arg_begin() + 1);
    llvm::Value* CastUserData = BodyBuilder.CreateBitCast(UserDataPtr, llvm::PointerType::get(CaptureStructTy, 0));

    for (size_t i = 0; i < CapturedNames.size(); i++) {
        llvm::Value* MemberPtr = BodyBuilder.CreateStructGEP(CaptureStructTy, CastUserData, i);
        BodyContext.NamedValues[CapturedNames[i]] = BodyBuilder.CreateLoad(CapturedTypes[i], MemberPtr);
    }

    // Codegen the body expression into the new function
    TypedValue BodyResult = Body->codegen(BodyContext);
    BodyBuilder.CreateRetVoid();

    // 4. Call flux_parallel_for in the current function
    llvm::Function* ParForFunc = TheModule->getFunction("flux_parallel_for");
    if (!ParForFunc) {
        ParForFunc = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {Int64Ty, Int64Ty, Int64Ty, VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_parallel_for", TheModule);
    }

    llvm::Value* ChunkSize = llvm::ConstantInt::get(Int64Ty, 0); // Auto-calculate chunk size
    llvm::Value* BodyFuncPtr = context.Builder.CreateBitCast(BodyFunc, VoidPtrTy);
    llvm::Value* CapturePtr = context.Builder.CreateBitCast(CaptureStruct, VoidPtrTy);

    context.Builder.CreateCall(ParForFunc, {StartIdx, EndIdx, ChunkSize, BodyFuncPtr});

    std::cout << "[CodeGen] Generated TRUE parallel for loop: " << VarName << std::endl;

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

} // namespace Flux
