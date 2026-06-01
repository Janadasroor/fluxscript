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

// Extended keyword codegen implementations
#include "flux/compiler/ast.h"
#include <iostream>
#include <set>

namespace Flux {

// ============ Try/Catch Codegen ============

void TryCatchExprAST::addCatch(const std::string& var, std::unique_ptr<ExprAST> handler)
{
    CatchClauses.emplace_back(var, std::move(handler));
}

void TryCatchExprAST::setFinally(std::unique_ptr<ExprAST> body)
{
    FinallyBody = std::move(body);
}

TypedValue TryCatchExprAST::codegen(CodegenContext& context)
{
    static int TryCount = 0;
    int ID = ++TryCount;
    std::string IDStr = std::to_string(ID);

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Create unique blocks with prefixes to avoid numbering collisions
    llvm::BasicBlock* TryBB = llvm::BasicBlock::Create(Ctx, "flux_try_" + IDStr, TheFunction);
    llvm::BasicBlock* CatchBB = llvm::BasicBlock::Create(Ctx, "flux_catch_" + IDStr, TheFunction);
    llvm::BasicBlock* FinallyBB = llvm::BasicBlock::Create(Ctx, "flux_finally_" + IDStr, TheFunction);
    llvm::BasicBlock* EndBB = llvm::BasicBlock::Create(Ctx, "flux_try_end_" + IDStr, TheFunction);

    // Result and exception storage
    llvm::Value* ResultPtr = context.Builder.CreateAlloca(DoubleTy, nullptr, "try_res" + IDStr);
    llvm::Value* ExceptionPtr = context.Builder.CreateAlloca(DoubleTy, nullptr, "ex_ptr" + IDStr);

    // Allocate jmp_buf (1024 bytes)
    llvm::Value* JmpBuf = context.Builder.CreateAlloca(llvm::ArrayType::get(llvm::Type::getInt8Ty(Ctx), 1024), nullptr,
                                                       "jmp_buf" + IDStr);
    llvm::Value* JmpBufPtr = context.Builder.CreateBitCast(JmpBuf, VoidPtrTy);

    // Register runtime helpers
    llvm::Function* PushF = context.TheModule->getFunction("flux_push_jmp_buf");
    if (!PushF)
        PushF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {VoidPtrTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_push_jmp_buf", context.TheModule);

    llvm::Function* PopF = context.TheModule->getFunction("flux_pop_jmp_buf");
    if (!PopF)
        PopF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {}, false),
                                      llvm::Function::ExternalLinkage, "flux_pop_jmp_buf", context.TheModule);

    llvm::Function* SetJmpF = context.TheModule->getFunction("setjmp");
    if (!SetJmpF)
        SetJmpF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                         llvm::Function::ExternalLinkage, "setjmp", context.TheModule);

    // Push jump buffer to stack
    context.Builder.CreateCall(PushF, {JmpBufPtr});

    // Call setjmp
    llvm::Value* JmpVal = context.Builder.CreateCall(SetJmpF, {JmpBufPtr}, "jmp_val" + IDStr);
    llvm::Value* IsCatch = context.Builder.CreateICmpNE(JmpVal, llvm::ConstantInt::get(Int32Ty, 0), "is_catch" + IDStr);
    context.Builder.CreateCondBr(IsCatch, CatchBB, TryBB);

    // Try block
    context.Builder.SetInsertPoint(TryBB);
    TypedValue TryResult = TryBody->codegen(context);
    if (!context.Builder.GetInsertBlock()->getTerminator()) {
        if (TryResult.Val)
            context.Builder.CreateStore(TryResult.Val, ResultPtr);
        context.Builder.CreateCall(PopF);
        context.Builder.CreateBr(FinallyBB);
    }

    // Catch block
    context.Builder.SetInsertPoint(CatchBB);
    context.Builder.CreateCall(PopF);

    // Bind the catch variable to the actual thrown value (preserved in
    // thread-local storage by flux_throw_error), not the setjmp return code.
    llvm::Function* LastThrownF = context.TheModule->getFunction("flux_last_thrown_value");
    if (!LastThrownF)
        LastThrownF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {}, false),
                                             llvm::Function::ExternalLinkage,
                                             "flux_last_thrown_value", context.TheModule);
    llvm::Value* ThrownVal = context.Builder.CreateCall(LastThrownF, {}, "thrown_val" + IDStr);

    if (!CatchClauses.empty()) {
        auto& [VarName, Handler] = CatchClauses[0];
        llvm::Value* OldVal = context.NamedValues[VarName];

        context.NamedValues[VarName] = ThrownVal;

        TypedValue CatchResult = Handler->codegen(context);
        if (!context.Builder.GetInsertBlock()->getTerminator()) {
            if (CatchResult.Val)
                context.Builder.CreateStore(CatchResult.Val, ResultPtr);
            context.Builder.CreateBr(FinallyBB);
        }
        context.NamedValues[VarName] = OldVal;
    } else {
        if (!context.Builder.GetInsertBlock()->getTerminator()) {
            context.Builder.CreateBr(FinallyBB);
        }
    }

    // Finally block
    context.Builder.SetInsertPoint(FinallyBB);
    if (FinallyBody) {
        FinallyBody->codegen(context);
    }
    if (!context.Builder.GetInsertBlock()->getTerminator()) {
        context.Builder.CreateBr(EndBB);
    }
    // End block
    context.Builder.SetInsertPoint(EndBB);
    if (!context.Builder.GetInsertBlock()->getTerminator()) {
        // If nothing was stored, ensure we return something
        llvm::Value* FinalRes = context.Builder.CreateLoad(DoubleTy, ResultPtr, "res" + IDStr);
        return TypedValue(FinalRes, TypeKind::Double);
    }
    return TypedValue(nullptr, TypeKind::Void);
}

// ============ `?` Propagation Codegen ============
//
// For `inner?` where `inner` evaluates to a Result<T, E> (or Option<T>)
// tagged-union, we emit:
//
//   %slot = alloca ResultTy
//   store %inner, %slot
//   %tag  = load i32, ptr gep(%slot, 0, 0)
//   switch %tag:
//     case Ok(0):  %payload = load T, ptr gep(%slot, 0, 1); continue
//     case Err(1): %ret = load ResultTy, %slot;  ret %ret
//
// This requires the enclosing function to return the same ResultTy so the
// `ret` instruction is type-correct.
TypedValue TryPropagateExprAST::codegen(CodegenContext& context)
{
    TypedValue InnerTV = Inner->codegen(context);
    if (!InnerTV.Val) {
        std::cerr << "[FLUX ERROR] `?` inner expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);

    if (context.Builder.GetInsertBlock()->getTerminator())
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);

    // Determine the struct shape of the inner value.
    llvm::Type* InnerTy = InnerTV.Val->getType();
    if (!InnerTy->isStructTy()) {
        std::cerr << "[FLUX ERROR] `?` requires a tagged-union (Result/Option); got non-struct type" << std::endl;
        return InnerTV; // fall through, best-effort
    }

    llvm::StructType* STy = llvm::cast<llvm::StructType>(InnerTy);
    if (STy->getNumElements() < 2) {
        std::cerr << "[FLUX ERROR] `?` requires a tagged-union with tag + payload" << std::endl;
        return InnerTV;
    }

    // Stash the inner value so both branches can read it.
    llvm::Value* Slot = context.Builder.CreateAlloca(STy, nullptr, "qmark_slot");
    context.Builder.CreateStore(InnerTV.Val, Slot);

    // Load tag (index 0). Enum tag is always i32.
    llvm::Value* TagAddr = context.Builder.CreateStructGEP(STy, Slot, 0, "qmark_tag_addr");
    llvm::Value* Tag = context.Builder.CreateLoad(Int32Ty, TagAddr, "qmark_tag");
    llvm::Value* IsErr = context.Builder.CreateICmpNE(Tag, llvm::ConstantInt::get(Int32Ty, 0), "qmark_is_err");

    // For the early-return path we need a fresh block added to the function,
    // and we need the function to return the same struct type so `ret` is legal.
    llvm::BasicBlock* ErrBB = llvm::BasicBlock::Create(Ctx, "qmark_err", TheFunction);
    llvm::BasicBlock* ContBB = llvm::BasicBlock::Create(Ctx, "qmark_cont", TheFunction);
    context.Builder.CreateCondBr(IsErr, ErrBB, ContBB);

    // Err path: reload the whole value and `ret` it.
    context.Builder.SetInsertPoint(ErrBB);
    llvm::Value* ErrVal = context.Builder.CreateLoad(STy, Slot, "qmark_err_val");
    if (TheFunction->getReturnType() == STy) {
        context.Builder.CreateRet(ErrVal);
    } else {
        // Function doesn't return the same type — degrade gracefully to a poison
        // constant so the rest of the function is still well-formed.
        llvm::Value* Poison = llvm::ConstantFP::get(DoubleTy, 0.0);
        context.Builder.CreateRet(Poison);
    }
    context.Builder.SetInsertPoint(ContBB);

    // Ok path: load the payload (index 1) and return it as the expression value.
    llvm::Type* PayloadTy = STy->getElementType(1);
    llvm::Value* PayloadAddr = context.Builder.CreateStructGEP(STy, Slot, 1, "qmark_payload_addr");
    llvm::Value* Payload = context.Builder.CreateLoad(PayloadTy, PayloadAddr, "qmark_payload");

    // Reflect the payload type back into TypedValue so downstream code can
    // use it. For the common case (Double payload) this maps to TypeKind::Double.
    TypeKind OutKind = TypeKind::Double;
    if (PayloadTy->isIntegerTy(1))
        OutKind = TypeKind::Bool;
    else if (PayloadTy->isIntegerTy(32))
        OutKind = TypeKind::Int;
    return TypedValue(Payload, OutKind);
}

// ============ Throw Codegen ============

TypedValue ThrowExprAST::codegen(CodegenContext& context)
{
    TypedValue ExTV = Exception->codegen(context);
    if (!ExTV.Val) {
        std::cerr << "[FLUX ERROR] throw expression sub-expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::LLVMContext& Ctx = context.TheContext;

    // If block already terminated, stop.
    if (context.Builder.GetInsertBlock()->getTerminator()) {
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }

    llvm::Module* TheModule = context.TheModule;

    // Always use runtime throw which handles the non-local jump via longjmp
    llvm::Function* ThrowFunc = TheModule->getFunction("flux_throw_error");
    if (!ThrowFunc) {
        llvm::Type* Params[] = {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0)};
        ThrowFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                           llvm::Function::ExternalLinkage, "flux_throw_error", TheModule);
    }

    llvm::Value* MsgPtr = context.Builder.CreateGlobalString("Flux runtime exception", "ex_msg");
    context.Builder.CreateCall(ThrowFunc, {ExTV.Val, MsgPtr});
    context.Builder.CreateUnreachable();

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Assert Codegen ============

TypedValue AssertExprAST::codegen(CodegenContext& context)
{
    // Generate condition check
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] assert condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // Create assertion check
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ContinueBB = llvm::BasicBlock::Create(context.TheContext, "assert_continue", TheFunction);
    llvm::BasicBlock* FailBB = llvm::BasicBlock::Create(context.TheContext, "assert_fail", TheFunction);

    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        IsTrue = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "assert_check");
    }

    context.Builder.CreateCondBr(IsTrue, ContinueBB, FailBB);

    // Fail block - would print error message
    context.Builder.SetInsertPoint(FailBB);
    llvm::Value* MsgPtr =
        context.Builder.CreateGlobalString(Message.empty() ? "Assertion failed!" : Message, "assert_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    context.Builder.CreateBr(ContinueBB);

    // Continue block
    context.Builder.SetInsertPoint(ContinueBB);
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

// ============ Yield Codegen ============

TypedValue YieldExprAST::codegen(CodegenContext& context)
{
    if (!context.GeneratorStateAlloca) {
        std::cerr << "[FLUX ERROR] yield used outside of a generator function." << std::endl;
        return TypedValue();
    }

    // 1. Generate value to yield
    TypedValue ValueTV = Value->codegen(context);
    if (!ValueTV.Val) {
        std::cerr << "[FLUX ERROR] yield value expression failed to codegen" << std::endl;
        return TypedValue();
    }

    // 2. Create resume block and record it
    int nextState = (int)context.YieldTargets.size();
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ResumeBB = llvm::BasicBlock::Create(context.TheContext, "yield_resume", TheFunction);
    context.YieldTargets.push_back(ResumeBB);

    // 3. Save next state index in the state struct
    // Generator state struct: { i32 state_index }
    llvm::Type* Int32T = llvm::Type::getInt32Ty(context.TheContext);
    llvm::SmallVector<llvm::Type*, 1> ElementTys = {Int32T};
    llvm::Type* StateTy = llvm::StructType::get(context.TheContext, ElementTys);
    llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.GeneratorStateAlloca, 0);
    context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), nextState),
                                IndexPtr);

    // 4. Return from the function with the yielded value
    context.Builder.CreateRet(ValueTV.Val);

    // 5. Continue codegen from the resume block
    context.Builder.SetInsertPoint(ResumeBB);

    return ValueTV;
}

// ============ Await Codegen ============

TypedValue AwaitExprAST::codegen(CodegenContext& context)
{
    if (!context.AsyncStateAlloca) {
        std::cerr << "[FLUX ERROR] await used outside of an async function." << std::endl;
        return TypedValue();
    }

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // 1. Generate the awaited expression (e.g., a future)
    TypedValue ValueTV = Value->codegen(context);
    if (!ValueTV.Val) {
        std::cerr << "[FLUX ERROR] await expression failed to codegen" << std::endl;
        return TypedValue();
    }

    // 2. Store the value in the entry-block alloca (dominates all blocks)
    context.Builder.CreateStore(ValueTV.Val, context.AsyncResultAlloca);

    // 3. Create a continuation block (for when the value is immediately ready)
    llvm::BasicBlock* ContBB = llvm::BasicBlock::Create(context.TheContext, "await_cont", TheFunction);

    // 4. Compare the result against 0.0 to check if it's ready
    auto ZeroVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0));
    llvm::Value* IsReady = context.Builder.CreateFCmpONE(ValueTV.Val, ZeroVal, "await_ready");

    // 5. If ready, branch to continuation; otherwise suspend
    llvm::BasicBlock* SuspendBB = llvm::BasicBlock::Create(context.TheContext, "await_suspend", TheFunction);
    context.Builder.CreateCondBr(IsReady, ContBB, SuspendBB);

    // 5. Suspend block: save state and return 0.0 (Pending)
    context.Builder.SetInsertPoint(SuspendBB);

    // Store the value in the entry-block alloca for resume
    context.Builder.CreateStore(ValueTV.Val, context.AsyncResultAlloca);

    // Save next state index in the state struct
    int nextState = (int)context.AwaitResumeTargets.size();
    llvm::Type* Int32T = llvm::Type::getInt32Ty(context.TheContext);
    llvm::SmallVector<llvm::Type*, 1> ElementTys = {Int32T};
    llvm::Type* StateTy = llvm::StructType::get(context.TheContext, ElementTys);
    llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.AsyncStateAlloca, 0);
    context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), nextState),
                                IndexPtr);

    // Return 0.0 (Pending) from the function
    auto PendingVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0));
    context.Builder.CreateRet(PendingVal);

    // 6. Create resume block and record it
    llvm::BasicBlock* ResumeBB = llvm::BasicBlock::Create(context.TheContext, "await_resume", TheFunction);
    context.AwaitResumeTargets.push_back(ResumeBB);

    // Resume block: re-poll the awaited expression
    context.Builder.SetInsertPoint(ResumeBB);
    TypedValue ResumeTV = Value->codegen(context);
    if (!ResumeTV.Val) {
        std::cerr << "[FLUX ERROR] await resume expression failed to codegen" << std::endl;
        return TypedValue();
    }
    context.Builder.CreateStore(ResumeTV.Val, context.AsyncResultAlloca);
    llvm::Value* ResumeReady = context.Builder.CreateFCmpONE(ResumeTV.Val, ZeroVal, "await_resume_ready");

    llvm::BasicBlock* ReSuspendBB = llvm::BasicBlock::Create(context.TheContext, "await_resuspend", TheFunction);
    context.Builder.CreateCondBr(ResumeReady, ContBB, ReSuspendBB);

    // Re-suspend: save same state and return 0.0
    context.Builder.SetInsertPoint(ReSuspendBB);
    llvm::Value* ReIndexPtr = context.Builder.CreateStructGEP(StateTy, context.AsyncStateAlloca, 0);
    context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), nextState),
                                ReIndexPtr);
    context.Builder.CreateRet(PendingVal);

    // 7. Set insert point to continuation block
    context.Builder.SetInsertPoint(ContBB);

    // 8. Return the value (from the alloca, which dominates both paths)
    llvm::Value* LoadedVal = context.Builder.CreateLoad(llvm::Type::getDoubleTy(context.TheContext),
                                                         context.AsyncResultAlloca, "await_val");
    return TypedValue(LoadedVal, ValueTV.Type);
}

// ============ Corner Case Codegen ============

void CornerExprAST::addCase(const std::string& name, std::unique_ptr<ExprAST> expr)
{
    Cases.emplace_back(name, std::move(expr));
}

TypedValue CornerExprAST::codegen(CodegenContext& context)
{
    // Generate corner case analysis as a switch-like structure
    // Evaluates each case expression and collects results into an array

    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    size_t NumCases = Cases.size();

    if (NumCases == 0) {
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }

    // Create result array on stack: { double, double, ... } for each case
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* ResultArrayTy = llvm::ArrayType::get(DoubleTy, NumCases);
    llvm::Value* ResultArray = context.Builder.CreateAlloca(ResultArrayTy, nullptr, "corner_results");

    // Evaluate each case and store in result array
    for (size_t i = 0; i < NumCases; i++) {
        auto& [CaseName, CaseExpr] = Cases[i];

        // Generate case expression
        TypedValue CaseTV = CaseExpr->codegen(context);
        if (!CaseTV.Val) {
            std::cerr << "[FLUX ERROR] corner case expression failed to codegen" << std::endl;
            return TypedValue();
        }

        // Store result in array at index i
        llvm::Value* Indices[] = {llvm::ConstantInt::get(Ctx, llvm::APInt(32, 0)),
                                  llvm::ConstantInt::get(Ctx, llvm::APInt(32, i))};
        llvm::Value* CasePtr =
            context.Builder.CreateGEP(ResultArrayTy, ResultArray, Indices, "case_" + std::to_string(i) + "_ptr");
        context.Builder.CreateStore(CaseTV.Val, CasePtr);

        std::cout << "[CodeGen] Corner case '" << CaseName << "' evaluated" << std::endl;
    }

    // Return pointer to results array
    llvm::Type* PtrTy = llvm::PointerType::get(Ctx, 0);
    return TypedValue(context.Builder.CreateBitCast(ResultArray, PtrTy), TypeKind::Double);
}

// ============ Pattern Match Codegen ============

void MatchExprAST::addArm(std::unique_ptr<ExprAST> pattern, std::unique_ptr<ExprAST> result,
                          const std::vector<std::string>& bindings)
{
    Arms.emplace_back(std::move(pattern), std::move(result));
    Bindings.push_back(bindings);
}

void MatchExprAST::setDefault(std::unique_ptr<ExprAST> arm)
{
    DefaultArm = std::move(arm);
}

static const std::vector<std::string> emptyBindings;

TypedValue MatchExprAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    TypedValue MatchValTV = Value->codegen(context);
    if (!MatchValTV.Val) {
        std::cerr << "[FLUX ERROR] match value expression failed to codegen" << std::endl;
        return TypedValue();
    }

    bool isEnumMatch = MatchValTV.Type.isEnum();
    bool isPayloadEnum = isEnumMatch && MatchValTV.Val->getType()->isStructTy();

    // Exhaustiveness check: if matching on an enum without a default arm,
    // verify that every variant is covered.
    if (isEnumMatch && !DefaultArm) {
        int enumTypeId = MatchValTV.Type.EnumTypeId;
        if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
            auto& enumInfo = context.EnumTypes[enumTypeId];
            std::set<std::string> matchedVariants;
            for (auto const& [Pattern, _] : Arms) {
                if (auto* callPat = dynamic_cast<CallExprAST*>(Pattern.get())) {
                    if (callPat->hasCalleeExpr()) {
                        if (auto* memberPat = dynamic_cast<MemberExprAST*>(callPat->getCalleeExpr()))
                            matchedVariants.insert(memberPat->getMemberName());
                    }
                } else if (auto* memberPat = dynamic_cast<MemberExprAST*>(Pattern.get())) {
                    matchedVariants.insert(memberPat->getMemberName());
                }
            }
            for (auto const& variant : enumInfo.Variants) {
                if (matchedVariants.find(variant) == matchedVariants.end()) {
                    std::cerr << "[FLUX ERROR] match on enum '" << enumInfo.Name
                              << "' does not cover variant '" << variant << "'" << std::endl;
                    return TypedValue();
                }
            }
        }
    }

    // Determine return type from result expressions (default to Double)
    llvm::Type* ResultTy = DoubleTy;

    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "match_merge");
    llvm::AllocaInst* ResultAlloc = context.Builder.CreateAlloca(ResultTy, nullptr, "match_result");

    // Build chained checks: each arm gets a check block and a match body block
    std::vector<llvm::BasicBlock*> checkBBs;
    std::vector<llvm::BasicBlock*> matchBBs;
    for (size_t i = 0; i < Arms.size(); ++i) {
        checkBBs.push_back(llvm::BasicBlock::Create(Ctx, "match_check_" + std::to_string(i), TheFunction));
        matchBBs.push_back(llvm::BasicBlock::Create(Ctx, "match_arm_" + std::to_string(i), TheFunction));
    }

    if (checkBBs.empty()) {
        // No arms — direct branch to merge/default
        if (DefaultArm) {
            TypedValue DefaultTV = DefaultArm->codegen(context);
            if (!DefaultTV.Val) {
                std::cerr << "[FLUX ERROR] match default arm (no-arms path) failed to codegen" << std::endl;
                return TypedValue();
            }
            llvm::Value* DefaultV = DefaultTV.Val;
            if (DefaultV->getType()->isIntegerTy())
                DefaultV = context.Builder.CreateSIToFP(DefaultV, DoubleTy, "default_cast");
            context.Builder.CreateStore(DefaultV, ResultAlloc);
        }
        context.Builder.CreateBr(MergeBB);
        TheFunction->insert(TheFunction->end(), MergeBB);
        context.Builder.SetInsertPoint(MergeBB);
        llvm::Value* ResultVal = context.Builder.CreateLoad(ResultTy, ResultAlloc, "match_result_val");
        return TypedValue(ResultVal, TypeKind::Double);
    }

    // Branch from current block to the first check
    context.Builder.CreateBr(checkBBs[0]);

    // Generate each arm
    llvm::BasicBlock* DefaultFallbackBB = nullptr;
    for (size_t i = 0; i < Arms.size(); ++i) {
        auto& [Pattern, Result] = Arms[i];
        const auto& armBindings = i < Bindings.size() ? Bindings[i] : emptyBindings;

        // Determine which block to branch to when this check fails
        llvm::BasicBlock* NextBB;
        if (i + 1 < Arms.size()) {
            NextBB = checkBBs[i + 1];
        } else if (DefaultArm) {
            NextBB = llvm::BasicBlock::Create(Ctx, "match_default", TheFunction);
            DefaultFallbackBB = NextBB;
        } else {
            NextBB = MergeBB;
        }

        // Emit check in the check block
        context.Builder.SetInsertPoint(checkBBs[i]);

        llvm::Value* IsMatch = nullptr;

        if (isPayloadEnum) {
            // Extract discriminant (i32 tag) from tagged union struct value
            llvm::Value* TagVal = context.Builder.CreateExtractValue(
                MatchValTV.Val, {0}, "tag");

            llvm::Value* PatDiscVal = nullptr;

            // If pattern has a binding variable, compute discriminant from AST without
            // codegen'ing the pattern (which would fail on the unbounded binding variable).
            if (!armBindings.empty()) {
                if (auto* callPat = dynamic_cast<CallExprAST*>(Pattern.get())) {
                    if (auto* memberPat = dynamic_cast<MemberExprAST*>(callPat->getCalleeExpr())) {
                        if (auto* objPat = dynamic_cast<VariableExprAST*>(memberPat->getObject())) {
                            std::string enumName = objPat->getName();
                            std::string varName = memberPat->getMemberName();
                            auto enumIt = context.EnumTypeIndex.find(enumName);
                            if (enumIt != context.EnumTypeIndex.end()) {
                                int enumId = enumIt->second;
                                if (enumId >= 0 && enumId < static_cast<int>(context.EnumTypes.size())) {
                                    auto& enumInfo = context.EnumTypes[enumId];
                                    for (size_t vi = 0; vi < enumInfo.Variants.size(); ++vi) {
                                        if (enumInfo.Variants[vi] == varName) {
                                            PatDiscVal = llvm::ConstantInt::get(
                                                llvm::Type::getInt32Ty(Ctx), vi);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Fallback: codegen pattern normally (works for bindless patterns like Option.Some(5.0))
            if (!PatDiscVal) {
                TypedValue PatternTV = Pattern->codegen(context);
                if (!PatternTV.Val) {
                    std::cerr << "[FLUX ERROR] match pattern (payload enum) failed to codegen" << std::endl;
                    return TypedValue();
                }
                PatDiscVal = PatternTV.Val;
                if (PatDiscVal->getType()->isStructTy()) {
                    PatDiscVal = context.Builder.CreateExtractValue(PatDiscVal, {0}, "pat_tag");
                }
            }

            // Cast to i32 if needed
            if (PatDiscVal->getType() != llvm::Type::getInt32Ty(Ctx))
                PatDiscVal = context.Builder.CreateIntCast(PatDiscVal, llvm::Type::getInt32Ty(Ctx), false, "pat_tag_casted");
            IsMatch = context.Builder.CreateICmpEQ(TagVal, PatDiscVal, "tag_cmp_" + std::to_string(i));
        } else if (isEnumMatch) {
            // Simple enum (i32 discriminant) — or tagged union if payload enum exists
            TypedValue PatternTV = Pattern->codegen(context);
            if (!PatternTV.Val) {
                std::cerr << "[FLUX ERROR] match pattern (simple enum) failed to codegen" << std::endl;
                return TypedValue();
            }
            llvm::Value* patVal = PatternTV.Val;
            // If pattern produced a struct, extract its tag (i32)
            if (patVal->getType()->isStructTy()) {
                patVal = context.Builder.CreateExtractValue(patVal, {0}, "pat_tag");
            }
            llvm::Value* matchVal = MatchValTV.Val;
            // If match value is a struct, extract its tag
            if (matchVal->getType()->isStructTy()) {
                matchVal = context.Builder.CreateExtractValue(matchVal, {0}, "val_tag");
            }
            if (patVal->getType() != matchVal->getType())
                patVal = context.Builder.CreateIntCast(patVal, matchVal->getType(), false, "pat_cast");
            IsMatch = context.Builder.CreateICmpEQ(matchVal, patVal, "cmp_" + std::to_string(i));
        } else {
            // Non-enum match: use existing comparison logic
            TypedValue PatternTV = Pattern->codegen(context);
            if (!PatternTV.Val) {
                std::cerr << "[FLUX ERROR] match pattern (non-enum) failed to codegen" << std::endl;
                return TypedValue();
            }

            llvm::Value*& PatternVal = PatternTV.Val;
            llvm::Value* MatchVal = MatchValTV.Val;

            if (MatchVal->getType()->isIntegerTy()) {
                if (PatternVal->getType() != MatchVal->getType())
                    PatternVal = context.Builder.CreateIntCast(PatternVal, MatchVal->getType(), false, "pat_cast");
                IsMatch = context.Builder.CreateICmpEQ(MatchVal, PatternVal, "cmp_" + std::to_string(i));
            } else if (MatchVal->getType()->isFloatingPointTy()) {
                if (PatternVal->getType()->isIntegerTy())
                    PatternVal = context.Builder.CreateSIToFP(PatternVal, MatchVal->getType(), "pat_to_fp");
                IsMatch = context.Builder.CreateFCmpOEQ(MatchVal, PatternVal, "cmp_" + std::to_string(i));
            } else {
                // Fallback: compare as integers via ptrtoint
                IsMatch = context.Builder.CreateICmpEQ(
                    context.Builder.CreatePtrToInt(MatchVal, llvm::Type::getInt64Ty(Ctx)),
                    context.Builder.CreatePtrToInt(PatternVal, llvm::Type::getInt64Ty(Ctx)),
                    "cmp_" + std::to_string(i));
            }
        }

        context.Builder.CreateCondBr(IsMatch, matchBBs[i], NextBB);

        // Emit match body
        context.Builder.SetInsertPoint(matchBBs[i]);

        // If this pattern has binding variables, extract payload fields and store them
        if (!armBindings.empty() && isPayloadEnum) {
            // Allocate the matched union value on the stack to get a pointer to it
            llvm::Value* matchValAlloca = context.Builder.CreateAlloca(MatchValTV.Val->getType(), nullptr, "match_val_alloc");
            context.Builder.CreateStore(MatchValTV.Val, matchValAlloca);

            // Get GEP pointer to the generic payload
            llvm::Value* payloadPtr = context.Builder.CreateStructGEP(MatchValTV.Val->getType(), matchValAlloca, 1, "payload_ptr");

            // Identify the actual matched variant payload type
            int matchedVarIdx = -1;
            std::string variantName;
            if (auto* callPat = dynamic_cast<CallExprAST*>(Pattern.get())) {
                if (auto* memberPat = dynamic_cast<MemberExprAST*>(callPat->getCalleeExpr())) {
                    variantName = memberPat->getMemberName();
                }
            } else if (auto* memberPat = dynamic_cast<MemberExprAST*>(Pattern.get())) {
                variantName = memberPat->getMemberName();
            }

            int enumTypeId = MatchValTV.Type.EnumTypeId;
            if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
                auto& enumInfo = context.EnumTypes[enumTypeId];
                for (size_t vi = 0; vi < enumInfo.Variants.size(); ++vi) {
                    if (enumInfo.Variants[vi] == variantName) {
                        matchedVarIdx = static_cast<int>(vi);
                        break;
                    }
                }
            }

            if (matchedVarIdx >= 0) {
                auto& enumInfo = context.EnumTypes[enumTypeId];
                FluxType concretePayloadType = enumInfo.VariantPayloads[matchedVarIdx];
                // Inline resolveUserStructType
                if (concretePayloadType.Kind == TypeKind::UserStruct && concretePayloadType.StructTypeId < 0 && !concretePayloadType.GenericName.empty()) {
                    auto it = context.StructTypeIndex.find(concretePayloadType.GenericName);
                    if (it != context.StructTypeIndex.end()) {
                        concretePayloadType.StructTypeId = it->second;
                        if (it->second >= 0 && it->second < static_cast<int>(context.StructTypes.size())) {
                            concretePayloadType.StructLLVMType = context.StructTypes[it->second].LLVMType;
                        }
                    }
                }
                // Inline resolveUserEnumType
                if (concretePayloadType.Kind == TypeKind::UserEnum && concretePayloadType.EnumTypeId < 0 && !concretePayloadType.GenericName.empty()) {
                    auto it = context.EnumTypeIndex.find(concretePayloadType.GenericName);
                    if (it != context.EnumTypeIndex.end()) {
                        concretePayloadType.EnumTypeId = it->second;
                        if (it->second >= 0 && it->second < static_cast<int>(context.EnumTypes.size())) {
                            concretePayloadType.EnumLLVMType = context.EnumTypes[it->second].LLVMType;
                        }
                    }
                }

                llvm::Type* concretePayloadLLVMTy = concretePayloadType.getLLVMType(context.TheContext);

                bool isBoxed = matchedVarIdx < static_cast<int>(enumInfo.VariantIsBoxed.size()) ? enumInfo.VariantIsBoxed[matchedVarIdx] : false;
                llvm::Value* concretePayloadPtr = nullptr;
                if (isBoxed) {
                    llvm::Value* unionPayloadPtr = context.Builder.CreatePointerCast(
                        payloadPtr, llvm::PointerType::get(llvm::PointerType::get(context.TheContext, 0), 0), "union_payload_ptr");
                    llvm::Value* heapPtrVal = context.Builder.CreateLoad(
                        llvm::PointerType::get(context.TheContext, 0), unionPayloadPtr, "heap_ptr_val");
                    concretePayloadPtr = context.Builder.CreatePointerCast(
                        heapPtrVal, llvm::PointerType::get(concretePayloadLLVMTy, 0), "concrete_payload_ptr");
                } else {
                    concretePayloadPtr = context.Builder.CreatePointerCast(
                        payloadPtr, llvm::PointerType::get(concretePayloadLLVMTy, 0), "concrete_payload_ptr");
                }

                if (armBindings.size() == 1 && !concretePayloadLLVMTy->isStructTy()) {
                    // Single scalar binding: load and store entire payload
                    llvm::Value* val = context.Builder.CreateLoad(concretePayloadLLVMTy, concretePayloadPtr, "scalar_payload");
                    llvm::AllocaInst* BindingAlloca = context.Builder.CreateAlloca(concretePayloadLLVMTy, nullptr, armBindings[0]);
                    context.Builder.CreateStore(val, BindingAlloca);
                    context.NamedValues[armBindings[0]] = BindingAlloca;
                } else {
                    // Multiple bindings: extract each field from the concrete payload struct
                    for (size_t bi = 0; bi < armBindings.size(); ++bi) {
                        llvm::Value* fieldPtr = context.Builder.CreateStructGEP(concretePayloadLLVMTy, concretePayloadPtr, bi, armBindings[bi]);
                        llvm::Type* fieldTy = concretePayloadLLVMTy->getStructElementType(bi);
                        llvm::Value* FieldVal = context.Builder.CreateLoad(fieldTy, fieldPtr, armBindings[bi] + "_val");
                        llvm::AllocaInst* FieldAlloca = context.Builder.CreateAlloca(fieldTy, nullptr, armBindings[bi]);
                        context.Builder.CreateStore(FieldVal, FieldAlloca);
                        context.NamedValues[armBindings[bi]] = FieldAlloca;
                    }
                }
            }
        }

        TypedValue ResultTV = Result->codegen(context);
        if (!ResultTV.Val) {
            std::cerr << "[FLUX ERROR] match arm result expression failed to codegen" << std::endl;
            return TypedValue();
        }

        // Clean up binding variables if we added any
        if (!armBindings.empty() && isPayloadEnum) {
            for (const auto& bv : armBindings) {
                context.NamedValues.erase(bv);
            }
        }

        llvm::Value* ResultV = ResultTV.Val;
        if (ResultV->getType() != ResultTy) {
            if (ResultV->getType()->isIntegerTy())
                ResultV = context.Builder.CreateSIToFP(ResultV, ResultTy, "match_cast");
        }
        context.Builder.CreateStore(ResultV, ResultAlloc);
        context.Builder.CreateBr(MergeBB);
    }

    // Emit default arm if present
    if (DefaultArm) {
        llvm::BasicBlock* DefaultBB = DefaultFallbackBB;
        if (!DefaultBB) {
            DefaultBB = llvm::BasicBlock::Create(Ctx, "match_default", TheFunction);
            context.Builder.SetInsertPoint(checkBBs.empty() ? context.Builder.GetInsertBlock() : checkBBs.back());
        } else {
            context.Builder.SetInsertPoint(DefaultBB);
        }
        TypedValue DefaultTV = DefaultArm->codegen(context);
        if (!DefaultTV.Val) {
            std::cerr << "[FLUX ERROR] match default arm result expression failed to codegen" << std::endl;
            return TypedValue();
        }
        llvm::Value* DefaultV = DefaultTV.Val;
        if (DefaultV->getType() != ResultTy) {
            if (DefaultV->getType()->isIntegerTy())
                DefaultV = context.Builder.CreateSIToFP(DefaultV, ResultTy, "default_cast");
        }
        context.Builder.CreateStore(DefaultV, ResultAlloc);
        context.Builder.CreateBr(MergeBB);
    }

    // Insert merge block and load result
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);
    llvm::Value* ResultVal = context.Builder.CreateLoad(ResultTy, ResultAlloc, "match_result_val");
    return TypedValue(ResultVal, TypeKind::Double);
}

// ============ Foreach Codegen ============

TypedValue ForeachExprAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Check if iterable is a call to a generator
    CallExprAST* Call = dynamic_cast<CallExprAST*>(Iterable.get());
    llvm::Function* GenF = nullptr;
    if (Call) {
        GenF = context.TheModule->getFunction(Call->getCallee());
    }

    bool isGenerator = GenF && GenF->arg_size() > 0 && GenF->getArg(0)->getType()->isPointerTy() &&
                       GenF->getArg(0)->getName() == "__gen_state";

    if (isGenerator) {
        // 1. Allocate generator state struct { i32 state_index }
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        llvm::SmallVector<llvm::Type*, 1> ElementTys = {Int32Ty};
        llvm::StructType* StateTy = llvm::StructType::get(Ctx, ElementTys);
        llvm::Value* StatePtr = context.Builder.CreateAlloca(StateTy, nullptr, "gen_state");

        // Initialize state to 0
        llvm::Value* IdxPtr = context.Builder.CreateStructGEP(StateTy, StatePtr, 0);
        context.Builder.CreateStore(llvm::ConstantInt::get(Int32Ty, 0), IdxPtr);

        // 2. Create loop blocks
        llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(Ctx, "gen_loop", TheFunction);
        llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "after_gen");

        context.Builder.CreateBr(LoopBB);
        context.Builder.SetInsertPoint(LoopBB);

        // 3. Prepare arguments for generator call
        std::vector<llvm::Value*> ArgsV;
        ArgsV.push_back(StatePtr); // First argument is state pointer

        for (const auto& Arg : Call->getArgs()) {
            TypedValue ArgTV = Arg->codegen(context);
            ArgsV.push_back(ArgTV.Val);
        }

        // 4. Call generator
        llvm::Value* RetVal = context.Builder.CreateCall(GenF, ArgsV, "yield_val");

        // 5. Check if finished (state == -1)
        llvm::Value* CurrentState = context.Builder.CreateLoad(Int32Ty, IdxPtr, "cur_state");
        llvm::Value* IsFinished = context.Builder.CreateICmpEQ(CurrentState, llvm::ConstantInt::get(Int32Ty, -1));
        context.Builder.CreateCondBr(IsFinished, AfterBB, LoopBB); // This logic is slightly wrong, need a body block

        // Fix: Jump to BodyBB if not finished
        LoopBB->getTerminator()->eraseFromParent();
        llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "gen_body", TheFunction);
        context.Builder.CreateCondBr(IsFinished, AfterBB, BodyBB);

        context.Builder.SetInsertPoint(BodyBB);

        // 6. Set loop variable
        llvm::Value* OldVal = context.NamedValues[VarName];
        context.NamedValues[VarName] = RetVal;

        // 7. Generate body
        context.CurrentLoopEnd = AfterBB;
        context.CurrentLoopCont = LoopBB;
        Body->codegen(context);

        context.Builder.CreateBr(LoopBB);

        // 8. Cleanup
        context.Builder.SetInsertPoint(AfterBB);
        context.NamedValues[VarName] = OldVal;

        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }

    // Default: Vector iteration (existing implementation)
    TypedValue IterableTV = Iterable->codegen(context);
    if (!IterableTV.Val) {
        std::cerr << "[FLUX ERROR] foreach iterable expression failed to codegen" << std::endl;
        return TypedValue();
    }

    // Extract pointer and size from vector struct
    // Vector struct: { double*, i32 }
    llvm::Value* DataPtr = context.Builder.CreateExtractValue(IterableTV.Val, {0}, "vec_data");
    llvm::Value* SizeVal = context.Builder.CreateExtractValue(IterableTV.Val, {1}, "vec_size");

    // Create loop blocks
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(Ctx, "foreach_loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "after_foreach");

    // Save old variable
    llvm::Value* OldVal = context.NamedValues[VarName];

    // Branch to loop
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);

    // Create PHI for index
    llvm::PHINode* IndexPhi = context.Builder.CreatePHI(llvm::Type::getInt32Ty(Ctx), 2, "foreach_idx");
    IndexPhi->addIncoming(llvm::ConstantInt::get(Ctx, llvm::APInt(32, 0)), PreheaderBB);

    // Load element from array
    llvm::Value* ElemPtr = context.Builder.CreateGEP(llvm::Type::getDoubleTy(Ctx), DataPtr, IndexPhi, "elem_ptr");
    llvm::Value* ElemVal = context.Builder.CreateLoad(llvm::Type::getDoubleTy(Ctx), ElemPtr, VarName.c_str());

    // Set loop variable
    context.NamedValues[VarName] = ElemVal;

    // Generate body
    llvm::BasicBlock* ContBB = llvm::BasicBlock::Create(Ctx, "foreach_cont", TheFunction);
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = ContBB;

    TypedValue BodyTV = Body->codegen(context);

    context.Builder.CreateBr(ContBB);
    context.Builder.SetInsertPoint(ContBB);

    // Increment index
    llvm::Value* NextIdx =
        context.Builder.CreateAdd(IndexPhi, llvm::ConstantInt::get(Ctx, llvm::APInt(32, 1)), "next_idx");
    IndexPhi->addIncoming(NextIdx, context.Builder.GetInsertBlock());

    // Check loop condition
    llvm::Value* ContinueCond = context.Builder.CreateICmpSLT(NextIdx, SizeVal, "foreach_cond");
    context.Builder.CreateCondBr(ContinueCond, LoopBB, AfterBB);

    // Continue after loop
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Restore variable
    context.NamedValues[VarName] = OldVal;
    context.CurrentLoopEnd = nullptr;
    context.CurrentLoopCont = nullptr;

    return BodyTV ? BodyTV : TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Repeat-Until Codegen ============

TypedValue RepeatUntilExprAST::codegen(CodegenContext& context)
{
    // Generate repeat-until loop (do-while style)
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "repeat_body", TheFunction);
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "until_cond");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "after_repeat");

    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;

    context.Builder.CreateBr(BodyBB);

    // Body
    context.Builder.SetInsertPoint(BodyBB);
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] repeat-until body expression failed to codegen" << std::endl;
        return TypedValue();
    }
    context.Builder.CreateBr(CondBB);

    // Condition
    TheFunction->insert(TheFunction->end(), CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] repeat-until condition expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Value* IsDone;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsDone = CondTV.Val;
    } else {
        IsDone = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "until_check");
    }

    context.Builder.CreateCondBr(IsDone, AfterBB, BodyBB);

    // After
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;

    return BodyTV;
}

// ============ Do-While Codegen ============

TypedValue DoWhileExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "dowhile_body", TheFunction);
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Ctx, "dowhile_cond");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "after_dowhile");

    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;

    context.Builder.CreateBr(BodyBB);

    // Body
    context.Builder.SetInsertPoint(BodyBB);
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] do-while body expression failed to codegen" << std::endl;
        return TypedValue();
    }
    context.Builder.CreateBr(CondBB);

    // Condition
    TheFunction->insert(TheFunction->end(), CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] do-while condition expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Value* CondBool;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        CondBool = CondTV.Val;
    } else {
        CondBool = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), "dowhile_check");
    }
    context.Builder.CreateCondBr(CondBool, BodyBB, AfterBB);

    // After
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;

    return BodyTV;
}

} // namespace Flux
