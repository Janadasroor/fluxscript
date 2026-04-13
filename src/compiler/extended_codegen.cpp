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
#include <iostream>
#include "flux/compiler/ast.h"

namespace Flux {

// ============ Try/Catch Codegen ============

void TryCatchExprAST::addCatch(const std::string& var, std::unique_ptr<ExprAST> handler) {
    CatchClauses.emplace_back(var, std::move(handler));
}

void TryCatchExprAST::setFinally(std::unique_ptr<ExprAST> body) {
    FinallyBody = std::move(body);
}

TypedValue TryCatchExprAST::codegen(CodegenContext& context) {
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
    llvm::Value* JmpBuf = context.Builder.CreateAlloca(llvm::ArrayType::get(llvm::Type::getInt8Ty(Ctx), 1024), nullptr, "jmp_buf" + IDStr);
    llvm::Value* JmpBufPtr = context.Builder.CreateBitCast(JmpBuf, VoidPtrTy);
    
    // Register runtime helpers
    llvm::Function* PushF = context.TheModule->getFunction("flux_push_jmp_buf");
    if (!PushF) PushF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_push_jmp_buf", context.TheModule.get());
    
    llvm::Function* PopF = context.TheModule->getFunction("flux_pop_jmp_buf");
    if (!PopF) PopF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {}, false), llvm::Function::ExternalLinkage, "flux_pop_jmp_buf", context.TheModule.get());
    
    llvm::Function* SetJmpF = context.TheModule->getFunction("setjmp");
    if (!SetJmpF) SetJmpF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "setjmp", context.TheModule.get());
    
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
        if (TryResult.Val) context.Builder.CreateStore(TryResult.Val, ResultPtr);
        context.Builder.CreateCall(PopF);
        context.Builder.CreateBr(FinallyBB);
    }
    
    // Catch block
    context.Builder.SetInsertPoint(CatchBB);
    context.Builder.CreateCall(PopF); 
    
    llvm::Value* ExCode = context.Builder.CreateSIToFP(JmpVal, DoubleTy, "ex_code" + IDStr);
    context.Builder.CreateStore(ExCode, ExceptionPtr);
    
    if (!CatchClauses.empty()) {
        auto& [VarName, Handler] = CatchClauses[0];
        llvm::Value* OldVal = context.NamedValues[VarName];
        
        // Bind the catch variable to the exception value
        llvm::Value* ThrownVal = context.Builder.CreateLoad(DoubleTy, ExceptionPtr, "thrown_val" + IDStr);
        context.NamedValues[VarName] = ThrownVal;
        
        TypedValue CatchResult = Handler->codegen(context);
        if (!context.Builder.GetInsertBlock()->getTerminator()) {
            if (CatchResult.Val) context.Builder.CreateStore(CatchResult.Val, ResultPtr);
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

// ============ Throw Codegen ============

TypedValue ThrowExprAST::codegen(CodegenContext& context) {
    TypedValue ExTV = Exception->codegen(context);
    if (!ExTV.Val) return TypedValue();
    
    llvm::LLVMContext& Ctx = context.TheContext;
    
    // If block already terminated, stop.
    if (context.Builder.GetInsertBlock()->getTerminator()) {
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }

    llvm::Module* TheModule = context.TheModule.get();
    
    // Always use runtime throw which handles the non-local jump via longjmp
    llvm::Function* ThrowFunc = TheModule->getFunction("flux_throw_error");
    if (!ThrowFunc) {
        llvm::Type* Params[] = { llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0) };
        ThrowFunc = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                         llvm::Function::ExternalLinkage, "flux_throw_error", TheModule);
    }
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr("Flux runtime exception", "ex_msg");
    context.Builder.CreateCall(ThrowFunc, {ExTV.Val, MsgPtr});
    context.Builder.CreateUnreachable();
    
    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Assert Codegen ============

TypedValue AssertExprAST::codegen(CodegenContext& context) {
    // Generate condition check
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    // Create assertion check
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* ContinueBB = llvm::BasicBlock::Create(context.TheContext, "assert_continue", TheFunction);
    llvm::BasicBlock* FailBB = llvm::BasicBlock::Create(context.TheContext, "assert_fail", TheFunction);
    
    llvm::Value* IsTrue = context.Builder.CreateFCmpONE(
        CondTV.Val,
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)),
        "assert_check");
    
    context.Builder.CreateCondBr(IsTrue, ContinueBB, FailBB);
    
    // Fail block - would print error message
    context.Builder.SetInsertPoint(FailBB);
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(
        Message.empty() ? "Assertion failed!" : Message, "assert_msg");
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

TypedValue YieldExprAST::codegen(CodegenContext& context) {
    // Generate yielded value
    TypedValue ValueTV = Value->codegen(context);
    if (!ValueTV.Val) return TypedValue();
    
    // In full implementation, this would save state and return from generator
    // For now, just return the value
    return ValueTV;
}

// ============ Corner Case Codegen ============

void CornerExprAST::addCase(const std::string& name, std::unique_ptr<ExprAST> expr) {
    Cases.emplace_back(name, std::move(expr));
}

TypedValue CornerExprAST::codegen(CodegenContext& context) {
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
        if (!CaseTV.Val) return TypedValue();
        
        // Store result in array at index i
        llvm::Value* Indices[] = {
            llvm::ConstantInt::get(Ctx, llvm::APInt(32, 0)),
            llvm::ConstantInt::get(Ctx, llvm::APInt(32, i))
        };
        llvm::Value* CasePtr = context.Builder.CreateGEP(ResultArrayTy, ResultArray, Indices, "case_" + std::to_string(i) + "_ptr");
        context.Builder.CreateStore(CaseTV.Val, CasePtr);
        
        std::cout << "[CodeGen] Corner case '" << CaseName << "' evaluated" << std::endl;
    }
    
    // Return pointer to results array
    llvm::Type* PtrTy = llvm::PointerType::get(Ctx, 0);
    return TypedValue(context.Builder.CreateBitCast(ResultArray, PtrTy), TypeKind::Double);
}

// ============ Pattern Match Codegen ============

void MatchExprAST::addArm(std::unique_ptr<ExprAST> pattern, std::unique_ptr<ExprAST> result) {
    Arms.emplace_back(std::move(pattern), std::move(result));
}

void MatchExprAST::setDefault(std::unique_ptr<ExprAST> arm) {
    DefaultArm = std::move(arm);
}

TypedValue MatchExprAST::codegen(CodegenContext& context) {
    // Generate pattern matching as a series of if-else comparisons
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // Generate the value to match against
    TypedValue MatchValTV = Value->codegen(context);
    if (!MatchValTV.Val) return TypedValue();

    // Create merge and result alloc
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "match_merge");
    llvm::AllocaInst* ResultAlloc = context.Builder.CreateAlloca(DoubleTy, nullptr, "match_result");

    llvm::BasicBlock* CurrentBB = context.Builder.GetInsertBlock();

    // Generate each arm as nested if-else
    llvm::BasicBlock* NextCheckBB = nullptr;

    for (int i = static_cast<int>(Arms.size()) - 1; i >= 0; i--) {
        auto& [Pattern, Result] = Arms[i];

        // Create the "else" block (next check or default)
        llvm::BasicBlock* ElseBB;
        if (NextCheckBB) {
            ElseBB = NextCheckBB;
        } else if (i == static_cast<int>(Arms.size()) - 1 && DefaultArm) {
            ElseBB = llvm::BasicBlock::Create(Ctx, "match_default", TheFunction);
        } else {
            ElseBB = MergeBB;
        }

        // Create match block
        llvm::BasicBlock* MatchBB = llvm::BasicBlock::Create(Ctx, "match_arm_" + std::to_string(i), TheFunction);

        // Re-generate pattern value (need to regenerate in current context)
        // Save and restore builder position
        llvm::IRBuilder<>::InsertPoint GuardIP = context.Builder.saveIP();
        context.Builder.SetInsertPoint(CurrentBB);

        TypedValue PatternTV = Pattern->codegen(context);
        if (!PatternTV.Val) return TypedValue();

        llvm::Value* IsMatch = context.Builder.CreateFCmpOEQ(
            MatchValTV.Val, PatternTV.Val, "match_check_" + std::to_string(i));

        context.Builder.CreateCondBr(IsMatch, MatchBB, ElseBB);

        // Generate match arm body
        context.Builder.SetInsertPoint(MatchBB);
        TypedValue ResultTV = Result->codegen(context);
        if (!ResultTV.Val) return TypedValue();
        context.Builder.CreateStore(ResultTV.Val, ResultAlloc);
        context.Builder.CreateBr(MergeBB);

        NextCheckBB = ElseBB;
        context.Builder.restoreIP(GuardIP);
    }

    // Generate default arm if exists and not yet handled
    if (DefaultArm && NextCheckBB != MergeBB) {
        // We need to check if NextCheckBB is the default block we created
        if (NextCheckBB && NextCheckBB->getName() == "match_default") {
            context.Builder.SetInsertPoint(NextCheckBB);
            TypedValue DefaultTV = DefaultArm->codegen(context);
            if (!DefaultTV.Val) return TypedValue();
            context.Builder.CreateStore(DefaultTV.Val, ResultAlloc);
            context.Builder.CreateBr(MergeBB);
        }
    }

    // Insert merge block and load result
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);
    llvm::Value* ResultVal = context.Builder.CreateLoad(DoubleTy, ResultAlloc, "match_result_val");
    return TypedValue(ResultVal, TypeKind::Double);
}

// ============ Foreach Codegen ============

TypedValue ForeachExprAST::codegen(CodegenContext& context) {
    // Generate foreach loop that iterates over a vector/collection
    // Assumes iterable is a {double*, i32} vector struct
    
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;
    
    // Generate iterable expression
    TypedValue IterableTV = Iterable->codegen(context);
    if (!IterableTV.Val) return TypedValue();
    
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
    llvm::Value* ElemPtr = context.Builder.CreateGEP(
        llvm::Type::getDoubleTy(Ctx),
        DataPtr,
        IndexPhi,
        "elem_ptr");
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
    llvm::Value* NextIdx = context.Builder.CreateAdd(
        IndexPhi,
        llvm::ConstantInt::get(Ctx, llvm::APInt(32, 1)),
        "next_idx");
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

TypedValue RepeatUntilExprAST::codegen(CodegenContext& context) {
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
    if (!BodyTV.Val) return TypedValue();
    context.Builder.CreateBr(CondBB);
    
    // Condition
    TheFunction->insert(TheFunction->end(), CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    llvm::Value* IsDone = context.Builder.CreateFCmpONE(
        CondTV.Val,
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)),
        "until_check");
    
    context.Builder.CreateCondBr(IsDone, AfterBB, BodyBB);
    
    // After
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

} // namespace Flux
