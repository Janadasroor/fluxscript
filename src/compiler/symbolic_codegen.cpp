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

// Symbolic AST code generation - Full Implementation
// Implements symbolic math compilation to LLVM IR via SymbolicEngine runtime
#include "flux/compiler/symbolic_ast.h"
#include "flux/runtime/symbolic_engine.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <iostream>

namespace Flux {

// ============================================================================
// SymDeclAST - Symbolic variable declaration
// ============================================================================
TypedValue SymDeclAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    // Call flux_sym_decl(const char* name)
    llvm::Function* SymDeclF = TheModule->getFunction("flux_sym_decl");
    if (!SymDeclF) {
        llvm::Type* Params[] = { llvm::PointerType::get(Ctx, 0) };
        SymDeclF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), Params, false),
                                         llvm::Function::ExternalLinkage, "flux_sym_decl", TheModule);
    }
    
    llvm::Value* NamePtr = context.Builder.CreateGlobalStringPtr(VarName, "sym_name");
    llvm::Value* SymPtr = context.Builder.CreateCall(SymDeclF, {NamePtr}, "sym_ptr");
    
    // Register in symbol table
    context.NamedValues[VarName] = SymPtr;
    context.NamedTypes[VarName] = FluxType(TypeKind::Symbolic);
    
    return TypedValue(SymPtr, TypeKind::Symbolic);
}

// ============================================================================
// SymExprAST - Symbolic expression handle
// ============================================================================
TypedValue SymExprAST::codegen(CodegenContext& context) {
    return Expr->codegen(context);
}

// ============================================================================
// SimplifyExprAST - Simplify symbolic expression
// ============================================================================
TypedValue SimplifyExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    llvm::Function* SimplifyF = TheModule->getFunction("flux_sym_simplify");
    if (!SimplifyF) {
        SimplifyF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
                                          llvm::Function::ExternalLinkage, "flux_sym_simplify", TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(SimplifyF, {ExprTV.Val}, "simplified"), TypeKind::Symbolic);
}

// ============================================================================
// DifferentiateExprAST - Symbolic differentiation
// ============================================================================
TypedValue DifferentiateExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    llvm::Function* DiffF = TheModule->getFunction("flux_sym_differentiate");
    if (!DiffF) {
        DiffF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0)}, false),
                                      llvm::Function::ExternalLinkage, "flux_sym_differentiate", TheModule);
    }
    
    llvm::Value* VarNamePtr = context.Builder.CreateGlobalStringPtr(Variable, "diff_var");
    return TypedValue(context.Builder.CreateCall(DiffF, {ExprTV.Val, VarNamePtr}, "derivative"), TypeKind::Symbolic);
}

// ============================================================================
// SubstituteExprAST - Substitute values in expression
// ============================================================================
TypedValue SubstituteExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    int count = Values.size();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    llvm::Value* NamesArr = context.Builder.CreateAlloca(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), nullptr, "subst_names");
    llvm::Value* ValsArr = context.Builder.CreateAlloca(llvm::ArrayType::get(DoubleTy, std::max(1, count)), nullptr, "subst_vals");
    
    int i = 0;
    for (auto& [name, valExpr] : Values) {
        llvm::Value* NamePtr = context.Builder.CreateGlobalStringPtr(name);
        llvm::Value* Idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), i);
        context.Builder.CreateStore(NamePtr, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), NamesArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        
        TypedValue ValTV = valExpr->codegen(context);
        context.Builder.CreateStore(ValTV.Val, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(DoubleTy, std::max(1, count)), ValsArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        i++;
    }
    
    llvm::Function* SubstF = TheModule->getFunction("flux_sym_substitute");
    if (!SubstF) {
        llvm::Type* Params[] = { DoubleTy, DoubleTy, VoidPtrTy, VoidPtrTy };
        SubstF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                       llvm::Function::ExternalLinkage, "flux_sym_substitute", TheModule);
    }
    
    llvm::Value* NamesPtr = context.Builder.CreateBitCast(NamesArr, VoidPtrTy);
    llvm::Value* ValsPtr = context.Builder.CreateBitCast(ValsArr, VoidPtrTy);
    
    return TypedValue(context.Builder.CreateCall(SubstF, {ExprTV.Val, llvm::ConstantFP::get(DoubleTy, (double)count), NamesPtr, ValsPtr}, "substituted"), TypeKind::Symbolic);
}

// ============================================================================
// EvaluateExprAST - Evaluate symbolic expression to a number
// ============================================================================
TypedValue EvaluateExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    int count = Values.size();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    llvm::Value* NamesArr = context.Builder.CreateAlloca(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), nullptr, "eval_names");
    llvm::Value* ValsArr = context.Builder.CreateAlloca(llvm::ArrayType::get(DoubleTy, std::max(1, count)), nullptr, "eval_vals");
    
    int i = 0;
    for (auto& [name, valExpr] : Values) {
        llvm::Value* NamePtr = context.Builder.CreateGlobalStringPtr(name);
        llvm::Value* Idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), i);
        context.Builder.CreateStore(NamePtr, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), NamesArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        
        TypedValue ValTV = valExpr->codegen(context);
        context.Builder.CreateStore(ValTV.Val, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(DoubleTy, std::max(1, count)), ValsArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        i++;
    }
    
    llvm::Function* EvalF = TheModule->getFunction("flux_sym_evaluate");
    if (!EvalF) {
        llvm::Type* Params[] = { DoubleTy, DoubleTy, VoidPtrTy, VoidPtrTy };
        EvalF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                      llvm::Function::ExternalLinkage, "flux_sym_evaluate", TheModule);
    }
    
    llvm::Value* NamesPtr = context.Builder.CreateBitCast(NamesArr, VoidPtrTy);
    llvm::Value* ValsPtr = context.Builder.CreateBitCast(ValsArr, VoidPtrTy);
    
    return TypedValue(context.Builder.CreateCall(EvalF, {ExprTV.Val, llvm::ConstantFP::get(DoubleTy, (double)count), NamesPtr, ValsPtr}, "eval_res"), TypeKind::Double);
}

// ============================================================================
// JacobianExprAST - Jacobian matrix generation
// ============================================================================
TypedValue JacobianExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    
    TypedValue ExprsTV = Expressions->codegen(context);
    TypedValue VarsTV = Variables->codegen(context);
    
    if (!ExprsTV.Val || !VarsTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    
    llvm::Function* JacF = context.TheModule->getFunction("flux_sym_jacobian");
    if (!JacF) {
        JacF = llvm::Function::Create(
            llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, VoidPtrTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_sym_jacobian", context.TheModule);
    }
    
    llvm::Value* ExprsData = context.Builder.CreateExtractValue(ExprsTV.Val, 0, "exprs_data");
    llvm::Value* ExprsSize = context.Builder.CreateExtractValue(ExprsTV.Val, 1, "exprs_size");
    
    llvm::Value* VarsData = context.Builder.CreateExtractValue(VarsTV.Val, 0, "vars_data");
    llvm::Value* VarsSize = context.Builder.CreateExtractValue(VarsTV.Val, 1, "vars_size");
    
    llvm::Value* JacPtr = context.Builder.CreateCall(JacF, {ExprsData, ExprsSize, VarsData, VarsSize}, "jac_ptr");
    
    llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, JacPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, ExprsSize, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, VarsSize, 2);
    
    return TypedValue(MatVal, TypeKind::Matrix);
}

// ============================================================================
// PDEExprAST - PDE solver registration
// ============================================================================
TypedValue PDEExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    
    TypedValue EqTV = Equation->codegen(context);
    if (!EqTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    
    llvm::Function* PdeRegF = context.TheModule->getFunction("flux_sym_pde_register");
    if (!PdeRegF) {
        PdeRegF = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, Int32Ty, VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_sym_pde_register", context.TheModule);
    }
    
    std::vector<llvm::Constant*> VarNames;
    for (const auto& var : IndependentVars) {
        VarNames.push_back(context.Builder.CreateGlobalStringPtr(var));
    }
    
    llvm::ArrayType* ArrTy = llvm::ArrayType::get(VoidPtrTy, std::max((size_t)1, VarNames.size()));
    llvm::GlobalVariable* GV = new llvm::GlobalVariable(*context.TheModule, ArrTy, true,
                                                        llvm::GlobalValue::InternalLinkage,
                                                        llvm::ConstantArray::get(ArrTy, VarNames),
                                                        "pde_vars");
    llvm::Value* NamesPtr = context.Builder.CreateBitCast(GV, VoidPtrTy);
    
    llvm::Value* PdeHandle = context.Builder.CreateCall(PdeRegF, {EqTV.Val, llvm::ConstantInt::get(Int32Ty, (int)IndependentVars.size()), NamesPtr}, "pde_handle");
    
    return TypedValue(PdeHandle, TypeKind::Symbolic);
}

// ============================================================================
// PartialDiffExprAST - Partial differentiation
// ============================================================================
TypedValue PartialDiffExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    
    llvm::Function* PdiffF = context.TheModule->getFunction("flux_sym_pdiff");
    if (!PdiffF) {
        PdiffF = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, VoidPtrTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_sym_pdiff", context.TheModule);
    }
    
    llvm::Value* VarPtr = context.Builder.CreateGlobalStringPtr(Variable);
    llvm::Value* ResPtr = context.Builder.CreateCall(PdiffF, {ExprTV.Val, VarPtr, llvm::ConstantInt::get(Int32Ty, Order)}, "pdiff_ptr");
    
    return TypedValue(ResPtr, TypeKind::Symbolic);
}

} // namespace Flux
