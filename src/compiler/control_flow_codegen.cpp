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

#include <iostream>
#include "flux/compiler/ast.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

namespace Flux {

// ============ Switch Statement Codegen ============

TypedValue SwitchExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    
    // Evaluate switch condition
    TypedValue CondTV = Condition->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    // Create basic blocks
    llvm::BasicBlock* SwitchEndBB = llvm::BasicBlock::Create(context.TheContext, "switch_end", TheFunction);
    context.CurrentSwitchEnd = SwitchEndBB;  // Set for break statements
    
    std::vector<llvm::BasicBlock*> CaseBBs;
    std::vector<llvm::Value*> CaseValues;
    
    // Create blocks for all cases
    for (size_t i = 0; i < Cases.size(); ++i) {
        CaseBBs.push_back(llvm::BasicBlock::Create(context.TheContext, "case_" + std::to_string(i), TheFunction));
    }
    
    llvm::BasicBlock* DefaultBB = llvm::BasicBlock::Create(context.TheContext, "default", TheFunction);
    
    // Generate case comparison chain
    llvm::Value* SwitchVal = CondTV.Val;
    llvm::BasicBlock* NextBB = nullptr;
    
    for (size_t i = 0; i < Cases.size(); ++i) {
        llvm::BasicBlock* CaseBB = CaseBBs[i];
        llvm::BasicBlock* NextCaseBB = (i + 1 < Cases.size()) ? CaseBBs[i + 1] : DefaultBB;
        
        // Generate comparison: if (switch_val == case_val) goto case_bb else goto next
        ExprAST* CaseVal = Cases[i].getValue();
        if (!CaseVal) continue;
        
        TypedValue CaseValTV = CaseVal->codegen(context);
        if (!CaseValTV.Val) continue;
        
        llvm::Value* Cmp = context.Builder.CreateFCmpOEQ(SwitchVal, CaseValTV.Val, "case_cmp_" + std::to_string(i));
        context.Builder.CreateCondBr(Cmp, CaseBB, NextCaseBB);
        
        // Move to case block
        context.Builder.SetInsertPoint(CaseBB);
        
        // Generate case body
        for (const auto& Stmt : Cases[i].getBody()) {
            TypedValue StmtTV = Stmt->codegen(context);
            if (!StmtTV.Val) break;
        }
        
        // Jump to switch end (unless break was encountered)
        if (context.Builder.GetInsertBlock()->getTerminator() == nullptr) {
            context.Builder.CreateBr(SwitchEndBB);
        }
    }
    
    // Generate default block
    context.Builder.SetInsertPoint(DefaultBB);
    for (const auto& Stmt : DefaultBody) {
        TypedValue StmtTV = Stmt->codegen(context);
        if (!StmtTV.Val) break;
    }
    if (context.Builder.GetInsertBlock()->getTerminator() == nullptr) {
        context.Builder.CreateBr(SwitchEndBB);
    }
    
    // Move to switch end
    context.Builder.SetInsertPoint(SwitchEndBB);
    context.CurrentSwitchEnd = nullptr;  // Clear break target
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Break Statement Codegen ============

TypedValue BreakExprAST::codegen(CodegenContext& context) {
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
    
    // Create branch to target
    context.Builder.CreateBr(TargetBB);
    
    // Create unreachable block after break (for IR validity)
    llvm::BasicBlock* AfterBreakBB = llvm::BasicBlock::Create(context.TheContext, "after_break");
    context.Builder.SetInsertPoint(AfterBreakBB);
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Continue Statement Codegen ============

TypedValue ContinueExprAST::codegen(CodegenContext& context) {
    if (!context.CurrentLoopCont) {
        // Error: continue outside of loop
        std::cerr << "Error: 'continue' statement outside of loop" << std::endl;
        return TypedValue();
    }
    
    // Branch to continue target (loop condition or increment)
    context.Builder.CreateBr(context.CurrentLoopCont);
    
    // Create unreachable block after continue
    llvm::BasicBlock* AfterContBB = llvm::BasicBlock::Create(context.TheContext, "after_continue");
    context.Builder.SetInsertPoint(AfterContBB);
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

// ============ Return Statement Codegen ============

TypedValue ReturnExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::Type* RetTy = TheFunction->getReturnType();

    TypedValue RetValTV(nullptr, TypeKind::Void);
    llvm::Value* RetVal = nullptr;

    if (Val) {
        RetValTV = Val->codegen(context);
        if (!RetValTV.Val) return TypedValue();
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

} // namespace Flux
