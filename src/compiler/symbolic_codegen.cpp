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

namespace Flux {

// ============================================================================
// SymDeclAST - Symbolic variable declaration
// ============================================================================
TypedValue SymDeclAST::codegen(CodegenContext& context) {
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
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
// SolveExprAST - Solve equation for variable
// ============================================================================
TypedValue SolveExprAST::codegen(CodegenContext& context) {
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    // Call flux_sym_solve(double expr_ptr, double rhs_ptr, const char* var)
    // For now we assume expr = 0
    llvm::Function* SolveF = TheModule->getFunction("flux_sym_solve");
    if (!SolveF) {
        llvm::Type* Params[] = { llvm::Type::getDoubleTy(Ctx), llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0) };
        SolveF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), Params, false),
                                       llvm::Function::ExternalLinkage, "flux_sym_solve", TheModule);
    }
    
    // Create symbolic zero
    llvm::Function* SymNumF = TheModule->getFunction("flux_sym_number");
    if (!SymNumF) {
        SymNumF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
                                        llvm::Function::ExternalLinkage, "flux_sym_number", TheModule);
    }
    llvm::Value* ZeroPtr = context.Builder.CreateCall(SymNumF, {llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0))}, "sym_zero");
    
    llvm::Value* VarNamePtr = context.Builder.CreateGlobalStringPtr(Variable, "solve_var");
    
    return TypedValue(context.Builder.CreateCall(SolveF, {ExprTV.Val, ZeroPtr, VarNamePtr}, "solution"), TypeKind::Double);
}

// ============================================================================
// SimplifyExprAST - Simplify symbolic expression
// ============================================================================
TypedValue SimplifyExprAST::codegen(CodegenContext& context) {
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
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
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
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
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    // This is more complex because of the map. We'll need to create arrays.
    int count = Values.size();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    llvm::Value* NamesArr = context.Builder.CreateAlloca(llvm::ArrayType::get(VoidPtrTy, count), nullptr, "subst_names");
    llvm::Value* ValsArr = context.Builder.CreateAlloca(llvm::ArrayType::get(DoubleTy, count), nullptr, "subst_vals");
    
    int i = 0;
    for (auto& [name, valExpr] : Values) {
        llvm::Value* NamePtr = context.Builder.CreateGlobalStringPtr(name);
        llvm::Value* Idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), i);
        context.Builder.CreateStore(NamePtr, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(VoidPtrTy, count), NamesArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        
        TypedValue ValTV = valExpr->codegen(context);
        context.Builder.CreateStore(ValTV.Val, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(DoubleTy, count), ValsArr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
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
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule.get();
    
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

} // namespace Flux
