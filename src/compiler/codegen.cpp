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
#include "flux/compiler/symbolic_ast.h"
#include "flux/compiler/lexer.h"
#include "flux/runtime/units.h"
#include <llvm/IR/Verifier.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <iostream>
#include <set>
#include <string>

namespace Flux {

std::string UnitDimensions::toString() const {
    if (isDimensionless()) return "dimensionless";
    std::string s;
    if (mass != 0) s += "m^" + std::to_string(mass);
    if (length != 0) s += "l^" + std::to_string(length);
    if (time != 0) s += "t^" + std::to_string(time);
    if (current != 0) s += "i^" + std::to_string(current);
    if (temperature != 0) s += "T^" + std::to_string(temperature);
    if (amount != 0) s += "n^" + std::to_string(amount);
    if (luminous != 0) s += "J^" + std::to_string(luminous);
    return s;
}

// ... rest of the namespace ...

static const char* getPermanentString(const std::string& s) {
    static std::set<std::string> pool;
    return pool.insert(s).first->c_str();
}

// Helper to check if a type is complex
static bool isComplexType(TypedValue V) {
    return V.Type.Kind == TypeKind::Complex;
}

void emitLocation(ExprAST* ast, CodegenContext& context) {
    return; // Temporarily disabled for stability
    if (!context.DebugEnabled || !ast || !context.DebugBuilder) return;
    
    llvm::DIScope* Scope;
    if (context.LexicalBlocks.empty())
        Scope = context.DebugCompileUnit;
    else
        Scope = context.LexicalBlocks.back();
    
    context.Builder.SetCurrentDebugLocation(
        llvm::DILocation::get(context.TheContext, ast->getLine(), ast->getCol(), Scope));
}

static FluxType typeFromLLVM(llvm::Type* Ty) {
    if (Ty->isDoubleTy()) return FluxType(TypeKind::Double);
    if (Ty->isFloatTy()) return FluxType(TypeKind::Float);
    if (Ty->isIntegerTy(32)) return FluxType(TypeKind::Int);
    if (Ty->isVoidTy()) return FluxType(TypeKind::Void);
    // Complex is <2 x double>
    if (Ty->isVectorTy()) {
        auto* VT = llvm::cast<llvm::VectorType>(Ty);
        if (!VT->getElementCount().isScalable() &&
            VT->getElementCount().getKnownMinValue() == 2 &&
            VT->getElementType()->isDoubleTy())
            return FluxType(TypeKind::Complex);
    }
    if (Ty->isStructTy()) {
        auto* ST = llvm::cast<llvm::StructType>(Ty);
        if (ST->getNumElements() == 3) return FluxType(TypeKind::Matrix);
    }
    if (Ty->isPointerTy()) return FluxType(TypeKind::String);
    return FluxType(TypeKind::Double);
}

// Helper to promote double to complex <2 x double>
static TypedValue promoteToComplex(TypedValue V, CodegenContext& context) {
    if (isComplexType(V)) return V;

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);

    llvm::Value* Val = V.Val;
    if (V.Type.Kind == TypeKind::Int) {
        Val = context.Builder.CreateSIToFP(Val, DoubleTy, "inttodouble");
    } else if (V.Type.Kind == TypeKind::Float) {
        Val = context.Builder.CreateFPExt(Val, DoubleTy, "floattodouble");
    }

    // Create <double_val, 0.0> vector
    llvm::Value* ZeroVec = llvm::ConstantAggregateZero::get(ComplexTy);
    llvm::Value* ComplexVal = context.Builder.CreateInsertElement(ZeroVec, Val, (uint64_t)0, "to_complex");
    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue NumberExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    UnitDimensions dims;
    double finalVal = Val;
    if (!Unit.empty()) {
        try {
            auto scaledUnit = Units::UnitRegistry::instance().parseUnitString(Unit);
            // Copy dimensions from runtime struct to compiler struct
            dims.mass = scaledUnit.dimensions.mass;
            dims.length = scaledUnit.dimensions.length;
            dims.time = scaledUnit.dimensions.time;
            dims.current = scaledUnit.dimensions.current;
            dims.temperature = scaledUnit.dimensions.temperature;
            dims.amount = scaledUnit.dimensions.amount;
            dims.luminous = scaledUnit.dimensions.luminous;
            
            // Automatic scaling at compile-time (e.g., 1mV -> 0.001V)
            finalVal = Val * scaledUnit.scale;
        } catch (...) {
            // Silently ignore unknown units for now, or report as warning
        }
    }
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(finalVal)), FluxType(TypeKind::Double, dims));
}

TypedValue FixedExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::Type* IntTy = llvm::Type::getIntNTy(context.TheContext, Bits);
    double scaled = Val * std::pow(2.0, Fract);
    int64_t fixedVal = (int64_t)std::round(scaled);
    return TypedValue(llvm::ConstantInt::get(IntTy, fixedVal), FluxType(TypeKind::Fixed, Bits, Fract));
}

TypedValue ToFixedExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    TypedValue ValTV = Value->codegen(context);
    if (!ValTV.Val) return TypedValue();
    
    llvm::Type* IntTy = llvm::Type::getIntNTy(context.TheContext, Bits);
    llvm::Value* Res = nullptr;
    
    if (ValTV.Type.Kind == TypeKind::Fixed) {
        if (ValTV.Type.Fract != Fract) {
            llvm::Value* RawVal = ValTV.Val;
            if (ValTV.Type.Fract < Fract) {
                RawVal = context.Builder.CreateShl(RawVal, Fract - ValTV.Type.Fract);
            } else {
                RawVal = context.Builder.CreateAShr(RawVal, ValTV.Type.Fract - Fract);
            }
            Res = context.Builder.CreateIntCast(RawVal, IntTy, true);
        } else {
            Res = context.Builder.CreateIntCast(ValTV.Val, IntTy, true);
        }
    } else {
        llvm::Value* FloatVal = ValTV.Val;
        if (ValTV.Type.Kind == TypeKind::Int) {
            FloatVal = context.Builder.CreateSIToFP(ValTV.Val, llvm::Type::getDoubleTy(context.TheContext));
        }
        
        double scale = std::pow(2.0, Fract);
        llvm::Value* Scaled = context.Builder.CreateFMul(FloatVal, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(scale)));
        Res = context.Builder.CreateFPToSI(Scaled, IntTy);
    }
    
    return TypedValue(Res, FluxType(TypeKind::Fixed, Bits, Fract));
}

TypedValue ComplexExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Create <2 x double> vector: <Real, Imag>
    llvm::Constant* RealVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Real));
    llvm::Constant* ImagVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Imag));
    llvm::Constant* Elts[] = {RealVal, ImagVal};
    llvm::Value* ComplexVal = llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant*>(Elts, 2));

    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue StringExprAST::codegen(CodegenContext& context) {
    uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(Val));
    llvm::Value* AddrVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
    llvm::Value* DoubleVal = context.Builder.CreateBitCast(AddrVal, llvm::Type::getDoubleTy(context.TheContext), "strptr_double");
    return TypedValue(DoubleVal, TypeKind::String);
}
TypedValue ImportExprAST::codegen(CodegenContext& context) {
    // Import is handled at the JITEngine/ModuleLoader level
    // This codegen just marks the import as processed
    // The actual module loading happens before codegen
    
    // If we have an alias, register it in the context
    // This allows namespace::function syntax to be resolved
    if (!Alias.empty()) {
        // Store alias mapping for later resolution
        // This would be used by VariableExprAST to resolve namespaced symbols
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
}

TypedValue BlockExprAST::codegen(CodegenContext& context) {
    TypedValue LastVal(nullptr, TypeKind::Void);
    
    // Save current scope
    auto OldNamedValues = context.NamedValues;
    auto OldNamedTypes = context.NamedTypes;

    std::cerr << "[DEBUG] BlockExprAST: generating " << Statements.size() << " statements" << std::endl;

    for (size_t i = 0; i < Statements.size(); ++i) {
        if (context.Builder.GetInsertBlock()->getTerminator()) {
            std::cerr << "[DEBUG] BlockExprAST: block already terminated at stmt " << i << std::endl;
            break;
        }

        LastVal = Statements[i]->codegen(context);
        if (!LastVal.Val && LastVal.Type.Kind != TypeKind::Void) {
            std::cerr << "[DEBUG] BlockExprAST: stmt " << i << " failed" << std::endl;
            // Restore scope on failure
            context.NamedValues = OldNamedValues;
            context.NamedTypes = OldNamedTypes;
            return TypedValue(nullptr, TypeKind::Void);
        }
    }

    // Restore scope
    context.NamedValues = OldNamedValues;
    context.NamedTypes = OldNamedTypes;

    return LastVal.Val ? LastVal : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}
TypedValue VariableExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    std::string trimmedName = Name;
    trimmedName.erase(0, trimmedName.find_first_not_of(" "));
    trimmedName.erase(trimmedName.find_last_not_of(" ") + 1);
    llvm::Value* V = context.NamedValues[trimmedName];
    if (!V) {
        // Handle namespaced variables: math::pi -> try unqualified lookup
        auto sepPos = Name.rfind("::");
        if (sepPos != std::string::npos) {
            std::string unqualified = Name.substr(sepPos + 2);
            V = context.NamedValues[unqualified];
        }
    }
    if (!V) {
        std::cerr << "Unknown variable name: " << Name << std::endl;
        std::cerr << "  Available variables: ";
        for (const auto& [k, v] : context.NamedValues) {
            std::cerr << k << " ";
        }
        std::cerr << std::endl;
        return TypedValue();
    }
    if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
        llvm::Type* Ty = Alloca->getAllocatedType();
        llvm::Value* LoadedV = context.Builder.CreateLoad(Ty, Alloca, Name.c_str());
        
        FluxType FTy;
        if (context.NamedTypes.count(Name)) {
            FTy = context.NamedTypes[Name];
        } else {
            FTy = typeFromLLVM(Ty);
        }
        return TypedValue(LoadedV, FTy);
    }
    
    FluxType FTy;
    if (context.NamedTypes.count(Name)) {
        FTy = context.NamedTypes[Name];
    } else {
        FTy = typeFromLLVM(V->getType());
    }
    return TypedValue(V, FTy);
}

TypedValue MemberExprAST::codegen(CodegenContext& context) {
    if (auto* OBJ = dynamic_cast<VariableExprAST*>(Object.get())) {
        std::string full_name = OBJ->getName() + "." + MemberName;
        // Check if there is a local variable with this full name first (e.g. from an import)
        if (context.NamedValues.count(full_name)) {
            llvm::Value* V = context.NamedValues[full_name];
            if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
                return TypedValue(context.Builder.CreateLoad(Alloca->getAllocatedType(), Alloca, full_name.c_str()), typeFromLLVM(Alloca->getAllocatedType()));
            }
            return TypedValue(V, typeFromLLVM(V->getType()));
        }
        
        // Treat as SPICE parameter probe: device.param
        llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        if (!GetPF) {
            GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule);
        }
        
        uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(full_name));
        llvm::Value* addrConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
        llvm::Value* PtrDouble = context.Builder.CreateBitCast(addrConst, DoubleTy, "ptr_double");
        
        return TypedValue(context.Builder.CreateCall(GetPF, {PtrDouble}, "getp"), TypeKind::Double);
    }
    std::cerr << "Member access only supported on top-level variables currently" << std::endl;
    return TypedValue();
}

TypedValue AssignExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    std::string TargetName;
    bool isSpice = false;
    llvm::Value* Variable = nullptr;

    if (auto* IDX = dynamic_cast<IndexExprAST*>(LHS.get())) {
        // A[i,j] = val — matrix element assignment
        if (!IDX->isMatrixIndex() || !IDX->getColIndex()) {
            std::cerr << "Matrix assignment requires two indices: A[row, col] = val\n";
            return TypedValue();
        }
        auto* array = const_cast<ExprAST*>(IDX->getArray());
        auto* rowIdx = const_cast<ExprAST*>(IDX->getRowIndex());
        auto* colIdx = const_cast<ExprAST*>(IDX->getColIndex());
        if (!array || !rowIdx || !colIdx) return TypedValue();
        TypedValue ArrayTV = array->codegen(context);
        if (!ArrayTV.Val || ArrayTV.Type.Kind != TypeKind::Matrix) {
            std::cerr << "Can only assign into matrices.\n";
            return TypedValue();
        }
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Value* MatPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "mat_ptr");
        TypedValue RowTV = rowIdx->codegen(context);
        TypedValue ColTV = colIdx->codegen(context);
        if (!RowTV.Val || !ColTV.Val) return TypedValue();
        llvm::Value* RowV = RowTV.Val, *ColV = ColTV.Val;
        if (RowV->getType()->isFloatingPointTy()) RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        if (ColV->getType()->isFloatingPointTy()) ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        TypedValue NewValTV = Val->codegen(context);
        if (!NewValTV.Val) return TypedValue();
        llvm::Function* SetF = context.TheModule->getFunction("flux_matrix_set");
        if (!SetF) SetF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                {VoidPtrTy, Int32Ty, Int32Ty, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_set", context.TheModule);
        context.Builder.CreateCall(SetF, {MatPtr, RowV, ColV, NewValTV.Val});
        return NewValTV;
    }

    if (auto* VAR = dynamic_cast<VariableExprAST*>(LHS.get())) {
        TargetName = VAR->getName();
        Variable = context.NamedValues[TargetName];
    } else if (auto* MEM = dynamic_cast<MemberExprAST*>(LHS.get())) {
        if (auto* OBJ = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(MEM->getObject()))) {
            TargetName = OBJ->getName() + "." + MEM->getMemberName();
            isSpice = true;
        }
    } else if (auto* PAR = dynamic_cast<ParameterExprAST*>(LHS.get())) {
        TargetName = PAR->getParamName();
        isSpice = true;
    }

    if (isSpice) {
        llvm::Function* SetPF = context.TheModule->getFunction("flux_set_parameter");
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        if (!SetPF) {
            SetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_set_parameter", context.TheModule);
        }
        TypedValue NewValTV = Val->codegen(context);
        if (!NewValTV.Val) return TypedValue();
        
        uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(TargetName));
        llvm::Value* addrConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
        llvm::Value* PtrDouble = context.Builder.CreateBitCast(addrConst, DoubleTy, "ptr_double");
        
        return TypedValue(context.Builder.CreateCall(SetPF, {PtrDouble, NewValTV.Val}, "setp"), TypeKind::Double);
    }

    if (!Variable) {
        // CLEAN SYNTAX ENHANCEMENT: Implicit variable creation
        // If variable doesn't exist, create it in the entry block of the current function
        llvm::BasicBlock* EntryBB = context.Builder.GetInsertBlock();
        if (EntryBB) {
            llvm::Function* TheFunction = EntryBB->getParent();
            if (TheFunction) {
                llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
                Variable = TmpB.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, TargetName);
                context.NamedValues[TargetName] = Variable;
                // Store initial zero value
                context.Builder.CreateStore(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), Variable);
            }
        }
    }

    if (!Variable) {
        std::cerr << "Unknown variable name or scope error: " << TargetName << std::endl;
        return TypedValue();
    }

    if (!llvm::isa<llvm::AllocaInst>(Variable)) {
        std::cerr << "Cannot assign to read-only variable: " << TargetName << std::endl;
        return TypedValue();
    }

    auto* Alloca = llvm::cast<llvm::AllocaInst>(Variable);
    llvm::Type* VarTy = Alloca->getAllocatedType();
    FluxType VarFluxTy = typeFromLLVM(VarTy);

    TypedValue CurrentTV(context.Builder.CreateLoad(VarTy, Variable, "current"), VarFluxTy);
    TypedValue ValTV = Val->codegen(context);
    if (!ValTV.Val) return TypedValue();

    if (Op != 0) {
        if (isComplexType(CurrentTV) || isComplexType(ValTV)) {
            TypedValue L = promoteToComplex(CurrentTV, context);
            TypedValue R = promoteToComplex(ValTV, context);

            llvm::Value* LReal = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "lreal");
            llvm::Value* LImag = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "limag");
            llvm::Value* RReal = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "rreal");
            llvm::Value* RImag = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "rimag");

            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);

            switch (Op) {
            case '+': {
                llvm::Value* ResReal = context.Builder.CreateFAdd(LReal, RReal, "addre");
                llvm::Value* ResImag = context.Builder.CreateFAdd(LImag, RImag, "addim");
                llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
                Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
                Res = context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '-': {
                llvm::Value* ResReal = context.Builder.CreateFSub(LReal, RReal, "subre");
                llvm::Value* ResImag = context.Builder.CreateFSub(LImag, RImag, "subim");
                llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
                Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
                Res = context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '*': {
                llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
                llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
                llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
                llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
                llvm::Value* ResReal = context.Builder.CreateFSub(ac, bd, "multre");
                llvm::Value* ResImag = context.Builder.CreateFAdd(ad, bc, "multim");
                llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
                Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
                Res = context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            case '/': {
                llvm::Value* ac = context.Builder.CreateFMul(LReal, RReal, "ac");
                llvm::Value* bd = context.Builder.CreateFMul(LImag, RImag, "bd");
                llvm::Value* bc = context.Builder.CreateFMul(LImag, RReal, "bc");
                llvm::Value* ad = context.Builder.CreateFMul(LReal, RImag, "ad");
                llvm::Value* c2 = context.Builder.CreateFMul(RReal, RReal, "c2");
                llvm::Value* d2 = context.Builder.CreateFMul(RImag, RImag, "d2");
                llvm::Value* den = context.Builder.CreateFAdd(c2, d2, "den");
                llvm::Value* ResRealNum = context.Builder.CreateFAdd(ac, bd, "numre");
                llvm::Value* ResImagNum = context.Builder.CreateFSub(bc, ad, "numim");
                llvm::Value* ResReal = context.Builder.CreateFDiv(ResRealNum, den, "divre");
                llvm::Value* ResImag = context.Builder.CreateFDiv(ResImagNum, den, "divim");
                llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
                Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
                Res = context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1);
                ValTV = TypedValue(Res, TypeKind::Complex);
                break;
            }
            }
        } else {
            switch (Op) {
            case '+':
                ValTV = TypedValue(context.Builder.CreateFAdd(CurrentTV.Val, ValTV.Val, "addtmp"), TypeKind::Double);
                break;
            case '-':
                ValTV = TypedValue(context.Builder.CreateFSub(CurrentTV.Val, ValTV.Val, "subtmp"), TypeKind::Double);
                break;
            case '*':
                ValTV = TypedValue(context.Builder.CreateFMul(CurrentTV.Val, ValTV.Val, "multmp"), TypeKind::Double);
                break;
            case '/':
                ValTV = TypedValue(context.Builder.CreateFDiv(CurrentTV.Val, ValTV.Val, "divtmp"), TypeKind::Double);
                break;
            }
        }
    }

    if (ValTV.Type.Kind != VarFluxTy.Kind) {
        if (VarFluxTy.Kind == TypeKind::Complex) {
            ValTV = promoteToComplex(ValTV, context);
        } else if (VarFluxTy.Kind == TypeKind::Double && ValTV.Type.Kind == TypeKind::Int) {
            ValTV = TypedValue(context.Builder.CreateSIToFP(ValTV.Val, VarTy, "cast"), TypeKind::Double);
        }
    }

    context.Builder.CreateStore(ValTV.Val, Variable);
    return ValTV;
}

// Helper to promote a value to a symbolic expression handle
static TypedValue promoteToSymbolic(TypedValue V, CodegenContext& context) {
    if (V.Type.Kind == TypeKind::Symbolic) return V;

    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    // We'll create a symbolic number: flux_sym_number(double val)
    // Actually, let's just use flux_sym_add with 0 if it's already a symbol? 
    // No, better to have a dedicated constructor.
    
    // For now, let's assume we have flux_sym_decl but we need flux_sym_number
    llvm::Function* SymNumF = TheModule->getFunction("flux_sym_number");
    if (!SymNumF) {
        SymNumF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(Ctx), {llvm::Type::getDoubleTy(Ctx)}, false),
                                        llvm::Function::ExternalLinkage, "flux_sym_number", TheModule);
    }
    
    llvm::Value* DoubleVal = V.Val;
    if (V.Type.Kind == TypeKind::Int) {
        DoubleVal = context.Builder.CreateSIToFP(DoubleVal, llvm::Type::getDoubleTy(Ctx), "conv");
    }
    
    llvm::Value* SymPtr = context.Builder.CreateCall(SymNumF, {DoubleVal}, "sym_num");
    return TypedValue(SymPtr, TypeKind::Symbolic);
}

TypedValue BinaryExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    TypedValue L = LHS->codegen(context);
    TypedValue R = RHS->codegen(context);
    if (!L.Val || !R.Val) return TypedValue();

    // 0. Handle symbolic operands
    if (L.Type.Kind == TypeKind::Symbolic || R.Type.Kind == TypeKind::Symbolic) {
        L = promoteToSymbolic(L, context);
        R = promoteToSymbolic(R, context);
        
        llvm::Module* TheModule = context.TheModule;
        llvm::LLVMContext& Ctx = context.TheContext;
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        
        std::string fnName;
        switch (Op) {
            case '+': fnName = "flux_sym_add"; break;
            case '-': fnName = "flux_sym_sub"; break;
            case '*': fnName = "flux_sym_mul"; break;
            case '/': fnName = "flux_sym_div"; break;
            case '^': 
            case static_cast<int>(TokenType::tok_power):
                fnName = "flux_sym_pow"; break;
            case static_cast<int>(TokenType::tok_equal):
                fnName = "flux_sym_eq"; break;
            case static_cast<int>(TokenType::tok_not_equal):
                fnName = "flux_sym_ne"; break;
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

    // Dimensional Analysis
    UnitDimensions ResDims;
    if (Op == '+' || Op == '-') {
        if (L.Type.Dimensions != R.Type.Dimensions) {
            std::cerr << "Unit mismatch error: " << L.Type.Dimensions.toString() 
                      << " and " << R.Type.Dimensions.toString() 
                      << " in operation '" << (char)Op << "'" << std::endl;
        }
        ResDims = L.Type.Dimensions;
    } else if (Op == '*') {
        ResDims = L.Type.Dimensions * R.Type.Dimensions;
    } else if (Op == '/') {
        ResDims = L.Type.Dimensions / R.Type.Dimensions;
    } else {
        // Comparisons, logical ops, etc. usually result in dimensionless or same as LHS
        ResDims = L.Type.Dimensions;
    }

    if (L.Type.Kind == TypeKind::Matrix || L.Type.Kind == TypeKind::ComplexMatrix ||
        R.Type.Kind == TypeKind::Matrix || R.Type.Kind == TypeKind::ComplexMatrix) {
        
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
                // Call flux_promote_matrix_to_complex
                llvm::Function* PromF = context.TheModule->getFunction("flux_promote_matrix_to_complex");
                if (!PromF) PromF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_promote_matrix_to_complex", context.TheModule);
                llvm::Value* LPtr = context.Builder.CreateExtractValue(L.Val, 0);
                llvm::Value* NewPtr = context.Builder.CreateCall(PromF, {LPtr});
                L.Val = context.Builder.CreateInsertValue(L.Val, NewPtr, 0);
                L.Type.Kind = TypeKind::ComplexMatrix;
            }
            if (!isRComplex && (R.Type.Kind == TypeKind::Matrix)) {
                llvm::Function* PromF = context.TheModule->getFunction("flux_promote_matrix_to_complex");
                if (!PromF) PromF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_promote_matrix_to_complex", context.TheModule);
                llvm::Value* RPtr = context.Builder.CreateExtractValue(R.Val, 0);
                llvm::Value* NewPtr = context.Builder.CreateCall(PromF, {RPtr});
                R.Val = context.Builder.CreateInsertValue(R.Val, NewPtr, 0);
                R.Type.Kind = TypeKind::ComplexMatrix;
            }
        }

        llvm::Value* LPtr = isMatMat || isMatSca ? context.Builder.CreateExtractValue(L.Val, 0, "l_mat_ptr") : nullptr;
        llvm::Value* RPtr = isMatMat || isScaMat ? context.Builder.CreateExtractValue(R.Val, 0, "r_mat_ptr") : nullptr;
        
        std::string runtimeFunc;
        std::string prefix = resultComplex ? "flux_complex_matrix_" : "flux_matrix_";

        switch (Op) {
        case '+': runtimeFunc = prefix + (isMatMat ? "add" : (isMatSca ? "add_ms" : "add_ms")); break;
        case '-': runtimeFunc = prefix + (isMatMat ? "sub" : (isMatSca ? "sub_ms" : "sub_sm")); break;
        case '*': runtimeFunc = prefix + (isMatMat ? "mul" : (isMatSca ? "mul_ms" : "mul_ms")); break;
        case '/': runtimeFunc = prefix + (isMatMat ? "ew_div" : (isMatSca ? "div_ms" : "div_sm")); break;
        case '^': runtimeFunc = prefix + "pow"; break;
        default:
            std::cerr << "Unsupported matrix operation: " << (char)Op << std::endl;
            return TypedValue();
        }

        llvm::Function* F = context.TheModule->getFunction(runtimeFunc);
        if (!F) {
            llvm::Type* ScalarTy = resultComplex ? ComplexTy : DoubleTy;
            llvm::Type* Params[] = { VoidPtrTy, VoidPtrTy };
            if (isMatSca || isScaMat) {
                if (runtimeFunc.find("_sm") != std::string::npos) {
                    Params[0] = ScalarTy; Params[1] = VoidPtrTy;
                } else {
                    Params[0] = VoidPtrTy; Params[1] = ScalarTy;
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
            ResPtr = context.Builder.CreateCall(F, {LPtr, ScalVal});
        } else {
            llvm::Value* ScalVal = resultComplex ? promoteToComplex(L, context).Val : L.Val;
            if (runtimeFunc.find("_sm") != std::string::npos)
                ResPtr = context.Builder.CreateCall(F, {ScalVal, RPtr});
            else
                ResPtr = context.Builder.CreateCall(F, {RPtr, ScalVal});
        }

        std::string rowsFn = resultComplex ? "flux_complex_matrix_rows" : "flux_matrix_rows";
        std::string colsFn = resultComplex ? "flux_complex_matrix_cols" : "flux_matrix_cols";
        
        llvm::Function* RowsF = context.TheModule->getFunction(rowsFn);
        if (!RowsF) RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, rowsFn, context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction(colsFn);
        if (!ColsF) ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, colsFn, context.TheModule);

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
             std::cerr << "[FLUX ERROR] Dimensional mismatch: " << (char)Op << " between incompatible units." << std::endl;
        }

        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);

        switch (Op) {
        case '+': {
            return TypedValue(context.Builder.CreateFAdd(L.Val, R.Val, "caddtmp"), FluxType(TypeKind::Complex, L.Type.Dimensions));
        }
        case '-': {
            return TypedValue(context.Builder.CreateFSub(L.Val, R.Val, "csubtmp"), FluxType(TypeKind::Complex, L.Type.Dimensions));
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
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1), FluxType(TypeKind::Complex, L.Type.Dimensions * R.Type.Dimensions));
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
            llvm::Value* denom = context.Builder.CreateFAdd(context.Builder.CreateFMul(c, c), context.Builder.CreateFMul(d, d), "cdenom");

            llvm::Value* ResReal = context.Builder.CreateFDiv(context.Builder.CreateFAdd(ac, bd), denom, "cdivre");
            llvm::Value* ResImag = context.Builder.CreateFDiv(context.Builder.CreateFSub(bc, ad), denom, "cdivim");

            llvm::Value* Res = llvm::UndefValue::get(ComplexTy);
            Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1), FluxType(TypeKind::Complex, L.Type.Dimensions / R.Type.Dimensions));
        }
        case static_cast<int>(TokenType::tok_equal): {
            llvm::Value* LReal = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "lre");
            llvm::Value* LImag = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "lim");
            llvm::Value* RReal = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "rre");
            llvm::Value* RImag = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "rim");
            llvm::Value* RealEq = context.Builder.CreateFCmpOEQ(LReal, RReal, "realeq");
            llvm::Value* ImagEq = context.Builder.CreateFCmpOEQ(LImag, RImag, "imageq");
            llvm::Value* And = context.Builder.CreateAnd(RealEq, ImagEq, "compeq");
            return TypedValue(context.Builder.CreateUIToFP(And, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
        }
        case static_cast<int>(TokenType::tok_not_equal): {
            llvm::Value* LReal = context.Builder.CreateExtractElement(L.Val, (uint64_t)0, "lre");
            llvm::Value* LImag = context.Builder.CreateExtractElement(L.Val, (uint64_t)1, "lim");
            llvm::Value* RReal = context.Builder.CreateExtractElement(R.Val, (uint64_t)0, "rre");
            llvm::Value* RImag = context.Builder.CreateExtractElement(R.Val, (uint64_t)1, "rim");
            llvm::Value* RealNe = context.Builder.CreateFCmpUNE(LReal, RReal, "realne");
            llvm::Value* ImagNe = context.Builder.CreateFCmpUNE(LImag, RImag, "imagne");
            llvm::Value* Or = context.Builder.CreateOr(RealNe, ImagNe, "compne");
            return TypedValue(context.Builder.CreateUIToFP(Or, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), TypeKind::Double);
        }
        default:
            std::cerr << "Binary operator not supported for complex numbers" << std::endl;
            return TypedValue();
        }
    }

    llvm::Value* LV = L.Val;
    llvm::Value* RV = R.Val;

    auto createBoolResult = [&](llvm::Value* cmp) -> TypedValue {
        return TypedValue(context.Builder.CreateUIToFP(cmp, llvm::Type::getDoubleTy(context.TheContext), "booltmp"), FluxType(TypeKind::Double, {}));
    };

    // Handle Fixed-Point Arithmetic
    if (L.Type.Kind == TypeKind::Fixed && R.Type.Kind == TypeKind::Fixed) {
        llvm::Value* Res = nullptr;
        // Assume same Q format for simplicity in this pass
        switch (Op) {
            case '+': Res = context.Builder.CreateAdd(LV, RV, "faddtmp"); break;
            case '-': Res = context.Builder.CreateSub(LV, RV, "fsubtmp"); break;
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
            case '<': return createBoolResult(context.Builder.CreateICmpSLT(LV, RV));
            case '>': return createBoolResult(context.Builder.CreateICmpSGT(LV, RV));
            case static_cast<int>(TokenType::tok_equal): return createBoolResult(context.Builder.CreateICmpEQ(LV, RV));
            case static_cast<int>(TokenType::tok_not_equal): return createBoolResult(context.Builder.CreateICmpNE(LV, RV));
        }
        if (Res) return TypedValue(Res, L.Type);
    }

    bool isIntOp = LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy();

        switch (Op) {
        case '+':
            if (isIntOp) return TypedValue(context.Builder.CreateAdd(LV, RV, "addtmp"), FluxType(TypeKind::Int, ResDims));
            return TypedValue(context.Builder.CreateFAdd(LV, RV, "addtmp"), FluxType(TypeKind::Double, ResDims));
        case '-':
            if (isIntOp) return TypedValue(context.Builder.CreateSub(LV, RV, "subtmp"), FluxType(TypeKind::Int, ResDims));
            return TypedValue(context.Builder.CreateFSub(LV, RV, "subtmp"), FluxType(TypeKind::Double, ResDims));
        case '*':
            if (isIntOp) return TypedValue(context.Builder.CreateMul(LV, RV, "multmp"), FluxType(TypeKind::Int, ResDims));
            return TypedValue(context.Builder.CreateFMul(LV, RV, "multmp"), FluxType(TypeKind::Double, ResDims));
        case '/':
            if (isIntOp) return TypedValue(context.Builder.CreateSDiv(LV, RV, "divtmp"), FluxType(TypeKind::Int, ResDims));
            return TypedValue(context.Builder.CreateFDiv(LV, RV, "divtmp"), FluxType(TypeKind::Double, ResDims));
    case static_cast<int>(TokenType::tok_bitwise_and): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "andlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "andrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAnd(LInt, RInt, "andtmp"), llvm::Type::getDoubleTy(context.TheContext), "andtmpfp"), FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_or): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "orlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "orrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateOr(LInt, RInt, "ortmp"), llvm::Type::getDoubleTy(context.TheContext), "ortmpfp"), FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_xor): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "xorlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "xorrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateXor(LInt, RInt, "xortmp"), llvm::Type::getDoubleTy(context.TheContext), "xortmpfp"), FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_shl): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shllhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shlrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateShl(LInt, RInt, "shltmp"), llvm::Type::getDoubleTy(context.TheContext), "shltmpfp"), FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_bitwise_shr): {
        llvm::Value* LInt = context.Builder.CreateFPToSI(LV, llvm::Type::getInt64Ty(context.TheContext), "shrlhsint");
        llvm::Value* RInt = context.Builder.CreateFPToSI(RV, llvm::Type::getInt64Ty(context.TheContext), "shrrhsint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateAShr(LInt, RInt, "shrtmp"), llvm::Type::getDoubleTy(context.TheContext), "shrtmpfp"), FluxType(TypeKind::Double, {}));
    }
    case static_cast<int>(TokenType::tok_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule);
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "powtmp"), FluxType(TypeKind::Double, L.Type.Dimensions));
    }
    case '<':
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpSLT(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpOLT(LV, RV, "cmptmp"));
    case '>':
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpSGT(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpOGT(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_less_equal):
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpSLE(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpOLE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_greater_equal):
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpSGE(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpOGE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_equal):
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpEQ(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpOEQ(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_not_equal):
        if (LV->getType()->isIntegerTy() && RV->getType()->isIntegerTy())
            return createBoolResult(context.Builder.CreateICmpNE(LV, RV, "cmptmp"));
        if (LV->getType()->isIntegerTy()) LV = context.Builder.CreateSIToFP(LV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        if (RV->getType()->isIntegerTy()) RV = context.Builder.CreateSIToFP(RV, llvm::Type::getDoubleTy(context.TheContext), "promote");
        return createBoolResult(context.Builder.CreateFCmpUNE(LV, RV, "cmptmp"));
    case static_cast<int>(TokenType::tok_logical_and): {
        llvm::Value* LCmp = context.Builder.CreateFCmpONE(LV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andlhs");
        llvm::Value* RCmp = context.Builder.CreateFCmpONE(RV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "andrhs");
        return createBoolResult(context.Builder.CreateAnd(LCmp, RCmp, "andtmp"));
    }
    case static_cast<int>(TokenType::tok_logical_or): {
        llvm::Value* LCmp = context.Builder.CreateFCmpONE(LV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orlhs");
        llvm::Value* RCmp = context.Builder.CreateFCmpONE(RV, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "orrhs");
        return createBoolResult(context.Builder.CreateOr(LCmp, RCmp, "ortmp"));
    }
    case static_cast<int>(TokenType::tok_ew_mul): {
        llvm::Function* EwMulF = context.TheModule->getFunction("flux_vector_mul_elementwise");
        if (!EwMulF) return TypedValue(context.Builder.CreateFMul(LV, RV, "ewmultmp"), FluxType(TypeKind::Double, L.Type.Dimensions * R.Type.Dimensions));
        return TypedValue(context.Builder.CreateCall(EwMulF, {LV, RV}, "ewmultmp"), FluxType(TypeKind::Double, L.Type.Dimensions * R.Type.Dimensions));
    }
    case static_cast<int>(TokenType::tok_ew_div): {
        llvm::Function* EwDivF = context.TheModule->getFunction("flux_vector_div_elementwise");
        if (!EwDivF) return TypedValue(context.Builder.CreateFDiv(LV, RV, "ewdivtmp"), FluxType(TypeKind::Double, L.Type.Dimensions / R.Type.Dimensions));
        return TypedValue(context.Builder.CreateCall(EwDivF, {LV, RV}, "ewdivtmp"), FluxType(TypeKind::Double, L.Type.Dimensions / R.Type.Dimensions));
    }
    case static_cast<int>(TokenType::tok_ew_power): {
        llvm::Function* PowF = context.TheModule->getFunction("llvm.pow.f64");
        if (!PowF) {
            llvm::FunctionType* PowFTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false);
            PowF = llvm::Function::Create(PowFTy, llvm::Function::ExternalLinkage, "llvm.pow.f64", context.TheModule);
        }
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "ewpowtmp"), FluxType(TypeKind::Double, L.Type.Dimensions));
    }
    default:
        std::cerr << "Invalid binary operator: " << Op << std::endl;
        return TypedValue();
    }
}

TypedValue UnaryExprAST::codegen(CodegenContext& context) {
    TypedValue OperandTV = Operand->codegen(context);
    if (!OperandTV.Val) return TypedValue();

    if (isComplexType(OperandTV)) {
        llvm::Value* Real = context.Builder.CreateExtractElement(OperandTV.Val, (uint64_t)0, "cre");
        llvm::Value* Imag = context.Builder.CreateExtractElement(OperandTV.Val, (uint64_t)1, "cim");
        switch (Op) {
        case '-': {
            llvm::Value* ResReal = context.Builder.CreateFNeg(Real, "negre");
            llvm::Value* ResImag = context.Builder.CreateFNeg(Imag, "negim");
            llvm::Value* Res = llvm::UndefValue::get(
                llvm::VectorType::get(llvm::Type::getDoubleTy(context.TheContext), 2, false));
            Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1), FluxType(TypeKind::Complex, OperandTV.Type.Dimensions));
        }
        case '+': return OperandTV;
        default: return TypedValue();
        }
    }

    llvm::Value* V = OperandTV.Val;
    switch (Op) {
    case '-':
        if (V->getType()->isIntegerTy()) return TypedValue(context.Builder.CreateNeg(V, "unaryminus"), OperandTV.Type);
        return TypedValue(context.Builder.CreateFNeg(V, "unaryminus"), OperandTV.Type);
    case '+': return OperandTV;
    case '~':
    case static_cast<int>(TokenType::tok_bitwise_not): {
        llvm::Value* OperandInt = context.Builder.CreateFPToSI(V, llvm::Type::getInt64Ty(context.TheContext), "notint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateNot(OperandInt, "nottmp"), llvm::Type::getDoubleTy(context.TheContext), "nottmpfp"), FluxType(TypeKind::Double, {}));
    }
    case '!':
    case static_cast<int>(TokenType::tok_logical_not): {
        llvm::Value* IsNonZero = context.Builder.CreateFCmpONE(V, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "isnonzero");
        return TypedValue(context.Builder.CreateUIToFP(context.Builder.CreateNot(IsNonZero, "lognot"), llvm::Type::getDoubleTy(context.TheContext), "booltmp"), FluxType(TypeKind::Double, {}));
    }
    default: return TypedValue();
    }
}

TypedValue TransposeExprAST::codegen(CodegenContext& context) {
    TypedValue V = Operand->codegen(context);
    if (!V.Val) return TypedValue();
    
    if (V.Type.Kind == TypeKind::Matrix) {
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Function* TransposeF = context.TheModule->getFunction("flux_matrix_transpose");
        if (!TransposeF) TransposeF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_transpose", context.TheModule);
        
        llvm::Value* MatPtr = context.Builder.CreateExtractValue(V.Val, 0, "mat_ptr");
        llvm::Value* Rows = context.Builder.CreateExtractValue(V.Val, 1, "mat_rows");
        llvm::Value* Cols = context.Builder.CreateExtractValue(V.Val, 2, "mat_cols");
        
        llvm::Value* ResPtr = context.Builder.CreateCall(TransposeF, {MatPtr}, "transtmp");
        
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, Cols, 1); // Swapped rows/cols
        MatVal = context.Builder.CreateInsertValue(MatVal, Rows, 2);
        
        return TypedValue(MatVal, TypeKind::Matrix);
    }
    
    // For scalars, transpose is just identity (or conjugate transpose for complex, but leaving it as is for now)
    return V;
}

TypedValue CallExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    std::string Name = Callee;
    auto sepPos = Name.rfind("::");
    if (sepPos != std::string::npos) {
        Name = Name.substr(sepPos + 2);
    }

    // 1. Handle built-ins that depend on argument types (Complex, Matrix, etc.)
    if (Name == "print" || Name == "println") {
        for (auto& Expr : Args) {
            TypedValue Arg = Expr->codegen(context);
            if (!Arg.Val) continue;

            llvm::Function* PrintFn = nullptr;
            std::vector<llvm::Value*> PrintArgs;

            if (Arg.Type.Kind == TypeKind::Matrix || Arg.Type.Kind == TypeKind::ComplexMatrix) {
                std::string fnName = (Arg.Type.Kind == TypeKind::ComplexMatrix) ? "print_complex_matrix" : "print_matrix";
                PrintFn = context.TheModule->getFunction(fnName);
                if (!PrintFn) PrintFn = llvm::Function::Create(
                    llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::PointerType::get(context.TheContext, 0)}, false),
                    llvm::Function::ExternalLinkage, fnName, context.TheModule);
                PrintArgs.push_back(context.Builder.CreateExtractValue(Arg.Val, 0));
            } else if (Arg.Type.Kind == TypeKind::String) {
                PrintFn = context.TheModule->getFunction("flux_print_string");
                if (!PrintFn) PrintFn = llvm::Function::Create(
                    llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::PointerType::get(context.TheContext, 0)}, false),
                    llvm::Function::ExternalLinkage, "flux_print_string", context.TheModule);
                
                // Convert double-bitcasted-pointer back to pointer
                llvm::Value* DoubleVal = Arg.Val;
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                llvm::Value* IntVal = context.Builder.CreateBitCast(DoubleVal, Int64Ty);
                PrintArgs.push_back(context.Builder.CreateIntToPtr(IntVal, llvm::PointerType::get(context.TheContext, 0)));
            } else {
                // Default: Print as double (handles Fixed-point too via previous conversion logic)
                PrintFn = context.TheModule->getFunction("flux_print_double");
                if (!PrintFn) PrintFn = llvm::Function::Create(
                    llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext)}, false),
                    llvm::Function::ExternalLinkage, "flux_print_double", context.TheModule);
                
                llvm::Value* Val = Arg.Val;
                if (Arg.Type.Kind == TypeKind::Fixed) {
                    double scale = std::pow(2.0, -Arg.Type.Fract);
                    llvm::Value* FloatVal = context.Builder.CreateSIToFP(Val, llvm::Type::getDoubleTy(context.TheContext));
                    Val = context.Builder.CreateFMul(FloatVal, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(scale)));
                }
                PrintArgs.push_back(Val);
            }

            if (PrintFn) context.Builder.CreateCall(PrintFn, PrintArgs);
        }
        if (Name == "println") {
            llvm::Function* PrintStr = context.TheModule->getFunction("flux_print_string");
            if (PrintStr) {
                llvm::Value* NewLine = context.Builder.CreateGlobalString("\n");
                context.Builder.CreateCall(PrintStr, {NewLine});
            }
        }
        return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
    }

    if (Args.size() == 1) {
        TypedValue Arg = Args[0]->codegen(context);
        if (!Arg.Val) {
            return TypedValue();
        }
        
        if (isComplexType(Arg)) {
            llvm::Value* Real = context.Builder.CreateExtractElement(Arg.Val, (uint64_t)0, "cre");
            llvm::Value* Imag = context.Builder.CreateExtractElement(Arg.Val, (uint64_t)1, "cim");

            if (Name == "real") return TypedValue(Real, TypeKind::Double);
            if (Name == "imag") return TypedValue(Imag, TypeKind::Double);
            if (Name == "conj") {
                llvm::Value* NegImag = context.Builder.CreateFNeg(Imag, "negimag");
                llvm::Value* Vec = context.Builder.CreateInsertElement(
                    llvm::PoisonValue::get(llvm::VectorType::get(
                        llvm::Type::getDoubleTy(context.TheContext), 2, false)),
                    Real, (uint64_t)0);
                return TypedValue(context.Builder.CreateInsertElement(Vec, NegImag, (uint64_t)1), TypeKind::Complex);
            }
            if (Name == "abs" || Name == "mag") {
                llvm::Value* Re2 = context.Builder.CreateFMul(Real, Real, "re2");
                llvm::Value* Im2 = context.Builder.CreateFMul(Imag, Imag, "im2");
                llvm::Value* Sum = context.Builder.CreateFAdd(Re2, Im2, "sum2");
                llvm::Function* SqrtF = context.TheModule->getFunction("llvm.sqrt.f64");
                if (!SqrtF) SqrtF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "llvm.sqrt.f64", context.TheModule);
                return TypedValue(context.Builder.CreateCall(SqrtF, {Sum}, "abs"), TypeKind::Double);
            }
            if (Name == "arg" || Name == "phase") {
                llvm::Function* Atan2F = context.TheModule->getFunction("atan2");
                if (!Atan2F) Atan2F = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false), llvm::Function::ExternalLinkage, "atan2", context.TheModule);
                return TypedValue(context.Builder.CreateCall(Atan2F, {Imag, Real}, "arg"), TypeKind::Double);
            }
        } else if (Arg.Type.Kind == TypeKind::Matrix) {
            llvm::Value* MatPtr = context.Builder.CreateExtractValue(Arg.Val, 0, "mat_ptr");
            llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

            if (Name == "det") {
                llvm::Function* DetF = context.TheModule->getFunction("flux_matrix_det");
                if (!DetF) DetF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_det", context.TheModule);
                return TypedValue(context.Builder.CreateCall(DetF, {MatPtr}, "det"), TypeKind::Double);
            }
            if (Name == "rows") {
                llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
                if (!RowsF) RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
                llvm::Value* RowsVal = context.Builder.CreateCall(RowsF, {MatPtr}, "rows");
                return TypedValue(context.Builder.CreateSIToFP(RowsVal, DoubleTy, "rows_fp"), TypeKind::Double);
            }
            if (Name == "cols") {
                llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
                if (!ColsF) ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
                llvm::Value* ColsVal = context.Builder.CreateCall(ColsF, {MatPtr}, "cols");
                return TypedValue(context.Builder.CreateSIToFP(ColsVal, DoubleTy, "cols_fp"), TypeKind::Double);
            }
            if (Name == "inv" || Name == "eig") {
                std::string fnName = (Name == "inv") ? "flux_matrix_inv" : "flux_matrix_eig";
                llvm::Function* Fn = context.TheModule->getFunction(fnName);
                if (!Fn) Fn = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, fnName, context.TheModule);
                llvm::Value* ResPtr = context.Builder.CreateCall(Fn, {MatPtr}, Name);
                
                llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
                if (!RowsF) RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
                llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
                if (!ColsF) ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false), llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);

                llvm::Value* ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
                llvm::Value* ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");

                llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
                return TypedValue(MatVal, TypeKind::Matrix);
            }
        } else if (Arg.Type.Kind == TypeKind::Double) {
            if (Name == "abs") {
                llvm::Function* FabsF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::fabs,
                    {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FabsF, {Arg.Val}, "abstmp"), TypeKind::Double);
            }
            if (Name == "floor") {
                llvm::Function* FloorF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::floor,
                    {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FloorF, {Arg.Val}, "floortmp"), TypeKind::Double);
            }
            if (Name == "ceil") {
                llvm::Function* CeilF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::ceil,
                    {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(CeilF, {Arg.Val}, "ceiltmp"), TypeKind::Double);
            }
            if (Name == "round") {
                llvm::Function* RoundF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::round,
                    {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(RoundF, {Arg.Val}, "roundtmp"), TypeKind::Double);
            }
        }
    }

    // 2. Handle non-argument built-ins (pi, e, etc. would go here if not externs)

    // 3. Regular function lookup
    llvm::Function* CalleeF = context.TheModule->getFunction(Callee);
    if (!CalleeF && sepPos != std::string::npos) {
        std::string unqualified = Callee.substr(sepPos + 2);
        CalleeF = context.TheModule->getFunction(unqualified);
    }
    if (!CalleeF) {
        llvm::Value* VarVal = context.NamedValues[Callee];
        if (!VarVal && sepPos != std::string::npos) {
             std::string unqualified = Callee.substr(sepPos + 2);
             VarVal = context.NamedValues[unqualified];
        }
        
        if (VarVal) {
            llvm::Type* VarTy = nullptr;
            if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(VarVal)) {
                VarTy = Alloca->getAllocatedType();
            } else {
                VarTy = VarVal->getType();
            }
            
            FluxType FTy = typeFromLLVM(VarTy);
            if (FTy.Kind == TypeKind::Matrix) {
                if (Args.size() != 1 && Args.size() != 2) {
                    std::cerr << "Matrix indexing requires 1 or 2 arguments." << std::endl;
                    return TypedValue();
                }
                
                llvm::Value* MatV = nullptr;
                if (llvm::isa<llvm::AllocaInst>(VarVal)) {
                    MatV = context.Builder.CreateLoad(VarTy, VarVal, Callee.c_str());
                } else {
                    MatV = VarVal;
                }
                
                llvm::Value* MatPtr = context.Builder.CreateExtractValue(MatV, 0, "mat_ptr");
                
                TypedValue RowTV = Args[0]->codegen(context);
                if (!RowTV.Val) return TypedValue();
                llvm::Value* RowV = RowTV.Val;
                llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
                
                if (RowV->getType()->isFloatingPointTy()) RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
                else if (RowV->getType()->isIntegerTy(64)) RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");
                
                llvm::Value* ColV = llvm::ConstantInt::get(Int32Ty, 0); // Default to col 0 for vector
                if (Args.size() == 2) {
                    TypedValue ColTV = Args[1]->codegen(context);
                    if (!ColTV.Val) return TypedValue();
                    ColV = ColTV.Val;
                    if (ColV->getType()->isFloatingPointTy()) ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
                    else if (ColV->getType()->isIntegerTy(64)) ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");
                }
                
                llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
                llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
                llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
                if (!GetElemF) GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule);
                
                return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), TypeKind::Double);
            }
        }
        
        // Check for registered extern function types first
        auto extIt = context.ExternFuncTypes.find(Callee);
        if (extIt != context.ExternFuncTypes.end()) {
            const auto& retType = extIt->second.first;
            const auto& argTypes = extIt->second.second;
            llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
            // Matrix returns: LLVM function returns void*, codegen wraps to {ptr,i32,i32}
            llvm::Type* RetLTy = (retType.Kind == TypeKind::Matrix) ? VoidPtrTy : (
                (retType.Kind == TypeKind::Void) ? llvm::Type::getVoidTy(context.TheContext) :
                retType.getLLVMType(context.TheContext));
            std::vector<llvm::Type*> ArgLTys;
            for (const auto& at : argTypes) {
                if (at.Kind == TypeKind::Matrix)
                    ArgLTys.push_back(VoidPtrTy);
                else
                    ArgLTys.push_back(at.getLLVMType(context.TheContext));
            }
            llvm::FunctionType* FT = llvm::FunctionType::get(RetLTy, ArgLTys, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Callee, context.TheModule);
        } else {
            // Automatically declare as extern double(double, ...) if not found
            std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(context.TheContext));
            llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), Doubles, false);
            CalleeF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Callee, context.TheModule);
        }
    }

    std::vector<llvm::Value*> ArgsV;
    if (CalleeF->arg_size() != Args.size()) {
        // Check if it's a generator (expects one extra hidden argument)
        if (CalleeF->arg_size() == Args.size() + 1 && 
            CalleeF->getArg(0)->getType()->isPointerTy() && 
            CalleeF->getArg(0)->getName() == "__gen_state") {
            
            // Regular call to generator: allocate temp state on stack
            llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
            std::vector<llvm::Type*> StateTypes = { Int32Ty };
            llvm::StructType* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            llvm::Value* StatePtr = context.Builder.CreateAlloca(StateTy, nullptr, "gen_temp_state");
            context.Builder.CreateStore(llvm::ConstantInt::get(Int32Ty, 0), 
                context.Builder.CreateStructGEP(StateTy, StatePtr, 0));
            
            ArgsV.push_back(StatePtr);
        } else {
            return TypedValue();
        }
    }

    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        TypedValue ArgTV = Args[i]->codegen(context);
        if (!ArgTV.Val) return TypedValue();
        llvm::Value* ArgV = ArgTV.Val;

        // If extern function expects void* but arg is a matrix struct, extract pointer
        llvm::Type* ArgExpectedTy = (i < (unsigned)CalleeF->arg_size()) ? CalleeF->getArg(i)->getType() : nullptr;
        if (ArgExpectedTy && ArgExpectedTy->isPointerTy() && ArgTV.Type.Kind == TypeKind::Matrix) {
            ArgV = context.Builder.CreateExtractValue(ArgV, 0, "mat_ptr");
        }

        // Ensure string arguments are bitcasted to double for the new ABI
        if (ArgTV.Type.Kind == TypeKind::String && ArgV->getType()->isPointerTy()) {
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
            ArgV = context.Builder.CreatePtrToInt(ArgV, Int64Ty, "strptr_int");
            ArgV = context.Builder.CreateBitCast(ArgV, llvm::Type::getDoubleTy(context.TheContext), "strptr_double");
        }

        // Automatic conversion from Fixed-Point to Double for standard functions (like print)
        if (ArgTV.Type.Kind == TypeKind::Fixed) {
            double scale = std::pow(2.0, -ArgTV.Type.Fract);
            llvm::Value* FloatVal = context.Builder.CreateSIToFP(ArgV, llvm::Type::getDoubleTy(context.TheContext));
            ArgV = context.Builder.CreateFMul(FloatVal, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(scale)));
        }

        llvm::Type* ExpectedTy = CalleeF->getArg(ArgsV.size())->getType();
        if (ArgV->getType() != ExpectedTy) {
            if (ExpectedTy->isIntegerTy() && ArgV->getType()->isFloatingPointTy()) {
                ArgV = context.Builder.CreateFPToSI(ArgV, ExpectedTy, "cast");
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isIntegerTy()) {
                ArgV = context.Builder.CreateSIToFP(ArgV, ExpectedTy, "cast");
            } else if (ExpectedTy->isVectorTy() && !ArgV->getType()->isVectorTy()) {
                // Check if expected vector is <2 x double> (Complex)
                auto* VT = llvm::dyn_cast<llvm::VectorType>(ExpectedTy);
                if (VT && !VT->getElementCount().isScalable() && 
                    VT->getElementCount().getKnownMinValue() == 2 &&
                    VT->getElementType()->isDoubleTy()) {
                    ArgV = promoteToComplex(ArgTV, context).Val;
                }
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isPointerTy()) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                ArgV = context.Builder.CreatePtrToInt(ArgV, Int64Ty, "ptr_to_int");
                ArgV = context.Builder.CreateBitCast(ArgV, ExpectedTy, "int_to_fp");
            }
            else if (ExpectedTy->isPointerTy() && ArgV->getType()->isFloatingPointTy() && ArgTV.Type.Kind == TypeKind::String) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                ArgV = context.Builder.CreateBitCast(ArgV, Int64Ty, "double_to_int");
                ArgV = context.Builder.CreateIntToPtr(ArgV, ExpectedTy, "int_to_ptr");
            }
        }
        ArgsV.push_back(ArgV);
    }
    auto extRetIt = context.ExternFuncTypes.find(Callee);
    if (extRetIt != context.ExternFuncTypes.end() && extRetIt->second.first.Kind == TypeKind::Matrix) {
        llvm::Value* RetPtr = context.Builder.CreateCall(CalleeF, ArgsV, "mat_ret");
        llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
        if (!RowsF) RowsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                {llvm::PointerType::get(context.TheContext, 0)}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
        if (!ColsF) ColsF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                {llvm::PointerType::get(context.TheContext, 0)}, false),
            llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
        llvm::Value* RetRows = context.Builder.CreateCall(RowsF, {RetPtr}, "mat_rows");
        llvm::Value* RetCols = context.Builder.CreateCall(ColsF, {RetPtr}, "mat_cols");
        llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(
            FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        llvm::Value* MatVal = llvm::PoisonValue::get(MatSTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetRows, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetCols, 2);
        return TypedValue(MatVal, TypeKind::Matrix);
    }
    if (extRetIt != context.ExternFuncTypes.end() && extRetIt->second.first.Kind == TypeKind::Void) {
        context.Builder.CreateCall(CalleeF, ArgsV);
        return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
    }
    return TypedValue(context.Builder.CreateCall(CalleeF, ArgsV, "calltmp"), typeFromLLVM(CalleeF->getReturnType()));
}


TypedValue IfExprAST::codegen(CodegenContext& context) {
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();
    
    llvm::Value* CondV = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "ifcond");
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(context.TheContext, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(context.TheContext, "else", TheFunction);
    llvm::BasicBlock* MergeBB = nullptr;
    
    context.Builder.CreateCondBr(CondV, ThenBB, ElseBB);
    
    // Generate Then block
    context.Builder.SetInsertPoint(ThenBB);
    TypedValue ThenTV = Then->codegen(context);
    if (!ThenTV.Val && ThenTV.Type.Kind != TypeKind::Void) return TypedValue();
    
    bool thenTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    ThenBB = context.Builder.GetInsertBlock();
    
    // Generate Else block
    context.Builder.SetInsertPoint(ElseBB);
    TypedValue ElseTV = Else->codegen(context);
    if (!ElseTV.Val && ElseTV.Type.Kind != TypeKind::Void) return TypedValue();
    
    bool elseTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    ElseBB = context.Builder.GetInsertBlock();
    
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
             if (TV->getType() != PhiTy) TV = context.Builder.CreateSIToFP(TV, PhiTy, "cast_then");
             PN->addIncoming(TV, ThenBB);
        }
        if (!elseTerminated) {
             llvm::Value* EV = ElseTV.Val;
             if (EV->getType() != PhiTy) EV = context.Builder.CreateSIToFP(EV, PhiTy, "cast_else");
             PN->addIncoming(EV, ElseBB);
        }
        return TypedValue(PN, ThenTV.Type);
    }

    // Both paths are terminated
    return ThenTV;
}

TypedValue ForExprAST::codegen(CodegenContext& context) {
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV = Step ? Step->codegen(context) : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
    if (!StartTV.Val || !EndTV.Val || !StepTV.Val) return TypedValue();
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* PreheaderBB = context.Builder.GetInsertBlock();
    llvm::BasicBlock* LoopBB = llvm::BasicBlock::Create(context.TheContext, "loop", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterloop");
    
    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = LoopBB;  // Continue jumps to loop condition
    
    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    llvm::PHINode* Variable = context.Builder.CreatePHI(llvm::Type::getDoubleTy(context.TheContext), 2, VarName.c_str());
    Variable->addIncoming(StartTV.Val, PreheaderBB);
    llvm::Value* OldVal = context.NamedValues[VarName];
    context.NamedValues[VarName] = Variable;
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    llvm::Value* NextVar = context.Builder.CreateFAdd(Variable, StepTV.Val, "nextvar");
    Variable->addIncoming(NextVar, context.Builder.GetInsertBlock());
    context.NamedValues[VarName] = OldVal;
    llvm::Value* EndCond = context.Builder.CreateFCmpOLT(NextVar, EndTV.Val, "loopcond");
    context.Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

TypedValue WhileExprAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(context.TheContext, "whilecond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(context.TheContext, "whilebody");
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(context.TheContext, "afterwhile");
    
    // Set up break/continue targets
    llvm::BasicBlock* OldLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* OldLoopCont = context.CurrentLoopCont;
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = CondBB;  // Continue jumps to condition
    
    context.Builder.CreateBr(CondBB);
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();
    llvm::Value* CondV = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), "whilecond");
    context.Builder.CreateCondBr(CondV, BodyBB, AfterBB);
    TheFunction->insert(TheFunction->end(), BodyBB);
    context.Builder.SetInsertPoint(BodyBB);
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    context.Builder.CreateBr(CondBB);
    TheFunction->insert(TheFunction->end(), AfterBB);
    context.Builder.SetInsertPoint(AfterBB);
    
    // Restore break/continue targets
    context.CurrentLoopEnd = OldLoopEnd;
    context.CurrentLoopCont = OldLoopCont;
    
    return BodyTV;
}

TypedValue LetExprAST::codegen(CodegenContext& context) {
    TypedValue InitTV = Init->codegen(context);
    if (!InitTV.Val) return TypedValue();

    FluxType ActualType = (Type.Kind == TypeKind::Auto) ? InitTV.Type : Type;
    llvm::Type* VarTy = ActualType.getLLVMType(context.TheContext);
    llvm::Value* InitV = InitTV.Val;
    if (InitV->getType() != VarTy) {
        if (VarTy->isIntegerTy() && InitV->getType()->isFloatingPointTy()) InitV = context.Builder.CreateFPToSI(InitV, VarTy, "cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isIntegerTy()) InitV = context.Builder.CreateSIToFP(InitV, VarTy, "cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isVectorTy()) {
            // Declared as double but init is complex vector  use vector type
            VarTy = InitV->getType();
        }
        else if ((VarTy->isStructTy() || VarTy->isVectorTy()) && InitV->getType() != VarTy) {
            // Complex type: use the actual init value type
            VarTy = InitV->getType();
        }
    }
    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(VarTy, nullptr, VarName.c_str());
    context.Builder.CreateStore(InitV, Alloca);
    llvm::Value* OldVal = context.NamedValues[VarName];
    FluxType OldType = context.NamedTypes[VarName];
    context.NamedValues[VarName] = Alloca;
    context.NamedTypes[VarName] = ActualType;
    if (!Body) return InitTV;
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) return TypedValue();
    context.NamedValues[VarName] = OldVal;
    context.NamedTypes[VarName] = OldType;
    return BodyTV;
}

TypedValue LambdaExprAST::codegen(CodegenContext& context) {
    std::vector<llvm::Type*> ArgTypes(Args.size(), llvm::Type::getDoubleTy(context.TheContext));
    llvm::FunctionType* LambdaTy = llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), ArgTypes, false);
    llvm::Function* LambdaFn = llvm::Function::Create(LambdaTy, llvm::Function::InternalLinkage, "lambda", context.TheModule);
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", LambdaFn);
    llvm::BasicBlock* OldBB = context.Builder.GetInsertBlock();
    std::map<std::string, llvm::Value*> OldNamedValues = context.NamedValues;
    context.NamedValues.clear();
    {
        llvm::IRBuilder<> LambdaBuilder(BB);
        unsigned Idx = 0;
        for (auto& Arg : LambdaFn->args()) {
            Arg.setName(Args[Idx++]);
            llvm::AllocaInst* Alloca = LambdaBuilder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, Arg.getName());
            LambdaBuilder.CreateStore(&Arg, Alloca);
            context.NamedValues[std::string(Arg.getName())] = Alloca;
        }
        context.Builder.SetInsertPoint(BB);
        TypedValue BodyTV = Body->codegen(context);
        if (BodyTV.Val) context.Builder.CreateRet(BodyTV.Val);
    }
    if (OldBB) context.Builder.SetInsertPoint(OldBB);
    context.NamedValues = OldNamedValues;
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue VectorExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF) MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, { Int32Ty }, false), llvm::Function::ExternalLinkage, "malloc", context.TheModule);
    llvm::Value* DataSize = llvm::ConstantInt::get(Int32Ty, Elements.size() * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
    for (size_t i = 0; i < Elements.size(); ++i) {
        TypedValue ElemTV = Elements[i]->codegen(context);
        if (ElemTV.Val) {
            llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
            context.Builder.CreateStore(ElemTV.Val, ElemPtr);
        }
    }
    llvm::Function* CreateVecSumF = context.TheModule->getFunction("flux_create_vector_sum");
    if (!CreateVecSumF) CreateVecSumF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), { VoidPtrTy, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_create_vector_sum", context.TheModule);
    return TypedValue(context.Builder.CreateCall(CreateVecSumF, {DataPtr, llvm::ConstantInt::get(Int32Ty, Elements.size())}, "vec_sum"), TypeKind::Double);
}

TypedValue MatrixExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    // Determine if the matrix should be complex
    bool isComplex = false;
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            if (dynamic_cast<ComplexExprAST*>(Rows[r][c].get())) {
                isComplex = true;
                break;
            }
        }
        if (isComplex) break;
    }

    llvm::Value* DataPtr = nullptr;
    int elemSize = isComplex ? 16 : 8;
    llvm::Type* ElemTy = isComplex ? ComplexTy : DoubleTy;

    // Allocate memory for matrix elements
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF) MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, { Int32Ty }, false), llvm::Function::ExternalLinkage, "malloc", context.TheModule);
    llvm::Value* DataSize = llvm::ConstantInt::get(Int32Ty, NumRows * NumCols * elemSize);
    DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "mat_data");
    DataPtr = context.Builder.CreateBitCast(DataPtr, llvm::PointerType::get(ElemTy, 0));
    
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            TypedValue ElemTV = Rows[r][c]->codegen(context);
            if (ElemTV.Val) {
                if (isComplex && ElemTV.Type.Kind != TypeKind::Complex) {
                    ElemTV = promoteToComplex(ElemTV, context);
                }
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(ElemTy, DataPtr, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), r * NumCols + c)}, "elem_ptr");
                context.Builder.CreateStore(ElemTV.Val, ElemPtr);
            }
        }
    }

    std::string createFn = isComplex ? "flux_create_complex_matrix" : "flux_create_matrix";
    llvm::Function* CreateMatF = context.TheModule->getFunction(createFn);
    if (!CreateMatF) {
        llvm::Type* Params[] = { llvm::PointerType::get(ElemTy, 0), Int32Ty, Int32Ty };
        CreateMatF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, Params, false), llvm::Function::ExternalLinkage, createFn, context.TheModule);
    }
    
    llvm::Value* RowsVal = llvm::ConstantInt::get(Int32Ty, NumRows);
    llvm::Value* ColsVal = llvm::ConstantInt::get(Int32Ty, NumCols);
    llvm::Value* MatPtr = context.Builder.CreateCall(CreateMatF, {DataPtr, RowsVal, ColsVal}, "mat_ptr_reg");
    
    FluxType resType = isComplex ? FluxType(TypeKind::ComplexMatrix) : FluxType(TypeKind::Matrix);
    llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(resType.getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, MatPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RowsVal, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, ColsVal, 2);
    
    return TypedValue(MatVal, resType);
}

TypedValue IndexExprAST::codegen(CodegenContext& context) {
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    TypedValue ArrayTV = Array->codegen(context);
    if (!ArrayTV.Val || ArrayTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "Can only index into matrices." << std::endl;
        return TypedValue();
    }

    llvm::Value* MatPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "mat_ptr");
    
    // Extract matrix dimensions for bounds checking
    llvm::Value* RowsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "mat_rows");
    llvm::Value* ColsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 2, "mat_cols");

    if (IsMatrixIndex) {
        // Check if either index is a RangeExprAST (for slicing)
        auto codegenRange = [&](std::unique_ptr<ExprAST>& idx, llvm::Value* dim,
                                llvm::Value*& lo, llvm::Value*& hi) -> bool {
            auto* r = dynamic_cast<RangeExprAST*>(idx.get());
            if (!r) { lo = llvm::ConstantInt::get(Int32Ty, 0); hi = dim; return true; }
            auto* sPtr = const_cast<ExprAST*>(r->getStart());
            auto* ePtr = const_cast<ExprAST*>(r->getEnd());
            if (!sPtr || !ePtr) return false;
            TypedValue s = sPtr->codegen(context);
            TypedValue e = ePtr->codegen(context);
            lo = s.Val; hi = e.Val;
            if (lo->getType()->isFloatingPointTy()) lo = context.Builder.CreateFPToSI(lo, Int32Ty, "slice_lo");
            if (hi->getType()->isFloatingPointTy()) hi = context.Builder.CreateFPToSI(hi, Int32Ty, "slice_hi");
            return true;
        };

        bool hasRowRange = dynamic_cast<RangeExprAST*>(RowIndex.get()) != nullptr;
        bool hasColRange = ColIndex ? dynamic_cast<RangeExprAST*>(ColIndex.get()) != nullptr : false;

        if (hasRowRange || hasColRange) {
            llvm::Value* r0, *r1, *c0, *c1;
            if (!codegenRange(RowIndex, RowsVal, r0, r1)) return TypedValue();
            if (!codegenRange(ColIndex ? ColIndex : RowIndex,
                              ColsVal, c0, c1)) return TypedValue();

            llvm::Function* SliceF = context.TheModule->getFunction("flux_matrix_slice");
            if (!SliceF) SliceF = llvm::Function::Create(
                llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty}, false),
                llvm::Function::ExternalLinkage, "flux_matrix_slice", context.TheModule);
            llvm::Value* SlicePtr = context.Builder.CreateCall(SliceF, {MatPtr, r0, r1, c0, c1}, "slice_ptr");

            // Get slice dimensions and wrap in matrix struct
            llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
            if (!RowsF) RowsF = llvm::Function::Create(
                llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
            llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
            if (!ColsF) ColsF = llvm::Function::Create(
                llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
            llvm::Value* SR = context.Builder.CreateCall(RowsF, {SlicePtr}, "sli_rows");
            llvm::Value* SC = context.Builder.CreateCall(ColsF, {SlicePtr}, "sli_cols");
            llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(
                FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
            llvm::Value* MV = llvm::PoisonValue::get(MatSTy);
            MV = context.Builder.CreateInsertValue(MV, SlicePtr, 0);
            MV = context.Builder.CreateInsertValue(MV, SR, 1);
            MV = context.Builder.CreateInsertValue(MV, SC, 2);
            return TypedValue(MV, TypeKind::Matrix);
        }

        // Scalar element access: A[row, col]
        TypedValue RowTV = RowIndex->codegen(context);
        TypedValue ColTV = ColIndex ? ColIndex->codegen(context) : TypedValue();
        if (!RowTV.Val || !ColTV.Val) return TypedValue();

        llvm::Value* RowV = RowTV.Val;
        llvm::Value* ColV = ColTV.Val;

        if (RowV->getType()->isFloatingPointTy()) RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        else if (RowV->getType()->isIntegerTy(64)) RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");

        if (ColV->getType()->isFloatingPointTy()) ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        else if (ColV->getType()->isIntegerTy(64)) ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");

        llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
        if (!GetElemF) GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, { VoidPtrTy, Int32Ty, Int32Ty }, false), llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule);

        return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), TypeKind::Double);
    }

    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue RangeExprAST::codegen(CodegenContext& context) {
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV = Step ? Step->codegen(context) : TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(1.0)), TypeKind::Double);
    llvm::Function* RangeF = context.TheModule->getFunction("flux_create_range_sum");
    if (!RangeF) RangeF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), { llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext) }, false), llvm::Function::ExternalLinkage, "flux_create_range_sum", context.TheModule);
    return TypedValue(context.Builder.CreateCall(RangeF, {StartTV.Val, StepTV.Val, EndTV.Val}, "range_sum"), TypeKind::Double);
}

llvm::Function* PrototypeAST::codegen(CodegenContext& context) {
    std::vector<llvm::Type*> ArgTypes;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* DoublePtrTy = llvm::PointerType::get(DoubleTy, 0);

    if (IsGenerator) {
        // Generators take a hidden pointer to their state struct
        ArgTypes.push_back(VoidPtrTy);
    }

    const bool isUpdateLike =
        Name == "update" ||
        (Name.rfind("update_", 0) == 0);

    for (const auto& Arg : Args) {
        const std::string& argName = Arg.first;
        if (isUpdateLike && argName == "inputs") {
            ArgTypes.push_back(DoublePtrTy);
        } else if (isUpdateLike && argName == "outputs") {
            ArgTypes.push_back(DoublePtrTy);
        } else if (isUpdateLike && argName == "state") {
            ArgTypes.push_back(VoidPtrTy);
        } else if (Arg.second.Kind == TypeKind::String) {
            ArgTypes.push_back(DoubleTy);
        } else {
            ArgTypes.push_back(Arg.second.getLLVMType(context.TheContext));
        }
    }
    
    llvm::Type* RetTy = (ReturnType.Kind == TypeKind::String) ? 
                        DoubleTy : ReturnType.getLLVMType(context.TheContext);
    
    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, context.TheModule);
    
    unsigned Idx = 0;
    auto ArgIt = F->arg_begin();
    if (IsGenerator) {
        ArgIt->setName("__gen_state");
        ++ArgIt;
    }
    for (; ArgIt != F->arg_end(); ++ArgIt) {
        ArgIt->setName(Args[Idx++].first);
    }
    return F;
}

llvm::Function* FunctionAST::codegen(CodegenContext& context) {
    bool isGenerator = Body->containsYield();
    if (isGenerator) Proto->setGenerator(true);

    llvm::Function* TheFunction = context.TheModule->getFunction(Proto->getName());
    if (!TheFunction) TheFunction = Proto->codegen(context);
    if (!TheFunction) return nullptr;

    llvm::DISubprogram* subprogram = nullptr;
    if (context.DebugBuilder && context.DebugCompileUnit && context.DebugFile) {
        auto typeArray = context.DebugBuilder->getOrCreateTypeArray({});
        auto* subroutineType = context.DebugBuilder->createSubroutineType(typeArray);
        subprogram = context.DebugBuilder->createFunction(
            context.DebugFile,
            Proto->getName(),
            Proto->getName(),
            context.DebugFile,
            1,
            subroutineType,
            1,
            llvm::DINode::FlagZero,
            llvm::DISubprogram::SPFlagDefinition);
        TheFunction->setSubprogram(subprogram);
    }

    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    llvm::BasicBlock* ReturnBB = llvm::BasicBlock::Create(context.TheContext, "return");
    context.Builder.SetInsertPoint(EntryBB);
    
    // Initialize context for this function
    context.CurrentCatchBB = nullptr;
    context.CurrentExceptionAlloca = nullptr;
    context.GeneratorStateAlloca = nullptr;
    context.GeneratorDispatcherBB = nullptr;
    context.YieldTargets.clear();

    llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);
    
    // Create return value alloca
    llvm::Value* RetValAlloca = nullptr;
    if (!RetTy->isVoidTy()) {
        RetValAlloca = context.Builder.CreateAlloca(RetTy, nullptr, "retval");
        context.Builder.CreateStore(llvm::Constant::getNullValue(RetTy), RetValAlloca);
    }

    // Update context for nested returns
    llvm::BasicBlock* SavedRetBB = context.CurrentReturnBB;
    llvm::Value* SavedRetAlloca = context.CurrentReturnValueAlloca;
    context.CurrentReturnBB = ReturnBB;
    context.CurrentReturnValueAlloca = RetValAlloca;

    if (isGenerator) {
        // Generator state struct: { i32 state_index }
        std::vector<llvm::Type*> StateTypes = { llvm::Type::getInt32Ty(context.TheContext) };
        llvm::Type* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
        llvm::Value* StatePtr = &(*TheFunction->arg_begin());
        
        context.GeneratorStateAlloca = StatePtr;
        context.GeneratorDispatcherBB = llvm::BasicBlock::Create(context.TheContext, "gen_dispatch", TheFunction);
        
        context.Builder.CreateBr(context.GeneratorDispatcherBB);
        context.Builder.SetInsertPoint(context.GeneratorDispatcherBB);
        
        // Initial entry block (state 0)
        llvm::BasicBlock* StartBB = llvm::BasicBlock::Create(context.TheContext, "gen_start", TheFunction);
        context.YieldTargets.push_back(StartBB);
    }

    if (subprogram) {
        context.Builder.SetCurrentDebugLocation(
            llvm::DILocation::get(context.TheContext, 1, 1, subprogram));
    }
    context.NamedValues.clear();
    const auto& ArgTypes = Proto->getArgs();
    unsigned Idx = 0;
    auto ArgIt = TheFunction->arg_begin();
    if (isGenerator) ++ArgIt; // Skip state pointer
    const bool isUpdateLike =
        Proto->getName() == "update" ||
        (Proto->getName().rfind("update_", 0) == 0);

    for (; ArgIt != TheFunction->arg_end(); ++ArgIt) {
        llvm::Type* ExpectedTy = ArgTypes[Idx].second.getLLVMType(context.TheContext);
        const std::string argName = std::string(ArgIt->getName());
        if (isUpdateLike &&
            (argName == "inputs" || argName == "outputs" || argName == "state")) {
            // Guardrail:
            // Update-like functions lower special runtime arguments to pointer
            // types in PrototypeAST::codegen(). Reusing the source-level type
            // here would allocate a scalar stack slot and store the pointer
            // bits into it, which corrupts native SmartSignal accesses like
            // `inputs[in0]`.
            ExpectedTy = ArgIt->getType();
        }
        llvm::Value* ArgVal = &(*ArgIt);
        
        // Handle bitcast from double to pointer for string arguments
        if (ArgVal->getType()->isFloatingPointTy() && ExpectedTy->isPointerTy()) {
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
            ArgVal = context.Builder.CreateBitCast(ArgVal, Int64Ty, "fp_to_int");
            ArgVal = context.Builder.CreateIntToPtr(ArgVal, ExpectedTy, "int_to_ptr");
        }
        
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(ExpectedTy, nullptr, argName);
        context.Builder.CreateStore(ArgVal, Alloca);
        context.NamedValues[argName] = Alloca;
        Idx++;
    }

    if (isGenerator) {
        context.Builder.SetInsertPoint(context.YieldTargets[0]);
    }

    if (TypedValue RetTV = Body->codegen(context)) {
        llvm::Value* RetVal = RetTV.Val;
        llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);
        
        if (isGenerator) {
            // End of generator: set state to -1
            std::vector<llvm::Type*> StateTypes = { llvm::Type::getInt32Ty(context.TheContext) };
            llvm::Type* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.GeneratorStateAlloca, 0);
            context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), -1), IndexPtr);
        }

        if (RetVal && RetVal->getType() != RetTy) {
            // Complex (<2 x double>) → double: extract real part
            if (RetTy->isDoubleTy() && RetVal->getType()->isVectorTy()) {
                RetVal = context.Builder.CreateExtractElement(RetVal, (uint64_t)0, "ret_real");
            } else if (RetTy->isIntegerTy() && RetVal->getType()->isFloatingPointTy()) RetVal = context.Builder.CreateFPToSI(RetVal, RetTy, "cast_final");
            else if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast_final");
            else if (RetTy->isFloatingPointTy() && RetVal->getType()->isPointerTy()) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                RetVal = context.Builder.CreatePtrToInt(RetVal, Int64Ty, "ptr_to_int");
                RetVal = context.Builder.CreateBitCast(RetVal, RetTy, "int_to_fp");
            } else if (RetTy->isPointerTy() && RetVal->getType()->isFloatingPointTy()) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                RetVal = context.Builder.CreateBitCast(RetVal, Int64Ty, "fp_to_int");
                RetVal = context.Builder.CreateIntToPtr(RetVal, RetTy, "int_to_ptr");
            }
        }

        if (!context.Builder.GetInsertBlock()->getTerminator()) {
            if (RetValAlloca) {
                if (RetVal) context.Builder.CreateStore(RetVal, RetValAlloca);
                context.Builder.CreateBr(ReturnBB);
            } else {
                context.Builder.CreateBr(ReturnBB);
            }
        }

        // Generate Return Block
        TheFunction->insert(TheFunction->end(), ReturnBB);
        context.Builder.SetInsertPoint(ReturnBB);
        if (RetValAlloca) {
            llvm::Value* FinalRetVal = context.Builder.CreateLoad(RetTy, RetValAlloca, "ret_final");
            context.Builder.CreateRet(FinalRetVal);
        } else {
            context.Builder.CreateRetVoid();
        }

        // Restore context
        context.CurrentReturnBB = SavedRetBB;
        context.CurrentReturnValueAlloca = SavedRetAlloca;

        if (isGenerator) {
            // Generate dispatcher switch
            context.Builder.SetInsertPoint(context.GeneratorDispatcherBB);
            std::vector<llvm::Type*> StateTypes = { llvm::Type::getInt32Ty(context.TheContext) };
            llvm::Type* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.GeneratorStateAlloca, 0);
            llvm::Value* StateIndex = context.Builder.CreateLoad(llvm::Type::getInt32Ty(context.TheContext), IndexPtr);
            
            llvm::SwitchInst* Switch = context.Builder.CreateSwitch(StateIndex, context.YieldTargets[0], (unsigned)context.YieldTargets.size());
            for (size_t i = 1; i < context.YieldTargets.size(); i++) {
                Switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), (int)i), context.YieldTargets[i]);
            }
        }

        if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
             std::cerr << "LLVM IR Verification FAILED for standard function. Dumping IR:" << std::endl;
             TheFunction->print(llvm::errs());
        }
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}

TypedValue VoltageExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetVF = context.TheModule->getFunction("flux_get_voltage");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetVF) {
        GetVF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_get_voltage", context.TheModule);
    }
    
    uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(NodeName));
    llvm::Value* addrConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(addrConst, DoubleTy, "ptr_double");
    
    return TypedValue(context.Builder.CreateCall(GetVF, {PtrDouble}, "v"), TypeKind::Double);
}

TypedValue CurrentExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetIF = context.TheModule->getFunction("flux_get_current");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetIF) {
        GetIF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_get_current", context.TheModule);
    }
    
    uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(BranchName));
    llvm::Value* addrConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(addrConst, DoubleTy, "ptr_double");
    
    return TypedValue(context.Builder.CreateCall(GetIF, {PtrDouble}, "i"), TypeKind::Double);
}

TypedValue ParameterExprAST::codegen(CodegenContext& context) {
    llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetPF) {
        GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false), llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule);
    }
    
    uint64_t addr = reinterpret_cast<uint64_t>(getPermanentString(ParamName));
    llvm::Value* addrConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), addr);
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(addrConst, DoubleTy, "ptr_double");
    
    return TypedValue(context.Builder.CreateCall(GetPF, {PtrDouble}, "p"), TypeKind::Double);
}

// ============================================================================
// SPICE Time-Domain Simulation Codegen
// ============================================================================

TypedValue BuiltinVarExprAST::codegen(CodegenContext& context) {
    // Generate calls for built-in variables: time, dt, temp
    std::string FuncName;
    if (Name == "time") FuncName = "flux_get_time";
    else if (Name == "dt") FuncName = "flux_get_dt";
    else if (Name == "temp") FuncName = "flux_get_temp";
    else {
        std::cerr << "Unknown built-in variable: " << Name << std::endl;
        return TypedValue();
    }

    llvm::Function* GetFunc = context.TheModule->getFunction(FuncName);
    if (!GetFunc) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        GetFunc = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {}, false),
                                         llvm::Function::ExternalLinkage,
                                         FuncName, context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(GetFunc, {}, Name.c_str()), TypeKind::Double);
}

TypedValue UpdateFuncAST::codegen(CodegenContext& context) {
    // Generate update(t, inputs) or update(t, inputs, outputs, state) function
    std::vector<llvm::Type*> ArgTypes;
    std::vector<std::string> ArgNames;

    // Time argument (double)
    ArgTypes.push_back(llvm::Type::getDoubleTy(context.TheContext));
    ArgNames.push_back(TimeVar);

    // Inputs argument (pointer to inputs array)
    llvm::Type* DoublePtrTy = llvm::PointerType::get(llvm::Type::getDoubleTy(context.TheContext), 0);
    ArgTypes.push_back(DoublePtrTy);
    ArgNames.push_back(InputsVar);

    // Optional outputs argument (pointer to outputs array)
    if (!OutputsVar.empty()) {
        ArgTypes.push_back(DoublePtrTy);
        ArgNames.push_back(OutputsVar);
    }

    // Optional state argument (i8* for opaque state pointer)
    if (!StateVar.empty()) {
        ArgTypes.push_back(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context.TheContext)));
        ArgNames.push_back(StateVar);
    }

    // Return type is double (output value)
    llvm::Type* RetTy = llvm::Type::getDoubleTy(context.TheContext);

    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* TheFunction = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "update", context.TheModule);

    // Create entry block and return block
    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    llvm::BasicBlock* ReturnBB = llvm::BasicBlock::Create(context.TheContext, "return");
    context.Builder.SetInsertPoint(EntryBB);

    // Create return value alloca
    llvm::Value* RetValAlloca = context.Builder.CreateAlloca(RetTy, nullptr, "retval");
    context.Builder.CreateStore(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), RetValAlloca);

    // Update context for nested returns
    llvm::BasicBlock* SavedRetBB = context.CurrentReturnBB;
    llvm::Value* SavedRetAlloca = context.CurrentReturnValueAlloca;
    context.CurrentReturnBB = ReturnBB;
    context.CurrentReturnValueAlloca = RetValAlloca;

    // Set argument names and allocate on stack
    unsigned Idx = 0;
    for (auto& Arg : TheFunction->args()) {
        Arg.setName(ArgNames[Idx]);
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(Arg.getType(), nullptr, ArgNames[Idx]);
        context.Builder.CreateStore(&Arg, Alloca);
        context.NamedValues[std::string(Arg.getName())] = Alloca;
        Idx++;
    }

    TypedValue RetTV = Body->codegen(context);
    
    // Jump to return block if not already terminated
    if (!context.Builder.GetInsertBlock()->getTerminator()) {
        llvm::Value* RetVal = RetTV.Val;
        if (RetVal && RetVal->getType() != RetTy) {
            if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast_final");
            }
        }
        if (!RetVal) RetVal = llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0));
        context.Builder.CreateStore(RetVal, RetValAlloca);
        context.Builder.CreateBr(ReturnBB);
    }

    // Generate Return Block
    TheFunction->insert(TheFunction->end(), ReturnBB);
    context.Builder.SetInsertPoint(ReturnBB);
    llvm::Value* FinalRetVal = context.Builder.CreateLoad(RetTy, RetValAlloca, "ret_final");
    context.Builder.CreateRet(FinalRetVal);

    // Restore context
    context.CurrentReturnBB = SavedRetBB;
    context.CurrentReturnValueAlloca = SavedRetAlloca;
    
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
         std::cerr << "LLVM IR Verification FAILED for function 'update'. Dumping IR:" << std::endl;
         TheFunction->print(llvm::errs());
    }
    return TypedValue(TheFunction, TypeKind::Double);

    TheFunction->eraseFromParent();
    return TypedValue();
}

// ============================================================================
// SPICE Behavioral Sources Codegen
// ============================================================================

TypedValue BSourceExprAST::codegen(CodegenContext& context) {
    // Generate B-source netlist entry
    // Bxxx n+ n- V=<expression> or I=<expression>
    std::string SourceType = IsCurrent ? "I" : "V";
    std::string NetlistEntry = "B" + Name + " " + PositiveNode + " " + NegativeNode + " " + SourceType + "=";
    
    // The expression will be evaluated and stored as a string
    // For now, we generate a placeholder and mark it for later netlist generation
    llvm::Function* RegisterBSource = context.TheModule->getFunction("flux_register_bsource");
    if (!RegisterBSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterBSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_bsource", context.TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterBSource, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(PositiveNode),
        context.Builder.CreateGlobalString(NegativeNode),
        context.Builder.CreateGlobalString(SourceType)
    }), TypeKind::Double);
}

TypedValue ESourceExprAST::codegen(CodegenContext& context) {
    // Generate E-source (VCVS) netlist entry
    // Exxx n+ n- nc+ nc- <gain>
    llvm::Function* RegisterESource = context.TheModule->getFunction("flux_register_esource");
    if (!RegisterESource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterESource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_esource", context.TheModule);
    }
    
    // Evaluate gain expression
    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterESource, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(PositiveNode),
        context.Builder.CreateGlobalString(NegativeNode),
        context.Builder.CreateGlobalString(ControlPosNode),
        context.Builder.CreateGlobalString(ControlNegNode),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(GainVal))
    }), TypeKind::Double);
}

TypedValue FSourceExprAST::codegen(CodegenContext& context) {
    // Generate F-source (CCCS) netlist entry
    // Fxxx n+ n- <vsource_name> <gain>
    llvm::Function* RegisterFSource = context.TheModule->getFunction("flux_register_fsource");
    if (!RegisterFSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterFSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_fsource", context.TheModule);
    }
    
    double GainVal = 1.0;
    if (Gain) {
        if (auto* NumGain = dynamic_cast<NumberExprAST*>(Gain.get())) {
            GainVal = NumGain->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterFSource, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(PositiveNode),
        context.Builder.CreateGlobalString(NegativeNode),
        context.Builder.CreateGlobalString(VoltageSourceName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(GainVal))
    }), TypeKind::Double);
}

TypedValue GSourceExprAST::codegen(CodegenContext& context) {
    // Generate G-source (VCCS) netlist entry
    // Gxxx n+ n- nc+ nc- <transconductance>
    llvm::Function* RegisterGSource = context.TheModule->getFunction("flux_register_gsource");
    if (!RegisterGSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterGSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_gsource", context.TheModule);
    }
    
    double TranscondVal = 1.0;
    if (Transconductance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transconductance.get())) {
            TranscondVal = NumVal->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterGSource, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(PositiveNode),
        context.Builder.CreateGlobalString(NegativeNode),
        context.Builder.CreateGlobalString(ControlPosNode),
        context.Builder.CreateGlobalString(ControlNegNode),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(TranscondVal))
    }), TypeKind::Double);
}

TypedValue HSourceExprAST::codegen(CodegenContext& context) {
    // Generate H-source (CCVS) netlist entry
    // Hxxx n+ n- <vsource_name> <transresistance>
    llvm::Function* RegisterHSource = context.TheModule->getFunction("flux_register_hsource");
    if (!RegisterHSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterHSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_hsource", context.TheModule);
    }
    
    double TransresVal = 1.0;
    if (Transresistance) {
        if (auto* NumVal = dynamic_cast<NumberExprAST*>(Transresistance.get())) {
            TransresVal = NumVal->getValue();
        }
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterHSource, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(PositiveNode),
        context.Builder.CreateGlobalString(NegativeNode),
        context.Builder.CreateGlobalString(VoltageSourceName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(TransresVal))
    }), TypeKind::Double);
}

// ============================================================================
// SPICE Analysis Control Codegen
// ============================================================================

void AnalysisExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue AnalysisExprAST::codegen(CodegenContext& context) {
    std::string AnalysisName;
    switch (Type) {
        case AnalysisType::TRAN: AnalysisName = "tran"; break;
        case AnalysisType::DC: AnalysisName = "dc"; break;
        case AnalysisType::AC: AnalysisName = "ac"; break;
        case AnalysisType::NOISE: AnalysisName = "noise"; break;
        case AnalysisType::OP: AnalysisName = "op"; break;
        case AnalysisType::TF: AnalysisName = "tf"; break;
        case AnalysisType::SENS: AnalysisName = "sens"; break;
        case AnalysisType::FOURIER: AnalysisName = "fourier"; break;
    }
    
    llvm::Function* RegisterAnalysis = context.TheModule->getFunction("flux_register_analysis");
    if (!RegisterAnalysis) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterAnalysis = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_analysis", context.TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterAnalysis, {
        context.Builder.CreateGlobalString(AnalysisName)
    }), TypeKind::Double);
}

void MeasureExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue MeasureExprAST::codegen(CodegenContext& context) {
    std::string MeasureName;
    switch (Type) {
        case MeasureType::MAX: MeasureName = "MAX"; break;
        case MeasureType::MIN: MeasureName = "MIN"; break;
        case MeasureType::AVG: MeasureName = "AVG"; break;
        case MeasureType::RMS: MeasureName = "RMS"; break;
        case MeasureType::TRIG: MeasureName = "TRIG"; break;
        case MeasureType::TARG: MeasureName = "TARG"; break;
        case MeasureType::WHEN: MeasureName = "WHEN"; break;
        case MeasureType::FIND: MeasureName = "FIND"; break;
        case MeasureType::DERIV: MeasureName = "DERIV"; break;
        case MeasureType::INTEG: MeasureName = "INTEG"; break;
    }
    
    llvm::Function* RegisterMeasure = context.TheModule->getFunction("flux_register_measure");
    if (!RegisterMeasure) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterMeasure = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_measure", context.TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterMeasure, {
        context.Builder.CreateGlobalString(Name),
        context.Builder.CreateGlobalString(MeasureName)
    }), TypeKind::Double);
}

TypedValue ProbeExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterProbe = context.TheModule->getFunction("flux_register_probe");
    if (!RegisterProbe) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterProbe = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_probe", context.TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterProbe, {
        context.Builder.CreateGlobalString(VariableName),
        context.Builder.CreateGlobalString(OutputName.empty() ? VariableName : OutputName)
    }), TypeKind::Double);
}

TypedValue SaveExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterSave = context.TheModule->getFunction("flux_register_save");
    if (!RegisterSave) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterSave = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_save", context.TheModule);
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterSave, {
        context.Builder.CreateGlobalString(VariableName)
    }), TypeKind::Double);
}

// ============================================================================
// SPICE Subcircuit and Model Codegen
// ============================================================================

void SubcktExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters.push_back({Name, std::move(Value)});
}

void SubcktExprAST::addStatement(std::unique_ptr<ExprAST> Stmt) {
    Body.push_back(std::move(Stmt));
}

void SubcktExprAST::addFunction(std::unique_ptr<FunctionAST> Func) {
    Functions.push_back(std::move(Func));
}

TypedValue SubcktExprAST::codegen(CodegenContext& context) {
    // Build pin list string
    std::string PinsStr;
    for (size_t i = 0; i < Pins.size(); ++i) {
        if (i > 0) PinsStr += " ";
        PinsStr += Pins[i];
    }
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    
    llvm::Function* RegisterSubckt = context.TheModule->getFunction("flux_register_subckt");
    if (!RegisterSubckt) {
        // Updated signature: double flux_register_subckt(double name_ptr_double, double pins_ptr_double)
        RegisterSubckt = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_subckt", context.TheModule);
    }
    
    // Use bitcast pattern for Name
    uint64_t nameAddr = reinterpret_cast<uint64_t>(getPermanentString(Name));
    llvm::Value* NameAddrVal = llvm::ConstantInt::get(Int64Ty, nameAddr);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameAddrVal, DoubleTy, "name_double");
    
    // Use bitcast pattern for PinsStr
    uint64_t pinsAddr = reinterpret_cast<uint64_t>(getPermanentString(PinsStr));
    llvm::Value* PinsAddrVal = llvm::ConstantInt::get(Int64Ty, pinsAddr);
    llvm::Value* PinsDouble = context.Builder.CreateBitCast(PinsAddrVal, DoubleTy, "pins_double");
    
    // Register the subcircuit
    TypedValue Result = TypedValue(context.Builder.CreateCall(RegisterSubckt, {NameDouble, PinsDouble}), TypeKind::Double);
    
    // Codegen body statements (netlist elements, local parameters, etc.)
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }
    
    // Codegen local functions defined inside the subcircuit
    for (auto& Func : Functions) {
        Func->codegen(context);
    }
    
    return Result;
}

void ModelExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value) {
    Parameters[Name] = std::move(Value);
}

TypedValue ModelExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterModel = context.TheModule->getFunction("flux_register_model");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    if (!RegisterModel) {
        RegisterModel = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_model", context.TheModule);
    }
    
    // Use bitcast pattern for Name
    uint64_t nameAddr = reinterpret_cast<uint64_t>(getPermanentString(Name));
    llvm::Value* NameAddrVal = llvm::ConstantInt::get(Int64Ty, nameAddr);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameAddrVal, DoubleTy, "name_double");
    
    // Use bitcast pattern for ModelType
    uint64_t typeAddr = reinterpret_cast<uint64_t>(getPermanentString(ModelType));
    llvm::Value* TypeAddrVal = llvm::ConstantInt::get(Int64Ty, typeAddr);
    llvm::Value* TypeDouble = context.Builder.CreateBitCast(TypeAddrVal, DoubleTy, "type_double");

    return TypedValue(context.Builder.CreateCall(RegisterModel, {NameDouble, TypeDouble}), TypeKind::Double);
}

TypedValue ParamExprAST::codegen(CodegenContext& context) {
    // Parameters are handled at the simulation setup level
    // This just marks the parameter as declared
    llvm::Function* RegisterParam = context.TheModule->getFunction("flux_register_param");
    if (!RegisterParam) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterParam = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_param", context.TheModule);
    }
    
    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }
    
    return TypedValue(context.Builder.CreateCall(RegisterParam, {
        context.Builder.CreateGlobalString(Name),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val))
    }), TypeKind::Double);
}

TypedValue ICExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterIC = context.TheModule->getFunction("flux_register_ic");
    if (!RegisterIC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterIC = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_ic", context.TheModule);
    }

    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }

    return TypedValue(context.Builder.CreateCall(RegisterIC, {
        context.Builder.CreateGlobalString(NodeName),
        llvm::ConstantFP::get(context.TheContext, llvm::APFloat(Val))
    }), TypeKind::Double);
}


TypedValue WorstCaseExprAST::codegen(CodegenContext& context) {
    llvm::Function* RegisterWC = context.TheModule->getFunction("flux_register_worst_case");
    if (!RegisterWC) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        RegisterWC = llvm::Function::Create(
            llvm::FunctionType::get(IntTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_worst_case", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterWC, {
        context.Builder.CreateGlobalString(OutputName)
    }), TypeKind::Int);
}

TypedValue StabilityExprAST::codegen(CodegenContext& context) {
    llvm::Function* Fn = context.TheModule->getFunction("flux_stability_analyze");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(IntTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_stability_analyze", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {
        context.Builder.CreateGlobalString(OutputName)
    }), TypeKind::Int);
}

TypedValue SensitivityExprAST::codegen(CodegenContext& context) {
    llvm::Function* Fn = context.TheModule->getFunction("flux_sensitivity_analyze");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(IntTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_sensitivity_analyze", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {
        context.Builder.CreateGlobalString(OutputName)
    }), TypeKind::Int);
}

TypedValue OptimizationExprAST::codegen(CodegenContext& context) {
    llvm::Function* Fn = context.TheModule->getFunction("flux_optimizer_run");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(IntTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_optimizer_run", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {
        context.Builder.CreateGlobalString(OutputName)
    }), TypeKind::Int);
}

TypedValue FFTExprAST::codegen(CodegenContext& context) {
    llvm::Function* Fn = context.TheModule->getFunction("flux_fft_analyze");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(IntTy, {CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_fft_analyze", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {
        context.Builder.CreateGlobalString(SignalName)
    }), TypeKind::Int);
}

// ============================================================================
// Statement-based Control Flow Codegen (void-returning, no PHI nodes)
// ============================================================================

TypedValue IfStmtAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Generate condition
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), "ifcond");
    }

    // Create basic blocks
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(Ctx, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(Ctx, "else", TheFunction);
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "ifcont");

    context.Builder.CreateCondBr(IsTrue, ThenBB, ElseBB);

    // Generate then block
    context.Builder.SetInsertPoint(ThenBB);
    for (auto& Stmt : ThenBody) {
        if (context.Builder.GetInsertBlock()->getTerminator()) break;
        Stmt->codegen(context);
    }
    bool thenTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!thenTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ThenBB = context.Builder.GetInsertBlock();

    // Generate else block
    context.Builder.SetInsertPoint(ElseBB);
    for (auto& Stmt : ElseBody) {
        if (context.Builder.GetInsertBlock()->getTerminator()) break;
        Stmt->codegen(context);
    }
    bool elseTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!elseTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ElseBB = context.Builder.GetInsertBlock();

    // Determine if we need to continue after the if
    if (thenTerminated && elseTerminated) {
        delete MergeBB;
        return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
    }

    // Continue at merge
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue ForStmtAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Save loop context
    llvm::BasicBlock* SavedLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* SavedLoopCont = context.CurrentLoopCont;

    // Generate init  handle "var = expr" pattern for auto-declaring loop vars
    if (Init) {
        if (auto* AssignInit = dynamic_cast<AssignExprAST*>(Init.get())) {
            // Special case: for (i = 0.0; ...)  auto-declare the loop variable
            if (auto* VarRef = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(AssignInit->getLHS()))) {
                std::string VarName = VarRef->getName();
                // Check if already declared
                if (context.NamedValues.find(VarName) == context.NamedValues.end()) {
                    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(
                        llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
                    context.NamedValues[VarName] = Alloca;
                }
                // Generate the assignment normally (will find the alloca we just created)
                Init->codegen(context);
            } else {
                Init->codegen(context);
            }
        } else if (auto* VarRef = dynamic_cast<VariableExprAST*>(Init.get())) {
            // Bare variable name: for (i; ...)  just ensure it exists
            std::string VarName = VarRef->getName();
            if (context.NamedValues.find(VarName) == context.NamedValues.end()) {
                llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(
                    llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
                context.NamedValues[VarName] = Alloca;
            }
        } else {
            Init->codegen(context);
        }
    }

    // Create blocks
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Ctx, "for.cond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "for.body", TheFunction);
    llvm::BasicBlock* StepBB = llvm::BasicBlock::Create(Ctx, "for.step", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "for.end", TheFunction);

    context.Builder.CreateBr(CondBB);

    // Condition
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), "forcond");
    }
    context.Builder.CreateCondBr(IsTrue, BodyBB, AfterBB);

    // Body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = StepBB;
    context.Builder.SetInsertPoint(BodyBB);
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }
    context.Builder.CreateBr(StepBB);

    // Step
    context.Builder.SetInsertPoint(StepBB);
    if (Step) {
        Step->codegen(context);
    }
    context.Builder.CreateBr(CondBB);

    // After
    context.Builder.SetInsertPoint(AfterBB);

    // Restore loop context
    context.CurrentLoopEnd = SavedLoopEnd;
    context.CurrentLoopCont = SavedLoopCont;

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue WhileStmtAST::codegen(CodegenContext& context) {
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Save loop context
    llvm::BasicBlock* SavedLoopEnd = context.CurrentLoopEnd;
    llvm::BasicBlock* SavedLoopCont = context.CurrentLoopCont;

    // Create blocks
    llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Ctx, "while.cond", TheFunction);
    llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Ctx, "while.body", TheFunction);
    llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Ctx, "while.end", TheFunction);

    context.Builder.CreateBr(CondBB);

    // Condition
    context.Builder.SetInsertPoint(CondBB);
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) return TypedValue();

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        IsTrue = context.Builder.CreateFCmpONE(
            CondTV.Val, llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), "whilecond");
    }
    context.Builder.CreateCondBr(IsTrue, BodyBB, AfterBB);

    // Body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = BodyBB;  // continue jumps to condition check
    context.Builder.SetInsertPoint(BodyBB);
    for (auto& Stmt : Body) {
        Stmt->codegen(context);
    }
    context.Builder.CreateBr(CondBB);

    // After
    context.Builder.SetInsertPoint(AfterBB);

    // Restore loop context
    context.CurrentLoopEnd = SavedLoopEnd;
    context.CurrentLoopCont = SavedLoopCont;

    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}


TypedValue NNCreateExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    // Call flux_nn_create(const int* layers, int numLayers)
    llvm::Function* CreateF = TheModule->getFunction("flux_nn_create");
    if (!CreateF) {
        CreateF = llvm::Function::Create(
            llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_nn_create", TheModule);
    }
    
    std::vector<llvm::Constant*> LayerConsts;
    for (int l : Layers) LayerConsts.push_back(llvm::ConstantInt::get(Int32Ty, l));
    
    llvm::ArrayType* ArrTy = llvm::ArrayType::get(Int32Ty, Layers.size());
    llvm::Value* LayersArray = context.Builder.CreateAlloca(ArrTy, nullptr, "nn_layers_local");
    for (size_t i = 0; i < Layers.size(); i++) {
        llvm::Value* Ptr = context.Builder.CreateStructGEP(ArrTy, LayersArray, i);
        context.Builder.CreateStore(LayerConsts[i], Ptr);
    }
    llvm::Value* LayersPtr = context.Builder.CreateBitCast(LayersArray, VoidPtrTy);
    
    llvm::Value* NNPtr = context.Builder.CreateCall(CreateF, {LayersPtr, llvm::ConstantInt::get(Int32Ty, Layers.size())}, "nn_ptr");
    
    return TypedValue(NNPtr, TypeKind::Double); // NN handle as double
}

TypedValue TrainExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Inputs->codegen(context);
    TypedValue OutTV = Outputs->codegen(context);
    
    if (!ModelTV.Val || !InTV.Val || !OutTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    // Call flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inDim, int outDim, int epochs)
    llvm::Function* TrainF = TheModule->getFunction("flux_nn_train");
    if (!TrainF) {
        llvm::Type* Params[] = { VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty };
        TrainF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                        llvm::Function::ExternalLinkage, "flux_nn_train", TheModule);
    }
    
    // Extract info from Matrix/Vector inputs/outputs
    llvm::Value* InPtr = context.Builder.CreateExtractValue(InTV.Val, 0, "in_ptr");
    llvm::Value* InRows = context.Builder.CreateExtractValue(InTV.Val, 1, "in_rows");
    llvm::Value* InCols = context.Builder.CreateExtractValue(InTV.Val, 2, "in_cols");
    
    llvm::Value* OutPtr = context.Builder.CreateExtractValue(OutTV.Val, 0, "out_ptr");
    llvm::Value* OutCols = context.Builder.CreateExtractValue(OutTV.Val, 2, "out_cols");

    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Value* ModelInt = context.Builder.CreateBitCast(ModelTV.Val, Int64Ty, "nn_int");
    llvm::Value* ModelPtr = context.Builder.CreateIntToPtr(ModelInt, VoidPtrTy, "nn_ptr");

    context.Builder.CreateCall(TrainF, {ModelPtr, InPtr, OutPtr, InRows, InCols, OutCols, llvm::ConstantInt::get(Int32Ty, Epochs)});
    
    return ModelTV;
}

TypedValue PredictExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Input->codegen(context);
    
    if (!ModelTV.Val || !InTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    // Call flux_nn_predict(void* nn, const double* input, double* output, int inDim, int outDim)
    // We'll return a new Vector
    
    // For now, let's assume output is a vector of size 1 for simplicity
    llvm::Function* PredictF = TheModule->getFunction("flux_nn_predict");
    if (!PredictF) {
        llvm::Type* Params[] = { VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty };
        PredictF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), Params, false),
                                          llvm::Function::ExternalLinkage, "flux_nn_predict", TheModule);
    }
    
    // Allocate output buffer on stack
    llvm::Value* OutBuf = context.Builder.CreateAlloca(DoubleTy, llvm::ConstantInt::get(Int32Ty, 1), "pred_out");
    
    llvm::Value* InPtr = context.Builder.CreateExtractValue(InTV.Val, 0, "in_ptr");
    llvm::Value* InSize = context.Builder.CreateExtractValue(InTV.Val, 1, "in_size");

    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Value* ModelInt = context.Builder.CreateBitCast(ModelTV.Val, Int64Ty, "nn_int");
    llvm::Value* ModelPtr = context.Builder.CreateIntToPtr(ModelInt, VoidPtrTy, "nn_ptr");

    context.Builder.CreateCall(PredictF, {ModelPtr, InPtr, OutBuf, InSize, llvm::ConstantInt::get(Int32Ty, 1)});
    
    // Return the value from buffer
    return TypedValue(context.Builder.CreateLoad(DoubleTy, OutBuf), TypeKind::Double);
}

TypedValue GoalExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    TypedValue TargetTV = Target->codegen(context);
    
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    
    llvm::Function* Fn = TheModule->getFunction("flux_register_goal");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_goal", TheModule);
    }
    
    context.Builder.CreateCall(Fn, {ExprTV.Val, TargetTV.Val});
    
    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue ThermalBlockAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue PowerTV = Power->codegen(context);
    TypedValue ResTV = Resistance->codegen(context);
    TypedValue CapTV = Capacitance->codegen(context);
    
    if (!PowerTV.Val || !ResTV.Val || !CapTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    
    // Call flux_thermal_step(double power, double resistance, double capacitance)
    // Returns current temperature
    llvm::Function* Fn = TheModule->getFunction("flux_thermal_step");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_thermal_step", TheModule);
    }
    
    llvm::Value* Temp = context.Builder.CreateCall(Fn, {PowerTV.Val, ResTV.Val, CapTV.Val}, "temp");
    
    return TypedValue(Temp, TypeKind::Double);
}


TypedValue MonteCarloExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue ExprTV = Expression->codegen(context);
    if (!ExprTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    // Convert parameters to C strings
    std::vector<llvm::Value*> ParamNames;
    for (const auto& p : Parameters) {
        ParamNames.push_back(context.Builder.CreateGlobalString(p));
    }
    
    llvm::ArrayType* ArrayTy = llvm::ArrayType::get(VoidPtrTy, ParamNames.size());
    llvm::Value* ParamsArray = context.Builder.CreateAlloca(ArrayTy, nullptr, "mc_params");
    for (size_t i = 0; i < ParamNames.size(); i++) {
        llvm::Value* Ptr = context.Builder.CreateStructGEP(ArrayTy, ParamsArray, i);
        context.Builder.CreateStore(ParamNames[i], Ptr);
    }
    
    // Call flux_weighted_monte_carlo(double expr_ptr, int param_count, const char** params, int iterations)
    llvm::Function* Fn = TheModule->getFunction("flux_weighted_monte_carlo");
    if (!Fn) {
        llvm::Type* Args[] = { DoubleTy, Int32Ty, VoidPtrTy, Int32Ty };
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, Args, false),
            llvm::Function::ExternalLinkage, "flux_weighted_monte_carlo", TheModule);
    }
    
    llvm::Value* Res = context.Builder.CreateCall(Fn, {
        ExprTV.Val, 
        llvm::ConstantInt::get(Int32Ty, ParamNames.size()), 
        context.Builder.CreateBitCast(ParamsArray, VoidPtrTy),
        llvm::ConstantInt::get(Int32Ty, Iterations)
    });
    
    return TypedValue(Res, TypeKind::Double);
}

TypedValue VirtualProbeExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue SigTV = Signal->codegen(context);
    if (!SigTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    // Call flux_virtual_probe(double value, const char* name)
    llvm::Function* Fn = TheModule->getFunction("flux_virtual_probe");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, VoidPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_virtual_probe", TheModule);
    }
    
    context.Builder.CreateCall(Fn, {SigTV.Val, context.Builder.CreateGlobalString(ProbeName)});
    
    return TypedValue(SigTV.Val, TypeKind::Double);
}

TypedValue HotSwapExprAST::codegen(CodegenContext& context) {
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;
    
    TypedValue NameTV = SubcktName->codegen(context);
    TypedValue ModelTV = Model->codegen(context);
    
    if (!NameTV.Val || !ModelTV.Val) return TypedValue();
    
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);
    
    llvm::Value* NamePtr = NameTV.Val;
    if (NameTV.Type.Kind == TypeKind::String) {
        // Already handled in CallExprAST::codegen? No, need to bitcast here.
        if (NamePtr->getType()->isFloatingPointTy()) {
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
            NamePtr = context.Builder.CreateBitCast(NamePtr, Int64Ty, "name_int");
            NamePtr = context.Builder.CreateIntToPtr(NamePtr, VoidPtrTy, "name_ptr");
        }
    } else if (NameTV.Type.Kind == TypeKind::Double) {
        // Assume it's a null pointer (0.0) or an address
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        NamePtr = context.Builder.CreateFPToSI(NamePtr, Int64Ty, "name_int");
        NamePtr = context.Builder.CreateIntToPtr(NamePtr, VoidPtrTy, "name_ptr");
    }
    
    // Call flux_hot_swap(const char* subckt_name, double model_ptr)
    llvm::Function* Fn = TheModule->getFunction("flux_hot_swap");
    if (!Fn) {
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {VoidPtrTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_hot_swap", TheModule);
    }
    
    context.Builder.CreateCall(Fn, {NamePtr, ModelTV.Val});
    
    return TypedValue(llvm::ConstantFP::get(Ctx, llvm::APFloat(1.0)), TypeKind::Double);
}
} // namespace Flux
