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
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/Verifier.h>
#include <string>

namespace Flux {

std::string UnitDimensions::toString() const
{
    if (isDimensionless())
        return "dimensionless";
    std::string s;
    if (mass != 0)
        s += "m^" + std::to_string(mass);
    if (length != 0)
        s += "l^" + std::to_string(length);
    if (time != 0)
        s += "t^" + std::to_string(time);
    if (current != 0)
        s += "i^" + std::to_string(current);
    if (temperature != 0)
        s += "T^" + std::to_string(temperature);
    if (amount != 0)
        s += "n^" + std::to_string(amount);
    if (luminous != 0)
        s += "J^" + std::to_string(luminous);
    return s;
}

std::string buildGenericTypeSuffix(const std::vector<FluxType>& genericArgs, CodegenContext& context)
{
    std::string suffix;
    for (size_t i = 0; i < genericArgs.size(); ++i) {
        if (i > 0)
            suffix += "_";
        FluxType resolved = genericArgs[i];
        resolveUserStructType(resolved, context);
        resolveUserEnumType(resolved, context);
        switch (resolved.Kind) {
        case TypeKind::Double:
            suffix += "Double";
            break;
        case TypeKind::Int:
            suffix += "Int";
            break;
        case TypeKind::Bool:
            suffix += "Bool";
            break;
        case TypeKind::Float:
            suffix += "Float";
            break;
        case TypeKind::String:
            suffix += "String";
            break;
        case TypeKind::Complex:
            suffix += "Complex";
            break;
        case TypeKind::Matrix:
            suffix += "Matrix";
            break;
        case TypeKind::ComplexMatrix:
            suffix += "Cmat";
            break;
        case TypeKind::Vector:
            suffix += "Vector";
            break;
        case TypeKind::Void:
            suffix += "Void";
            break;
        case TypeKind::UserStruct: {
            int sid = resolved.StructTypeId;
            if (sid >= 0 && sid < static_cast<int>(context.StructTypes.size()))
                suffix += context.StructTypes[sid].Name;
            else
                suffix += "S";
            break;
        }
        case TypeKind::UserEnum: {
            int eid = resolved.EnumTypeId;
            if (eid >= 0 && eid < static_cast<int>(context.EnumTypes.size()))
                suffix += context.EnumTypes[eid].Name;
            else
                suffix += "E";
            break;
        }
        case TypeKind::Generic:
            suffix += resolved.GenericName;
            break;
        default:
            suffix += "T";
            break;
        }
    }
    return suffix;
}

std::string specializeGenericEnum(const std::string& enumName, const std::vector<FluxType>& genericArgs,
                                  CodegenContext& context)
{
    std::vector<FluxType> concreteGenericArgs = genericArgs;
    for (auto& arg : concreteGenericArgs)
        ensureGenericTypeArgSpecialized(arg, context);

    std::string suffix = buildGenericTypeSuffix(concreteGenericArgs, context);
    std::string mangledName = enumName + "_" + suffix;

    auto existingIt = context.EnumTypeIndex.find(mangledName);
    if (existingIt != context.EnumTypeIndex.end())
        return mangledName;

    auto genIt = context.GenericEnums.find(enumName);
    if (genIt == context.GenericEnums.end())
        return "";

    EnumDeclAST* genericEnum = genIt->second;
    const auto& genericParams = genericEnum->getGenericParams();
    const auto& variants = genericEnum->getVariants();
    const auto& variantPayloads = genericEnum->getVariantPayloads();

    if (concreteGenericArgs.size() != genericParams.size()) {
        std::cerr << "Generic enum " << enumName << " expects " << genericParams.size() << " type args, got "
                  << concreteGenericArgs.size() << std::endl;
        return "";
    }

    std::map<std::string, FluxType> typeMap;
    for (size_t i = 0; i < genericParams.size(); ++i)
        typeMap[genericParams[i]] = concreteGenericArgs[i];

    std::vector<FluxType> concretePayloads;
    for (auto& pt : variantPayloads) {
        FluxType concreteType = pt;
        if (pt.isGeneric()) {
            auto mapIt = typeMap.find(pt.GenericName);
            if (mapIt != typeMap.end())
                concreteType = mapIt->second;
        }
        ensureGenericTypeArgSpecialized(concreteType, context);
        concretePayloads.push_back(concreteType);
    }

    for (size_t i = 0; i < concretePayloads.size(); ++i) {
        auto& pt = concretePayloads[i];
        if (pt.Kind == TypeKind::UserStruct && !pt.GenericName.empty()) {
            auto structIt = context.StructTypeIndex.find(pt.GenericName);
            if (structIt != context.StructTypeIndex.end()) {
                int structId = structIt->second;
                auto& structInfo = context.StructTypes[structId];
                bool needsSpecialization = false;
                for (auto& [fName, fType] : structInfo.Fields) {
                    if (fType.isGeneric()) {
                        needsSpecialization = true;
                        break;
                    }
                }
                if (needsSpecialization) {
                    std::string specializedStructName = pt.GenericName + "_" + suffix;
                    auto existing = context.StructTypeIndex.find(specializedStructName);
                    if (existing == context.StructTypeIndex.end()) {
                        std::vector<std::pair<std::string, FluxType>> concreteFields;
                        for (auto& [fName, fType] : structInfo.Fields) {
                            if (fType.isGeneric()) {
                                auto mapIt = typeMap.find(fType.GenericName);
                                if (mapIt != typeMap.end())
                                    concreteFields.push_back({fName, mapIt->second});
                                else
                                    concreteFields.push_back({fName, fType});
                            } else {
                                concreteFields.push_back({fName, fType});
                            }
                        }
                        std::vector<llvm::Type*> fieldTypes;
                        for (auto& [fName, fType] : concreteFields) {
                            resolveUserStructType(fType, context);
                            resolveUserEnumType(fType, context);
                            fieldTypes.push_back(fType.getLLVMType(context.TheContext));
                        }
                        auto* llvmStructTy =
                            llvm::StructType::create(context.TheContext, fieldTypes, specializedStructName);
                        int newId = static_cast<int>(context.StructTypes.size());
                        CodegenContext::StructTypeInfo newInfo;
                        newInfo.Name = specializedStructName;
                        newInfo.Fields = concreteFields;
                        newInfo.LLVMType = llvmStructTy;
                        context.StructTypes.push_back(std::move(newInfo));
                        context.StructTypeIndex[specializedStructName] = newId;
                    }
                    pt.StructTypeId = context.StructTypeIndex[specializedStructName];
                    pt.GenericName = specializedStructName;
                }
            }
        }
    }

    CodegenContext::EnumTypeInfo info;
    info.Name = mangledName;
    info.Variants = variants;
    info.VariantPayloads = concretePayloads;

    llvm::StructType* taggedUnionTy = nullptr;
    bool hasPayload = false;
    for (auto& pt : concretePayloads) {
        if (pt.Kind != TypeKind::Void) {
            hasPayload = true;
            break;
        }
    }
    if (hasPayload) {
        llvm::SmallVector<llvm::Type*, 4> memberTypes;
        memberTypes.push_back(llvm::Type::getInt32Ty(context.TheContext));
        info.VariantIsBoxed.resize(concretePayloads.size(), false);

        llvm::Type* largestTy = llvm::Type::getInt8Ty(context.TheContext);
        for (size_t i = 0; i < concretePayloads.size(); ++i) {
            auto& pt = concretePayloads[i];
            if (pt.Kind != TypeKind::Void) {
                resolveUserStructType(pt, context);
                resolveUserEnumType(pt, context);
                llvm::Type* payloadLLVM = pt.getLLVMType(context.TheContext);
                if (!payloadLLVM) {
                    std::cerr << "Failed to get LLVM type for payload of enum " << mangledName << std::endl;
                    return "";
                }
                uint64_t sizeInBits = context.TheModule->getDataLayout().getTypeSizeInBits(payloadLLVM);

                llvm::Type* effectiveLLVMTy = payloadLLVM;
                if (sizeInBits > 128) {
                    info.VariantIsBoxed[i] = true;
                    effectiveLLVMTy = llvm::PointerType::get(context.TheContext, 0);
                }

                if (context.TheModule->getDataLayout().getTypeSizeInBits(effectiveLLVMTy) >
                    context.TheModule->getDataLayout().getTypeSizeInBits(largestTy))
                    largestTy = effectiveLLVMTy;
            }
        }
        memberTypes.push_back(largestTy);
        taggedUnionTy = llvm::StructType::create(context.TheContext, memberTypes, mangledName);
        info.LLVMType = taggedUnionTy;
    }

    int enumId = static_cast<int>(context.EnumTypes.size());
    context.EnumTypes.push_back(std::move(info));
    context.EnumTypeIndex[mangledName] = enumId;

    return mangledName;
}

static bool checkNotMoved(const std::string& name, CodegenContext& context)
{
    if (context.MovedVariables.count(name)) {
        std::cerr << "[FLUX ERROR] use of moved variable '" << name << "'" << std::endl;
        return false;
    }
    return true;
}

static llvm::Value* boolCondition(llvm::Value* V, llvm::IRBuilder<>& Builder, llvm::LLVMContext& Ctx)
{
    if (V->getType()->isIntegerTy(1))
        return V;
    return Builder.CreateFCmpONE(V, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "boolcond");
}

TypedValue NumberExprAST::codegen(CodegenContext& context)
{
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
    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), finalVal),
                      FluxType(TypeKind::Double, dims));
}

TypedValue IntExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    return TypedValue(llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(context.TheContext), Val),
                      FluxType(TypeKind::Int));
}

TypedValue FixedExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::Type* IntTy = llvm::Type::getIntNTy(context.TheContext, Bits);
    double scaled = Val * std::pow(2.0, Fract);
    int64_t fixedVal = (int64_t)std::round(scaled);
    return TypedValue(llvm::ConstantInt::get(IntTy, fixedVal), FluxType(TypeKind::Fixed, Bits, Fract));
}

TypedValue ToFixedExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    TypedValue ValTV = Value->codegen(context);
    if (!ValTV.Val) {
        std::cerr << "[FLUX ERROR] ToFixed value expression failed to codegen" << std::endl;
        return TypedValue();
    }

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
        llvm::Value* Scaled = context.Builder.CreateFMul(
            FloatVal, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
        Res = context.Builder.CreateFPToSI(Scaled, IntTy);
    }

    return TypedValue(Res, FluxType(TypeKind::Fixed, Bits, Fract));
}

TypedValue ComplexExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Create <2 x double> vector: <Real, Imag>
    llvm::Constant* RealVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Real);
    llvm::Constant* ImagVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Imag);
    llvm::Constant* Elts[] = {RealVal, ImagVal};
    llvm::Value* ComplexVal = llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant*>(Elts, 2));

    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue PhasorExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // Codegen magnitude and phase
    TypedValue MagTV = Magnitude->codegen(context);
    if (!MagTV.Val) {
        std::cerr << "[FLUX ERROR] Phasor magnitude expression failed to codegen" << std::endl;
        return TypedValue();
    }
    llvm::Value* Mag = MagTV.Val;
    if (MagTV.Type.Kind == TypeKind::Int)
        Mag = context.Builder.CreateSIToFP(Mag, DoubleTy, "mag_dbl");

    TypedValue PhaseTV = PhaseDeg->codegen(context);
    if (!PhaseTV.Val) {
        std::cerr << "[FLUX ERROR] Phasor phase expression failed to codegen" << std::endl;
        return TypedValue();
    }
    llvm::Value* Phase = PhaseTV.Val;
    if (PhaseTV.Type.Kind == TypeKind::Int)
        Phase = context.Builder.CreateSIToFP(Phase, DoubleTy, "phase_dbl");

    // phase_rad = phase_deg * pi / 180
    llvm::Value* Deg2Rad = llvm::ConstantFP::get(DoubleTy, 3.141592653589793 / 180.0);
    llvm::Value* PhaseRad = context.Builder.CreateFMul(Phase, Deg2Rad, "phase_rad");

    // Get cos and sin intrinsics
    llvm::Function* CosF = llvm::Intrinsic::getOrInsertDeclaration(context.TheModule, llvm::Intrinsic::cos, {DoubleTy});
    llvm::Function* SinF = llvm::Intrinsic::getOrInsertDeclaration(context.TheModule, llvm::Intrinsic::sin, {DoubleTy});

    llvm::Value* CosVal = context.Builder.CreateCall(CosF, {PhaseRad}, "cos_phase");
    llvm::Value* SinVal = context.Builder.CreateCall(SinF, {PhaseRad}, "sin_phase");

    // real = mag * cos(phase), imag = mag * sin(phase)
    llvm::Value* Real = context.Builder.CreateFMul(Mag, CosVal, "real");
    llvm::Value* Imag = context.Builder.CreateFMul(Mag, SinVal, "imag");

    // Build <2 x double> complex vector
    llvm::Value* ComplexVal = llvm::PoisonValue::get(llvm::VectorType::get(DoubleTy, 2, false));
    llvm::Value* ZeroIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0);
    llvm::Value* OneIdx = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1);
    ComplexVal = context.Builder.CreateInsertElement(ComplexVal, Real, ZeroIdx, "real_ins");
    ComplexVal = context.Builder.CreateInsertElement(ComplexVal, Imag, OneIdx, "imag_ins");

    return TypedValue(ComplexVal, TypeKind::Complex);
}

TypedValue StringExprAST::codegen(CodegenContext& context)
{
    llvm::Value* StrPtr = context.Builder.CreateGlobalString(Val, "str");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* DoubleVal =
        context.Builder.CreateBitCast(IntPtr, llvm::Type::getDoubleTy(context.TheContext), "strptr_double");
    return TypedValue(DoubleVal, TypeKind::String);
}

TypedValue InterpolatedStringExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);

    // Helper to get/create extern function declarations
    auto getOrCreateFn = [&](const std::string& name, llvm::Type* retTy, auto params) -> llvm::Function* {
        llvm::Function* F = context.TheModule->getFunction(name);
        if (!F) {
            std::vector<llvm::Type*> paramVec(params.begin(), params.end());
            F = llvm::Function::Create(llvm::FunctionType::get(retTy, paramVec, false), llvm::Function::ExternalLinkage,
                                       name, context.TheModule);
        }
        return F;
    };

    llvm::Function* ConcatF =
        getOrCreateFn("flux_string_concat", DoubleTy, std::vector<llvm::Type*>{DoubleTy, DoubleTy});
    llvm::Function* ToStrF = getOrCreateFn("flux_double_to_string", DoubleTy, std::vector<llvm::Type*>{DoubleTy});

    llvm::Value* Result = nullptr;

    for (const auto& part : Parts) {
        llvm::Value* PartVal = nullptr;
        if (!part.isExpr) {
            // Static text — use permanent string
            llvm::Value* StrPtr = context.Builder.CreateGlobalString(part.text, "strpart");
            llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
            PartVal = context.Builder.CreateBitCast(IntPtr, DoubleTy, "strpart");
        } else {
            // Expression — evaluate and convert to string
            TypedValue ExprTV = part.expr->codegen(context);
            if (!ExprTV.Val) {
                std::cerr << "[FLUX ERROR] String interpolation sub-expression failed to codegen" << std::endl;
                return TypedValue();
            }
            if (ExprTV.Type.Kind == TypeKind::String) {
                PartVal = ExprTV.Val;
            } else {
                // Convert to double first (handles int, bool, etc.)
                llvm::Value* DoubleVal = ExprTV.Val;
                if (ExprTV.Type.Kind == TypeKind::Int || ExprTV.Type.Kind == TypeKind::Bool) {
                    if (DoubleVal->getType()->isIntegerTy(1))
                        DoubleVal = context.Builder.CreateUIToFP(DoubleVal, DoubleTy, "bool_to_dbl");
                    else if (DoubleVal->getType()->isIntegerTy())
                        DoubleVal = context.Builder.CreateSIToFP(DoubleVal, DoubleTy, "int_to_dbl");
                }
                PartVal = context.Builder.CreateCall(ToStrF, {DoubleVal}, "valtostr");
            }
        }

        if (!Result) {
            Result = PartVal;
        } else {
            Result = context.Builder.CreateCall(ConcatF, {Result, PartVal}, "strcat");
        }
    }

    return TypedValue(Result ? Result : llvm::ConstantFP::get(DoubleTy, 0.0), TypeKind::String);
}

TypedValue BoolExprAST::codegen(CodegenContext& context)
{
    llvm::Value* V =
        Val ? llvm::ConstantInt::getTrue(context.TheContext) : llvm::ConstantInt::getFalse(context.TheContext);
    return TypedValue(V, TypeKind::Bool);
}
TypedValue ImportExprAST::codegen(CodegenContext& context)
{
    if (context.importModuleFn) {
        if (!context.importModuleFn(ModuleName, Alias, Symbols)) {
            std::cerr << "Import failed: " << ModuleName << "\n";
            return TypedValue();
        }
    } else {
        std::cerr << "Warning: import '" << ModuleName << "' inside function body but no import callback configured\n";
    }
    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);
}

TypedValue BlockExprAST::codegen(CodegenContext& context)
{
    TypedValue LastVal(nullptr, TypeKind::Void);

    // Save current scope
    auto OldNamedValues = context.NamedValues;
    auto OldNamedTypes = context.NamedTypes;
    auto OldMovedVariables = context.MovedVariables;

    for (size_t i = 0; i < Statements.size(); ++i) {
        if (context.Builder.GetInsertBlock()->getTerminator()) {
            break;
        }

        LastVal = Statements[i]->codegen(context);
        if (!LastVal.Val && LastVal.Type.Kind != TypeKind::Void) {
            // Restore scope on failure
            context.NamedValues = OldNamedValues;
            context.NamedTypes = OldNamedTypes;
            context.MovedVariables = OldMovedVariables;
            return TypedValue(nullptr, TypeKind::Void);
        }
    }

    // Check for variables falling out of scope that have onDestroy methods (including parents, derived-to-base order)
    for (const auto& [varName, allocaVal] : context.NamedValues) {
        if (OldNamedValues.find(varName) == OldNamedValues.end()) {
            auto typeIt = context.NamedTypes.find(varName);
            if (typeIt != context.NamedTypes.end()) {
                FluxType varType = typeIt->second;
                if (varType.Kind == TypeKind::UserStruct && varType.StructTypeId >= 0 &&
                    varType.StructTypeId < static_cast<int>(context.StructTypes.size())) {
                    std::string structName = context.StructTypes[varType.StructTypeId].Name;

                    std::vector<std::string> destroyChain;
                    std::string currentDestroyType = structName;
                    while (!currentDestroyType.empty()) {
                        destroyChain.push_back(currentDestroyType);
                        auto indexIt = context.StructTypeIndex.find(currentDestroyType);
                        if (indexIt != context.StructTypeIndex.end()) {
                            int curId = indexIt->second;
                            if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                                currentDestroyType = context.StructTypes[curId].ParentName;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }

                    for (const std::string& chainTypeName : destroyChain) {
                        auto methodsIt = context.TypeMethods.find(chainTypeName);
                        if (methodsIt != context.TypeMethods.end()) {
                            auto destroyIt = methodsIt->second.find("onDestroy");
                            if (destroyIt != methodsIt->second.end()) {
                                llvm::Function* destroyFn = destroyIt->second;
                                if (destroyFn) {
                                    auto indexIt = context.StructTypeIndex.find(chainTypeName);
                                    if (indexIt != context.StructTypeIndex.end()) {
                                        int curId = indexIt->second;
                                        if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                                            auto& structInfo = context.StructTypes[curId];
                                            llvm::StructType* curLLVMType = structInfo.LLVMType;
                                            FluxType curStructType = FluxType::userStruct(curId, curLLVMType);

                                            llvm::Value* selfArg = allocaVal;
                                            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(allocaVal)) {
                                                if (allocaInst->getAllocatedType()->isPointerTy()) {
                                                    selfArg = context.Builder.CreateLoad(allocaInst->getAllocatedType(),
                                                                                         allocaInst, "self_ptr");
                                                }
                                            }
                                            llvm::Type* expectedType = destroyFn->getFunctionType()->getParamType(0);
                                            if (selfArg->getType() != expectedType) {
                                                selfArg = context.Builder.CreateBitCast(selfArg, expectedType);
                                            }

                                            std::vector<llvm::Value*> destroyArgs;
                                            if (shouldPassByPointer(curStructType, context)) {
                                                destroyArgs.push_back(selfArg);
                                            } else {
                                                llvm::Value* loadedVal =
                                                    context.Builder.CreateLoad(curLLVMType, selfArg, "self_destroy");
                                                destroyArgs.push_back(loadedVal);
                                            }
                                            context.Builder.CreateCall(destroyFn, destroyArgs);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Restore scope
    context.NamedValues = OldNamedValues;
    context.NamedTypes = OldNamedTypes;
    context.MovedVariables = OldMovedVariables;
    return LastVal;
}
TypedValue VariableExprAST::codegen(CodegenContext& context)
{
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
    if (V && !checkNotMoved(trimmedName, context)) {
        return TypedValue();
    }
    if (!V) {
        // Check if it's a function in the module (function pointer)
        if (auto* Fn = context.TheModule->getFunction(trimmedName)) {
            // Store function pointer as double: ptr → ptrtoint to i64 → bitcast to double
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Value* fnInt = context.Builder.CreatePtrToInt(Fn, Int64Ty, "fnptr_int");
            llvm::Value* fnDouble = context.Builder.CreateBitCast(fnInt, DoubleTy, "fnptr_double");
            FluxType FnTy(TypeKind::Double);
            FnTy.GenericName = trimmedName;
            return TypedValue(fnDouble, FnTy);
        }
        std::cerr << "Unknown variable name: " << Name << " (trimmed: '" << trimmedName << "')" << std::endl;
        std::cerr << "  Available variables: ";
        for (const auto& [k, v] : context.NamedValues) {
            std::cerr << "'" << k << "' ";
        }
        std::cerr << std::endl;
        std::cerr << "  NamedTypes: ";
        for (const auto& [k, v] : context.NamedTypes) {
            std::cerr << "'" << k << "'=" << static_cast<int>(v.Kind) << " ";
        }
        std::cerr << std::endl;
        return TypedValue();
    }
    if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
        llvm::Type* Ty = Alloca->getAllocatedType();
        llvm::Value* LoadedV = context.Builder.CreateLoad(Ty, Alloca, Name.c_str());

        FluxType FTy;
        if (context.NamedTypes.count(trimmedName)) {
            FTy = context.NamedTypes[trimmedName];
            resolveUserStructType(FTy, context);
            resolveUserEnumType(FTy, context);
        } else {
            FTy = typeFromLLVM(Ty);
        }
        return TypedValue(LoadedV, FTy);
    }

    FluxType FTy;
    if (context.NamedTypes.count(Name)) {
        FTy = context.NamedTypes[Name];
        resolveUserStructType(FTy, context);
        resolveUserEnumType(FTy, context);
    } else {
        FTy = typeFromLLVM(V->getType());
    }
    return TypedValue(V, FTy);
}

TypedValue MemberExprAST::codegen(CodegenContext& context)
{
    // Check for enum variant access first: EnumName.Variant
    if (auto* OBJ = dynamic_cast<VariableExprAST*>(Object.get())) {
        const std::string& objName = OBJ->getName();
        auto enumIt = context.EnumTypeIndex.find(objName);
        if (enumIt != context.EnumTypeIndex.end()) {
            int enumTypeId = enumIt->second;
            if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
                auto& enumInfo = context.EnumTypes[enumTypeId];
                for (size_t i = 0; i < enumInfo.Variants.size(); ++i) {
                    if (enumInfo.Variants[i] == MemberName) {
                        llvm::Type* int32Ty = llvm::Type::getInt32Ty(context.TheContext);
                        llvm::Value* disc = llvm::ConstantInt::get(int32Ty, static_cast<int>(i));

                        // If the enum has any payload-bearing variants, return a proper tagged union
                        if (enumInfo.LLVMType) {
                            // Check if any variant has a payload
                            bool hasPayload = false;
                            for (auto& pt : enumInfo.VariantPayloads) {
                                if (pt.Kind != TypeKind::Void) {
                                    hasPayload = true;
                                    break;
                                }
                            }
                            if (hasPayload) {
                                llvm::StructType* taggedTy = enumInfo.LLVMType;
                                llvm::Value* structPtr =
                                    context.Builder.CreateAlloca(taggedTy, nullptr, MemberName + "_enum");
                                llvm::Value* tagPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 0, "tag");
                                context.Builder.CreateStore(disc, tagPtr);
                                llvm::Value* loaded = context.Builder.CreateLoad(taggedTy, structPtr, MemberName);
                                return TypedValue(loaded, FluxType::userEnum(enumTypeId, taggedTy));
                            }
                        }
                        return TypedValue(disc, FluxType::userEnum(enumTypeId));
                    }
                }
                std::cerr << "Enum " << objName << " has no variant named '" << MemberName << "'" << std::endl;
                return TypedValue();
            }
        }
    }

    // Try to codegen the object as an expression (for struct field access)
    TypedValue objVal = Object->codegen(context);
    if (objVal.Val && objVal.Type.Kind == TypeKind::Ref && objVal.Type.RefInnerType) {
        FluxType innerTy = *objVal.Type.RefInnerType;
        resolveUserStructType(innerTy, context);
        resolveUserEnumType(innerTy, context);
        objVal = TypedValue(objVal.Val, innerTy);
    }
    if (objVal.Val && objVal.Type.isStruct()) {
        // Struct field access: GEP + load (same as before)
        int typeId = objVal.Type.StructTypeId;
        if (typeId < 0 || typeId >= static_cast<int>(context.StructTypes.size())) {
            std::cerr << "Invalid struct type ID: " << typeId << std::endl;
            return TypedValue();
        }
        auto& structInfo = context.StructTypes[typeId];

        // Find field index by name
        int fieldIdx = -1;
        llvm::Type* fieldLLVMType = nullptr;
        FluxType fieldFluxType;
        for (size_t i = 0; i < structInfo.Fields.size(); ++i) {
            if (structInfo.Fields[i].first == MemberName) {
                fieldIdx = static_cast<int>(i);
                fieldFluxType = structInfo.Fields[i].second;
                // Substitute generic type parameters using the object's concrete GenericArgs
                if (fieldFluxType.Kind == TypeKind::Generic && !objVal.Type.GenericArgs.empty()) {
                    std::vector<std::string> genericParamNames;
                    for (auto& [fn, ft] : structInfo.Fields) {
                        if (ft.Kind == TypeKind::Generic) {
                            bool found = false;
                            for (auto& n : genericParamNames) {
                                if (n == ft.GenericName) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                genericParamNames.push_back(ft.GenericName);
                        }
                    }
                    for (size_t gi = 0; gi < genericParamNames.size() && gi < objVal.Type.GenericArgs.size(); ++gi) {
                        if (genericParamNames[gi] == fieldFluxType.GenericName) {
                            fieldFluxType = objVal.Type.GenericArgs[gi];
                            break;
                        }
                    }
                }
                // Fallback: if still generic, look up the specialized struct by mangled name
                // from StructTypeIndex (e.g., "Pair_Double_Double" for Pair[Double, Double])
                if (fieldFluxType.Kind == TypeKind::Generic && objVal.Type.Kind == TypeKind::UserStruct) {
                    for (auto& [specName, specId] : context.StructTypeIndex) {
                        if (specId >= 0 && specId < static_cast<int>(context.StructTypes.size()) &&
                            specName != structInfo.Name && specName.find(structInfo.Name + "_") == 0) {
                            auto& specInfo = context.StructTypes[specId];
                            for (size_t si = 0; si < specInfo.Fields.size(); ++si) {
                                if (specInfo.Fields[si].first == MemberName &&
                                    specInfo.Fields[si].second.Kind != TypeKind::Generic) {
                                    fieldFluxType = specInfo.Fields[si].second;
                                    resolveUserStructType(fieldFluxType, context);
                                    resolveUserEnumType(fieldFluxType, context);
                                    fieldLLVMType = fieldFluxType.getLLVMType(context.TheContext);
                                    goto field_found;
                                }
                            }
                        }
                    }
                }
                resolveUserStructType(fieldFluxType, context);
                resolveUserEnumType(fieldFluxType, context);
                fieldLLVMType = fieldFluxType.getLLVMType(context.TheContext);
            field_found:;
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "Struct " << structInfo.Name << " has no field named '" << MemberName << "'" << std::endl;
            return TypedValue();
        }

        llvm::Value* structPtr = nullptr;
        if (objVal.Val->getType()->isPointerTy()) {
            structPtr = objVal.Val;
        } else if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(objVal.Val)) {
            structPtr = allocaInst;
        } else {
            structPtr = context.Builder.CreateAlloca(objVal.Val->getType(), nullptr, "structtmp");
            context.Builder.CreateStore(objVal.Val, structPtr);
        }

        if (structPtr->getType() != llvm::PointerType::get(context.TheContext, 0)) {
            structPtr = context.Builder.CreateBitCast(structPtr, llvm::PointerType::get(context.TheContext, 0));
        }

        if (!structInfo.LLVMType) {
            std::cerr << "Struct " << structInfo.Name << " has no LLVM type" << std::endl;
            return TypedValue();
        }
        llvm::Value* gep = context.Builder.CreateStructGEP(structInfo.LLVMType, structPtr, fieldIdx, MemberName);
        llvm::Value* loaded = context.Builder.CreateLoad(fieldLLVMType, gep, MemberName);
        return TypedValue(loaded, fieldFluxType);
    }

    // Handle enum payload field access: enum_var.field_name
    // e.g. x.value where x : Option[Double] and Some { value: T }
    if (objVal.Val && objVal.Type.Kind == TypeKind::UserEnum) {
        int enumTypeId = objVal.Type.EnumTypeId;
        if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
            auto& enumInfo = context.EnumTypes[enumTypeId];
            // Iterate variants to find one whose payload struct has MemberName
            for (size_t vi = 0; vi < enumInfo.Variants.size(); ++vi) {
                if (enumInfo.VariantPayloads[vi].Kind == TypeKind::Void)
                    continue;
                // Look up the synthetic payload struct (e.g. __enum_Option_Some_Fields)
                std::string payloadStructName = "__enum_" + enumInfo.Name + "_" + enumInfo.Variants[vi] + "_Fields";
                auto structIt = context.StructTypeIndex.find(payloadStructName);
                if (structIt == context.StructTypeIndex.end())
                    continue;
                int payloadStructId = structIt->second;
                if (payloadStructId < 0 || payloadStructId >= static_cast<int>(context.StructTypes.size()))
                    continue;
                auto& payloadStructInfo = context.StructTypes[payloadStructId];
                // Find field index by name
                int fieldIdx = -1;
                for (size_t fi = 0; fi < payloadStructInfo.Fields.size(); ++fi) {
                    if (payloadStructInfo.Fields[fi].first == MemberName) {
                        fieldIdx = static_cast<int>(fi);
                        break;
                    }
                }
                if (fieldIdx < 0)
                    continue; // field not in this variant's payload, try next
                // Extract payload from tagged union { i32 tag, <payload> }
                llvm::Value* structPtr = nullptr;
                if (objVal.Val->getType()->isPointerTy()) {
                    structPtr = objVal.Val;
                } else {
                    structPtr = context.Builder.CreateAlloca(objVal.Val->getType(), nullptr, "enumtmp");
                    context.Builder.CreateStore(objVal.Val, structPtr);
                }
                // GEP to payload slot (element 1) in the tagged union
                llvm::StructType* taggedTy = enumInfo.LLVMType;
                if (!taggedTy)
                    continue;
                llvm::Value* payloadPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 1, "payload");
                // Resolve the field LLVM type from the payload struct info
                FluxType fieldFluxType = payloadStructInfo.Fields[fieldIdx].second;
                resolveUserStructType(fieldFluxType, context);
                resolveUserEnumType(fieldFluxType, context);
                llvm::Type* fieldLLVMType = fieldFluxType.getLLVMType(context.TheContext);
                // Check if this variant is boxed (heap-allocated payload)
                bool isBoxed = vi < enumInfo.VariantIsBoxed.size() ? enumInfo.VariantIsBoxed[vi] : false;
                llvm::Value* fieldPtr = nullptr;
                if (isBoxed) {
                    // Boxed: load heap pointer from union slot, then GEP into heap
                    llvm::Value* heapPtr = context.Builder.CreateLoad(llvm::PointerType::get(context.TheContext, 0),
                                                                      payloadPtr, "heap_ptr");
                    llvm::Value* concretePtr = context.Builder.CreatePointerCast(
                        heapPtr, llvm::PointerType::get(context.TheContext, 0), "concrete_ptr");
                    fieldPtr =
                        context.Builder.CreateStructGEP(payloadStructInfo.LLVMType, concretePtr, fieldIdx, MemberName);
                } else {
                    // Non-boxed: direct GEP into the union's payload slot
                    fieldPtr =
                        context.Builder.CreateStructGEP(payloadStructInfo.LLVMType, payloadPtr, fieldIdx, MemberName);
                }
                llvm::Value* loaded = context.Builder.CreateLoad(fieldLLVMType, fieldPtr, MemberName);
                return TypedValue(loaded, fieldFluxType);
            }
        }
    }

    // Check for SPICE parameter probe: device.param (only when object is a variable)
    if (auto* OBJ = dynamic_cast<VariableExprAST*>(Object.get())) {
        const std::string& objName = OBJ->getName();
        std::string full_name = objName + "." + MemberName;

        // Check if there is a local variable with this full name first (e.g. from an import)
        if (context.NamedValues.count(full_name)) {
            llvm::Value* V = context.NamedValues[full_name];
            if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
                return TypedValue(context.Builder.CreateLoad(Alloca->getAllocatedType(), Alloca, full_name.c_str()),
                                  typeFromLLVM(Alloca->getAllocatedType()));
            }
            return TypedValue(V, typeFromLLVM(V->getType()));
        }

        // Treat as SPICE parameter probe: device.param
        llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        if (!GetPF) {
            GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                           llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule);
        }

        llvm::Value* StrPtr = context.Builder.CreateGlobalString(full_name, "ptr_double");
        llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
        llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

        return TypedValue(context.Builder.CreateCall(GetPF, {PtrDouble}, "getp"), TypeKind::Double);
    }
    std::cerr << "Member access only supported on struct types and top-level variables currently" << std::endl;
    return TypedValue();
}

TypedValue AssignExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);

    if (auto* UNARY = dynamic_cast<UnaryExprAST*>(LHS.get())) {
        if (UNARY->getOp() == '*') {
            TypedValue PtrTV = const_cast<ExprAST*>(UNARY->getOperand())->codegen(context);
            if (!PtrTV.Val || !PtrTV.Val->getType()->isPointerTy()) {
                std::cerr << "[FLUX ERROR] Cannot assign through non-pointer dereference" << std::endl;
                return TypedValue();
            }
            TypedValue ValTV = Val->codegen(context);
            if (!ValTV.Val) {
                std::cerr << "[FLUX ERROR] Dereference assignment value expression failed to codegen" << std::endl;
                return TypedValue();
            }
            llvm::Type* InnerTy = nullptr;
            if (PtrTV.Type.Kind == TypeKind::Ref && PtrTV.Type.RefInnerType) {
                InnerTy = PtrTV.Type.RefInnerType->getLLVMType(context.TheContext);
            }
            llvm::Value* StoreVal = ValTV.Val;
            if (InnerTy && StoreVal->getType() != InnerTy) {
                if (StoreVal->getType()->isFloatingPointTy() && InnerTy->isIntegerTy())
                    StoreVal = context.Builder.CreateFPToSI(StoreVal, InnerTy, "ref_cast");
                else if (StoreVal->getType()->isIntegerTy() && InnerTy->isFloatingPointTy())
                    StoreVal = context.Builder.CreateSIToFP(StoreVal, InnerTy, "ref_cast");
                else
                    StoreVal = context.Builder.CreateBitCast(StoreVal, InnerTy, "ref_cast");
            }
            context.Builder.CreateStore(StoreVal, PtrTV.Val);
            return ValTV;
        }
    }

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
        if (!array || !rowIdx || !colIdx) {
            std::cerr << "[FLUX ERROR] Matrix assignment row/col index failed to codegen" << std::endl;
            return TypedValue();
        }
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
        if (!RowTV.Val || !ColTV.Val) {
            std::cerr << "[FLUX ERROR] Matrix assignment row/col index expression failed to codegen" << std::endl;
            return TypedValue();
        }
        llvm::Value *RowV = RowTV.Val, *ColV = ColTV.Val;
        if (RowV->getType()->isFloatingPointTy())
            RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        if (ColV->getType()->isFloatingPointTy())
            ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        if (Op != 0) {
            llvm::Function* GetF = context.TheModule->getFunction("flux_matrix_get");
            if (!GetF)
                GetF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {VoidPtrTy, Int32Ty, Int32Ty}, false),
                                              llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule);
            llvm::Value* CurV = context.Builder.CreateCall(GetF, {MatPtr, RowV, ColV}, "curval");
            TypedValue RHSVal = Val->codegen(context);
            if (!RHSVal.Val) {
                std::cerr << "[FLUX ERROR] Compound-assign RHS expression failed to codegen" << std::endl;
                return TypedValue();
            }
            llvm::Value* RHSV = RHSVal.Val;
            if (RHSV->getType()->isIntegerTy() && !RHSV->getType()->isIntegerTy(1))
                RHSV = context.Builder.CreateSIToFP(RHSV, DoubleTy, "rhs_double");
            switch (Op) {
            case '+':
                CurV = context.Builder.CreateFAdd(CurV, RHSV, "addtmp");
                break;
            case '-':
                CurV = context.Builder.CreateFSub(CurV, RHSV, "subtmp");
                break;
            case '*':
                CurV = context.Builder.CreateFMul(CurV, RHSV, "multmp");
                break;
            case '/':
                CurV = context.Builder.CreateFDiv(CurV, RHSV, "divtmp");
                break;
            }
            TypedValue NewValTV(CurV, TypeKind::Double);
            llvm::Function* SetF = context.TheModule->getFunction("flux_matrix_set");
            if (!SetF)
                SetF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                                      {VoidPtrTy, Int32Ty, Int32Ty, DoubleTy}, false),
                                              llvm::Function::ExternalLinkage, "flux_matrix_set", context.TheModule);
            context.Builder.CreateCall(SetF, {MatPtr, RowV, ColV, CurV});
            return NewValTV;
        } else {
            TypedValue NewValTV = Val->codegen(context);
            if (!NewValTV.Val) {
                std::cerr << "[FLUX ERROR] Matrix assignment value expression failed to codegen" << std::endl;
                return TypedValue();
            }
            llvm::Value* NewValV = NewValTV.Val;
            if (NewValV->getType()->isIntegerTy() && !NewValV->getType()->isIntegerTy(1))
                NewValV = context.Builder.CreateSIToFP(NewValV, DoubleTy, "val_double");
            llvm::Function* SetF = context.TheModule->getFunction("flux_matrix_set");
            if (!SetF)
                SetF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext),
                                                                      {VoidPtrTy, Int32Ty, Int32Ty, DoubleTy}, false),
                                              llvm::Function::ExternalLinkage, "flux_matrix_set", context.TheModule);
            context.Builder.CreateCall(SetF, {MatPtr, RowV, ColV, NewValV});
            return NewValTV;
        }
    }

    if (auto* VAR = dynamic_cast<VariableExprAST*>(LHS.get())) {
        TargetName = VAR->getName();
        Variable = context.NamedValues[TargetName];
    } else if (auto* MEM = dynamic_cast<MemberExprAST*>(LHS.get())) {
        // Check if object is a variable (for in-place struct field assignment)
        if (auto* objVar = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(MEM->getObject()))) {
            const std::string& objName = objVar->getName();
            llvm::Value* objAlloca = context.NamedValues[objName];
            if (objAlloca && llvm::isa<llvm::AllocaInst>(objAlloca)) {
                // Check type of the variable
                auto typeIt = context.NamedTypes.find(objName);
                FluxType varType = (typeIt != context.NamedTypes.end()) ? typeIt->second : FluxType();
                bool isRefType = (varType.Kind == TypeKind::Ref && varType.RefInnerType);
                if (isRefType) {
                    varType = *varType.RefInnerType;
                    resolveUserStructType(varType, context);
                    resolveUserEnumType(varType, context);
                }
                if (varType.isStruct()) {
                    int typeId = varType.StructTypeId;
                    if (typeId >= 0 && typeId < static_cast<int>(context.StructTypes.size())) {
                        auto& structInfo = context.StructTypes[typeId];
                        int fieldIdx = -1;
                        for (size_t i = 0; i < structInfo.Fields.size(); ++i) {
                            if (structInfo.Fields[i].first == MEM->getMemberName()) {
                                fieldIdx = static_cast<int>(i);
                                break;
                            }
                        }
                        if (fieldIdx >= 0) {
                            if (!structInfo.LLVMType) {
                                std::cerr << "Struct " << structInfo.Name << " has no LLVM type" << std::endl;
                                return TypedValue();
                            }
                            auto* allocaInst = llvm::cast<llvm::AllocaInst>(objAlloca);
                            llvm::Value* structPtr = objAlloca;
                            if (allocaInst->getAllocatedType()->isPointerTy()) {
                                structPtr =
                                    context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, "ptr_val");
                            }
                            if (structPtr->getType() != llvm::PointerType::get(context.TheContext, 0)) {
                                structPtr = context.Builder.CreateBitCast(
                                    structPtr, llvm::PointerType::get(context.TheContext, 0));
                            }
                            llvm::Value* gep = context.Builder.CreateStructGEP(structInfo.LLVMType, structPtr, fieldIdx,
                                                                               MEM->getMemberName());
                            TypedValue rhsVal = Val->codegen(context);
                            if (!rhsVal.Val) {
                                std::cerr << "[FLUX ERROR] Struct field assignment value expression failed to codegen"
                                          << std::endl;
                                return TypedValue();
                            }
                            // Mark non-Copy source variables as moved on field assignment
                            if (!isCopyType(rhsVal.Type)) {
                                if (auto* rhsVar = dynamic_cast<VariableExprAST*>(Val.get())) {
                                    std::string srcName = rhsVar->getName();
                                    context.MovedVariables.insert(srcName);
                                }
                            }
                            llvm::Value* rhsValPtr = rhsVal.Val;
                            if (shouldPassByPointer(rhsVal.Type, context)) {
                                if (rhsValPtr->getType()->isPointerTy()) {
                                    llvm::Type* rhsStructTy = rhsVal.Type.getLLVMType(context.TheContext);
                                    rhsValPtr = context.Builder.CreateLoad(rhsStructTy, rhsValPtr, "rhs_val");
                                }
                            }
                            context.Builder.CreateStore(rhsValPtr, gep);
                            return rhsVal;
                        }
                    }
                }
            }
        }
        // Fall back to SPICE parameter behavior
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
            SetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                           llvm::Function::ExternalLinkage, "flux_set_parameter", context.TheModule);
        }
        TypedValue NewValTV = Val->codegen(context);
        if (!NewValTV.Val) {
            std::cerr << "[FLUX ERROR] SPICE set_parameter value expression failed to codegen" << std::endl;
            return TypedValue();
        }

        llvm::Value* StrPtr = context.Builder.CreateGlobalString(TargetName, "ptr_double");
        llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
        llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

        llvm::Value* ValToSet = NewValTV.Val;
        if (ValToSet->getType()->isIntegerTy()) {
            ValToSet = context.Builder.CreateSIToFP(ValToSet, DoubleTy, "val_double");
        }

        return TypedValue(context.Builder.CreateCall(SetPF, {PtrDouble, ValToSet}, "setp"), TypeKind::Double);
    }

    if (!Variable) {
        // CLEAN SYNTAX ENHANCEMENT: Implicit variable creation
        // If variable doesn't exist, create it in the entry block of the current function
        llvm::BasicBlock* EntryBB = context.Builder.GetInsertBlock();
        if (EntryBB) {
            llvm::Function* TheFunction = EntryBB->getParent();
            if (TheFunction) {
                // Codegen the value first to know its type
                TypedValue ValTV = Val->codegen(context);
                if (!ValTV.Val) {
                    std::cerr << "[FLUX ERROR] Implicit variable creation failed to codegen initializer" << std::endl;
                    return TypedValue();
                }
                // Mark non-Copy source variables as moved on assignment
                if (!isCopyType(ValTV.Type)) {
                    if (auto* rhsVar = dynamic_cast<VariableExprAST*>(Val.get())) {
                        std::string srcName = rhsVar->getName();
                        srcName.erase(0, srcName.find_first_not_of(" "));
                        srcName.erase(srcName.find_last_not_of(" ") + 1);
                        context.MovedVariables.insert(srcName);
                    }
                }
                llvm::Type* AllocaTy = ValTV.Val->getType();
                llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
                Variable = TmpB.CreateAlloca(AllocaTy, nullptr, TargetName);
                context.NamedValues[TargetName] = Variable;
                context.NamedTypes[TargetName] = ValTV.Type;
                context.Builder.CreateStore(ValTV.Val, Variable);
                return ValTV;
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
    FluxType VarFluxTy;
    if (context.NamedTypes.count(TargetName)) {
        VarFluxTy = context.NamedTypes[TargetName];
        resolveUserStructType(VarFluxTy, context);
        resolveUserEnumType(VarFluxTy, context);
    } else {
        VarFluxTy = typeFromLLVM(VarTy);
    }

    TypedValue CurrentTV(context.Builder.CreateLoad(VarTy, Variable, "current"), VarFluxTy);
    TypedValue ValTV = Val->codegen(context);
    if (!ValTV.Val) {
        std::cerr << "[FLUX ERROR] Assignment value expression failed to codegen" << std::endl;
        return TypedValue();
    }

    // Mark non-Copy source variables as moved on assignment
    if (!isCopyType(ValTV.Type)) {
        if (auto* rhsVar = dynamic_cast<VariableExprAST*>(Val.get())) {
            std::string srcName = rhsVar->getName();
            srcName.erase(0, srcName.find_first_not_of(" "));
            srcName.erase(srcName.find_last_not_of(" ") + 1);
            context.MovedVariables.insert(srcName);
        }
    }

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

    llvm::Value* storeVal = ValTV.Val;
    if (shouldPassByPointer(VarFluxTy, context)) {
        if (!storeVal->getType()->isPointerTy()) {
            llvm::Value* tempAlloca = context.Builder.CreateAlloca(storeVal->getType(), nullptr, "assign_temp");
            context.Builder.CreateStore(storeVal, tempAlloca);
            storeVal = tempAlloca;
        }
    }
    context.Builder.CreateStore(storeVal, Variable);
    return ValTV;
}

// Helper to promote a value to a symbolic expression handle
TypedValue UnaryExprAST::codegen(CodegenContext& context)
{
    TypedValue OperandTV = Operand->codegen(context);
    if (!OperandTV.Val) {
        std::cerr << "[FLUX ERROR] Unary operand failed to codegen" << std::endl;
        return TypedValue();
    }

    if (isComplexType(OperandTV)) {
        llvm::Value* Real = context.Builder.CreateExtractElement(OperandTV.Val, (uint64_t)0, "cre");
        llvm::Value* Imag = context.Builder.CreateExtractElement(OperandTV.Val, (uint64_t)1, "cim");
        switch (Op) {
        case '-': {
            llvm::Value* ResReal = context.Builder.CreateFNeg(Real, "negre");
            llvm::Value* ResImag = context.Builder.CreateFNeg(Imag, "negim");
            llvm::Value* Res =
                llvm::UndefValue::get(llvm::VectorType::get(llvm::Type::getDoubleTy(context.TheContext), 2, false));
            Res = context.Builder.CreateInsertElement(Res, ResReal, (uint64_t)0);
            return TypedValue(context.Builder.CreateInsertElement(Res, ResImag, (uint64_t)1),
                              FluxType(TypeKind::Complex, OperandTV.Type.Dimensions));
        }
        case '+':
            return OperandTV;
        default:
            std::cerr << "[FLUX ERROR] Unsupported unary operator" << std::endl;
            return TypedValue();
        }
    }

    // Unary operator overloading for user-defined types
    if (Op == '-') {
        std::string typeName;
        if (OperandTV.Type.Kind == TypeKind::UserStruct && OperandTV.Type.StructTypeId >= 0 &&
            OperandTV.Type.StructTypeId < static_cast<int>(context.StructTypes.size())) {
            typeName = context.StructTypes[OperandTV.Type.StructTypeId].Name;
        } else if (OperandTV.Type.Kind == TypeKind::UserEnum && OperandTV.Type.EnumTypeId >= 0 &&
                   OperandTV.Type.EnumTypeId < static_cast<int>(context.EnumTypes.size())) {
            typeName = context.EnumTypes[OperandTV.Type.EnumTypeId].Name;
        }
        if (!typeName.empty()) {
            auto typeIt = context.TypeMethods.find(typeName);
            if (typeIt != context.TypeMethods.end()) {
                auto methodIt = typeIt->second.find("neg");
                if (methodIt != typeIt->second.end()) {
                    llvm::Function* calleeFn = methodIt->second;
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
                            sretLLVMTy = llvm::cast<llvm::StructType>(
                                FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                        sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "neg_sret");
                        callArgs.push_back(sretPtr);
                    }
                    {
                        llvm::Value* selfVal = OperandTV.Val;
                        if (shouldPassByPointer(OperandTV.Type, context)) {
                            if (!selfVal->getType()->isPointerTy()) {
                                llvm::Value* tempAlloca =
                                    context.Builder.CreateAlloca(selfVal->getType(), nullptr, "neg_self_temp");
                                context.Builder.CreateStore(selfVal, tempAlloca);
                                selfVal = tempAlloca;
                            }
                        } else {
                            if ((OperandTV.Type.Kind == TypeKind::UserStruct ||
                                 OperandTV.Type.Kind == TypeKind::UserEnum) &&
                                selfVal->getType()->isPointerTy()) {
                                llvm::Type* selfLLVMTy = OperandTV.Type.getLLVMType(context.TheContext);
                                selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal);
                            }
                        }
                        unsigned firstArgIdx = isSretCall ? 1 : 0;
                        if (calleeFn->arg_size() > firstArgIdx &&
                            selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
                            llvm::Type* expectedTy = calleeFn->getArg(firstArgIdx)->getType();
                            if (expectedTy->isPointerTy() && selfVal->getType()->isPointerTy())
                                selfVal = context.Builder.CreatePointerCast(selfVal, expectedTy);
                            else if (selfVal->getType()->isIntegerTy() && expectedTy->isFloatingPointTy())
                                selfVal = context.Builder.CreateSIToFP(selfVal, expectedTy);
                            else if (selfVal->getType()->isFloatingPointTy() && expectedTy->isIntegerTy())
                                selfVal = context.Builder.CreateFPToSI(selfVal, expectedTy);
                        }
                        callArgs.push_back(selfVal);
                    }
                    if (isSretCall) {
                        context.Builder.CreateCall(calleeFn, callArgs);
                        llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                        if (!sretLLVMTy)
                            sretLLVMTy = llvm::cast<llvm::StructType>(
                                FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                        llvm::Value* result = context.Builder.CreateLoad(sretLLVMTy, sretPtr);
                        return TypedValue(result, retType);
                    } else {
                        llvm::Value* result = context.Builder.CreateCall(calleeFn, callArgs, "neg");
                        return TypedValue(result, retType);
                    }
                }
            }
        }
    }

    llvm::Value* V = OperandTV.Val;
    switch (Op) {
    case '-':
        if (V->getType()->isIntegerTy())
            return TypedValue(context.Builder.CreateNeg(V, "unaryminus"), OperandTV.Type);
        return TypedValue(context.Builder.CreateFNeg(V, "unaryminus"), OperandTV.Type);
    case '+':
        return OperandTV;
    case '~':
    case static_cast<int>(TokenType::tok_bitwise_not): {
        llvm::Value* OperandInt = context.Builder.CreateFPToSI(V, llvm::Type::getInt64Ty(context.TheContext), "notint");
        return TypedValue(context.Builder.CreateSIToFP(context.Builder.CreateNot(OperandInt, "nottmp"),
                                                       llvm::Type::getDoubleTy(context.TheContext), "nottmpfp"),
                          FluxType(TypeKind::Double, {}));
    }
    case '!':
    case static_cast<int>(TokenType::tok_logical_not): {
        llvm::Value* Cond = boolCondition(V, context.Builder, context.TheContext);
        return TypedValue(context.Builder.CreateNot(Cond, "lognot"), TypeKind::Bool);
    }
    case static_cast<int>(TokenType::tok_bitwise_and):
    case static_cast<int>(TokenType::tok_bitwise_and) + 2600: {
        // &expr or &mut expr: return pointer to the operand's storage
        bool isMut = (Op == static_cast<int>(TokenType::tok_bitwise_and) + 2600);
        if (auto* VarExpr = dynamic_cast<VariableExprAST*>(Operand.get())) {
            const std::string& VarName = VarExpr->getName();
            if (auto it = context.NamedValues.find(VarName); it != context.NamedValues.end()) {
                return TypedValue(it->second, FluxType::reference(OperandTV.Type, isMut));
            }
        }
        if (auto* MemExpr = dynamic_cast<MemberExprAST*>(Operand.get())) {
            auto* ObjExpr = const_cast<ExprAST*>(MemExpr->getObject());
            if (auto* ObjVar = dynamic_cast<VariableExprAST*>(ObjExpr)) {
                const std::string& ObjName = ObjVar->getName();
                llvm::Value* objAlloca = context.NamedValues[ObjName];
                if (objAlloca && llvm::isa<llvm::AllocaInst>(objAlloca)) {
                    auto typeIt = context.NamedTypes.find(ObjName);
                    if (typeIt != context.NamedTypes.end() && typeIt->second.isStruct()) {
                        int typeId = typeIt->second.StructTypeId;
                        if (typeId >= 0 && typeId < static_cast<int>(context.StructTypes.size())) {
                            auto& structInfo = context.StructTypes[typeId];
                            const std::string& fieldName = MemExpr->getMemberName();
                            for (size_t i = 0; i < structInfo.Fields.size(); ++i) {
                                if (structInfo.Fields[i].first == fieldName) {
                                    if (!structInfo.LLVMType)
                                        break;
                                    auto* allocaInst = llvm::cast<llvm::AllocaInst>(objAlloca);
                                    llvm::Value* structPtr = objAlloca;
                                    if (allocaInst->getAllocatedType()->isPointerTy())
                                        structPtr = context.Builder.CreateLoad(allocaInst->getAllocatedType(),
                                                                               allocaInst, "ptr_val");
                                    if (structPtr->getType() != llvm::PointerType::get(context.TheContext, 0))
                                        structPtr = context.Builder.CreateBitCast(
                                            structPtr, llvm::PointerType::get(context.TheContext, 0));
                                    llvm::Value* gep = context.Builder.CreateStructGEP(
                                        structInfo.LLVMType, structPtr, static_cast<unsigned>(i), fieldName);
                                    FluxType fieldType = structInfo.Fields[i].second;
                                    return TypedValue(gep, FluxType::reference(fieldType, isMut));
                                }
                            }
                        }
                    }
                }
            }
        }
        llvm::Type* ValTy = V->getType();
        llvm::Value* Alloca = context.Builder.CreateAlloca(ValTy, nullptr, "ref_slot");
        context.Builder.CreateStore(V, Alloca);
        return TypedValue(Alloca, FluxType::reference(OperandTV.Type, isMut));
    }
    case '*': {
        // *expr: dereference — operand must be a Ref type (pointer)
        if (!V->getType()->isPointerTy()) {
            std::cerr << "[FLUX ERROR] Cannot dereference non-pointer type" << std::endl;
            return TypedValue();
        }
        FluxType innerType = (OperandTV.Type.Kind == TypeKind::Ref && OperandTV.Type.RefInnerType)
                                 ? *OperandTV.Type.RefInnerType
                                 : TypeKind::Double;
        llvm::Type* InnerLLVMTy = innerType.getLLVMType(context.TheContext);
        if (!InnerLLVMTy) {
            std::cerr << "[FLUX ERROR] Cannot dereference: unknown inner type" << std::endl;
            return TypedValue();
        }
        llvm::Value* Loaded = context.Builder.CreateLoad(InnerLLVMTy, V, "deref");
        return TypedValue(Loaded, innerType);
    }
    default:
        std::cerr << "[FLUX ERROR] Unsupported unary operator" << std::endl;
        return TypedValue();
    }
}

TypedValue TransposeExprAST::codegen(CodegenContext& context)
{
    TypedValue V = Operand->codegen(context);
    if (!V.Val) {
        std::cerr << "[FLUX ERROR] Transpose operand expression failed to codegen" << std::endl;
        return TypedValue();
    }

    if (V.Type.Kind == TypeKind::Matrix || V.Type.Kind == TypeKind::ComplexMatrix) {
        bool isComplex = (V.Type.Kind == TypeKind::ComplexMatrix);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        std::string fnName = isComplex ? "flux_complex_matrix_transpose" : "flux_matrix_transpose";
        llvm::Function* TransposeF = context.TheModule->getFunction(fnName);
        if (!TransposeF)
            TransposeF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false),
                                                llvm::Function::ExternalLinkage, fnName, context.TheModule);
        llvm::Value* MatPtr = context.Builder.CreateExtractValue(V.Val, 0, "mat_ptr");
        llvm::Value* Rows = context.Builder.CreateExtractValue(V.Val, 1, "mat_rows");
        llvm::Value* Cols = context.Builder.CreateExtractValue(V.Val, 2, "mat_cols");
        llvm::Value* ResPtr = context.Builder.CreateCall(TransposeF, {MatPtr}, "transtmp");
        llvm::StructType* MatStructTy =
            llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, Cols, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, Rows, 2);
        FluxType matType(isComplex ? TypeKind::ComplexMatrix : TypeKind::Matrix);
        matType.Dimensions = V.Type.Dimensions;
        return TypedValue(MatVal, matType);
    }

    // For scalars, transpose is just identity (or conjugate transpose for complex, but leaving it as is for now)
    return V;
}

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
    context.CurrentLoopCont = LoopBB; // Continue jumps to loop condition

    // Create alloca for the loop variable in the preheader (before loop body
    // starts, so the initial store runs only once, not every iteration).
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
    // Load current value, add step, store back
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
    context.CurrentLoopCont = CondBB; // Continue jumps to condition

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

TypedValue LetExprAST::codegen(CodegenContext& context)
{
    TypedValue InitTV = Init->codegen(context);
    if (!InitTV.Val) {
        std::cerr << "[FLUX ERROR] Let initializer failed to codegen" << std::endl;
        return TypedValue();
    }

    FluxType ActualType = (Type.Kind == TypeKind::Auto) ? InitTV.Type : Type;

    // If the declared type is a trait object (dyn Trait), create a fat pointer
    if (Type.isTraitObject() && InitTV.Type.isStruct()) {
        int traitId = Type.TraitObjectTypeId;
        if (traitId >= 0 && traitId < static_cast<int>(context.Traits.size())) {
            const auto& traitInfo = context.Traits[traitId];
            std::string initTypeName = (InitTV.Type.StructTypeId >= 0 &&
                                        InitTV.Type.StructTypeId < static_cast<int>(context.StructTypes.size()))
                                           ? context.StructTypes[InitTV.Type.StructTypeId].Name
                                           : "";

            if (!initTypeName.empty()) {
                auto vtableIdxIt = context.VTableIndex.find({traitInfo.Name, initTypeName});
                if (vtableIdxIt != context.VTableIndex.end()) {
                    int vtableIdx = vtableIdxIt->second;
                    if (vtableIdx >= 0 && vtableIdx < static_cast<int>(context.VTables.size())) {
                        auto& vtableEntry = context.VTables[vtableIdx];
                        llvm::PointerType* i8PtrTy = llvm::PointerType::get(context.TheContext, 0);

                        llvm::Value* dataPtr = InitTV.Val;
                        if (!dataPtr->getType()->isPointerTy()) {
                            dataPtr = context.Builder.CreateAlloca(dataPtr->getType(), nullptr, "tmp");
                            context.Builder.CreateStore(InitTV.Val, dataPtr);
                        }
                        if (dataPtr->getType() != i8PtrTy)
                            dataPtr = context.Builder.CreatePointerCast(dataPtr, i8PtrTy, "data_ptr");

                        llvm::Value* vtablePtr =
                            context.Builder.CreatePointerCast(vtableEntry.VTableGlobal, i8PtrTy, "vtable_ptr");

                        llvm::Value* fatPtrAlloca = context.Builder.CreateAlloca(Type.getLLVMType(context.TheContext),
                                                                                 nullptr, VarName + "_fat");
                        llvm::Value* dataFieldPtr = context.Builder.CreateStructGEP(
                            Type.getLLVMType(context.TheContext), fatPtrAlloca, 0, "data_field");
                        llvm::Value* vtableFieldPtr = context.Builder.CreateStructGEP(
                            Type.getLLVMType(context.TheContext), fatPtrAlloca, 1, "vtable_field");
                        context.Builder.CreateStore(dataPtr, dataFieldPtr);
                        context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                        InitTV.Val = context.Builder.CreateLoad(Type.getLLVMType(context.TheContext), fatPtrAlloca,
                                                                VarName + "_fat_val");
                        InitTV.Type = Type;
                        ActualType = Type;
                    }
                }
            }
        }
    }

    // Compile-time dimensional analysis: check that the initializer's
    // unit dimensions match the declared type's dimensions.
    if (Type.Kind != TypeKind::Auto) {
        if (Type.Dimensions.mass != InitTV.Type.Dimensions.mass ||
            Type.Dimensions.length != InitTV.Type.Dimensions.length ||
            Type.Dimensions.time != InitTV.Type.Dimensions.time ||
            Type.Dimensions.current != InitTV.Type.Dimensions.current ||
            Type.Dimensions.temperature != InitTV.Type.Dimensions.temperature ||
            Type.Dimensions.amount != InitTV.Type.Dimensions.amount ||
            Type.Dimensions.luminous != InitTV.Type.Dimensions.luminous) {
            llvm::errs() << "[Flux] Unit mismatch: declared type has dimensions " << Type.Dimensions.toString()
                         << " but initializer has dimensions " << InitTV.Type.Dimensions.toString() << "\n";
            return TypedValue();
        }
    }
    llvm::Type* VarTy = ActualType.getLLVMType(context.TheContext);
    llvm::Value* InitV = InitTV.Val;
    if (InitV->getType() != VarTy) {
        if (VarTy->isIntegerTy(1) && InitV->getType()->isFloatingPointTy()) {
            // double → bool: compare against 0.0
            InitV = context.Builder.CreateFCmpONE(
                InitV, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), "dbltobool");
        } else if (VarTy->isIntegerTy() && InitV->getType()->isIntegerTy(1)) {
            // bool → int: zero-extend i1 to i32
            InitV = context.Builder.CreateZExt(InitV, VarTy, "bool_to_int");
        } else if (VarTy->isIntegerTy() && InitV->getType()->isFloatingPointTy())
            InitV = context.Builder.CreateFPToSI(InitV, VarTy, "cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isIntegerTy(1))
            InitV = context.Builder.CreateUIToFP(InitV, VarTy, "bool_cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isIntegerTy())
            InitV = context.Builder.CreateSIToFP(InitV, VarTy, "cast");
        else if (VarTy->isFloatingPointTy() && InitV->getType()->isVectorTy()) {
            // Declared as double but init is complex vector  use vector type
            VarTy = InitV->getType();
        } else if ((VarTy->isStructTy() || VarTy->isVectorTy()) && InitV->getType() != VarTy) {
            // Complex type: use the actual init value type
            VarTy = InitV->getType();
        }
    }
    // If initializer is a variable of non-Copy type, mark source as moved
    if (!isCopyType(InitTV.Type)) {
        if (auto* initVar = dynamic_cast<VariableExprAST*>(Init.get())) {
            std::string srcName = initVar->getName();
            srcName.erase(0, srcName.find_first_not_of(" "));
            srcName.erase(srcName.find_last_not_of(" ") + 1);
            context.MovedVariables.insert(srcName);
        } else if (auto* initMember = dynamic_cast<MemberExprAST*>(Init.get())) {
            if (auto* objVar = dynamic_cast<VariableExprAST*>(const_cast<ExprAST*>(initMember->getObject()))) {
                std::string srcName = objVar->getName();
                context.MovedVariables.insert(srcName);
            }
        }
    }

    // For async functions, place allocas in the entry block so they dominate
    // all resume points (BUG #13 fix: avoid "Instruction does not dominate all uses!")
    llvm::BasicBlock* SavedInsertBB = nullptr;
    if (context.AsyncStateAlloca) {
        SavedInsertBB = context.Builder.GetInsertBlock();
        context.Builder.SetInsertPoint(&SavedInsertBB->getParent()->getEntryBlock(),
                                       SavedInsertBB->getParent()->getEntryBlock().getFirstInsertionPt());
    }
    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(VarTy, nullptr, VarName.c_str());
    if (SavedInsertBB)
        context.Builder.SetInsertPoint(SavedInsertBB);
    context.Builder.CreateStore(InitV, Alloca);
    llvm::Value* OldVal = context.NamedValues[VarName];
    FluxType OldType = context.NamedTypes[VarName];
    bool OldWasMoved = context.MovedVariables.count(VarName) > 0;
    context.MovedVariables.erase(VarName);
    context.NamedValues[VarName] = Alloca;
    context.NamedTypes[VarName] = ActualType;

    // Push lexical block and emit debug info for let binding
    if (context.DebugEnabled) {
        llvm::DIType* DbgTy = nullptr;
        if (ActualType.Kind == TypeKind::Double) {
            DbgTy = context.DebugBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
        } else if (ActualType.Kind == TypeKind::Int) {
            DbgTy = context.DebugBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
        } else if (ActualType.Kind == TypeKind::Bool) {
            DbgTy = context.DebugBuilder->createBasicType("bool", 1, llvm::dwarf::DW_ATE_boolean);
        }
        if (DbgTy) {
            llvm::DIScope* scope =
                context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
            auto* DbgVar =
                context.DebugBuilder->createAutoVariable(scope, VarName, context.DebugFile, Init->getLine(), DbgTy);
            auto* DbgLoc = llvm::DILocation::get(context.TheContext, Init->getLine(), Init->getCol(), scope);
            context.DebugBuilder->insertDeclare(Alloca, DbgVar, context.DebugBuilder->createExpression(), DbgLoc,
                                                context.Builder.GetInsertBlock());
        }

        llvm::DIScope* parentScope =
            context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }

    if (!Body)
        return InitTV;
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] Let body failed to codegen" << std::endl;
        return TypedValue();
    }

    // Call onDestroy if VarName is a UserStruct with onDestroy (including parents, derived-to-base order)
    if (ActualType.Kind == TypeKind::UserStruct && ActualType.StructTypeId >= 0 &&
        ActualType.StructTypeId < static_cast<int>(context.StructTypes.size())) {
        std::string structName = context.StructTypes[ActualType.StructTypeId].Name;

        std::vector<std::string> destroyChain;
        std::string currentDestroyType = structName;
        while (!currentDestroyType.empty()) {
            destroyChain.push_back(currentDestroyType);
            auto indexIt = context.StructTypeIndex.find(currentDestroyType);
            if (indexIt != context.StructTypeIndex.end()) {
                int curId = indexIt->second;
                if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                    currentDestroyType = context.StructTypes[curId].ParentName;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        for (const std::string& chainTypeName : destroyChain) {
            auto methodsIt = context.TypeMethods.find(chainTypeName);
            if (methodsIt != context.TypeMethods.end()) {
                auto destroyIt = methodsIt->second.find("onDestroy");
                if (destroyIt != methodsIt->second.end()) {
                    llvm::Function* destroyFn = destroyIt->second;
                    if (destroyFn) {
                        auto indexIt = context.StructTypeIndex.find(chainTypeName);
                        if (indexIt != context.StructTypeIndex.end()) {
                            int curId = indexIt->second;
                            if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                                auto& structInfo = context.StructTypes[curId];
                                llvm::StructType* curLLVMType = structInfo.LLVMType;
                                FluxType curStructType = FluxType::userStruct(curId, curLLVMType);

                                llvm::Value* selfArg = Alloca;
                                if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(Alloca)) {
                                    if (allocaInst->getAllocatedType()->isPointerTy()) {
                                        selfArg = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst,
                                                                             "self_ptr");
                                    }
                                }
                                llvm::Type* expectedType = destroyFn->getFunctionType()->getParamType(0);
                                if (selfArg->getType() != expectedType) {
                                    selfArg = context.Builder.CreateBitCast(selfArg, expectedType);
                                }

                                std::vector<llvm::Value*> destroyArgs;
                                if (shouldPassByPointer(curStructType, context)) {
                                    destroyArgs.push_back(selfArg);
                                } else {
                                    llvm::Value* loadedVal =
                                        context.Builder.CreateLoad(curLLVMType, selfArg, "self_destroy");
                                    destroyArgs.push_back(loadedVal);
                                }
                                context.Builder.CreateCall(destroyFn, destroyArgs);
                            }
                        }
                    }
                }
            }
        }
    }

    if (context.DebugEnabled)
        context.LexicalBlocks.pop_back();
    context.NamedValues[VarName] = OldVal;
    context.NamedTypes[VarName] = OldType;
    if (OldWasMoved)
        context.MovedVariables.insert(VarName);
    else
        context.MovedVariables.erase(VarName);
    return BodyTV;
}

TypedValue LambdaExprAST::codegen(CodegenContext& context)
{
    std::vector<llvm::Type*> ArgTypes(Args.size(), llvm::Type::getDoubleTy(context.TheContext));
    llvm::FunctionType* LambdaTy =
        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), ArgTypes, false);
    llvm::Function* LambdaFn =
        llvm::Function::Create(LambdaTy, llvm::Function::InternalLinkage, "lambda", context.TheModule);
    llvm::BasicBlock* BB = llvm::BasicBlock::Create(context.TheContext, "entry", LambdaFn);
    llvm::BasicBlock* OldBB = context.Builder.GetInsertBlock();
    std::map<std::string, llvm::Value*> OldNamedValues = context.NamedValues;
    auto OldMovedVars = context.MovedVariables;
    context.NamedValues.clear();
    context.MovedVariables.clear();
    {
        llvm::IRBuilder<> LambdaBuilder(BB);
        unsigned Idx = 0;
        for (auto& Arg : LambdaFn->args()) {
            Arg.setName(Args[Idx++]);
            llvm::AllocaInst* Alloca =
                LambdaBuilder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, Arg.getName());
            LambdaBuilder.CreateStore(&Arg, Alloca);
            context.NamedValues[std::string(Arg.getName())] = Alloca;
        }
        context.Builder.SetInsertPoint(BB);
        TypedValue BodyTV = Body->codegen(context);
        if (BodyTV.Val)
            context.Builder.CreateRet(BodyTV.Val);
    }
    if (OldBB)
        context.Builder.SetInsertPoint(OldBB);
    context.NamedValues = OldNamedValues;
    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
}

TypedValue VectorExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Function* MallocF = context.TheModule->getFunction("malloc");
    if (!MallocF)
        MallocF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {Int64Ty}, false),
                                         llvm::Function::ExternalLinkage, "malloc", context.TheModule);

    // Codegen all elements first to determine types
    std::vector<TypedValue> elemTVs;
    elemTVs.reserve(Elements.size());
    FluxType elementType(TypeKind::Double);
    bool hasVectorElements = false;
    for (auto& elem : Elements) {
        TypedValue etv = elem->codegen(context);
        elemTVs.push_back(etv);
        if (etv.Val && etv.Type.Kind == TypeKind::Vector) {
            hasVectorElements = true;
            elementType = etv.Type;
        }
    }

    if (hasVectorElements) {
        // Nested vector: store pointers to heap-allocated inner vectors
        llvm::Value* DataSize = llvm::ConstantInt::get(Int64Ty, Elements.size() * 8);
        llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
        llvm::Type* VecStructTy = FluxType(TypeKind::Vector).getLLVMType(context.TheContext);
        for (size_t i = 0; i < Elements.size(); ++i) {
            TypedValue& ElemTV = elemTVs[i];
            if (!ElemTV.Val)
                continue;
            if (ElemTV.Type.Kind == TypeKind::Vector) {
                // Allocate heap space for inner vector struct, store it, store pointer
                llvm::Value* HeapVec =
                    context.Builder.CreateCall(MallocF, llvm::ConstantInt::get(Int64Ty, 16), "inner_vec_heap");
                llvm::Value* VecPtr = context.Builder.CreatePointerCast(
                    HeapVec, llvm::PointerType::get(context.TheContext, 0), "inner_vec_ptr");
                context.Builder.CreateStore(ElemTV.Val, VecPtr);
                // Bitcast pointer to double for storage in the double data array
                llvm::Value* PtrAsInt = context.Builder.CreatePtrToInt(HeapVec, Int64Ty, "heap_i64");
                llvm::Value* PtrAsDouble = context.Builder.CreateBitCast(PtrAsInt, DoubleTy, "ptr_as_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
                context.Builder.CreateStore(PtrAsDouble, ElemPtr);
            } else {
                // Scalar element in mixed vector — convert to double and store
                llvm::Value* Val = ElemTV.Val;
                if (Val->getType()->isIntegerTy())
                    Val = context.Builder.CreateSIToFP(Val, DoubleTy, "elem_dbl");
                else if (Val->getType()->isFloatTy())
                    Val = context.Builder.CreateFPExt(Val, DoubleTy, "elem_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
                context.Builder.CreateStore(Val, ElemPtr);
            }
        }
        FluxType vecType(TypeKind::Vector);
        vecType.VecElementType = new FluxType(elementType);
        llvm::StructType* VecSTy = llvm::cast<llvm::StructType>(vecType.getLLVMType(context.TheContext));
        llvm::Value* VecVal = llvm::PoisonValue::get(VecSTy);
        VecVal = context.Builder.CreateInsertValue(VecVal, DataPtr, 0);
        VecVal = context.Builder.CreateInsertValue(VecVal, llvm::ConstantInt::get(Int32Ty, Elements.size()), 1);
        return TypedValue(VecVal, vecType);
    }

    // Scalar elements: flat double array (original behavior)
    llvm::Value* DataSize = llvm::ConstantInt::get(Int64Ty, Elements.size() * 8);
    llvm::Value* DataPtr = context.Builder.CreateCall(MallocF, {DataSize}, "vec_data");
    for (size_t i = 0; i < Elements.size(); ++i) {
        TypedValue& ElemTV = elemTVs[i];
        if (ElemTV.Val) {
            llvm::Value* Val = ElemTV.Val;
            if (Val->getType()->isIntegerTy())
                Val = context.Builder.CreateSIToFP(Val, DoubleTy, "elem_dbl");
            else if (Val->getType()->isFloatTy())
                Val = context.Builder.CreateFPExt(Val, DoubleTy, "elem_dbl");
            llvm::Value* ElemPtr =
                context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
            context.Builder.CreateStore(Val, ElemPtr);
        }
    }
    FluxType vecType(TypeKind::Vector);
    // VecElementType left nullptr (nullptr = Double elements)
    llvm::StructType* VecSTy = llvm::cast<llvm::StructType>(vecType.getLLVMType(context.TheContext));
    llvm::Value* VecVal = llvm::PoisonValue::get(VecSTy);
    VecVal = context.Builder.CreateInsertValue(VecVal, DataPtr, 0);
    VecVal = context.Builder.CreateInsertValue(VecVal, llvm::ConstantInt::get(Int32Ty, Elements.size()), 1);
    return TypedValue(VecVal, vecType);
}

TypedValue MatrixExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* ComplexTy = llvm::VectorType::get(DoubleTy, 2, false);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    bool isComplex = false;
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            if (dynamic_cast<ComplexExprAST*>(Rows[r][c].get())) {
                isComplex = true;
                break;
            }
        }
        if (isComplex)
            break;
    }

    llvm::Type* ElemTy = isComplex ? ComplexTy : DoubleTy;

    // Allocate Eigen matrix directly — no malloc, no memcpy
    std::string newFnName = isComplex ? "flux_register_new_complex_matrix" : "flux_register_new_matrix";
    llvm::Function* NewMatF = context.TheModule->getFunction(newFnName);
    if (!NewMatF) {
        llvm::Type* Params[] = {Int32Ty, Int32Ty};
        NewMatF = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, Params, false),
                                         llvm::Function::ExternalLinkage, newFnName, context.TheModule);
    }

    llvm::Value* RowsVal = llvm::ConstantInt::get(Int32Ty, NumRows);
    llvm::Value* ColsVal = llvm::ConstantInt::get(Int32Ty, NumCols);
    llvm::Value* DataPtr = context.Builder.CreateCall(NewMatF, {RowsVal, ColsVal}, "mat_data");
    DataPtr = context.Builder.CreateBitCast(DataPtr, llvm::PointerType::get(context.TheContext, 0));

    // Write elements directly into Eigen's internal data buffer
    UnitDimensions elemDims;
    for (int r = 0; r < NumRows; ++r) {
        for (int c = 0; c < NumCols; ++c) {
            TypedValue ElemTV = Rows[r][c]->codegen(context);
            if (ElemTV.Val) {
                if (r == 0 && c == 0)
                    elemDims = ElemTV.Type.Dimensions;
                if (isComplex && ElemTV.Type.Kind != TypeKind::Complex) {
                    ElemTV = promoteToComplex(ElemTV, context);
                }
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(
                    ElemTy, DataPtr,
                    {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), c * NumRows + r)}, "elem_ptr");
                context.Builder.CreateStore(ElemTV.Val, ElemPtr);
            }
        }
    }

    FluxType resType = isComplex ? FluxType(TypeKind::ComplexMatrix) : FluxType(TypeKind::Matrix);
    resType.Dimensions = elemDims;
    llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(resType.getLLVMType(context.TheContext));
    llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
    MatVal = context.Builder.CreateInsertValue(MatVal, DataPtr, 0);
    MatVal = context.Builder.CreateInsertValue(MatVal, RowsVal, 1);
    MatVal = context.Builder.CreateInsertValue(MatVal, ColsVal, 2);

    return TypedValue(MatVal, resType);
}

TypedValue IndexExprAST::codegen(CodegenContext& context)
{
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);

    TypedValue ArrayTV = Array->codegen(context);
    if (!ArrayTV.Val) {
        std::cerr << "[FLUX ERROR] Index array expression failed to codegen" << std::endl;
        return TypedValue();
    }

    if (ArrayTV.Type.Kind == TypeKind::Vector) {
        llvm::Value* DataPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "vec_ptr");
        llvm::Value* LenVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "vec_len");
        (void)LenVal;

        TypedValue IdxTV = RowIndex->codegen(context);
        if (!IdxTV.Val) {
            std::cerr << "[FLUX ERROR] Vector index expression failed to codegen" << std::endl;
            return TypedValue();
        }
        llvm::Value* IdxV = IdxTV.Val;
        if (IdxV->getType()->isFloatingPointTy())
            IdxV = context.Builder.CreateFPToSI(IdxV, Int32Ty, "idx_int");
        else if (IdxV->getType()->isIntegerTy(64))
            IdxV = context.Builder.CreateTrunc(IdxV, Int32Ty, "idx_int");

        // Check if vector contains nested vectors (VecElementType non-null and Kind == Vector)
        if (ArrayTV.Type.VecElementType && ArrayTV.Type.VecElementType->Kind == TypeKind::Vector) {
            // Load bit-cast pointer from data slot, inttoptr, then load inner vector struct
            llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
            llvm::Value* ElemSlot = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {Idx64}, "elem_slot");
            llvm::Value* DblBits = context.Builder.CreateLoad(DoubleTy, ElemSlot, "ptr_bits_as_dbl");
            llvm::Value* PtrInt = context.Builder.CreateBitCast(DblBits, Int64Ty, "ptr_bits_i64");
            llvm::Value* InnerVecVoidPtr = context.Builder.CreateIntToPtr(PtrInt, VoidPtrTy, "inner_vec_raw");
            llvm::Value* InnerVecVal = context.Builder.CreateLoad(
                FluxType(TypeKind::Vector).getLLVMType(context.TheContext), InnerVecVoidPtr, "inner_vec");
            return TypedValue(InnerVecVal, *ArrayTV.Type.VecElementType);
        }

        llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
        llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr, {Idx64}, "elem_ptr");
        llvm::Value* ElemVal = context.Builder.CreateLoad(DoubleTy, ElemPtr, "elem_val");
        return TypedValue(ElemVal, TypeKind::Double);
    }

    if (ArrayTV.Type.Kind == TypeKind::Double && ArrayTV.Val->getType()->isPointerTy()) {
        TypedValue IdxTV = RowIndex->codegen(context);
        if (!IdxTV.Val)
            return TypedValue();
        llvm::Value* IdxV = IdxTV.Val;
        if (IdxV->getType()->isFloatingPointTy())
            IdxV = context.Builder.CreateFPToSI(IdxV, Int32Ty, "idx_int");
        else if (IdxV->getType()->isIntegerTy(64))
            IdxV = context.Builder.CreateTrunc(IdxV, Int32Ty, "idx_int");
        llvm::Value* Idx64 = context.Builder.CreateSExt(IdxV, Int64Ty, "idx_64");
        llvm::Value* ElemPtr = context.Builder.CreateGEP(DoubleTy, ArrayTV.Val, {Idx64}, "raw_ptr_idx");
        llvm::Value* ElemVal = context.Builder.CreateLoad(DoubleTy, ElemPtr, "raw_elem_val");
        return TypedValue(ElemVal, TypeKind::Double);
    }
    if (ArrayTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "Can only index into matrices or vectors." << std::endl;
        return TypedValue();
    }

    llvm::Value* MatPtr = context.Builder.CreateExtractValue(ArrayTV.Val, 0, "mat_ptr");

    // Extract matrix dimensions for bounds checking
    llvm::Value* RowsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 1, "mat_rows");
    llvm::Value* ColsVal = context.Builder.CreateExtractValue(ArrayTV.Val, 2, "mat_cols");

    if (IsMatrixIndex) {
        // Check if either index is a RangeExprAST (for slicing)
        auto codegenRange = [&](std::unique_ptr<ExprAST>& idx, llvm::Value* dim, llvm::Value*& lo,
                                llvm::Value*& hi) -> bool {
            auto* r = dynamic_cast<RangeExprAST*>(idx.get());
            if (!r) {
                lo = llvm::ConstantInt::get(Int32Ty, 0);
                hi = dim;
                return true;
            }
            auto* sPtr = const_cast<ExprAST*>(r->getStart());
            auto* ePtr = const_cast<ExprAST*>(r->getEnd());
            if (!sPtr || !ePtr)
                return false;
            TypedValue s = sPtr->codegen(context);
            TypedValue e = ePtr->codegen(context);
            lo = s.Val;
            hi = e.Val;
            if (lo->getType()->isFloatingPointTy())
                lo = context.Builder.CreateFPToSI(lo, Int32Ty, "slice_lo");
            if (hi->getType()->isFloatingPointTy())
                hi = context.Builder.CreateFPToSI(hi, Int32Ty, "slice_hi");
            return true;
        };

        bool hasRowRange = dynamic_cast<RangeExprAST*>(RowIndex.get()) != nullptr;
        bool hasColRange = ColIndex ? dynamic_cast<RangeExprAST*>(ColIndex.get()) != nullptr : false;

        if (hasRowRange || hasColRange) {
            llvm::Value *r0, *r1, *c0, *c1;
            if (!codegenRange(RowIndex, RowsVal, r0, r1)) {
                std::cerr << "[FLUX ERROR] Matrix slice row range failed to codegen" << std::endl;
                return TypedValue();
            }
            if (!codegenRange(ColIndex ? ColIndex : RowIndex, ColsVal, c0, c1)) {
                std::cerr << "[FLUX ERROR] Matrix slice column range failed to codegen" << std::endl;
                return TypedValue();
            }

            llvm::Function* SliceF = context.TheModule->getFunction("flux_matrix_slice");
            if (!SliceF)
                SliceF = llvm::Function::Create(
                    llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty}, false),
                    llvm::Function::ExternalLinkage, "flux_matrix_slice", context.TheModule);
            llvm::Value* SlicePtr = context.Builder.CreateCall(SliceF, {MatPtr, r0, r1, c0, c1}, "slice_ptr");

            // Get slice dimensions and wrap in matrix struct
            llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
            if (!RowsF)
                RowsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
            llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
            if (!ColsF)
                ColsF = llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
            llvm::Value* SR = context.Builder.CreateCall(RowsF, {SlicePtr}, "sli_rows");
            llvm::Value* SC = context.Builder.CreateCall(ColsF, {SlicePtr}, "sli_cols");
            FluxType sliceMatType(TypeKind::Matrix);
            sliceMatType.Dimensions = ArrayTV.Type.Dimensions;
            llvm::StructType* MatSTy = llvm::cast<llvm::StructType>(sliceMatType.getLLVMType(context.TheContext));
            llvm::Value* MV = llvm::PoisonValue::get(MatSTy);
            MV = context.Builder.CreateInsertValue(MV, SlicePtr, 0);
            MV = context.Builder.CreateInsertValue(MV, SR, 1);
            MV = context.Builder.CreateInsertValue(MV, SC, 2);
            return TypedValue(MV, sliceMatType);
        }

        // Scalar element access: A[row, col]
        TypedValue RowTV = RowIndex->codegen(context);
        TypedValue ColTV = ColIndex ? ColIndex->codegen(context) : TypedValue();
        if (!RowTV.Val || !ColTV.Val) {
            std::cerr << "[FLUX ERROR] Matrix element index expression failed to codegen" << std::endl;
            return TypedValue();
        }

        llvm::Value* RowV = RowTV.Val;
        llvm::Value* ColV = ColTV.Val;

        if (RowV->getType()->isFloatingPointTy())
            RowV = context.Builder.CreateFPToSI(RowV, Int32Ty, "row_int");
        else if (RowV->getType()->isIntegerTy(64))
            RowV = context.Builder.CreateTrunc(RowV, Int32Ty, "row_int");

        if (ColV->getType()->isFloatingPointTy())
            ColV = context.Builder.CreateFPToSI(ColV, Int32Ty, "col_int");
        else if (ColV->getType()->isIntegerTy(64))
            ColV = context.Builder.CreateTrunc(ColV, Int32Ty, "col_int");

        llvm::Function* GetElemF = context.TheModule->getFunction("flux_matrix_get");
        if (!GetElemF)
            GetElemF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {VoidPtrTy, Int32Ty, Int32Ty}, false),
                                              llvm::Function::ExternalLinkage, "flux_matrix_get", context.TheModule);

        FluxType idxRetType(TypeKind::Double);
        idxRetType.Dimensions = ArrayTV.Type.Dimensions;
        return TypedValue(context.Builder.CreateCall(GetElemF, {MatPtr, RowV, ColV}, "elem_val"), idxRetType);
    }

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
}

TypedValue RangeExprAST::codegen(CodegenContext& context)
{
    TypedValue StartTV = Start->codegen(context);
    TypedValue EndTV = End->codegen(context);
    TypedValue StepTV =
        Step ? Step->codegen(context)
             : TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 1.0), TypeKind::Double);

    auto ensureFP = [&](TypedValue& tv) {
        if (!tv.Val) {
            tv.Val = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
        } else if (tv.Val->getType()->isIntegerTy()) {
            tv.Val = context.Builder.CreateSIToFP(tv.Val, llvm::Type::getDoubleTy(context.TheContext), "range_cast");
        } else if (tv.Val->getType()->isIntegerTy(1)) {
            tv.Val = context.Builder.CreateUIToFP(tv.Val, llvm::Type::getDoubleTy(context.TheContext), "range_cast");
        }
    };
    ensureFP(StartTV);
    ensureFP(EndTV);
    ensureFP(StepTV);

    // Look up the extern function type entry for proper arg decomposition
    auto it = context.ExternFuncTypes.find("flux_create_range_sum");
    if (it == context.ExternFuncTypes.end()) {
        std::cerr << "RangeExprAST: flux_create_range_sum not registered with ExternFuncTypes\n";
        return TypedValue(nullptr, TypeKind::Void);
    }

    auto& retType = it->second.first;
    auto& argTypes = it->second.second;
    llvm::Type* RetLTy = (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix)
                             ? llvm::PointerType::get(context.TheContext, 0)
                             : retType.getLLVMType(context.TheContext);

    std::vector<llvm::Type*> ArgLTys;
    for (auto& at : argTypes) {
        if (at.Kind == TypeKind::Matrix || at.Kind == TypeKind::ComplexMatrix)
            ArgLTys.push_back(llvm::PointerType::get(context.TheContext, 0));
        else if (at.Kind == TypeKind::Int)
            ArgLTys.push_back(llvm::Type::getInt32Ty(context.TheContext));
        else
            ArgLTys.push_back(at.getLLVMType(context.TheContext));
    }

    std::string fnName = "flux_create_range_sum";
    llvm::Function* F = context.TheModule->getFunction(fnName);
    if (!F) {
        llvm::FunctionType* FT = llvm::FunctionType::get(RetLTy, ArgLTys, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, context.TheModule);
    }

    std::vector<llvm::Value*> CallArgs;
    CallArgs.push_back(StartTV.Val);
    CallArgs.push_back(StepTV.Val);
    CallArgs.push_back(EndTV.Val);

    llvm::Value* ResPtr = context.Builder.CreateCall(F, CallArgs, "range_res");

    if (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix) {
        // Wrap void* into {ptr, i32, i32}
        llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(retType.getLLVMType(context.TheContext));
        llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
        if (!RowsF)
            RowsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                               {llvm::PointerType::get(context.TheContext, 0)}, false),
                                       llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
        if (!ColsF)
            ColsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                               {llvm::PointerType::get(context.TheContext, 0)}, false),
                                       llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
        llvm::Value* ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
        llvm::Value* ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");
        llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
        return TypedValue(MatVal, retType);
    }

    return TypedValue(ResPtr, retType);
}

} // namespace Flux
