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
#include "flux/compiler/lexer.h"
#include "flux/compiler/symbolic_ast.h"
#include "flux/runtime/units.h"
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <string>

namespace Flux {

static TypedValue promoteToSymbolic(TypedValue V, CodegenContext& context)
{
    if (V.Type.Kind == TypeKind::Symbolic)
        return V;

    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // We'll create a symbolic number: flux_sym_number(double val)
    // Actually, let's just use flux_sym_add with 0 if it's already a symbol?
    // No, better to have a dedicated constructor.

    // For now, let's assume we have flux_sym_decl but we need flux_sym_number
    llvm::Function* SymNumF = TheModule->getFunction("flux_sym_number");
    if (!SymNumF) {
        SymNumF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
            llvm::Function::ExternalLinkage, "flux_sym_number", TheModule);
    }

    llvm::Value* DoubleVal = V.Val;
    if (V.Type.Kind == TypeKind::Int) {
        DoubleVal = context.Builder.CreateSIToFP(DoubleVal, llvm::Type::getDoubleTy(Ctx), "conv");
    }

    llvm::Value* SymPtr = context.Builder.CreateCall(SymNumF, {DoubleVal}, "sym_num");
    return TypedValue(SymPtr, TypeKind::Symbolic);
}

TypedValue BinaryExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    TypedValue L = LHS->codegen(context);
    TypedValue R = RHS->codegen(context);
    if (!L.Val || !R.Val) {
        std::cerr << "[FLUX ERROR] Binary operand sub-expression failed to codegen" << std::endl;
        return TypedValue();
    }

    // 0. Handle symbolic operands
    if (L.Type.Kind == TypeKind::Symbolic || R.Type.Kind == TypeKind::Symbolic) {
        L = promoteToSymbolic(L, context);
        R = promoteToSymbolic(R, context);

        llvm::Module* TheModule = context.TheModule;
        llvm::LLVMContext& Ctx = context.TheContext;
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

        std::string fnName;
        switch (Op) {
        case '+':
            fnName = "flux_sym_add";
            break;
        case '-':
            fnName = "flux_sym_sub";
            break;
        case '*':
            fnName = "flux_sym_mul";
            break;
        case '/':
            fnName = "flux_sym_div";
            break;
        case '^':
        case static_cast<int>(TokenType::tok_power):
            fnName = "flux_sym_pow";
            break;
        case static_cast<int>(TokenType::tok_equal):
            fnName = "flux_sym_eq";
            break;
        case static_cast<int>(TokenType::tok_not_equal):
            fnName = "flux_sym_ne";
            break;
        default:
            std::cerr << "Symbolic operator not supported: " << Op << " (char: " << (char)Op << ")" << std::endl;
            return TypedValue();
        }

        llvm::Function* SymFn = TheModule->getFunction(fnName);
        if (!SymFn) {
            SymFn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                           llvm::Function::ExternalLinkage, fnName, TheModule);
        }

        return TypedValue(context.Builder.CreateCall(SymFn, {L.Val, R.Val}, "symtmp"), TypeKind::Symbolic);
    }

    // 0b. Operator overloading for user-defined types
    {
        static const std::map<int, std::string> opToMethod = {
            {'+', "add"},
            {'-', "sub"},
            {'*', "mul"},
            {'/', "div"},
            {'%', "rem"},
            {'<', "lt"},
            {'>', "gt"},
            {static_cast<int>(TokenType::tok_less_equal), "le"},
            {static_cast<int>(TokenType::tok_greater_equal), "ge"},
            {static_cast<int>(TokenType::tok_equal), "eq"},
            {static_cast<int>(TokenType::tok_not_equal), "ne"},
        };
        auto findOverload = [&](const TypedValue& lhs, const std::string& methodName,
                                const TypedValue& rhs) -> TypedValue {
            std::string typeName;
            if (lhs.Type.Kind == TypeKind::UserStruct && lhs.Type.StructTypeId >= 0 &&
                lhs.Type.StructTypeId < static_cast<int>(context.StructTypes.size())) {
                typeName = context.StructTypes[lhs.Type.StructTypeId].Name;
            } else if (lhs.Type.Kind == TypeKind::UserEnum && lhs.Type.EnumTypeId >= 0 &&
                       lhs.Type.EnumTypeId < static_cast<int>(context.EnumTypes.size())) {
                typeName = context.EnumTypes[lhs.Type.EnumTypeId].Name;
            }
            if (typeName.empty())
                return TypedValue();
            auto typeIt = context.TypeMethods.find(typeName);
            if (typeIt == context.TypeMethods.end())
                return TypedValue();
            auto methodIt = typeIt->second.find(methodName);
            if (methodIt == typeIt->second.end())
                return TypedValue();
            llvm::Function* calleeFn = methodIt->second;
            if (!calleeFn)
                return TypedValue();
            FluxType retType = context.FuncReturnTypes[calleeFn->getName().str()];
            resolveUserStructType(retType, context);
            resolveUserEnumType(retType, context);
            std::vector<llvm::Value*> callArgs;
            bool isSretCall = calleeFn->getReturnType()->isVoidTy() && calleeFn->arg_size() > 0 &&
                              calleeFn->hasParamAttribute(0, llvm::Attribute::StructRet);
            llvm::Value* sretPtr = nullptr;
            if (isSretCall) {
                llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                if (!sretLLVMTy)
                    sretLLVMTy =
                        llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "op_sret");
                callArgs.push_back(sretPtr);
            }
            {
                llvm::Value* selfVal = lhs.Val;
                if (shouldPassByPointer(lhs.Type, context)) {
                    if (!selfVal->getType()->isPointerTy()) {
                        llvm::Value* tempAlloca =
                            context.Builder.CreateAlloca(selfVal->getType(), nullptr, "op_self_temp");
                        context.Builder.CreateStore(selfVal, tempAlloca);
                        selfVal = tempAlloca;
                    }
                } else {
                    if ((lhs.Type.Kind == TypeKind::UserStruct || lhs.Type.Kind == TypeKind::UserEnum) &&
                        selfVal->getType()->isPointerTy()) {
                        llvm::Type* selfLLVMTy = lhs.Type.getLLVMType(context.TheContext);
                        selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal, "op_self_loaded");
                    }
                }
                unsigned firstArgIdx = isSretCall ? 1 : 0;
                if (calleeFn->arg_size() > firstArgIdx &&
                    selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
                    llvm::Type* firstArgTy = calleeFn->getArg(firstArgIdx)->getType();
                    if (firstArgTy->isPointerTy() && selfVal->getType()->isPointerTy())
                        selfVal = context.Builder.CreatePointerCast(selfVal, firstArgTy);
                    else if (selfVal->getType()->isIntegerTy() && firstArgTy->isFloatingPointTy())
                        selfVal = context.Builder.CreateSIToFP(selfVal, firstArgTy);
                    else if (selfVal->getType()->isFloatingPointTy() && firstArgTy->isIntegerTy())
                        selfVal = context.Builder.CreateFPToSI(selfVal, firstArgTy);
                }
                callArgs.push_back(selfVal);
            }
            {
                llvm::Value* argVal = rhs.Val;
                if (shouldPassByPointer(rhs.Type, context)) {
                    if (!argVal->getType()->isPointerTy()) {
                        llvm::Value* tempAlloca =
                            context.Builder.CreateAlloca(argVal->getType(), nullptr, "op_arg_temp");
                        context.Builder.CreateStore(argVal, tempAlloca);
                        argVal = tempAlloca;
                    }
                } else {
                    if ((rhs.Type.Kind == TypeKind::UserStruct || rhs.Type.Kind == TypeKind::UserEnum) &&
                        argVal->getType()->isPointerTy()) {
                        llvm::Type* structTy = rhs.Type.getLLVMType(context.TheContext);
                        argVal = context.Builder.CreateLoad(structTy, argVal);
                    }
                }
                size_t calleeIdx = isSretCall ? 2 : 1;
                if (calleeFn->arg_size() > calleeIdx && argVal->getType() != calleeFn->getArg(calleeIdx)->getType()) {
                    llvm::Type* expectedTy = calleeFn->getArg(calleeIdx)->getType();
                    if (expectedTy->isPointerTy() && argVal->getType()->isPointerTy())
                        argVal = context.Builder.CreatePointerCast(argVal, expectedTy);
                    else if (argVal->getType()->isIntegerTy() && expectedTy->isFloatingPointTy())
                        argVal = context.Builder.CreateSIToFP(argVal, expectedTy);
                    else if (argVal->getType()->isFloatingPointTy() && expectedTy->isIntegerTy())
                        argVal = context.Builder.CreateFPToSI(argVal, expectedTy);
                }
                callArgs.push_back(argVal);
            }
            if (isSretCall) {
                context.Builder.CreateCall(calleeFn, callArgs);
                llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                if (!sretLLVMTy)
                    sretLLVMTy =
                        llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                llvm::Value* result = context.Builder.CreateLoad(sretLLVMTy, sretPtr);
                return TypedValue(result, retType);
            } else {
                llvm::Value* result = context.Builder.CreateCall(calleeFn, callArgs, methodName);
                return TypedValue(result, retType);
            }
        };
        auto opIt = opToMethod.find(Op);
        if (opIt != opToMethod.end()) {
            TypedValue overloadResult = findOverload(L, opIt->second, R);
            if (overloadResult.Val)
                return overloadResult;
        }
    }

    // 1. Type checking for binary operations
    {
        auto isNumeric = [](TypeKind K) {
            return K == TypeKind::Double || K == TypeKind::Float || K == TypeKind::Int || K == TypeKind::Bool;
        };
        if (L.Type.Kind == TypeKind::String || R.Type.Kind == TypeKind::String) {
            if (Op == '+' && L.Type.Kind == TypeKind::String && R.Type.Kind == TypeKind::String) {
                // String concatenation — handled below
            } else if ((Op == static_cast<int>(TokenType::tok_equal) ||
                        Op == static_cast<int>(TokenType::tok_not_equal)) &&
                       L.Type.Kind == TypeKind::String && R.Type.Kind == TypeKind::String) {
                // String comparison — handled below
            } else if (Op == '-' || Op == '*' || Op == '/' || Op == '<' || Op == '>' ||
                       Op == static_cast<int>(TokenType::tok_less_equal) ||
                       Op == static_cast<int>(TokenType::tok_greater_equal)) {
                std::cerr << "Type error: cannot use string in arithmetic or comparison ('" << (char)Op << "')"
                          << std::endl;
                return TypedValue();
            }
        }
        if (L.Type.Kind == TypeKind::Fixed || R.Type.Kind == TypeKind::Fixed) {
            // Fixed-point arithmetic is handled below
        } else if (L.Type.Kind == TypeKind::Complex || R.Type.Kind == TypeKind::Complex) {
            // Complex arithmetic is handled below
        } else if (L.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::Matrix ||
                   L.Type.Kind == TypeKind::ComplexMatrix || R.Type.Kind == TypeKind::ComplexMatrix) {
            // Matrix ops handled below
        } else if (L.Type.Kind == TypeKind::UserEnum && R.Type.Kind == TypeKind::UserEnum) {
            if (Op != static_cast<int>(TokenType::tok_equal) && Op != static_cast<int>(TokenType::tok_not_equal)) {
                std::cerr << "Type error: cannot use enum in operation '" << (char)Op << "'" << std::endl;
                return TypedValue();
            }
            if (L.Type.EnumTypeId != R.Type.EnumTypeId) {
                std::cerr << "Type error: cannot compare different enum types" << std::endl;
                return TypedValue();
            }
        } else if (L.Type.Kind == TypeKind::Vector && R.Type.Kind == TypeKind::Vector) {
            if (Op != static_cast<int>(TokenType::tok_equal) && Op != static_cast<int>(TokenType::tok_not_equal)) {
                std::cerr << "Type error: cannot use vector in operation '" << (char)Op << "'" << std::endl;
                return TypedValue();
            }
        } else if (L.Type.Kind != TypeKind::String && R.Type.Kind != TypeKind::String &&
                   (!isNumeric(L.Type.Kind) || !isNumeric(R.Type.Kind))) {
            std::cerr << "Type error: invalid operand types for operator '" << (char)Op << "' ("
                      << static_cast<int>(L.Type.Kind) << " and " << static_cast<int>(R.Type.Kind) << ")" << std::endl;
            return TypedValue();
        }
    }

    // 2. Dimensional Analysis
    UnitDimensions ResDims;
    if (Op == '+' || Op == '-') {
        if (L.Type.Dimensions != R.Type.Dimensions) {
            std::cerr << "Unit mismatch error: " << L.Type.Dimensions.toString() << " and "
                      << R.Type.Dimensions.toString() << " in operation '" << (char)Op << "'" << std::endl;
            return TypedValue();
        }
        ResDims = L.Type.Dimensions;
    } else if (Op == '*') {
        ResDims = L.Type.Dimensions * R.Type.Dimensions;
    } else if (Op == '/') {
        ResDims = L.Type.Dimensions / R.Type.Dimensions;
    } else if (Op == '%') {
        if (L.Type.Dimensions != R.Type.Dimensions) {
            std::cerr << "Unit mismatch error: " << L.Type.Dimensions.toString() << " and "
                      << R.Type.Dimensions.toString() << " in modulo" << std::endl;
            return TypedValue();
        }
        ResDims = L.Type.Dimensions;
    } else if (Op == '<' || Op == '>' || Op == static_cast<int>(TokenType::tok_less_equal) ||
               Op == static_cast<int>(TokenType::tok_greater_equal)) {
        if (L.Type.Dimensions != R.Type.Dimensions) {
            std::cerr << "Unit mismatch error: " << L.Type.Dimensions.toString() << " and "
                      << R.Type.Dimensions.toString() << " in ordered comparison" << std::endl;
            return TypedValue();
        }
        ResDims = {};
    } else if (Op == static_cast<int>(TokenType::tok_equal) || Op == static_cast<int>(TokenType::tok_not_equal)) {
        if (L.Type.Dimensions != R.Type.Dimensions && !L.Type.Dimensions.isDimensionless() &&
            !R.Type.Dimensions.isDimensionless()) {
            std::cerr << "Unit mismatch error: " << L.Type.Dimensions.toString() << " and "
                      << R.Type.Dimensions.toString() << " in equality comparison" << std::endl;
            return TypedValue();
        }
        ResDims = {};
    } else {
        // Other operators: logical ops, etc.
        ResDims = {};
    }

    if (L.Type.Kind == TypeKind::Matrix || L.Type.Kind == TypeKind::ComplexMatrix || R.Type.Kind == TypeKind::Matrix ||
        R.Type.Kind == TypeKind::ComplexMatrix) {

        bool isLComplex = (L.Type.Kind == TypeKind::ComplexMatrix);
        bool isRComplex = (R.Type.Kind == TypeKind::ComplexMatrix);
        bool isScalarLComplex = (L.Type.Kind == TypeKind::Complex);
        bool isScalarRComplex = (R.Type.Kind == TypeKind::Complex);

        bool resultComplex = isLComplex || isRComplex || isScalarLComplex || isScalarRComplex;

        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

        FluxType resType = resultComplex ? FluxType(TypeKind::ComplexMatrix) : FluxType(TypeKind::Matrix);
        resType.Dimensions = ResDims;
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(resType.getLLVMType(context.TheContext));

        bool isMatMat = (L.Type.Kind == TypeKind::Matrix || L.Type.Kind == TypeKind::ComplexMatrix) &&
                        (R.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::ComplexMatrix);
        bool isMatSca = (L.Type.Kind == TypeKind::Matrix || L.Type.Kind == TypeKind::ComplexMatrix) &&
                        !(R.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::ComplexMatrix);
        bool isScaMat = !(L.Type.Kind == TypeKind::Matrix || L.Type.Kind == TypeKind::ComplexMatrix) &&
                        (R.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::ComplexMatrix);

        // Promotion to complex if needed
        if (resultComplex) {
            if (!isLComplex && (L.Type.Kind == TypeKind::Matrix)) {
                // Call flux_promote_matrix_to_complex(ptr, rows, cols)
                llvm::Function* PromF = context.TheModule->getFunction("flux_promote_matrix_to_complex");
                if (!PromF)
                    PromF = llvm::Function::Create(
                        llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, Int32Ty}, false),
                        llvm::Function::ExternalLinkage, "flux_promote_matrix_to_complex", context.TheModule);
                llvm::Value* LPtr = context.Builder.CreateExtractValue(L.Val, 0, "l_ptr");
                llvm::Value* LRows = context.Builder.CreateExtractValue(L.Val, 1, "l_rows");
                llvm::Value* LCols = context.Builder.CreateExtractValue(L.Val, 2, "l_cols");
                llvm::Value* NewPtr = context.Builder.CreateCall(PromF, {LPtr, LRows, LCols});
                L.Val = context.Builder.CreateInsertValue(L.Val, NewPtr, 0);
                L.Type.Kind = TypeKind::ComplexMatrix;
            }
            if (!isRComplex && (R.Type.Kind == TypeKind::Matrix)) {
                llvm::Function* PromF = context.TheModule->getFunction("flux_promote_matrix_to_complex");
                if (!PromF)
                    PromF = llvm::Function::Create(
                        llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, Int32Ty}, false),
                        llvm::Function::ExternalLinkage, "flux_promote_matrix_to_complex", context.TheModule);
                llvm::Value* RPtr = context.Builder.CreateExtractValue(R.Val, 0, "r_ptr");
                llvm::Value* RRows = context.Builder.CreateExtractValue(R.Val, 1, "r_rows");
                llvm::Value* RCols = context.Builder.CreateExtractValue(R.Val, 2, "r_cols");
                llvm::Value* NewPtr = context.Builder.CreateCall(PromF, {RPtr, RRows, RCols});
                R.Val = context.Builder.CreateInsertValue(R.Val, NewPtr, 0);
                R.Type.Kind = TypeKind::ComplexMatrix;
            }
        }

        llvm::Value* LPtr = isMatMat || isMatSca ? context.Builder.CreateExtractValue(L.Val, 0, "l_mat_ptr") : nullptr;
        llvm::Value* RPtr = isMatMat || isScaMat ? context.Builder.CreateExtractValue(R.Val, 0, "r_mat_ptr") : nullptr;

        std::string runtimeFunc;
        std::string prefix = resultComplex ? "flux_complex_matrix_" : "flux_matrix_";

        switch (Op) {
        case '+':
            runtimeFunc = prefix + (isMatMat ? "add" : (isMatSca ? "add_ms" : "add_ms"));
            break;
        case '-':
            runtimeFunc = prefix + (isMatMat ? "sub" : (isMatSca ? "sub_ms" : "sub_sm"));
            break;
        case '*':
            runtimeFunc = prefix + (isMatMat ? "mul" : (isMatSca ? "mul_ms" : "mul_ms"));
            break;
        case '/':
            runtimeFunc = prefix + (isMatMat ? "ew_div" : (isMatSca ? "div_ms" : "div_sm"));
            break;
        case '^':
            runtimeFunc = prefix + "pow";
            break;
        default:
            std::cerr << "Unsupported matrix operation: " << (char)Op << std::endl;
            return TypedValue();
        }

        llvm::Function* F = context.TheModule->getFunction(runtimeFunc);
        if (!F) {
            llvm::Type* ScalarTy = resultComplex ? ComplexTy : DoubleTy;
            std::vector<llvm::Type*> Params;
            if (isMatMat) {
                Params = {VoidPtrTy, VoidPtrTy};
            } else if (isMatSca) {
                if (resultComplex) {
                    Params = {VoidPtrTy, DoubleTy, DoubleTy};
                } else {
                    Params = {VoidPtrTy, ScalarTy};
                }
            } else { // isScaMat
                if (resultComplex) {
                    if (runtimeFunc.find("_sm") != std::string::npos) {
                        Params = {DoubleTy, DoubleTy, VoidPtrTy};
                    } else {
                        Params = {VoidPtrTy, DoubleTy, DoubleTy};
                    }
                } else {
                    if (runtimeFunc.find("_sm") != std::string::npos) {
                        Params = {ScalarTy, VoidPtrTy};
                    } else {
                        Params = {VoidPtrTy, ScalarTy};
                    }
                }
            }
            F = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, Params, false),
                                       llvm::Function::ExternalLinkage, runtimeFunc, context.TheModule);
        }

        llvm::Value* ResPtr = nullptr;
        if (isMatMat) {
            ResPtr = context.Builder.CreateCall(F, {LPtr, RPtr});
        } else if (isMatSca) {
            llvm::Value* ScalVal = resultComplex ? promoteToComplex(R, context).Val : R.Val;
            if (resultComplex) {
                llvm::Value* Re = context.Builder.CreateExtractElement(ScalVal, (uint64_t)0, "scal_re");
                llvm::Value* Im = context.Builder.CreateExtractElement(ScalVal, 1, "scal_im");
                ResPtr = context.Builder.CreateCall(F, {LPtr, Re, Im});
            } else {
                ResPtr = context.Builder.CreateCall(F, {LPtr, ScalVal});
            }
        } else {
            llvm::Value* ScalVal = resultComplex ? promoteToComplex(L, context).Val : L.Val;
            if (resultComplex) {
                llvm::Value* Re = context.Builder.CreateExtractElement(ScalVal, (uint64_t)0, "scal_re");
                llvm::Value* Im = context.Builder.CreateExtractElement(ScalVal, 1, "scal_im");
                if (runtimeFunc.find("_sm") != std::string::npos)
                    ResPtr = context.Builder.CreateCall(F, {Re, Im, RPtr});
                else
                    ResPtr = context.Builder.CreateCall(F, {RPtr, Re, Im});
            } else {
                if (runtimeFunc.find("_sm") != std::string::npos)
                    ResPtr = context.Builder.CreateCall(F, {ScalVal, RPtr});
                else
                    ResPtr = context.Builder.CreateCall(F, {RPtr, ScalVal});
            }
        }

        std::string rowsFn = resultComplex ? "flux_complex_matrix_rows" : "flux_matrix_rows";
        std::string colsFn = resultComplex ? "flux_complex_matrix_cols" : "flux_matrix_cols";

        llvm::Function* RowsF = context.TheModule->getFunction(rowsFn);
        if (!RowsF)
            RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                           llvm::Function::ExternalLinkage, rowsFn, context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction(colsFn);
        if (!ColsF)
            ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                           llvm::Function::ExternalLinkage, colsFn, context.TheModule);

        llvm::Value* ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
        llvm::Value* ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");

        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
        return TypedValue(MatVal, resType);
    }

    if (isComplexType(L) || isComplexType(R)) {
        L = promoteToComplex(L, context);
        R = promoteToComplex(R, context);

        // Ensure dimensions match for addition/subtraction
        if ((Op == '+' || Op == '-') && L.Type.Dimensions != R.Type.Dimensions) {
            std::cerr << "[FLUX ERROR] Dimensional mismatch: " << (char)Op << " between incompatible units."
                      << std::endl;
        }

        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);

        switch (Op) {
        case '+': {
            return TypedValue(context.Builder.CreateFAdd(L.Val, R.Val, "caddtmp"),
                              FluxType(TypeKind::Complex, L.Type.Dimensions));
        }
        case '-': {
            return TypedValue(context.Builder.CreateFSub(L.Val, R.Val, "csubtmp"),
                              FluxType(TypeKind::Complex, L.Type.Dimensions));
        }
        case '*': {
            llvm::Value* a = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "ca");
            llvm::Value* b = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "cb");
            llvm::Value* c = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "cc");
            llvm::Value* d = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "cd");

            llvm::Value* ac = context.Builder.CreateFMul(a, c, "cac");
            llvm::Value* bd = context.Builder.CreateFMul(b, d, "cbd");
            llvm::Value* ad = context.Builder.CreateFMul(a, d, "cad");
            llvm::Value* bc = context.Builder.CreateFMul(b, c, "cbc");

            llvm::Value* ResReal = context.Builder.CreateFSub(ac, bd, "cmultre");
            llvm::Value* ResImag = context.Builder.CreateFAdd(ad, bc, "cmultim");

            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1),
                              FluxType(TypeKind::Complex, L.Type.Dimensions * R.Type.Dimensions));
        }
        case '/': {
            llvm::Value* a = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "ca");
            llvm::Value* b = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "cb");
            llvm::Value* c = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "cc");
            llvm::Value* d = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "cd");

            llvm::Value* ac = context.Builder.CreateFMul(a, c, "cac");
            llvm::Value* bd = context.Builder.CreateFMul(b, d, "cbd");
            llvm::Value* bc = context.Builder.CreateFMul(b, c, "cbc");
            llvm::Value* ad = context.Builder.CreateFMul(a, d, "cad");
            llvm::Value* denom = context.Builder.CreateFAdd(context.Builder.CreateFMul(c, c),
                                                            context.Builder.CreateFMul(d, d), "cdenom");

            llvm::Value* ResReal = context.Builder.CreateFDiv(context.Builder.CreateFAdd(ac, bd), denom, "cdivre");
            llvm::Value* ResImag = context.Builder.CreateFDiv(context.Builder.CreateFSub(bc, ad), denom, "cdivim");

            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1),
                              FluxType(TypeKind::Complex, L.Type.Dimensions / R.Type.Dimensions));
        }
        case static_cast<int>(TokenType::tok_equal): {
            llvm::Value* LReal = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "lre");
            llvm::Value* LImag = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "lim");
            llvm::Value* RReal = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "rre");
            llvm::Value* RImag = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "rim");
            llvm::Value* RealEq = context.Builder.CreateFCmpOEQ(LReal, RReal, "realeq");
            llvm::Value* ImagEq = context.Builder.CreateFCmpOEQ(LImag, RImag, "imageq");
            llvm::Value* And = context.Builder.CreateAnd(RealEq, ImagEq, "compeq");
            return TypedValue(context.Builder.CreateUIToFP(And, llvm::Type::getDoubleTy(context.TheContext), "booltmp"),
                              TypeKind::Double);
        }
        case static_cast<int>(TokenType::tok_not_equal): {
            llvm::Value* LReal = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "lre");
            llvm::Value* LImag = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "lim");
            llvm::Value* RReal = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "rre");
            llvm::Value* RImag = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "rim");
            llvm::Value* RealNe = context.Builder.CreateFCmpUNE(LReal, RReal, "realne");
            llvm::Value* ImagNe = context.Builder.CreateFCmpUNE(LImag, RImag, "imagne");
            llvm::Value* Or = context.Builder.CreateOr(RealNe, ImagNe, "compne");
            return TypedValue(context.Builder.CreateUIToFP(Or, llvm::Type::getDoubleTy(context.TheContext), "booltmp"),
                              TypeKind::Double);
        }
        default:
            std::cerr << "Binary operator not supported for complex numbers" << std::endl;
            return TypedValue();
        }
    }

    // String operations (either side may be String — other side is a string handle at ABI level)
    if (L.Type.Kind == TypeKind::String || R.Type.Kind == TypeKind::String) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Value* StrL = L.Val;
        llvm::Value* StrR = R.Val;

        if (Op == '+') {
            // String concatenation via flux_string_concat
            llvm::Function* ConcatF = context.TheModule->getFunction("flux_string_concat");
            if (!ConcatF) {
                llvm::Type* Params[] = {DoubleTy, DoubleTy};
                ConcatF =
                    llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                           llvm::Function::ExternalLinkage, "flux_string_concat", context.TheModule);
            }
            return TypedValue(context.Builder.CreateCall(ConcatF, {StrL, StrR}, "strcat"), TypeKind::String);
        }
        if (Op == static_cast<int>(TokenType::tok_equal)) {
            // String comparison via flux_strcmp == 0
            llvm::Function* CmpF = context.TheModule->getFunction("flux_strcmp");
            if (!CmpF) {
                llvm::Type* Params[] = {DoubleTy, DoubleTy};
                CmpF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                              llvm::Function::ExternalLinkage, "flux_strcmp", context.TheModule);
            }
            llvm::Value* CmpRes = context.Builder.CreateCall(CmpF, {StrL, StrR}, "strcmp");
            llvm::Value* Zero = llvm::ConstantFP::get(DoubleTy, 0.0);
            llvm::Value* IsEq = context.Builder.CreateFCmpOEQ(CmpRes, Zero, "streq");
            return TypedValue(context.Builder.CreateUIToFP(IsEq, DoubleTy, "booltmp"), TypeKind::Bool);
        }
        if (Op == static_cast<int>(TokenType::tok_not_equal)) {
            llvm::Function* CmpF = context.TheModule->getFunction("flux_strcmp");
            if (!CmpF) {
                llvm::Type* Params[] = {DoubleTy, DoubleTy};
                CmpF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, Params, false),
                                              llvm::Function::ExternalLinkage, "flux_strcmp", context.TheModule);
            }
            llvm::Value* CmpRes = context.Builder.CreateCall(CmpF, {StrL, StrR}, "strcmp");
            llvm::Value* Zero = llvm::ConstantFP::get(DoubleTy, 0.0);
            llvm::Value* IsNe = context.Builder.CreateFCmpONE(CmpRes, Zero, "strne");
            return TypedValue(context.Builder.CreateUIToFP(IsNe, DoubleTy, "booltmp"), TypeKind::Bool);
        }
    }

    // Vector equality / inequality
    if (L.Type.Kind == TypeKind::Vector && R.Type.Kind == TypeKind::Vector &&
        (Op == static_cast<int>(TokenType::tok_equal) || Op == static_cast<int>(TokenType::tok_not_equal))) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Function* VecEqF = context.TheModule->getFunction("flux_vec_eq");
        if (!VecEqF) {
            llvm::FunctionType* FTy =
                llvm::FunctionType::get(DoubleTy, {VoidPtrTy, Int32Ty, VoidPtrTy, Int32Ty}, false);
            VecEqF = llvm::Function::Create(FTy, llvm::Function::ExternalLinkage, "flux_vec_eq", context.TheModule);
        }
        llvm::Value* LData = context.Builder.CreateExtractValue(L.Val, 0, "l_vec_ptr");
        llvm::Value* LLen = context.Builder.CreateExtractValue(L.Val, 1, "l_vec_len");
        llvm::Value* RData = context.Builder.CreateExtractValue(R.Val, 0, "r_vec_ptr");
        llvm::Value* RLen = context.Builder.CreateExtractValue(R.Val, 1, "r_vec_len");
        llvm::Value* IsEq = context.Builder.CreateCall(VecEqF, {LData, LLen, RData, RLen}, "veceq");
        if (Op == static_cast<int>(TokenType::tok_equal))
            return TypedValue(context.Builder.CreateUIToFP(context.Builder.CreateFCmpOEQ(
                                                               IsEq, llvm::ConstantFP::get(DoubleTy, 1.0), "veceq_cmp"),
                                                           DoubleTy, "booltmp"),
                              TypeKind::Bool);
        else
            return TypedValue(context.Builder.CreateUIToFP(context.Builder.CreateFCmpONE(
                                                               IsEq, llvm::ConstantFP::get(DoubleTy, 1.0), "vecne_cmp"),
                                                           DoubleTy, "booltmp"),
                              TypeKind::Bool);
    }

    llvm::Value* LV = L.Val;
    llvm::Value* RV = R.Val;

    auto createBoolResult = [&](llvm::Value* cmp) -> TypedValue { return TypedValue(cmp, TypeKind::Bool); };

    // Promote integer to double for mixed float/int comparisons.
    // Uses UIToFP for i1 (bool) to avoid sign-extension issues, SIToFP for wider ints.
    auto promoteIntToFP = [&](llvm::Value*& V) {
        if (V->getType()->isIntegerTy(1))
            V = context.Builder.CreateUIToFP(V, llvm::Type::getDoubleTy(context.TheContext), "bool_to_fp");
        else if (V->getType()->isIntegerTy())
            V = context.Builder.CreateSIToFP(V, llvm::Type::getDoubleTy(context.TheContext), "promote");
    };

    // Reduce any value to i1 branch condition: i1 directly, otherwise FCmpONE against 0.0
    auto boolCondition = [&](llvm::Value* V) -> llvm::Value* {
        if (V->getType()->isIntegerTy(1))
            return V;
        return context.Builder.CreateFCmpONE(V, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0),
                                             "boolcond");
    };

    // Handle Fixed-Point Arithmetic
    if (L.Type.Kind == TypeKind::Fixed && R.Type.Kind == TypeKind::Fixed) {
        llvm::Value* Res = nullptr;
        // Assume same Q format for simplicity in this pass
        switch (Op) {
        case '+':
            Res = context.Builder.CreateAdd(LV, RV, "faddtmp");
            break;
        case '-':
            Res = context.Builder.CreateSub(LV, RV, "fsubtmp");
            break;
        case '*': {
            // (L * R) >> Fract. Use double bit-width for intermediate result.
            llvm::Type* WideTy = llvm::Type::getIntNTy(context.TheContext, L.Type.Bits * 2);
            llvm::Value* WideL = context.Builder.CreateSExt(LV, WideTy);
            llvm::Value* WideR = context.Builder.CreateSExt(RV, WideTy);
            llvm::Value* WideRes = context.Builder.CreateMul(WideL, WideR);
            Res = context.Builder.CreateAShr(WideRes, L.Type.Fract);
            Res = context.Builder.CreateTrunc(Res, LV->getType());
            break;
        }
        case '/': {
            // (L << Fract) / R. Use double bit-width for intermediate result.
            llvm::Type* WideTy = llvm::Type::getIntNTy(context.TheContext, L.Type.Bits * 2);
            llvm::Value* WideL = context.Builder.CreateSExt(LV, WideTy);
            llvm::Value* ShiftedL = context.Builder.CreateShl(WideL, L.Type.Fract);
            llvm::Value* WideR = context.Builder.CreateSExt(RV, WideTy);
            llvm::Value* WideRes = context.Builder.CreateSDiv(ShiftedL, WideR);
            Res = context.Builder.CreateTrunc(WideRes, LV->getType());
            break;
        }
        case '<':
            return createBoolResult(context.Builder.CreateICmpSLT(LV, RV));
        case '>':
            return createBoolResult(context.Builder.CreateICmpSGT(LV, RV));
        case static_cast<int>(TokenType::tok_equal):
            return createBoolResult(context.Builder.CreateICmpEQ(LV, RV));
        case static_cast<int>(TokenType::tok_not_equal):
            return createBoolResult(context.Builder.CreateICmpNE(LV, RV));
        }
        if (Res)
            return TypedValue(Res, L.Type);
    }

    bool isIntOp = LV->getType() == RV->getType() && LV->getType()->isIntegerTy();

    switch (Op) {
    case '+':
        if (isIntOp)
            return TypedValue(context.Builder.CreateAdd(LV, RV, "addtmp"), FluxType(TypeKind::Int, ResDims));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return TypedValue(context.Builder.CreateFAdd(LV, RV, "addtmp"), FluxType(TypeKind::Double, ResDims));
    case '-':
        if (isIntOp)
            return TypedValue(context.Builder.CreateSub(LV, RV, "subtmp"), FluxType(TypeKind::Int, ResDims));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return TypedValue(context.Builder.CreateFSub(LV, RV, "subtmp"), FluxType(TypeKind::Double, ResDims));
    case '*':
        if (isIntOp)
            return TypedValue(context.Builder.CreateMul(LV, RV, "multmp"), FluxType(TypeKind::Int, ResDims));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return TypedValue(context.Builder.CreateFMul(LV, RV, "multmp"), FluxType(TypeKind::Double, ResDims));
    case '/':
        if (isIntOp)
            return TypedValue(context.Builder.CreateSDiv(LV, RV, "divtmp"), FluxType(TypeKind::Int, ResDims));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return TypedValue(context.Builder.CreateFDiv(LV, RV, "divtmp"), FluxType(TypeKind::Double, ResDims));
    case '%': {
        if (isIntOp)
            return TypedValue(context.Builder.CreateSRem(LV, RV, "modtmp"), FluxType(TypeKind::Int, ResDims));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Function* FmodF = context.TheModule->getFunction("fmod");
        if (!FmodF)
            FmodF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                                   {llvm::Type::getDoubleTy(context.TheContext),
                                                                    llvm::Type::getDoubleTy(context.TheContext)},
                                                                   false),
                                           llvm::Function::ExternalLinkage, "fmod", context.TheModule);
        return TypedValue(context.Builder.CreateCall(FmodF, {LV, RV}, "modtmp"), FluxType(TypeKind::Double, ResDims));
    }
    case static_cast<int>(TokenType::tok_bitwise_and): {
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "andlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "andrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAnd(LInt, RInt, "andtmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "andtmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_or): {
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "orlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "orrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateOr(LInt, RInt, "ortmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "ortmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_xor): {
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "xorlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "xorrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateXor(LInt, RInt, "xortmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "xortmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_shl): {
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shllhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shlrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateShl(LInt, RInt, "shltmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "shltmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_shr): {
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shrlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shrrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAShr(LInt, RInt, "shrtmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "shrtmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(
                llvm::Type::getDoubleTy(context.TheContext),
                {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule);
        }
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        UnitDimensions dims = L.Type.Dimensions;
        if (auto* C = llvm::dyn_cast<llvm::ConstantFP>(RV)) {
            double exp = C->getValueAPF().convertToDouble();
            if (std::abs(exp - std::round(exp)) < 1e-10) {
                int e = static_cast<int>(std::round(exp));
                dims.mass = static_cast<int8_t>(dims.mass * e);
                dims.length = static_cast<int8_t>(dims.length * e);
                dims.time = static_cast<int8_t>(dims.time * e);
                dims.current = static_cast<int8_t>(dims.current * e);
                dims.temperature = static_cast<int8_t>(dims.temperature * e);
                dims.amount = static_cast<int8_t>(dims.amount * e);
                dims.luminous = static_cast<int8_t>(dims.luminous * e);
            } else {
                dims = {};
            }
        } else {
            dims = {};
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "powtmp"), FluxType(TypeKind::Double, dims));
    }
    case '<':
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpSLT(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpOLT(LV, RV, "cmptmp"));
    case '>':
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpSGT(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpOGT(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_less_equal):
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpSLE(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpOLE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_greater_equal):
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpSGE(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpOGE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_equal):
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpEQ(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpOEQ(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_not_equal):
        if (isIntOp)
            return createBoolResult(context.Builder.CreateICmpNE(LV, RV, "cmptmp"));
        promoteIntToFP(LV);
        promoteIntToFP(RV);
        return createBoolResult(context.Builder.CreateFCmpUNE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_logical_and): {
        llvm::Value* LCmp = boolCondition(LV);
        llvm::Value* RCmp = boolCondition(RV);
        return createBoolResult(context.Builder.CreateAnd(LCmp, RCmp, "andtmp"));
    }
    case static_cast<int>(TokenType::tok_logical_or): {
        llvm::Value* LCmp = boolCondition(LV);
        llvm::Value* RCmp = boolCondition(RV);
        return createBoolResult(context.Builder.CreateOr(LCmp, RCmp, "ortmp"));
    }
    case static_cast<int>(TokenType::tok_ew_mul): {
        llvm::Function* EwMulF = context.TheModule->getFunction("flux_vector_mul_elementwise");
        if (!EwMulF)
            return TypedValue(context.Builder.CreateFMul(LV, RV, "ewmultmp"),
                              FluxType(TypeKind::Double, L.Type.Dimensions * R.Type.Dimensions));
        return TypedValue(context.Builder.CreateCall(EwMulF, {LV, RV}, "ewmultmp"),
                          FluxType(TypeKind::Double, L.Type.Dimensions * R.Type.Dimensions));
    }
    case static_cast<int>(TokenType::tok_ew_div): {
        llvm::Function* EwDivF = context.TheModule->getFunction("flux_vector_div_elementwise");
        if (!EwDivF)
            return TypedValue(context.Builder.CreateFDiv(LV, RV, "ewdivtmp"),
                              FluxType(TypeKind::Double, L.Type.Dimensions / R.Type.Dimensions));
        return TypedValue(context.Builder.CreateCall(EwDivF, {LV, RV}, "ewdivtmp"),
                          FluxType(TypeKind::Double, L.Type.Dimensions / R.Type.Dimensions));
    }
    case static_cast<int>(TokenType::tok_ew_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(
                llvm::Type::getDoubleTy(context.TheContext),
                {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule);
        }
        UnitDimensions ewDims;
        if (auto* C = llvm::dyn_cast<llvm::ConstantFP>(RV)) {
            double exp = C->getValueAPF().convertToDouble();
            if (std::abs(exp - std::round(exp)) < 1e-10) {
                int e = static_cast<int>(std::round(exp));
                ewDims.mass = static_cast<int8_t>(L.Type.Dimensions.mass * e);
                ewDims.length = static_cast<int8_t>(L.Type.Dimensions.length * e);
                ewDims.time = static_cast<int8_t>(L.Type.Dimensions.time * e);
                ewDims.current = static_cast<int8_t>(L.Type.Dimensions.current * e);
                ewDims.temperature = static_cast<int8_t>(L.Type.Dimensions.temperature * e);
                ewDims.amount = static_cast<int8_t>(L.Type.Dimensions.amount * e);
                ewDims.luminous = static_cast<int8_t>(L.Type.Dimensions.luminous * e);
            }
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "ewpowtmp"), FluxType(TypeKind::Double, ewDims));
    }
    default:
        std::cerr << "Invalid binary operator: " << Op << std::endl;
        return TypedValue();
    }
}

} // namespace Flux
