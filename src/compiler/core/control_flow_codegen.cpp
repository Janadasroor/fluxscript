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

#include "flux/compiler/ast.h"
#include "flux/compiler/codegen_helpers.h"
#include <iostream>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>

namespace Flux {

// ============ Switch Statement Codegen ============

TypedValue SwitchExprAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& LLVMCtx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(LLVMCtx);

    // Evaluate switch condition
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val)
        return TypedValue();

    llvm::Value* SwitchVal = CondTV.Val;

    // Create alloca for switch result in the entry block
    llvm::BasicBlock& EntryBB = TheFunction->getEntryBlock();
    llvm::BasicBlock* SavedBB = context.Builder.GetInsertBlock();
    bool entryEmpty = EntryBB.empty();
    llvm::Value* SwitchResultAlloca = nullptr;
    if (entryEmpty) {
        context.Builder.SetInsertPoint(&EntryBB);
    } else {
        context.Builder.SetInsertPoint(&EntryBB, EntryBB.begin());
    }
    SwitchResultAlloca = context.Builder.CreateAlloca(DoubleTy, nullptr, "switch_result");
    context.Builder.CreateStore(llvm::ConstantFP::get(DoubleTy, 0.0), SwitchResultAlloca);
    context.Builder.SetInsertPoint(SavedBB);

    // Create basic blocks
    llvm::BasicBlock* SwitchEndBB = llvm::BasicBlock::Create(LLVMCtx, "switch_end", TheFunction);
    context.CurrentSwitchEnd = SwitchEndBB; // Set for break statements

    std::vector<llvm::BasicBlock*> CaseBBs;
    std::vector<llvm::BasicBlock*> CmpBBs;

    // Create comparison and case blocks for all cases
    for (size_t i = 0; i < Cases.size(); ++i) {
        CmpBBs.push_back(llvm::BasicBlock::Create(LLVMCtx, "cmp_" + std::to_string(i), TheFunction));
        CaseBBs.push_back(llvm::BasicBlock::Create(LLVMCtx, "case_" + std::to_string(i), TheFunction));
    }

    llvm::BasicBlock* DefaultBB = llvm::BasicBlock::Create(LLVMCtx, "default", TheFunction);

    // Branch from current block to first comparison block
    {
        llvm::BasicBlock* FirstBB = CmpBBs.empty() ? DefaultBB : CmpBBs[0];
        if (context.Builder.GetInsertBlock()->getTerminator() == nullptr) {
            context.Builder.CreateBr(FirstBB);
        }
    }

    // Generate case comparison chain
    for (size_t i = 0; i < Cases.size(); ++i) {
        context.Builder.SetInsertPoint(CmpBBs[i]);

        ExprAST* CaseVal = Cases[i].getValue();
        if (!CaseVal)
            continue;

        TypedValue CaseValTV = CaseVal->codegen(context);
        if (!CaseValTV.Val)
            continue;

        llvm::BasicBlock* NextBB = (i + 1 < Cases.size()) ? CmpBBs[i + 1] : DefaultBB;

        llvm::Value* Cmp = context.Builder.CreateFCmpOEQ(SwitchVal, CaseValTV.Val, "case_cmp_" + std::to_string(i));
        context.Builder.CreateCondBr(Cmp, CaseBBs[i], NextBB);

        // Generate case body
        context.Builder.SetInsertPoint(CaseBBs[i]);
        llvm::BasicBlock* BeforeBody = context.Builder.GetInsertBlock();
        TypedValue LastBodyTV(nullptr, TypeKind::Void);
        for (const auto& Stmt : Cases[i].getBody()) {
            TypedValue StmtTV = Stmt->codegen(context);
            if (!StmtTV.Val) {
                LastBodyTV = StmtTV;
                break;
            }
            LastBodyTV = StmtTV;
        }

        // If the body already terminated (return/break), the case block has a
        // terminator but the successor block (unreachable/after_break) may
        // need one too. If the body did NOT terminate (normal flow), store
        // the last expression value to switch result and branch to switch_end.
        if (BeforeBody->getTerminator()) {
            // Body terminated via return/break — seal successor block if open
            llvm::BasicBlock* CurBB = context.Builder.GetInsertBlock();
            if (CurBB != BeforeBody && CurBB->getTerminator() == nullptr) {
                context.Builder.CreateUnreachable();
            }
        } else if (context.Builder.GetInsertBlock()->getTerminator() == nullptr) {
            // Store last expression value as the switch result
            if (LastBodyTV.Val) {
                context.Builder.CreateStore(LastBodyTV.Val, SwitchResultAlloca);
            }
            context.Builder.CreateBr(SwitchEndBB);
        }
    }

    // Generate default block
    context.Builder.SetInsertPoint(DefaultBB);
    llvm::BasicBlock* BeforeDefault = context.Builder.GetInsertBlock();
    TypedValue LastDefaultTV(nullptr, TypeKind::Void);
    for (const auto& Stmt : DefaultBody) {
        TypedValue StmtTV = Stmt->codegen(context);
        if (!StmtTV.Val) {
            LastDefaultTV = StmtTV;
            break;
        }
        LastDefaultTV = StmtTV;
    }
    if (BeforeDefault->getTerminator()) {
        llvm::BasicBlock* CurBB = context.Builder.GetInsertBlock();
        if (CurBB != BeforeDefault && CurBB->getTerminator() == nullptr) {
            context.Builder.CreateUnreachable();
        }
    } else if (context.Builder.GetInsertBlock()->getTerminator() == nullptr) {
        if (LastDefaultTV.Val) {
            context.Builder.CreateStore(LastDefaultTV.Val, SwitchResultAlloca);
        }
        context.Builder.CreateBr(SwitchEndBB);
    }

    // Move to switch end — this is the continuation block for subsequent code.
    // No branch to return — let the normal function epilogue handle that.
    context.Builder.SetInsertPoint(SwitchEndBB);
    context.CurrentSwitchEnd = nullptr; // Clear break target

    // Load the switch result from the dedicated alloca
    llvm::Value* SwitchResult = context.Builder.CreateLoad(DoubleTy, SwitchResultAlloca, "switch_result");

    return TypedValue(SwitchResult, TypeKind::Double);
}

// ============ Break Statement Codegen ============

TypedValue BreakExprAST::codegen(CodegenContext& context)
{
    // Determine where to break to (loop or switch)
    llvm::BasicBlock* TargetBB = nullptr;

    if (context.CurrentLoopEnd) {
        TargetBB = context.CurrentLoopEnd;
    } else if (context.CurrentSwitchEnd) {
        TargetBB = context.CurrentSwitchEnd;
    }

    if (!TargetBB) {
        // Error: break outside of loop or switch
        std::cerr << "Error: 'break' statement outside of loop or switch" << std::endl;
        return TypedValue();
    }

    // Get current function
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // Create branch to target
    context.Builder.CreateBr(TargetBB);

    // Create unreachable block after break (for IR validity)
    llvm::BasicBlock* AfterBreakBB = llvm::BasicBlock::Create(context.TheContext, "after_break", TheFunction);
    context.Builder.SetInsertPoint(AfterBreakBB);

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
}

// ============ Continue Statement Codegen ============

TypedValue ContinueExprAST::codegen(CodegenContext& context)
{
    if (!context.CurrentLoopCont) {
        // Error: continue outside of loop
        std::cerr << "Error: 'continue' statement outside of loop" << std::endl;
        return TypedValue();
    }

    // Get current function
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    // Branch to continue target (loop condition or increment)
    context.Builder.CreateBr(context.CurrentLoopCont);

    // Create unreachable block after continue
    llvm::BasicBlock* AfterContBB = llvm::BasicBlock::Create(context.TheContext, "after_continue", TheFunction);
    context.Builder.SetInsertPoint(AfterContBB);

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
}

// ============ Return Statement Codegen ============

TypedValue ReturnExprAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::Type* RetTy = TheFunction->getReturnType();

    TypedValue RetValTV(nullptr, TypeKind::Void);
    llvm::Value* RetVal = nullptr;

    if (Val) {
        RetValTV = Val->codegen(context);
        if (!RetValTV.Val)
            return TypedValue();
        RetVal = RetValTV.Val;

        // Handle type conversion for return value
        if (RetVal->getType() != RetTy && !RetTy->isVoidTy()) {
            if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast_ret");
            } else if (RetTy->isIntegerTy() && RetVal->getType()->isFloatingPointTy()) {
                RetVal = context.Builder.CreateFPToSI(RetVal, RetTy, "cast_ret");
            }
        }
    }

    // Use unified return block if available
    if (context.CurrentReturnBB) {
        if (context.CurrentReturnValueAlloca && RetVal) {
            context.Builder.CreateStore(RetVal, context.CurrentReturnValueAlloca);
        }
        context.Builder.CreateBr(context.CurrentReturnBB);

        // Subsequent instructions in the same block are unreachable
        llvm::BasicBlock* UnreachableBB = llvm::BasicBlock::Create(context.TheContext, "unreachable", TheFunction);
        context.Builder.SetInsertPoint(UnreachableBB);
    } else {
        // Fallback to direct return
        if (RetTy->isVoidTy() || !RetVal) {
            context.Builder.CreateRetVoid();
        } else {
            context.Builder.CreateRet(RetVal);
        }
    }

    return RetValTV;
}

// ============ If Expression Codegen ============

TypedValue IfExprAST::codegen(CodegenContext& context)
{
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] If condition sub-expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Value* CondV = boolCondition(CondTV.Val, context.Builder, context.TheContext);
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(context.TheContext, "else", TheFunction);
    llvm::BasicBlock* MergeBB = nullptr;

    context.Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Generate Then block
    context.Builder.SetInsertPoint(ThenBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope =
            context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue ThenTV = Then->codegen(context);
    if (!ThenTV.Val && ThenTV.Type.Kind != TypeKind::Void) {
        std::cerr << "[FLUX ERROR] If-then branch failed to codegen" << std::endl;
        return TypedValue();
    }

    bool thenTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    ThenBB = context.Builder.GetInsertBlock();
    if (context.DebugEnabled)
        context.LexicalBlocks.pop_back();

    // Generate Else block
    context.Builder.SetInsertPoint(ElseBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope =
            context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue ElseTV = Else->codegen(context);
    if (!ElseTV.Val && ElseTV.Type.Kind != TypeKind::Void) {
        std::cerr << "[FLUX ERROR] If-else branch failed to codegen" << std::endl;
        return TypedValue();
    }

    bool elseTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    ElseBB = context.Builder.GetInsertBlock();
    if (context.DebugEnabled)
        context.LexicalBlocks.pop_back();

    // Determine if we need a merge block
    if (!thenTerminated || !elseTerminated) {
        MergeBB = llvm::BasicBlock::Create(context.TheContext, "ifcont", TheFunction);

        if (!thenTerminated) {
            context.Builder.SetInsertPoint(ThenBB);
            context.Builder.CreateBr(MergeBB);
            ThenBB = context.Builder.GetInsertBlock();
        }

        if (!elseTerminated) {
            context.Builder.SetInsertPoint(ElseBB);
            context.Builder.CreateBr(MergeBB);
            ElseBB = context.Builder.GetInsertBlock();
        }

        context.Builder.SetInsertPoint(MergeBB);

        if (ThenTV.Type.Kind == TypeKind::Void && ElseTV.Type.Kind == TypeKind::Void) {
            return ThenTV;
        }

        // PHI node for reachable paths
        llvm::Type* PhiTy = ThenTV.Val ? ThenTV.Val->getType() : ElseTV.Val->getType();
        llvm::PHINode* PN = context.Builder.CreatePHI(PhiTy, 2, "iftmp");

        if (!thenTerminated) {
            llvm::Value* TV = ThenTV.Val;
            if (TV->getType() != PhiTy)
                TV = context.Builder.CreateSIToFP(TV, PhiTy, "cast_then");
            PN->addIncoming(TV, ThenBB);
        }
        if (!elseTerminated) {
            llvm::Value* EV = ElseTV.Val;
            if (EV->getType() != PhiTy)
                EV = context.Builder.CreateSIToFP(EV, PhiTy, "cast_else");
            PN->addIncoming(EV, ElseBB);
        }
        return TypedValue(PN, ThenTV.Type);
    }

    // Both paths are terminated
    return ThenTV;
}

// ============ For Expression Codegen ============

TypedValue ForExprAST::codegen(CodegenContext& context)
{
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV =
        Step ? Step->codegen(context)
             : TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
    if (!StartTV.Val || !EndTV.Val || !StepTV.Val) {
        std::cerr << "[FLUX ERROR] For-loop range sub-expression failed to codegen" << std::endl;
        return TypedValue();
    }
    auto ensureDouble = [&](TypedValue& TV) {
        if (TV.Type.Kind == TypeKind::Int)
            TV = TypedValue(
                context.Builder.CreateSIToFP(TV.Val, llvm::Type::getDoubleTy(context.TheContext), "int2double"),
                TypeKind::Double);
    };
    ensureDouble(StartTV);
    ensureDouble(EndTV);
    ensureDouble(StepTV);
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(context.TheContext, "loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterloop");

    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = LoopBB;

    llvm::AllocaInst* LoopAlloca =
        context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
    context.Builder.CreateStore(StartTV.Val, LoopAlloca);
    context.NamedValues[VarName] = LoopAlloca;

    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope =
            context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] For-loop body failed to codegen" << std::endl;
        return TypedValue();
    }
    llvm::Value* CurrentVar =
        context.Builder.CreateLoad(llvm::Type::getDoubleTy(context.TheContext), LoopAlloca, VarName.c_str());
    llvm::Value* NextVar = context.Builder.CreateFAdd(CurrentVar, StepTV.Val, "nextvar");
    context.Builder.CreateStore(NextVar, LoopAlloca);
    if (context.DebugEnabled)
        context.LexicalBlocks.pop_back();
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndTV.Val, "loopcond");
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;

    return BodyTV;
}

// ============ While Expression Codegen ============

TypedValue WhileExprAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "whilecond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "whilebody");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterwhile");

    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;

    context.Builder.CreateBr(CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] While condition failed to codegen" << std::endl;
        return TypedValue();
    }
    llvm::Value* CondV = boolCondition(CondTV.Val, context.Builder, context.TheContext);
    context.Builder.CreateCondBr(CondV, BodyBB, AfterBB);
    TheFunction->insert(TheFunction->end(), BodyBB);
    context.Builder.SetInsertPoint(BodyBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope =
            context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] While body failed to codegen" << std::endl;
        return TypedValue();
    }
    if (context.DebugEnabled)
        context.LexicalBlocks.pop_back();
    context.Builder.CreateBr(CondBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);

    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;

    return BodyTV;
}

} // namespace Flux
