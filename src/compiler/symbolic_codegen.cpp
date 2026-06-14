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
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace Flux {

// ============================================================================
// SymDeclAST - Symbolic variable declaration
// ============================================================================
TypedValue SymDeclAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Call flux_sym_decl(const char* name)
    llvm::Function* SymDeclF = TheModule->getFunction("flux_sym_decl");
    if (!SymDeclF) {
        llvm::Type* Params[] = {llvm::PointerType::get(Ctx, 0)};
        SymDeclF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), Params, false),
                                          llvm::Function::ExternalLinkage, "flux_sym_decl", TheModule);
    }

    llvm::Value* NamePtr = context.Builder.CreateGlobalString(VarName, "sym_name");
    llvm::Value* SymPtr = context.Builder.CreateCall(SymDeclF, {NamePtr}, "sym_ptr");

    // Register in symbol table
    context.NamedValues[VarName] = SymPtr;
    context.NamedTypes[VarName] = FluxType(TypeKind::Symbolic);

    return TypedValue(SymPtr, TypeKind::Symbolic);
}

// ============================================================================
// SymExprAST - Symbolic expression handle
// ============================================================================
TypedValue SymExprAST::codegen(CodegenContext& context)
{
    return Expr->codegen(context);
}

// ============================================================================
// SimplifyExprAST - Simplify symbolic expression
// ============================================================================
TypedValue SimplifyExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    llvm::Function* SimplifyF = TheModule->getFunction("flux_sym_simplify");
    if (!SimplifyF) {
        SimplifyF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_simplify", TheModule);
    }

    return TypedValue(context.Builder.CreateCall(SimplifyF, {ExprTV.Val}, "simplified"), TypeKind::Symbolic);
}

// ============================================================================
// DifferentiateExprAST - Symbolic differentiation
// ============================================================================
TypedValue DifferentiateExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    // differentiate() only works with symbolic expressions (sym x) because it
    // operates on the symbolic AST at runtime via the SymbolicEngine. When the
    // inner expression is JIT-compiled to a concrete value (e.g. a plain Double),
    // the symbolic information is lost and flux_sym_differentiate would try to
    // interpret the concrete value's bit pattern as a SymbolicExpr* pointer,
    // causing SIGSEGV. Emit a clear compile-time error instead.
    if (ExprTV.Type.Kind != TypeKind::Symbolic) {
        std::cerr << "[Error] differentiate() requires a symbolic expression. "
                  << "Declare variables with 'sym' instead of 'let' or use "
                  << "pre-computed analytical derivatives for compiled functions."
                  << std::endl;
        return TypedValue();
    }

    llvm::Function* DiffF = TheModule->getFunction("flux_sym_differentiate");
    if (!DiffF) {
        DiffF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx),
                                    {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_differentiate", TheModule);
    }

    llvm::Value* VarNamePtr = context.Builder.CreateGlobalString(Variable, "diff_var");
    return TypedValue(context.Builder.CreateCall(DiffF, {ExprTV.Val, VarNamePtr}, "derivative"), TypeKind::Symbolic);
}

// ============================================================================
// SubstituteExprAST - Substitute values in expression
// ============================================================================
TypedValue SubstituteExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    int count = Values.size();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    llvm::Value* NamesArr =
        context.Builder.CreateAlloca(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), nullptr, "subst_names");
    llvm::Value* ValsArr =
        context.Builder.CreateAlloca(llvm::ArrayType::get(DoubleTy, std::max(1, count)), nullptr, "subst_vals");

    int i = 0;
    for (auto& [name, valExpr] : Values) {
        llvm::Value* NamePtr = context.Builder.CreateGlobalString(name);
        llvm::Value* Idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), i);
        context.Builder.CreateStore(
            NamePtr, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), NamesArr,
                                                       {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));

        TypedValue ValTV = valExpr->codegen(context);
        context.Builder.CreateStore(ValTV.Val, context.Builder.CreateInBoundsGEP(
                                                   llvm::ArrayType::get(DoubleTy, std::max(1, count)), ValsArr,
                                                   {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        i++;
    }

    llvm::Function* SubstF = TheModule->getFunction("flux_sym_substitute");
    if (!SubstF) {
        llvm::Type* Params[] = {DoubleTy, DoubleTy, VoidPtrTy, VoidPtrTy};
        SubstF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                        llvm::Function::ExternalLinkage, "flux_sym_substitute", TheModule);
    }

    llvm::Value* NamesPtr = context.Builder.CreateBitCast(NamesArr, VoidPtrTy);
    llvm::Value* ValsPtr = context.Builder.CreateBitCast(ValsArr, VoidPtrTy);

    return TypedValue(
        context.Builder.CreateCall(
            SubstF, {ExprTV.Val, llvm::ConstantFP::get(DoubleTy, (double)count), NamesPtr, ValsPtr}, "substituted"),
        TypeKind::Symbolic);
}

// ============================================================================
// EvaluateExprAST - Evaluate symbolic expression to a number
// ============================================================================
TypedValue EvaluateExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    int count = Values.size();
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    llvm::Value* NamesArr =
        context.Builder.CreateAlloca(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), nullptr, "eval_names");
    llvm::Value* ValsArr =
        context.Builder.CreateAlloca(llvm::ArrayType::get(DoubleTy, std::max(1, count)), nullptr, "eval_vals");

    int i = 0;
    for (auto& [name, valExpr] : Values) {
        llvm::Value* NamePtr = context.Builder.CreateGlobalString(name);
        llvm::Value* Idx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), i);
        context.Builder.CreateStore(
            NamePtr, context.Builder.CreateInBoundsGEP(llvm::ArrayType::get(VoidPtrTy, std::max(1, count)), NamesArr,
                                                       {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));

        TypedValue ValTV = valExpr->codegen(context);
        context.Builder.CreateStore(ValTV.Val, context.Builder.CreateInBoundsGEP(
                                                   llvm::ArrayType::get(DoubleTy, std::max(1, count)), ValsArr,
                                                   {llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0), Idx}));
        i++;
    }

    llvm::Function* EvalF = TheModule->getFunction("flux_sym_evaluate");
    if (!EvalF) {
        llvm::Type* Params[] = {DoubleTy, DoubleTy, VoidPtrTy, VoidPtrTy};
        EvalF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                       llvm::Function::ExternalLinkage, "flux_sym_evaluate", TheModule);
    }

    llvm::Value* NamesPtr = context.Builder.CreateBitCast(NamesArr, VoidPtrTy);
    llvm::Value* ValsPtr = context.Builder.CreateBitCast(ValsArr, VoidPtrTy);

    return TypedValue(
        context.Builder.CreateCall(
            EvalF, {ExprTV.Val, llvm::ConstantFP::get(DoubleTy, (double)count), NamesPtr, ValsPtr}, "eval_res"),
        TypeKind::Double);
}

// ============================================================================
// JacobianExprAST - Jacobian matrix generation
// ============================================================================
TypedValue JacobianExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);

    TypedValue ExprsTV = Expressions->codegen(context);
    TypedValue VarsTV = Variables->codegen(context);

    if (!ExprsTV.Val || !VarsTV.Val)
        return TypedValue();

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

    llvm::Function* JacF = context.TheModule->getFunction("flux_sym_jacobian");
    if (!JacF) {
        JacF =
            llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, VoidPtrTy, Int32Ty}, false),
                                   llvm::Function::ExternalLinkage, "flux_sym_jacobian", context.TheModule);
    }

    llvm::Value* ExprsData = context.Builder.CreateExtractValue(ExprsTV.Val, 0, "exprs_data");
    llvm::Value* ExprsSize = context.Builder.CreateExtractValue(ExprsTV.Val, 1, "exprs_size");

    llvm::Value* VarsData = context.Builder.CreateExtractValue(VarsTV.Val, 0, "vars_data");
    llvm::Value* VarsSize = context.Builder.CreateExtractValue(VarsTV.Val, 1, "vars_size");

    llvm::Value* JacPtr = context.Builder.CreateCall(JacF, {ExprsData, ExprsSize, VarsData, VarsSize}, "jac_ptr");

    llvm::StructType* MatStructTy =
        llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, JacPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, ExprsSize, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, VarsSize, 2);

    return TypedValue(MatVal, TypeKind::Matrix);
}

// ============================================================================
// PDEExprAST - PDE solver registration
// ============================================================================
TypedValue PDEExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);

    TypedValue EqTV = Equation->codegen(context);
    if (!EqTV.Val)
        return TypedValue();

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

    llvm::Function* PdeRegF = context.TheModule->getFunction("flux_sym_pde_register");
    if (!PdeRegF) {
        PdeRegF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, Int32Ty, VoidPtrTy}, false),
                                         llvm::Function::ExternalLinkage, "flux_sym_pde_register", context.TheModule);
    }

    std::vector<llvm::Constant*> VarNames;
    for (const auto& var : IndependentVars) {
        VarNames.push_back(context.Builder.CreateGlobalString(var));
    }

    llvm::ArrayType* ArrTy = llvm::ArrayType::get(VoidPtrTy, std::max((size_t)1, VarNames.size()));
    llvm::GlobalVariable* GV =
        new llvm::GlobalVariable(*context.TheModule, ArrTy, true, llvm::GlobalValue::InternalLinkage,
                                 llvm::ConstantArray::get(ArrTy, VarNames), "pde_vars");
    llvm::Value* NamesPtr = context.Builder.CreateBitCast(GV, VoidPtrTy);

    llvm::Value* PdeHandle = context.Builder.CreateCall(
        PdeRegF, {EqTV.Val, llvm::ConstantInt::get(Int32Ty, (int)IndependentVars.size()), NamesPtr}, "pde_handle");

    return TypedValue(PdeHandle, TypeKind::Symbolic);
}

// ============================================================================
// PartialDiffExprAST - Partial differentiation
// ============================================================================
TypedValue PartialDiffExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

    llvm::Function* PdiffF = context.TheModule->getFunction("flux_sym_pdiff");
    if (!PdiffF) {
        PdiffF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, VoidPtrTy, Int32Ty}, false),
                                        llvm::Function::ExternalLinkage, "flux_sym_pdiff", context.TheModule);
    }

    llvm::Value* VarPtr = context.Builder.CreateGlobalString(Variable);
    llvm::Value* ResPtr =
        context.Builder.CreateCall(PdiffF, {ExprTV.Val, VarPtr, llvm::ConstantInt::get(Int32Ty, Order)}, "pdiff_ptr");

    return TypedValue(ResPtr, TypeKind::Symbolic);
}

// ============================================================================
// IntegrateExprAST - Symbolic integration
// ============================================================================
TypedValue IntegrateExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    llvm::Function* IntF = TheModule->getFunction("flux_sym_integrate");
    if (!IntF) {
        IntF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx),
                                    {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_integrate", TheModule);
    }

    llvm::Value* VarNamePtr = context.Builder.CreateGlobalString(Variable, "int_var");
    return TypedValue(context.Builder.CreateCall(IntF, {ExprTV.Val, VarNamePtr}, "integrated"), TypeKind::Symbolic);
}

// ============================================================================
// LaplaceExprAST - Laplace transform
// ============================================================================
TypedValue LaplaceExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    llvm::Function* LapF = TheModule->getFunction("flux_sym_laplace");
    if (!LapF) {
        LapF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx),
                                    {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0),
                                     llvm::PointerType::get(Ctx, 0)},
                                    false),
            llvm::Function::ExternalLinkage, "flux_sym_laplace", TheModule);
    }

    llvm::Value* TVarPtr = context.Builder.CreateGlobalString(TimeVar, "lap_t");
    llvm::Value* SVarPtr = context.Builder.CreateGlobalString(SVar, "lap_s");
    return TypedValue(context.Builder.CreateCall(LapF, {ExprTV.Val, TVarPtr, SVarPtr}, "laplace_res"),
                      TypeKind::Symbolic);
}

// ============================================================================
// InverseLaplaceExprAST - Inverse Laplace transform
// ============================================================================
TypedValue InverseLaplaceExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val)
        return TypedValue();

    llvm::Function* ILapF = TheModule->getFunction("flux_sym_inverse_laplace");
    if (!ILapF) {
        ILapF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx),
                                    {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0),
                                     llvm::PointerType::get(Ctx, 0)},
                                    false),
            llvm::Function::ExternalLinkage, "flux_sym_inverse_laplace", TheModule);
    }

    llvm::Value* SVarPtr = context.Builder.CreateGlobalString(SVar, "ilap_s");
    llvm::Value* TVarPtr = context.Builder.CreateGlobalString(TimeVar, "ilap_t");
    return TypedValue(context.Builder.CreateCall(ILapF, {ExprTV.Val, SVarPtr, TVarPtr}, "inv_laplace_res"),
                      TypeKind::Symbolic);
}

// ============================================================================
// ExpandExprAST - Expand symbolic expression
// ============================================================================
TypedValue ExpandExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Function* Fn = TheModule->getFunction("flux_sym_expand");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_expand", TheModule);
    }

    return TypedValue(context.Builder.CreateCall(Fn, {ExprTV.Val}, "expanded"), TypeKind::Symbolic);
}

// ============================================================================
// FactorExprAST - Factor symbolic expression
// ============================================================================
TypedValue FactorExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Function* Fn = TheModule->getFunction("flux_sym_factor");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_factor", TheModule);
    }

    return TypedValue(context.Builder.CreateCall(Fn, {ExprTV.Val}, "factored"), TypeKind::Symbolic);
}

// ============================================================================
// CollectExprAST - Collect terms by variable
// ============================================================================
TypedValue CollectExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Function* Fn = TheModule->getFunction("flux_sym_collect");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx),
                                    {llvm::Type::getDoubleTy(Ctx), llvm::PointerType::get(Ctx, 0)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_collect", TheModule);
    }

    llvm::Value* VarPtr = context.Builder.CreateGlobalString(Variable, "collect_var");
    return TypedValue(context.Builder.CreateCall(Fn, {ExprTV.Val, VarPtr}, "collected"), TypeKind::Symbolic);
}

// ============================================================================
// NumeratorExprAST - Get numerator of rational expression
// ============================================================================
TypedValue NumeratorExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Function* Fn = TheModule->getFunction("flux_sym_numerator");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_numerator", TheModule);
    }

    return TypedValue(context.Builder.CreateCall(Fn, {ExprTV.Val}, "numerator"), TypeKind::Symbolic);
}

// ============================================================================
// DenominatorExprAST - Get denominator of rational expression
// ============================================================================
TypedValue DenominatorExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Function* Fn = TheModule->getFunction("flux_sym_denominator");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_denominator", TheModule);
    }

    return TypedValue(context.Builder.CreateCall(Fn, {ExprTV.Val}, "denominator"), TypeKind::Symbolic);
}

// ============================================================================
// PolesExprAST - Find poles of transfer function
// ============================================================================
TypedValue PolesExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    llvm::Function* Fn = TheModule->getFunction("flux_sym_poles");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(VoidPtrTy, {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_poles", TheModule);
    }

    llvm::Value* MatPtr = context.Builder.CreateCall(Fn, {ExprTV.Val}, "poles_ptr");

    llvm::Function* RowsF = TheModule->getFunction("flux_matrix_rows");
    if (!RowsF) {
        RowsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx), {VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_rows", TheModule);
    }

    llvm::Function* ColsF = TheModule->getFunction("flux_matrix_cols");
    if (!ColsF) {
        ColsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx), {VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_cols", TheModule);
    }

    llvm::Value* RetRows = context.Builder.CreateCall(RowsF, {MatPtr}, "poles_rows");
    llvm::Value* RetCols = context.Builder.CreateCall(ColsF, {MatPtr}, "poles_cols");

    llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(Ctx));
    llvm::Value* MatVal = llvm::PoisonValue::get(MatSTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, MatPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RetRows, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, RetCols, 2);

    return TypedValue(MatVal, TypeKind::Matrix);
}

TypedValue ZerosExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();

    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    llvm::Function* Fn = TheModule->getFunction("flux_sym_zeros");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(VoidPtrTy, {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_zeros", TheModule);
    }

    llvm::Value* MatPtr = context.Builder.CreateCall(Fn, {ExprTV.Val}, "zeros_ptr");

    llvm::Function* RowsF = TheModule->getFunction("flux_matrix_rows");
    if (!RowsF) {
        RowsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx), {VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_rows", TheModule);
    }

    llvm::Function* ColsF = TheModule->getFunction("flux_matrix_cols");
    if (!ColsF) {
        ColsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx), {VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_cols", TheModule);
    }

    llvm::Value* RetRows = context.Builder.CreateCall(RowsF, {MatPtr}, "zeros_rows");
    llvm::Value* RetCols = context.Builder.CreateCall(ColsF, {MatPtr}, "zeros_cols");

    llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(Ctx));
    llvm::Value* MatVal = llvm::PoisonValue::get(MatSTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, MatPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RetRows, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, RetCols, 2);

    return TypedValue(MatVal, TypeKind::Matrix);
}

} // namespace Flux
