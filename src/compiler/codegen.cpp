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

// ... rest of the namespace ...

// Helper to check if a type is complex
static bool isComplexType(TypedValue V)
{
    return V.Type.Kind == TypeKind::Complex;
}

void emitLocation(ExprAST* ast, CodegenContext& context)
{
    if (!context.DebugEnabled || !ast || !context.DebugBuilder)
        return;

    llvm::DIScope* Scope;
    if (context.LexicalBlocks.empty()) {
        llvm::Function* F = context.Builder.GetInsertBlock()
                                ? context.Builder.GetInsertBlock()->getParent()
                                : nullptr;
        if (F && F->getSubprogram())
            Scope = F->getSubprogram();
        else
            Scope = context.DebugCompileUnit;
    } else {
        Scope = context.LexicalBlocks.back();
    }

    context.Builder.SetCurrentDebugLocation(
        llvm::DILocation::get(context.TheContext, ast->getLine(), ast->getCol(), Scope));
}

static void resolveUserStructType(FluxType& type, CodegenContext& context)
{
    if (type.Kind == TypeKind::UserStruct) {
        if (type.StructTypeId < 0 && !type.GenericName.empty()) {
            auto it = context.StructTypeIndex.find(type.GenericName);
            if (it != context.StructTypeIndex.end()) {
                type.StructTypeId = it->second;
            }
        }
        if (type.StructTypeId >= 0) {
            if (type.StructTypeId < static_cast<int>(context.StructTypes.size())) {
                if (!type.StructLLVMType) {
                    type.StructLLVMType = context.StructTypes[type.StructTypeId].LLVMType;
                }
                type.isCopy = context.StructTypes[type.StructTypeId].isCopy;
            }
        }
    }
}

static void resolveUserEnumType(FluxType& type, CodegenContext& context)
{
    if (type.Kind == TypeKind::UserEnum) {
        if (type.EnumTypeId < 0 && !type.GenericName.empty()) {
            auto it = context.EnumTypeIndex.find(type.GenericName);
            if (it != context.EnumTypeIndex.end()) {
                type.EnumTypeId = it->second;
            }
        }
        if (type.EnumTypeId >= 0) {
            if (type.EnumTypeId < static_cast<int>(context.EnumTypes.size())) {
                if (!type.EnumLLVMType) {
                    type.EnumLLVMType = context.EnumTypes[type.EnumTypeId].LLVMType;
                }
                type.isCopy = context.EnumTypes[type.EnumTypeId].isCopy;
            }
        }
    }
}

/// Check that all trait bounds on generic parameters are satisfied for the
/// given type map. Returns true if all bounds are satisfied.
static bool checkTraitBounds(
    const std::map<std::string, std::vector<std::string>>& genericParamBounds,
    const std::map<std::string, FluxType>& typeMap,
    CodegenContext& context)
{
    for (const auto& [paramName, boundTraits] : genericParamBounds) {
        auto typeIt = typeMap.find(paramName);
        if (typeIt == typeMap.end())
            continue;
        const FluxType& concreteType = typeIt->second;

        // Determine the concrete type name for trait lookup
        std::string typeName;
        if (concreteType.isStruct()) {
            int id = concreteType.StructTypeId;
            if (id >= 0 && id < static_cast<int>(context.StructTypes.size()))
                typeName = context.StructTypes[id].Name;
        } else if (concreteType.isEnum()) {
            int id = concreteType.EnumTypeId;
            if (id >= 0 && id < static_cast<int>(context.EnumTypes.size()))
                typeName = context.EnumTypes[id].Name;
        } else if (concreteType.Kind == TypeKind::Double) {
            typeName = "Double";
        } else if (concreteType.Kind == TypeKind::Int) {
            typeName = "Int";
        } else if (concreteType.Kind == TypeKind::Float) {
            typeName = "Float";
        } else if (concreteType.Kind == TypeKind::Bool) {
            typeName = "Bool";
        } else if (concreteType.Kind == TypeKind::String) {
            typeName = "String";
        } else if (concreteType.Kind == TypeKind::Matrix) {
            typeName = "Matrix";
        } else if (concreteType.Kind == TypeKind::Complex) {
            typeName = "Complex";
        } else if (concreteType.Kind == TypeKind::Vector) {
            typeName = "Vector";
        }

        if (typeName.empty())
            continue;

        for (const auto& traitName : boundTraits) {
            if (context.TraitImplementations.count({traitName, typeName}) == 0) {
                llvm::errs() << "error: type '" << typeName << "' does not implement trait '"
                             << traitName << "'\n";
                return false;
            }
        }
    }
    return true;
}

// Forward declarations for helpers used by ensureGenericTypeArgSpecialized
static std::string buildGenericTypeSuffix(const std::vector<FluxType>& genericArgs,
                                           CodegenContext& context);
static std::string specializeGenericEnum(const std::string& enumName,
                                          const std::vector<FluxType>& genericArgs,
                                          CodegenContext& context);

// Recursively specialize a generic type argument (struct or enum)
// so that its StructTypeId/EnumTypeId and LLVM type are resolved.
// Handles nested generics like Pair[Pair[Double, Double], Double].
static void ensureGenericTypeArgSpecialized(FluxType& typeArg, CodegenContext& context)
{
    if (typeArg.Kind == TypeKind::UserStruct && typeArg.StructTypeId < 0 && !typeArg.GenericArgs.empty()) {
        for (auto& nestedArg : typeArg.GenericArgs)
            ensureGenericTypeArgSpecialized(nestedArg, context);

        auto genIt = context.GenericStructs.find(typeArg.GenericName);
        if (genIt == context.GenericStructs.end()) return;
        const auto& genericParams = genIt->second->getGenericParams();
        if (typeArg.GenericArgs.size() != genericParams.size()) return;

        std::string suffix = "_" + buildGenericTypeSuffix(typeArg.GenericArgs, context);
        std::string resolvedName = typeArg.GenericName + suffix;

        auto existingIt = context.StructTypeIndex.find(resolvedName);
        if (existingIt != context.StructTypeIndex.end()) {
            typeArg.StructTypeId = existingIt->second;
            typeArg.StructLLVMType = context.StructTypes[existingIt->second].LLVMType;
            typeArg.GenericName = resolvedName;
            return;
        }

        context.CompiledStructSpecializations.insert(resolvedName);

        std::map<std::string, FluxType> typeMap;
        for (size_t i = 0; i < genericParams.size(); ++i)
            typeMap[genericParams[i]] = typeArg.GenericArgs[i];

        const auto& genericFields = genIt->second->getFields();
        std::vector<std::pair<std::string, FluxType>> concreteFields;
        for (auto& [fName, fType] : genericFields) {
            FluxType concreteType = fType;
            if (fType.isGeneric()) {
                auto mapIt = typeMap.find(fType.GenericName);
                if (mapIt != typeMap.end())
                    concreteType = mapIt->second;
            }
            ensureGenericTypeArgSpecialized(concreteType, context);
            concreteFields.push_back({fName, concreteType});
        }
        std::vector<llvm::Type*> fieldTypes;
        for (auto& [_, ft] : concreteFields)
            fieldTypes.push_back(ft.getLLVMType(context.TheContext));

        auto* llvmStructTy = llvm::StructType::create(context.TheContext, fieldTypes, resolvedName);
        int newId = static_cast<int>(context.StructTypes.size());
        CodegenContext::StructTypeInfo info;
        info.Name = resolvedName;
        info.ParentName = genIt->second->getParentName();
        info.Fields = concreteFields;
        info.LLVMType = llvmStructTy;
        context.StructTypes.push_back(std::move(info));
        context.StructTypeIndex[resolvedName] = newId;

        typeArg.StructTypeId = newId;
        typeArg.StructLLVMType = llvmStructTy;
        typeArg.GenericName = resolvedName;
    }

    if (typeArg.Kind == TypeKind::UserEnum && typeArg.EnumTypeId < 0 && !typeArg.GenericArgs.empty()) {
        for (auto& nestedArg : typeArg.GenericArgs)
            ensureGenericTypeArgSpecialized(nestedArg, context);

        std::string mangled = specializeGenericEnum(typeArg.GenericName, typeArg.GenericArgs, context);
        if (!mangled.empty()) {
            auto it = context.EnumTypeIndex.find(mangled);
            if (it != context.EnumTypeIndex.end()) {
                typeArg.EnumTypeId = it->second;
                typeArg.EnumLLVMType = context.EnumTypes[it->second].LLVMType;
                typeArg.GenericName = mangled;
            }
        }
    }
}

// Helper: build a suffix string from generic type args by concatenating
// each type's name (e.g., [Double, Double] -> "Double_Double").
static std::string buildGenericTypeSuffix(const std::vector<FluxType>& genericArgs,
                                           CodegenContext& context)
{
    std::string suffix;
    for (size_t i = 0; i < genericArgs.size(); ++i) {
        if (i > 0) suffix += "_";
        FluxType resolved = genericArgs[i];
        resolveUserStructType(resolved, context);
        resolveUserEnumType(resolved, context);
        switch (resolved.Kind) {
        case TypeKind::Double: suffix += "Double"; break;
        case TypeKind::Int:    suffix += "Int"; break;
        case TypeKind::Bool:   suffix += "Bool"; break;
        case TypeKind::Float:  suffix += "Float"; break;
        case TypeKind::String: suffix += "String"; break;
        case TypeKind::Complex: suffix += "Complex"; break;
        case TypeKind::Matrix: suffix += "Matrix"; break;
        case TypeKind::ComplexMatrix: suffix += "Cmat"; break;
        case TypeKind::Vector: suffix += "Vector"; break;
        case TypeKind::Void: suffix += "Void"; break;
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

// Helper: specialize a generic enum with concrete type arguments.
// Creates a synthetic enum type with substituted payloads, registers it
// in context.EnumTypes/EnumTypeIndex, and returns the mangled name.
// Returns empty string on failure.
static std::string specializeGenericEnum(const std::string& enumName,
                                          const std::vector<FluxType>& genericArgs,
                                          CodegenContext& context)
{
    // Deep-copy generic args and recursively specialize nested generic types
    // (e.g., Option[Pair[Double, Double]]) before building the type map.
    std::vector<FluxType> concreteGenericArgs = genericArgs;
    for (auto& arg : concreteGenericArgs)
        ensureGenericTypeArgSpecialized(arg, context);

    std::string suffix = buildGenericTypeSuffix(concreteGenericArgs, context);
    std::string mangledName = enumName + "_" + suffix;

    // Already specialized?
    auto existingIt = context.EnumTypeIndex.find(mangledName);
    if (existingIt != context.EnumTypeIndex.end())
        return mangledName;

    // Look up the generic enum definition
    auto genIt = context.GenericEnums.find(enumName);
    if (genIt == context.GenericEnums.end())
        return "";

    EnumDeclAST* genericEnum = genIt->second;
    const auto& genericParams = genericEnum->getGenericParams();
    const auto& variants = genericEnum->getVariants();
    const auto& variantPayloads = genericEnum->getVariantPayloads();

    if (concreteGenericArgs.size() != genericParams.size()) {
        std::cerr << "Generic enum " << enumName << " expects " << genericParams.size()
                  << " type args, got " << concreteGenericArgs.size() << std::endl;
        return "";
    }

    // Build type map from the now-fully-specialized type args
    std::map<std::string, FluxType> typeMap;
    for (size_t i = 0; i < genericParams.size(); ++i)
        typeMap[genericParams[i]] = concreteGenericArgs[i];

    // Substitute concrete types in payloads
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

    // Specialize any UserStruct payloads that may contain generic type parameters
    for (size_t i = 0; i < concretePayloads.size(); ++i) {
        auto& pt = concretePayloads[i];
        if (pt.Kind == TypeKind::UserStruct && !pt.GenericName.empty()) {
            auto structIt = context.StructTypeIndex.find(pt.GenericName);
            if (structIt != context.StructTypeIndex.end()) {
                int structId = structIt->second;
                auto& structInfo = context.StructTypes[structId];
                bool needsSpecialization = false;
                for (auto& [fName, fType] : structInfo.Fields) {
                    if (fType.isGeneric()) { needsSpecialization = true; break; }
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
                        auto* llvmStructTy = llvm::StructType::create(context.TheContext, fieldTypes, specializedStructName);
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

    // Create synthetic enum info
    CodegenContext::EnumTypeInfo info;
    info.Name = mangledName;
    info.Variants = variants;
    info.VariantPayloads = concretePayloads;

    // Create LLVM tagged union type
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

static bool shouldPassByPointer(const FluxType& type, CodegenContext& context)
{
    if (type.Kind != TypeKind::UserStruct && type.Kind != TypeKind::UserEnum)
        return false;

    FluxType resolved = type;
    resolveUserStructType(resolved, context);
    resolveUserEnumType(resolved, context);

    llvm::Type* llvmTy = resolved.getLLVMType(context.TheContext);
    if (!llvmTy)
        return false;

    uint64_t size = context.TheModule->getDataLayout().getTypeStoreSize(llvmTy);
#ifdef _WIN32
    return size > 8;
#else
    return size > 16;
#endif
}

static bool isCopyType(const FluxType& type)
{
    // Return false for types that explicitly opt out via ~Copy annotation
    if (type.Kind == TypeKind::UserStruct || type.Kind == TypeKind::UserEnum)
        return type.isCopy;
    return true;
}

static bool checkNotMoved(const std::string& name, CodegenContext& context)
{
    if (context.MovedVariables.count(name)) {
        std::cerr << "[FLUX ERROR] use of moved variable '" << name << "'" << std::endl;
        return false;
    }
    return true;
}

static bool shouldReturnBySRet(const FluxType& type, CodegenContext& context)
{
    FluxType resolved = type;
    resolveUserStructType(resolved, context);
    resolveUserEnumType(resolved, context);

    if (resolved.Kind == TypeKind::Matrix || resolved.Kind == TypeKind::ComplexMatrix) {
        return true;
    }
#ifdef _WIN32
    if (resolved.Kind == TypeKind::Vector) {
        return true;
    }
#endif

    if (resolved.Kind == TypeKind::UserStruct || resolved.Kind == TypeKind::UserEnum) {
        llvm::Type* llvmTy = resolved.getLLVMType(context.TheContext);
        if (!llvmTy)
            return false;

        uint64_t size = context.TheModule->getDataLayout().getTypeStoreSize(llvmTy);
#ifdef _WIN32
        // On Windows x64: struct returns of size 1, 2, 4, 8 are in RAX, others are by reference (sret)
        return (size != 1 && size != 2 && size != 4 && size != 8);
#else
        // On Linux/macOS System V and ARM64 ABI: struct returns up to 16 bytes are in RAX/RDX, others are sret
        return size > 16;
#endif
    }

    return false;
}



static FluxType typeFromLLVM(llvm::Type* Ty)
{
    if (Ty->isDoubleTy())
        return FluxType(TypeKind::Double);
    if (Ty->isFloatTy())
        return FluxType(TypeKind::Float);
    if (Ty->isIntegerTy(32))
        return FluxType(TypeKind::Int);
    if (Ty->isIntegerTy(1))
        return FluxType(TypeKind::Bool);
    if (Ty->isVoidTy())
        return FluxType(TypeKind::Void);
    // Complex is <2 x double>
    if (Ty->isVectorTy()) {
        auto* VT = llvm::cast<llvm::VectorType>(Ty);
        if (!VT->getElementCount().isScalable() && VT->getElementCount().getKnownMinValue() == 2 &&
            VT->getElementType()->isDoubleTy())
            return FluxType(TypeKind::Complex);
    }
    if (Ty->isStructTy()) {
        auto* ST = llvm::cast<llvm::StructType>(Ty);
        if (ST->getNumElements() == 3)
            return FluxType(TypeKind::Matrix);
    }
    if (Ty->isPointerTy())
        return FluxType(TypeKind::String);
    return FluxType(TypeKind::Double);
}

// Convert any value to i1 branch condition: i1 directly, otherwise FCmpONE against 0.0
static llvm::Value* boolCondition(llvm::Value* V, llvm::IRBuilder<>& Builder, llvm::LLVMContext& Ctx)
{
    if (V->getType()->isIntegerTy(1))
        return V;
    return Builder.CreateFCmpONE(V, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "boolcond");
}

// Helper to promote double to complex <2 x double>
static TypedValue promoteToComplex(TypedValue V, CodegenContext& context)
{
    if (isComplexType(V))
        return V;

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
        llvm::Value* Scaled =
            context.Builder.CreateFMul(FloatVal, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
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
    llvm::Function* CosF = llvm::Intrinsic::getDeclaration(context.TheModule, llvm::Intrinsic::cos, {DoubleTy});
    llvm::Function* SinF = llvm::Intrinsic::getDeclaration(context.TheModule, llvm::Intrinsic::sin, {DoubleTy});

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
                                                    selfArg = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, "self_ptr");
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
                                                llvm::Value* loadedVal = context.Builder.CreateLoad(curLLVMType, selfArg, "self_destroy");
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
                                if (pt.Kind != TypeKind::Void) { hasPayload = true; break; }
                            }
                            if (hasPayload) {
                                llvm::StructType* taggedTy = enumInfo.LLVMType;
                                llvm::Value* structPtr = context.Builder.CreateAlloca(taggedTy, nullptr,
                                    MemberName + "_enum");
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
                                if (n == ft.GenericName) { found = true; break; }
                            }
                            if (!found) genericParamNames.push_back(ft.GenericName);
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
                            specName != structInfo.Name &&
                            specName.find(structInfo.Name + "_") == 0) {
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
                if (!taggedTy) continue;
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
                    llvm::Value* heapPtr = context.Builder.CreateLoad(
                        llvm::PointerType::get(context.TheContext, 0), payloadPtr, "heap_ptr");
                    llvm::Value* concretePtr = context.Builder.CreatePointerCast(
                        heapPtr, llvm::PointerType::get(payloadStructInfo.LLVMType, 0), "concrete_ptr");
                    fieldPtr = context.Builder.CreateStructGEP(
                        payloadStructInfo.LLVMType, concretePtr, fieldIdx, MemberName);
                } else {
                    // Non-boxed: direct GEP into the union's payload slot
                    fieldPtr = context.Builder.CreateStructGEP(
                        payloadStructInfo.LLVMType, payloadPtr, fieldIdx, MemberName);
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
            case '+': CurV = context.Builder.CreateFAdd(CurV, RHSV, "addtmp"); break;
            case '-': CurV = context.Builder.CreateFSub(CurV, RHSV, "subtmp"); break;
            case '*': CurV = context.Builder.CreateFMul(CurV, RHSV, "multmp"); break;
            case '/': CurV = context.Builder.CreateFDiv(CurV, RHSV, "divtmp"); break;
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
                if (typeIt != context.NamedTypes.end() && typeIt->second.isStruct()) {
                    int typeId = typeIt->second.StructTypeId;
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
                                structPtr = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, "ptr_val");
                            }
                            if (structPtr->getType() != llvm::PointerType::get(context.TheContext, 0)) {
                                structPtr = context.Builder.CreateBitCast(structPtr, llvm::PointerType::get(context.TheContext, 0));
                            }
                            llvm::Value* gep = context.Builder.CreateStructGEP(structInfo.LLVMType, structPtr, fieldIdx, MEM->getMemberName());
                            TypedValue rhsVal = Val->codegen(context);
                            if (!rhsVal.Val) {
                                std::cerr << "[FLUX ERROR] Struct field assignment value expression failed to codegen" << std::endl;
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

        return TypedValue(context.Builder.CreateCall(SetPF, {PtrDouble, NewValTV.Val}, "setp"), TypeKind::Double);
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
            {'+', "add"}, {'-', "sub"}, {'*', "mul"}, {'/', "div"}, {'%', "rem"},
            {'<', "lt"}, {'>', "gt"},
            {static_cast<int>(TokenType::tok_less_equal), "le"},
            {static_cast<int>(TokenType::tok_greater_equal), "ge"},
            {static_cast<int>(TokenType::tok_equal), "eq"},
            {static_cast<int>(TokenType::tok_not_equal), "ne"},
        };
        auto findOverload = [&](const TypedValue& lhs, const std::string& methodName, const TypedValue& rhs) -> TypedValue {
            std::string typeName;
            if (lhs.Type.Kind == TypeKind::UserStruct && lhs.Type.StructTypeId >= 0 &&
                lhs.Type.StructTypeId < static_cast<int>(context.StructTypes.size())) {
                typeName = context.StructTypes[lhs.Type.StructTypeId].Name;
            } else if (lhs.Type.Kind == TypeKind::UserEnum && lhs.Type.EnumTypeId >= 0 &&
                       lhs.Type.EnumTypeId < static_cast<int>(context.EnumTypes.size())) {
                typeName = context.EnumTypes[lhs.Type.EnumTypeId].Name;
            }
            if (typeName.empty()) return TypedValue();
            auto typeIt = context.TypeMethods.find(typeName);
            if (typeIt == context.TypeMethods.end()) return TypedValue();
            auto methodIt = typeIt->second.find(methodName);
            if (methodIt == typeIt->second.end()) return TypedValue();
            llvm::Function* calleeFn = methodIt->second;
            if (!calleeFn) return TypedValue();
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
                    sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "op_sret");
                callArgs.push_back(sretPtr);
            }
            {
                llvm::Value* selfVal = lhs.Val;
                if (shouldPassByPointer(lhs.Type, context)) {
                    if (!selfVal->getType()->isPointerTy()) {
                        llvm::Value* tempAlloca = context.Builder.CreateAlloca(selfVal->getType(), nullptr, "op_self_temp");
                        context.Builder.CreateStore(selfVal, tempAlloca);
                        selfVal = tempAlloca;
                    }
                } else {
                    if ((lhs.Type.Kind == TypeKind::UserStruct || lhs.Type.Kind == TypeKind::UserEnum) && selfVal->getType()->isPointerTy()) {
                        llvm::Type* selfLLVMTy = lhs.Type.getLLVMType(context.TheContext);
                        selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal, "op_self_loaded");
                    }
                }
                unsigned firstArgIdx = isSretCall ? 1 : 0;
                if (calleeFn->arg_size() > firstArgIdx && selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
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
                        llvm::Value* tempAlloca = context.Builder.CreateAlloca(argVal->getType(), nullptr, "op_arg_temp");
                        context.Builder.CreateStore(argVal, tempAlloca);
                        argVal = tempAlloca;
                    }
                } else {
                    if ((rhs.Type.Kind == TypeKind::UserStruct || rhs.Type.Kind == TypeKind::UserEnum) && argVal->getType()->isPointerTy()) {
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
                    sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
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
            if (overloadResult.Val) return overloadResult;
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
            if (Op != static_cast<int>(TokenType::tok_equal) &&
                Op != static_cast<int>(TokenType::tok_not_equal)) {
                std::cerr << "Type error: cannot use enum in operation '" << (char)Op << "'" << std::endl;
                return TypedValue();
            }
            if (L.Type.EnumTypeId != R.Type.EnumTypeId) {
                std::cerr << "Type error: cannot compare different enum types" << std::endl;
                return TypedValue();
            }
        } else if (L.Type.Kind == TypeKind::Vector && R.Type.Kind == TypeKind::Vector) {
            if (Op != static_cast<int>(TokenType::tok_equal) &&
                Op != static_cast<int>(TokenType::tok_not_equal)) {
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
    } else if (Op == static_cast<int>(TokenType::tok_equal) ||
               Op == static_cast<int>(TokenType::tok_not_equal)) {
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
        (Op == static_cast<int>(TokenType::tok_equal) ||
         Op == static_cast<int>(TokenType::tok_not_equal))) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
        llvm::Function* VecEqF = context.TheModule->getFunction("flux_vec_eq");
        if (!VecEqF) {
            llvm::FunctionType* FTy =
                llvm::FunctionType::get(DoubleTy, {VoidPtrTy, Int32Ty, VoidPtrTy, Int32Ty}, false);
            VecEqF = llvm::Function::Create(FTy, llvm::Function::ExternalLinkage, "flux_vec_eq",
                                            context.TheModule);
        }
        llvm::Value* LData = context.Builder.CreateExtractValue(L.Val, 0, "l_vec_ptr");
        llvm::Value* LLen = context.Builder.CreateExtractValue(L.Val, 1, "l_vec_len");
        llvm::Value* RData = context.Builder.CreateExtractValue(R.Val, 0, "r_vec_ptr");
        llvm::Value* RLen = context.Builder.CreateExtractValue(R.Val, 1, "r_vec_len");
        llvm::Value* IsEq = context.Builder.CreateCall(VecEqF, {LData, LLen, RData, RLen}, "veceq");
        if (Op == static_cast<int>(TokenType::tok_equal))
            return TypedValue(context.Builder.CreateUIToFP(
                                  context.Builder.CreateFCmpOEQ(IsEq, llvm::ConstantFP::get(DoubleTy, 1.0), "veceq_cmp"),
                                  DoubleTy, "booltmp"),
                              TypeKind::Bool);
        else
            return TypedValue(context.Builder.CreateUIToFP(
                                  context.Builder.CreateFCmpONE(IsEq, llvm::ConstantFP::get(DoubleTy, 1.0), "vecne_cmp"),
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
            FmodF = llvm::Function::Create(
                llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                        {llvm::Type::getDoubleTy(context.TheContext), llvm::Type::getDoubleTy(context.TheContext)}, false),
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
        return TypedValue(context.Builder.CreateCall(PowF, {LV, RV}, "ewpowtmp"),
                          FluxType(TypeKind::Double, ewDims));
    }
    default:
        std::cerr << "Invalid binary operator: " << Op << std::endl;
        return TypedValue();
    }
}

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
                            sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                        sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "neg_sret");
                        callArgs.push_back(sretPtr);
                    }
                    {
                        llvm::Value* selfVal = OperandTV.Val;
                        if (shouldPassByPointer(OperandTV.Type, context)) {
                            if (!selfVal->getType()->isPointerTy()) {
                                llvm::Value* tempAlloca = context.Builder.CreateAlloca(selfVal->getType(), nullptr, "neg_self_temp");
                                context.Builder.CreateStore(selfVal, tempAlloca);
                                selfVal = tempAlloca;
                            }
                        } else {
                            if ((OperandTV.Type.Kind == TypeKind::UserStruct || OperandTV.Type.Kind == TypeKind::UserEnum) && selfVal->getType()->isPointerTy()) {
                                llvm::Type* selfLLVMTy = OperandTV.Type.getLLVMType(context.TheContext);
                                selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal);
                            }
                        }
                        unsigned firstArgIdx = isSretCall ? 1 : 0;
                        if (calleeFn->arg_size() > firstArgIdx && selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
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
                            sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
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
        // &expr or &mut expr: create alloca, store operand, return pointer
        bool isMut = (Op == static_cast<int>(TokenType::tok_bitwise_and) + 2600);
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
            TransposeF =
                llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false),
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

TypedValue CallExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);

    // Handle expression-based callee (e.g., obj.method(args) or enum variant construction)
    if (CalleeExpr) {
        // Check if this is an enum variant construction with payload
        if (auto* member = dynamic_cast<MemberExprAST*>(CalleeExpr.get())) {
            if (auto* obj = dynamic_cast<const VariableExprAST*>(member->getObject())) {
                const std::string& enumName = obj->getName();
                // Handle generic enum: Enum[TypeArgs].Variant(args)
                if (!obj->getTypeArgs().empty() && context.GenericEnums.count(enumName)) {
                    std::string mangledEnumName = specializeGenericEnum(enumName, obj->getTypeArgs(), context);
                    if (mangledEnumName.empty()) {
                        std::cerr << "Failed to specialize generic enum: " << enumName << std::endl;
                        return TypedValue();
                    }
                    auto specializedEnumIt = context.EnumTypeIndex.find(mangledEnumName);
                    if (specializedEnumIt == context.EnumTypeIndex.end()) {
                        std::cerr << "Specialized enum not found: " << mangledEnumName << std::endl;
                        return TypedValue();
                    }
                    int enumTypeId = specializedEnumIt->second;
                    if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
                        auto& enumInfo = context.EnumTypes[enumTypeId];
                        const std::string& variantName = member->getMemberName();
                        for (size_t i = 0; i < enumInfo.Variants.size(); ++i) {
                            if (enumInfo.Variants[i] == variantName) {
                                int disc = static_cast<int>(i);
                                FluxType& payloadType = enumInfo.VariantPayloads[i];
                                if (payloadType.Kind != TypeKind::Void && !Args.empty()) {
                                    llvm::StructType* taggedTy = enumInfo.LLVMType;
                                    if (!taggedTy) {
                                        std::cerr << "[FLUX ERROR] Specialized enum tagged union has no LLVM struct type" << std::endl;
                                        return TypedValue();
                                    }
                                    llvm::Value* structPtr = context.Builder.CreateAlloca(taggedTy, nullptr,
                                        mangledEnumName + "." + variantName);
                                    llvm::Value* tagPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 0, "tag");
                                    context.Builder.CreateStore(
                                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), disc), tagPtr);
                                    llvm::Value* payloadVal = nullptr;
                                    if (payloadType.Kind == TypeKind::UserStruct) {
                                        resolveUserStructType(payloadType, context);
                                        int anonStructId = payloadType.StructTypeId;
                                        if (anonStructId < 0) {
                                            std::cerr << "[FLUX ERROR] Anonymous struct type ID is invalid for enum payload" << std::endl;
                                            return TypedValue();
                                        }
                                        auto& anonInfo = context.StructTypes[anonStructId];
                                        llvm::Type* anonTy = anonInfo.LLVMType;
                                        if (!anonTy) {
                                            std::cerr << "[FLUX ERROR] Anonymous struct has no LLVM type for enum payload" << std::endl;
                                            return TypedValue();
                                        }
                                        if (Args.size() == 1) {
                                            TypedValue argTV = Args[0]->codegen(context);
                                            if (argTV.Val && argTV.Type.Kind == TypeKind::UserStruct) {
                                                payloadVal = argTV.Val;
                                            }
                                        }
                                        if (!payloadVal) {
                                            llvm::AllocaInst* anonAlloca = context.Builder.CreateAlloca(anonTy, nullptr, "payload_struct");
                                            for (size_t ai = 0; ai < Args.size() && ai < anonInfo.Fields.size(); ++ai) {
                                                TypedValue fv = Args[ai]->codegen(context);
                                                if (!fv.Val) {
                                                    std::cerr << "[FLUX ERROR] Anonymous struct field expression failed to codegen" << std::endl;
                                                    return TypedValue();
                                                }
                                                llvm::Value* fptr = context.Builder.CreateStructGEP(anonTy, anonAlloca, ai, anonInfo.Fields[ai].first);
                                                context.Builder.CreateStore(fv.Val, fptr);
                                            }
                                            payloadVal = context.Builder.CreateLoad(anonTy, anonAlloca, "payload_loaded");
                                        }
                                    } else {
                                        TypedValue argTV = Args[0]->codegen(context);
                                        if (!argTV.Val) {
                                            std::cerr << "[FLUX ERROR] Direct enum payload expression failed to codegen" << std::endl;
                                            return TypedValue();
                                        }
                                        payloadVal = argTV.Val;
                                    }
                                    resolveUserStructType(payloadType, context);
                                    resolveUserEnumType(payloadType, context);
                                    llvm::Type* concretePayloadTy = payloadType.getLLVMType(context.TheContext);
                                    if (payloadVal->getType()->isStructTy() && !concretePayloadTy->isStructTy()) {
                                        if (llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(payloadVal->getType())) {
                                            if (st->getNumElements() == 1)
                                                payloadVal = context.Builder.CreateExtractValue(payloadVal, {0}, "payload_val");
                                        }
                                    }
                                    if (payloadVal->getType() != concretePayloadTy) {
                                        if (concretePayloadTy->isFloatingPointTy() && payloadVal->getType()->isIntegerTy()) {
                                            payloadVal = context.Builder.CreateSIToFP(payloadVal, concretePayloadTy, "payload_cast");
                                        } else if (concretePayloadTy->isIntegerTy() && payloadVal->getType()->isFloatingPointTy()) {
                                            payloadVal = context.Builder.CreateFPToSI(payloadVal, concretePayloadTy, "payload_cast");
                                        } else if (concretePayloadTy->isPointerTy() && payloadVal->getType()->isPointerTy()) {
                                            payloadVal = context.Builder.CreatePointerCast(payloadVal, concretePayloadTy, "payload_cast");
                                        } else {
                                            payloadVal = context.Builder.CreateBitCast(payloadVal, concretePayloadTy, "payload_cast");
                                        }
                                    }
                                    bool isBoxed = disc < static_cast<int>(enumInfo.VariantIsBoxed.size()) ? enumInfo.VariantIsBoxed[disc] : false;
                                    llvm::Value* payloadPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 1, "payload");
                                    if (isBoxed) {
                                        llvm::Function* mallocFn = context.TheModule->getFunction("flux_malloc");
                                        if (!mallocFn) {
                                            llvm::Type* sizeTy = llvm::Type::getInt64Ty(context.TheContext);
                                            mallocFn = llvm::Function::Create(
                                                llvm::FunctionType::get(llvm::PointerType::get(context.TheContext, 0), {sizeTy}, false),
                                                llvm::Function::ExternalLinkage, "flux_malloc", context.TheModule);
                                        }
                                        uint64_t sizeInBytes = context.TheModule->getDataLayout().getTypeStoreSize(concretePayloadTy);
                                        llvm::Value* sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), sizeInBytes);
                                        llvm::Value* heapAlloc = context.Builder.CreateCall(mallocFn, {sizeVal}, "heap_alloc");
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            heapAlloc, llvm::PointerType::get(context.TheContext, 0), "concrete_heap_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);
                                        llvm::Value* unionPayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0), "union_payload_ptr");
                                        context.Builder.CreateStore(heapAlloc, unionPayloadPtr);
                                    } else {
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0), "concrete_payload_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);
                                    }
                                    llvm::Value* loaded = context.Builder.CreateLoad(taggedTy, structPtr, variantName);
                                    return TypedValue(loaded, FluxType::userEnum(enumTypeId, taggedTy));
                                }
                                break;
                            }
                        }
                    }
                }
                auto enumIt = context.EnumTypeIndex.find(enumName);
                if (enumIt != context.EnumTypeIndex.end()) {
                    int enumTypeId = enumIt->second;
                    if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
                        auto& enumInfo = context.EnumTypes[enumTypeId];
                        const std::string& variantName = member->getMemberName();
                        for (size_t i = 0; i < enumInfo.Variants.size(); ++i) {
                            if (enumInfo.Variants[i] == variantName) {
                                int disc = static_cast<int>(i);
                                // Check if variant expects a payload
                                FluxType& payloadType = enumInfo.VariantPayloads[i];
                                if (payloadType.Kind != TypeKind::Void && !Args.empty()) {
                                    // Create the tagged union struct
                                    llvm::StructType* taggedTy = enumInfo.LLVMType;
                                    if (!taggedTy) {
                                        std::cerr << "[FLUX ERROR] Enum variant tagged union has no LLVM struct type" << std::endl;
                                        return TypedValue();
                                    }
                                    llvm::Value* structPtr = context.Builder.CreateAlloca(taggedTy, nullptr,
                                        enumName + "." + variantName);
                                    llvm::Value* tagPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 0, "tag");
                                    context.Builder.CreateStore(
                                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), disc), tagPtr);
                                    llvm::Value* payloadVal = nullptr;
                                    if (payloadType.Kind == TypeKind::UserStruct) {
                                        resolveUserStructType(payloadType, context);
                                        int anonStructId = payloadType.StructTypeId;
                                        if (anonStructId < 0) {
                                            std::cerr << "[FLUX ERROR] Anonymous struct type ID is invalid for enum payload" << std::endl;
                                            return TypedValue();
                                        }
                                        auto& anonInfo = context.StructTypes[anonStructId];
                                        llvm::Type* anonTy = anonInfo.LLVMType;
                                        if (!anonTy) {
                                            std::cerr << "[FLUX ERROR] Anonymous struct has no LLVM type for enum payload" << std::endl;
                                            return TypedValue();
                                        }

                                        bool isDirectStruct = false;
                                        if (Args.size() == 1) {
                                            TypedValue argTV = Args[0]->codegen(context);
                                            if (argTV.Val && argTV.Type.Kind == TypeKind::UserStruct && argTV.Type.StructTypeId == anonStructId) {
                                                payloadVal = argTV.Val;
                                                isDirectStruct = true;
                                            }
                                        }

                                        if (!isDirectStruct) {
                                            llvm::AllocaInst* anonAlloca = context.Builder.CreateAlloca(anonTy, nullptr, "payload_struct");
                                            for (size_t ai = 0; ai < Args.size() && ai < anonInfo.Fields.size(); ++ai) {
                                                    TypedValue fv = Args[ai]->codegen(context);
                                                if (!fv.Val) {
                                                    std::cerr << "[FLUX ERROR] Anonymous struct field expression failed to codegen" << std::endl;
                                                    return TypedValue();
                                                }
                                                llvm::Value* fptr = context.Builder.CreateStructGEP(anonTy, anonAlloca, ai, anonInfo.Fields[ai].first);
                                                context.Builder.CreateStore(fv.Val, fptr);
                                            }
                                            payloadVal = context.Builder.CreateLoad(anonTy, anonAlloca, "payload_loaded");
                                        }
                                    } else {
                                        TypedValue argTV = Args[0]->codegen(context);
                                        if (!argTV.Val) {
                                            std::cerr << "[FLUX ERROR] Direct enum payload expression failed to codegen" << std::endl;
                                            return TypedValue();
                                        }
                                        payloadVal = argTV.Val;
                                    }
                                    resolveUserStructType(payloadType, context);
                                    resolveUserEnumType(payloadType, context);
                                    llvm::Type* concretePayloadTy = payloadType.getLLVMType(context.TheContext);

                                    if (payloadVal->getType() != concretePayloadTy) {
                                        if (concretePayloadTy->isFloatingPointTy() && payloadVal->getType()->isIntegerTy()) {
                                            payloadVal = context.Builder.CreateSIToFP(payloadVal, concretePayloadTy, "payload_cast");
                                        } else if (concretePayloadTy->isIntegerTy() && payloadVal->getType()->isFloatingPointTy()) {
                                            payloadVal = context.Builder.CreateFPToSI(payloadVal, concretePayloadTy, "payload_cast");
                                        } else if (concretePayloadTy->isPointerTy() && payloadVal->getType()->isPointerTy()) {
                                            payloadVal = context.Builder.CreatePointerCast(payloadVal, concretePayloadTy, "payload_cast");
                                        } else {
                                            payloadVal = context.Builder.CreateBitCast(payloadVal, concretePayloadTy, "payload_cast");
                                        }
                                    }

                                    bool isBoxed = disc < static_cast<int>(enumInfo.VariantIsBoxed.size()) ? enumInfo.VariantIsBoxed[disc] : false;
                                    llvm::Value* payloadPtr = context.Builder.CreateStructGEP(taggedTy, structPtr, 1, "payload");

                                    if (isBoxed) {
                                        llvm::Function* mallocFn = context.TheModule->getFunction("flux_malloc");
                                        if (!mallocFn) {
                                            llvm::Type* sizeTy = llvm::Type::getInt64Ty(context.TheContext);
                                            mallocFn = llvm::Function::Create(
                                                llvm::FunctionType::get(llvm::PointerType::get(context.TheContext, 0), {sizeTy}, false),
                                                llvm::Function::ExternalLinkage, "flux_malloc", context.TheModule);
                                        }

                                        uint64_t sizeInBytes = context.TheModule->getDataLayout().getTypeStoreSize(concretePayloadTy);
                                        llvm::Value* sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context.TheContext), sizeInBytes);

                                        llvm::Value* heapAlloc = context.Builder.CreateCall(mallocFn, {sizeVal}, "heap_alloc");

                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            heapAlloc, llvm::PointerType::get(context.TheContext, 0), "concrete_heap_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);

                                        llvm::Value* unionPayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0), "union_payload_ptr");
                                        context.Builder.CreateStore(heapAlloc, unionPayloadPtr);
                                    } else {
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0), "concrete_payload_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);
                                    }
                                    llvm::Value* loaded = context.Builder.CreateLoad(taggedTy, structPtr, variantName);
                                    return TypedValue(loaded, FluxType::userEnum(enumTypeId, taggedTy));
                                }
                                break;
                            }
                        }
                    }
                }
            }

            // Method call: obj.method(args)
            ExprAST* objExpr = member->getObject();
            bool selfIsVariable = false;
            bool passSelfByPtr = false;
            llvm::Value* selfAllocaPtr = nullptr;
            if (auto* varExpr = dynamic_cast<VariableExprAST*>(objExpr)) {
                auto nameIt = context.NamedValues.find(varExpr->getName());
                if (nameIt != context.NamedValues.end()) {
                    selfAllocaPtr = nameIt->second;
                    // If the alloca holds a pointer (e.g. self param inside method body),
                    // load the actual pointer value instead of passing the alloca address
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(selfAllocaPtr)) {
                        if (allocaInst->getAllocatedType()->isPointerTy()) {
                            selfAllocaPtr = context.Builder.CreateLoad(
                                allocaInst->getAllocatedType(), allocaInst, "self_loaded_ptr");
                        }
                    }
                    selfIsVariable = true;
                }

                // Check if this is a module namespace access (e.g., defs.func())
                // Module namespaces are stored as "name.*" in NamedValues
                const std::string& objName = varExpr->getName();
                if (context.ModuleNamespaces.count(objName)) {
                    const std::string& fnName = member->getMemberName();
                    llvm::Function* calleeFn = context.TheModule->getFunction(fnName);
                    if (!calleeFn) {
                        std::cerr << "Module '" << objName << "' has no function '" << fnName << "'" << std::endl;
                        return TypedValue();
                    }
                    // Build call arguments
                    FluxType retType = context.FuncReturnTypes.count(fnName)
                        ? context.FuncReturnTypes[fnName]
                        : FluxType(TypeKind::Double);
                    resolveUserStructType(retType, context);
                    resolveUserEnumType(retType, context);
                    bool isSretCall = calleeFn->getReturnType()->isVoidTy() && calleeFn->arg_size() > 0 &&
                                      calleeFn->hasParamAttribute(0, llvm::Attribute::StructRet);
                    std::vector<llvm::Value*> callArgs;
                    llvm::Value* sretPtr = nullptr;
                    if (isSretCall) {
                        llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                        if (!sretLLVMTy) {
                            sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                        }
                        sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "sret_ret");
                        callArgs.push_back(sretPtr);
                    }
                    for (auto& Arg : Args) {
                        TypedValue argVal = Arg->codegen(context);
                        if (!argVal.Val) {
                            std::cerr << "[FLUX ERROR] Module call argument failed to codegen" << std::endl;
                            return TypedValue();
                        }
                        callArgs.push_back(argVal.Val);
                    }
                    if (isSretCall) {
                        context.Builder.CreateCall(calleeFn, callArgs);
                        llvm::Value* result = context.Builder.CreateLoad(
                            retType.getLLVMType(context.TheContext), sretPtr, "sret_val");
                        return TypedValue(result, retType);
                    }
                    llvm::Value* result = context.Builder.CreateCall(calleeFn, callArgs, "calltmp");
                    return TypedValue(result, retType);
                }
            }
            TypedValue objVal = member->getObject()->codegen(context);
            if (!objVal.Val) {
                std::cerr << "[FLUX ERROR] Method call: object expression failed to codegen" << std::endl;
                return TypedValue();
            }
            passSelfByPtr = shouldPassByPointer(objVal.Type, context);

            // Trait object dispatch: extract vtable/data from fat pointer and dispatch dynamically
            if (objVal.Type.isTraitObject()) {
                int traitId = objVal.Type.TraitObjectTypeId;
                if (traitId < 0 || traitId >= static_cast<int>(context.Traits.size())) {
                    std::cerr << "Invalid trait object type ID" << std::endl;
                    return TypedValue();
                }
                const auto& traitInfo = context.Traits[traitId];
                const std::string& methodName = member->getMemberName();

                // Find method index in trait
                int methodIdx = -1;
                for (size_t mi = 0; mi < traitInfo.Methods.size(); ++mi) {
                    if (traitInfo.Methods[mi].Name == methodName) {
                        methodIdx = static_cast<int>(mi);
                        break;
                    }
                }
                if (methodIdx < 0) {
                    std::cerr << "Method '" << methodName << "' not found in trait '" << traitInfo.Name << "'" << std::endl;
                    return TypedValue();
                }

                llvm::PointerType* i8PtrTy = llvm::PointerType::get(context.TheContext, 0);
                llvm::Type* fatPtrTy = objVal.Type.getLLVMType(context.TheContext);

                // Extract data and vtable pointers from fat pointer
                llvm::Value* fatPtrAlloca = context.Builder.CreateAlloca(fatPtrTy, nullptr, "fat_ptr");
                context.Builder.CreateStore(objVal.Val, fatPtrAlloca);
                llvm::Value* dataPtr = context.Builder.CreateLoad(
                    i8PtrTy, context.Builder.CreateStructGEP(fatPtrTy, fatPtrAlloca, 0, "data_gep"), "data");
                llvm::Value* vtablePtrLoaded = context.Builder.CreateLoad(
                    i8PtrTy, context.Builder.CreateStructGEP(fatPtrTy, fatPtrAlloca, 1, "vtable_gep"), "vtable");

                // Load function pointer from vtable (slot = 1 + methodIdx, slot 0 reserved)
                llvm::Value* fnPtrPtr = context.Builder.CreateGEP(
                    i8PtrTy, vtablePtrLoaded,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), 1 + methodIdx),
                    "fn_ptr_gep");
                llvm::Value* fnPtr = context.Builder.CreateLoad(i8PtrTy, fnPtrPtr, "fn_ptr");

                // Bitcast fn ptr to the correct function type
                const auto& sig = traitInfo.Methods[methodIdx];
                std::vector<llvm::Type*> fnArgTypes = {i8PtrTy};
                for (size_t ai = 1; ai < sig.Args.size(); ++ai) {
                    FluxType argType = sig.Args[ai].second;
                    resolveUserStructType(argType, context);
                    resolveUserEnumType(argType, context);
                    fnArgTypes.push_back(argType.getLLVMType(context.TheContext));
                }
                llvm::Type* fnRetTy = sig.ReturnType.getLLVMType(context.TheContext);
                llvm::FunctionType* fnFT = llvm::FunctionType::get(fnRetTy, fnArgTypes, false);
                llvm::Value* typedFnPtr = context.Builder.CreatePointerCast(fnPtr,
                    llvm::PointerType::get(fnFT, 0), "typed_fn");

                // Build call arguments
                std::vector<llvm::Value*> callArgs;
                bool isSret = fnRetTy->isVoidTy();
                llvm::Value* sretAlloca = nullptr;
                if (isSret) {
                    sretAlloca = context.Builder.CreateAlloca(
                        sig.ReturnType.getLLVMType(context.TheContext), nullptr, "sret_tmp");
                    callArgs.push_back(sretAlloca);
                    callArgs.push_back(dataPtr);
                } else {
                    callArgs.push_back(dataPtr);
                }

                for (auto& Arg : Args) {
                    TypedValue argVal = Arg->codegen(context);
                    if (!argVal.Val) return TypedValue();
                    callArgs.push_back(argVal.Val);
                }

                llvm::CallInst* call = context.Builder.CreateCall(fnFT, typedFnPtr, callArgs, methodName);
                if (isSret) {
                    llvm::Value* result = context.Builder.CreateLoad(
                        sig.ReturnType.getLLVMType(context.TheContext), sretAlloca, "sret_val");
                    return TypedValue(result, sig.ReturnType);
                }
                return TypedValue(call, sig.ReturnType);
            }

            // Determine the object's type name from its FluxType
            std::string typeName;
            if (objVal.Type.Kind == TypeKind::UserStruct && objVal.Type.StructTypeId >= 0 &&
                objVal.Type.StructTypeId < static_cast<int>(context.StructTypes.size())) {
                typeName = context.StructTypes[objVal.Type.StructTypeId].Name;
            } else if (objVal.Type.Kind == TypeKind::UserEnum && objVal.Type.EnumTypeId >= 0 &&
                       objVal.Type.EnumTypeId < static_cast<int>(context.EnumTypes.size())) {
                typeName = context.EnumTypes[objVal.Type.EnumTypeId].Name;
            } else if (objVal.Type.Kind == TypeKind::String) {
                typeName = "String";
            }

            if (typeName.empty()) {
                std::cerr << "Cannot dispatch method on value with no type name" << std::endl;
                return TypedValue();
            }

            const std::string& methodName = member->getMemberName();
            auto typeIt = context.TypeMethods.find(typeName);
            if (typeIt == context.TypeMethods.end()) {
                std::cerr << "No methods registered for type '" << typeName << "'" << std::endl;
                return TypedValue();
            }

            auto methodIt = typeIt->second.find(methodName);
            if (methodIt == typeIt->second.end()) {
                std::cerr << "No method '" << methodName << "' on type '" << typeName << "'" << std::endl;
                return TypedValue();
            }

            llvm::Function* calleeFn = methodIt->second;
            if (!calleeFn) {
                std::cerr << "Method '" << methodName << "' on type '" << typeName << "' has no codegen function" << std::endl;
                return TypedValue();
            }

            // Build args: self + user args
            std::vector<llvm::Value*> callArgs;
            bool isSretCall = calleeFn->getReturnType()->isVoidTy() && calleeFn->arg_size() > 0 &&
                              calleeFn->hasParamAttribute(0, llvm::Attribute::StructRet);
            llvm::Value* sretPtr = nullptr;
            FluxType retType = context.FuncReturnTypes[calleeFn->getName().str()];
            resolveUserStructType(retType, context);
            resolveUserEnumType(retType, context);

            if (isSretCall) {
                llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                if (!sretLLVMTy) {
                    sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                }
                sretPtr = context.Builder.CreateAlloca(sretLLVMTy, nullptr, "sret_ret");
                callArgs.push_back(sretPtr);
            }

            {
                llvm::Value* selfVal = objVal.Val;
                if (passSelfByPtr) {
                    if (selfIsVariable && selfAllocaPtr) {
                        selfVal = selfAllocaPtr;
                    } else if (!selfVal->getType()->isPointerTy()) {
                        llvm::Value* tempAlloca = context.Builder.CreateAlloca(selfVal->getType(), nullptr, "self_temp");
                        context.Builder.CreateStore(selfVal, tempAlloca);
                        selfVal = tempAlloca;
                    }
                } else {
                    if ((objVal.Type.Kind == TypeKind::UserStruct || objVal.Type.Kind == TypeKind::UserEnum) && selfVal->getType()->isPointerTy()) {
                        llvm::Type* selfLLVMTy = objVal.Type.getLLVMType(context.TheContext);
                        selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal, "self_loaded");
                    }
                }
                
                unsigned firstArgIdx = isSretCall ? 1 : 0;
                if (calleeFn->arg_size() > firstArgIdx && selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
                    llvm::Type* firstArgTy = calleeFn->getArg(firstArgIdx)->getType();
                    if (firstArgTy->isPointerTy() && selfVal->getType()->isPointerTy())
                        selfVal = context.Builder.CreatePointerCast(selfVal, firstArgTy);
                    else if (selfVal->getType()->isIntegerTy() && firstArgTy->isFloatingPointTy())
                        selfVal = context.Builder.CreateSIToFP(selfVal, firstArgTy, "self_cast");
                    else if (selfVal->getType()->isFloatingPointTy() && firstArgTy->isIntegerTy())
                        selfVal = context.Builder.CreateFPToSI(selfVal, firstArgTy, "self_cast");
                }
                callArgs.push_back(selfVal);
            }

            size_t calleeIdx = isSretCall ? 2 : 1;
            size_t argIdx = 0;
            for (auto& Arg : Args) {
                TypedValue argVal = Arg->codegen(context);
                if (!argVal.Val) {
                    std::cerr << "[FLUX ERROR] Method call argument expression failed to codegen" << std::endl;
                    return TypedValue();
                }
                // Mark non-Copy variable arguments as moved
                if (!isCopyType(argVal.Type)) {
                    if (auto* argVar = dynamic_cast<VariableExprAST*>(Arg.get())) {
                        std::string srcName = argVar->getName();
                        context.MovedVariables.insert(srcName);
                    }
                }
                // Cast argument to match the callee's parameter type
                if (calleeFn->arg_size() > calleeIdx) {
                    llvm::Type* expectedTy = calleeFn->getArg(calleeIdx)->getType();
                    if (shouldPassByPointer(argVal.Type, context)) {
                        if (!argVal.Val->getType()->isPointerTy()) {
                            llvm::Value* tempAlloca = context.Builder.CreateAlloca(argVal.Val->getType(), nullptr, "arg_temp");
                            context.Builder.CreateStore(argVal.Val, tempAlloca);
                            argVal.Val = tempAlloca;
                        }
                    } else {
                        if ((argVal.Type.Kind == TypeKind::UserStruct || argVal.Type.Kind == TypeKind::UserEnum) && argVal.Val->getType()->isPointerTy()) {
                            llvm::Type* structTy = argVal.Type.getLLVMType(context.TheContext);
                            argVal.Val = context.Builder.CreateLoad(structTy, argVal.Val, "arg_loaded");
                        }
                    }
                    
                    // Dyn trait argument upcast for method calls
                    if (argVal.Val->getType() != expectedTy && expectedTy->isStructTy() &&
                        argVal.Type.Kind == TypeKind::UserStruct) {
                        auto* ST = llvm::cast<llvm::StructType>(expectedTy);
                        if (ST->getNumElements() == 2 &&
                            ST->getElementType(0)->isPointerTy() &&
                            ST->getElementType(1)->isPointerTy()) {
                            std::string fnName = calleeFn->getName().str();
                            const std::vector<FluxType>* paramTypes = nullptr;
                            auto pmit = context.CrossModuleParamTypes.find(fnName);
                            if (pmit != context.CrossModuleParamTypes.end())
                                paramTypes = &pmit->second;
                            if (paramTypes && argIdx < paramTypes->size() && (*paramTypes)[argIdx].isTraitObject()) {
                                int traitId = (*paramTypes)[argIdx].TraitObjectTypeId;
                                if (traitId >= 0 && traitId < static_cast<int>(context.Traits.size())) {
                                    const auto& traitInfo = context.Traits[traitId];
                                    int structId = argVal.Type.StructTypeId;
                                    if (structId >= 0 && structId < static_cast<int>(context.StructTypes.size())) {
                                        std::string structName = context.StructTypes[structId].Name;
                                        auto vtableIdxIt = context.VTableIndex.find({traitInfo.Name, structName});
                                        if (vtableIdxIt != context.VTableIndex.end()) {
                                            int vtableIdx = vtableIdxIt->second;
                                            if (vtableIdx >= 0 && vtableIdx < static_cast<int>(context.VTables.size())) {
                                                auto& vtableEntry = context.VTables[vtableIdx];
                                                llvm::PointerType* i8PtrTy = llvm::PointerType::get(context.TheContext, 0);
                                                llvm::Value* dataPtr = argVal.Val;
                                                if (!dataPtr->getType()->isPointerTy()) {
                                                    dataPtr = context.Builder.CreateAlloca(dataPtr->getType(), nullptr, "tmp");
                                                    context.Builder.CreateStore(argVal.Val, dataPtr);
                                                }
                                                if (dataPtr->getType() != i8PtrTy)
                                                    dataPtr = context.Builder.CreatePointerCast(dataPtr, i8PtrTy, "data_ptr");
                                                llvm::Value* vtablePtr = context.Builder.CreatePointerCast(
                                                    vtableEntry.VTableGlobal, i8PtrTy, "vtable_ptr");
                                                llvm::Value* fatPtrAlloca = context.Builder.CreateAlloca(
                                                    expectedTy, nullptr, "fat_arg");
                                                llvm::Value* dataFieldPtr = context.Builder.CreateStructGEP(
                                                    expectedTy, fatPtrAlloca, 0, "data_field");
                                                llvm::Value* vtableFieldPtr = context.Builder.CreateStructGEP(
                                                    expectedTy, fatPtrAlloca, 1, "vtable_field");
                                                context.Builder.CreateStore(dataPtr, dataFieldPtr);
                                                context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                                                argVal.Val = context.Builder.CreateLoad(
                                                    expectedTy, fatPtrAlloca, "fat_arg_val");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (argVal.Val->getType() != expectedTy) {
                        if (expectedTy->isPointerTy() && argVal.Val->getType()->isPointerTy())
                            argVal.Val = context.Builder.CreatePointerCast(argVal.Val, expectedTy);
                        else if (argVal.Val->getType()->isIntegerTy() && expectedTy->isFloatingPointTy())
                            argVal.Val = context.Builder.CreateSIToFP(argVal.Val, expectedTy, "arg_cast");
                        else if (argVal.Val->getType()->isFloatingPointTy() && expectedTy->isIntegerTy())
                            argVal.Val = context.Builder.CreateFPToSI(argVal.Val, expectedTy, "arg_cast");
                    }
                }
                callArgs.push_back(argVal.Val);
                ++calleeIdx;
                ++argIdx;
            }

            if (isSretCall) {
                context.Builder.CreateCall(calleeFn, callArgs);
                llvm::Type* sretLLVMTy = retType.getLLVMType(context.TheContext);
                if (!sretLLVMTy) {
                    sretLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
                }
                llvm::Value* result = context.Builder.CreateLoad(sretLLVMTy, sretPtr, "sret_val");
                return TypedValue(result, retType);
            } else {
                llvm::Value* result = context.Builder.CreateCall(calleeFn, callArgs, methodName);
                return TypedValue(result, retType);
            }
        }
        std::cerr << "Expression-based call not supported for non-member callee" << std::endl;
        return TypedValue();
    }

    std::string Name = Callee;
    auto sepPos = Name.rfind("::");
    if (sepPos != std::string::npos) {
        Name = Name.substr(sepPos + 2);
    }

    // 1. Handle built-ins that depend on argument types (Complex, Matrix, etc.)
    if (Name == "print" || Name == "println") {
        for (auto& Expr : Args) {
            TypedValue Arg = Expr->codegen(context);
            if (!Arg.Val)
                continue;

            llvm::Function* PrintFn = nullptr;
            std::vector<llvm::Value*> PrintArgs;

            if (Arg.Type.Kind == TypeKind::Matrix || Arg.Type.Kind == TypeKind::ComplexMatrix) {
                std::string fnName =
                    (Arg.Type.Kind == TypeKind::ComplexMatrix) ? "print_complex_matrix" : "print_matrix";
                PrintFn = context.TheModule->getFunction(fnName);
                if (!PrintFn)
                    PrintFn = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                {llvm::PointerType::get(context.TheContext, 0)}, false),
                        llvm::Function::ExternalLinkage, fnName, context.TheModule);
                PrintArgs.push_back(context.Builder.CreateExtractValue(Arg.Val, 0));
            } else if (Arg.Type.Kind == TypeKind::String) {
                PrintFn = context.TheModule->getFunction("flux_print_string");
                if (!PrintFn)
                    PrintFn = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                {llvm::Type::getDoubleTy(context.TheContext)}, false),
                        llvm::Function::ExternalLinkage, "flux_print_string", context.TheModule);

                // String value is already a double (bitcasted pointer), pass directly
                PrintArgs.push_back(Arg.Val);
            } else {
                // Default: Print as double (handles Fixed-point too via previous conversion logic)
                PrintFn = context.TheModule->getFunction("flux_print_double");
                if (!PrintFn)
                    PrintFn = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                {llvm::Type::getDoubleTy(context.TheContext)}, false),
                        llvm::Function::ExternalLinkage, "flux_print_double", context.TheModule);

                llvm::Value* Val = Arg.Val;
                if (Arg.Type.Kind == TypeKind::Fixed) {
                    double scale = std::pow(2.0, -Arg.Type.Fract);
                    llvm::Value* FloatVal =
                        context.Builder.CreateSIToFP(Val, llvm::Type::getDoubleTy(context.TheContext));
                    Val = context.Builder.CreateFMul(FloatVal,
                                                     llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
                } else if (Arg.Type.Kind == TypeKind::Int && !Arg.Val->getType()->isDoubleTy()) {
                    Val = context.Builder.CreateSIToFP(Val, llvm::Type::getDoubleTy(context.TheContext));
                } else if (Arg.Type.Kind == TypeKind::Bool) {
                    Val = context.Builder.CreateUIToFP(Val, llvm::Type::getDoubleTy(context.TheContext));
                }
                PrintArgs.push_back(Val);
            }

            if (PrintFn)
                context.Builder.CreateCall(PrintFn, PrintArgs);
        }
        if (Name == "println") {
            llvm::Function* PrintStr = context.TheModule->getFunction("flux_print_string");
            if (PrintStr) {
                llvm::Value* NewLine = context.Builder.CreateGlobalString("\n");
                llvm::Value* NewLineAsDouble = context.Builder.CreateBitCast(
                    context.Builder.CreatePtrToInt(NewLine, llvm::Type::getInt64Ty(context.TheContext)),
                    llvm::Type::getDoubleTy(context.TheContext));
                context.Builder.CreateCall(PrintStr, {NewLineAsDouble});
            }
        }
        return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
    }

    if (Args.size() == 1) {
        TypedValue Arg = Args[0]->codegen(context);
        if (!Arg.Val) {
            std::cerr << "[FLUX ERROR] Call argument expression failed to codegen" << std::endl;
            return TypedValue();
        }

        if (isComplexType(Arg)) {
            llvm::Value* Real = context.Builder.CreateExtractElement(Arg.Val, (uint64_t)0, "cre");
            llvm::Value* Imag = context.Builder.CreateExtractElement(Arg.Val, (uint64_t)1, "cim");

            if (Name == "real")
                return TypedValue(Real, TypeKind::Double);
            if (Name == "imag")
                return TypedValue(Imag, TypeKind::Double);
            if (Name == "conj") {
                llvm::Value* NegImag = context.Builder.CreateFNeg(Imag, "negimag");
                llvm::Value* Vec =
                    context.Builder.CreateInsertElement(llvm::PoisonValue::get(llvm::VectorType::get(
                                                            llvm::Type::getDoubleTy(context.TheContext), 2, false)),
                                                        Real, (uint64_t)0);
                return TypedValue(context.Builder.CreateInsertElement(Vec, NegImag, (uint64_t)1), TypeKind::Complex);
            }
            if (Name == "abs" || Name == "mag") {
                llvm::Value* Re2 = context.Builder.CreateFMul(Real, Real, "re2");
                llvm::Value* Im2 = context.Builder.CreateFMul(Imag, Imag, "im2");
                llvm::Value* Sum = context.Builder.CreateFAdd(Re2, Im2, "sum2");
                llvm::Function* SqrtF = context.TheModule->getFunction("llvm.sqrt.f64");
                if (!SqrtF)
                    SqrtF = llvm::Function::Create(
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                {llvm::Type::getDoubleTy(context.TheContext)}, false),
                        llvm::Function::ExternalLinkage, "llvm.sqrt.f64", context.TheModule);
                return TypedValue(context.Builder.CreateCall(SqrtF, {Sum}, "abs"), TypeKind::Double);
            }
            if (Name == "arg" || Name == "phase") {
                llvm::Function* Atan2F = context.TheModule->getFunction("atan2");
                if (!Atan2F)
                    Atan2F =
                        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext),
                                                                       {llvm::Type::getDoubleTy(context.TheContext),
                                                                        llvm::Type::getDoubleTy(context.TheContext)},
                                                                       false),
                                               llvm::Function::ExternalLinkage, "atan2", context.TheModule);
                return TypedValue(context.Builder.CreateCall(Atan2F, {Imag, Real}, "arg"), TypeKind::Double);
            }
        } else if (Arg.Type.Kind == TypeKind::Matrix) {
            llvm::Value* MatPtr = context.Builder.CreateExtractValue(Arg.Val, 0, "mat_ptr");
            llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
            llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
            llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);

            if (Name == "det") {
                llvm::Function* DetF = context.TheModule->getFunction("flux_matrix_det");
                if (!DetF)
                    DetF =
                        llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_det", context.TheModule);
                return TypedValue(context.Builder.CreateCall(DetF, {MatPtr}, "det"), TypeKind::Double);
            }
            if (Name == "rows") {
                llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
                if (!RowsF)
                    RowsF =
                        llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
                llvm::Value* RowsVal = context.Builder.CreateCall(RowsF, {MatPtr}, "rows");
                return TypedValue(context.Builder.CreateSIToFP(RowsVal, DoubleTy, "rows_fp"), TypeKind::Double);
            }
            if (Name == "cols") {
                llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
                if (!ColsF)
                    ColsF =
                        llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);
                llvm::Value* ColsVal = context.Builder.CreateCall(ColsF, {MatPtr}, "cols");
                return TypedValue(context.Builder.CreateSIToFP(ColsVal, DoubleTy, "cols_fp"), TypeKind::Double);
            }
            if (Name == "inv" || Name == "eig") {
                std::string fnName = (Name == "inv") ? "flux_matrix_inv" : "flux_matrix_eig";
                llvm::Function* Fn = context.TheModule->getFunction(fnName);
                if (!Fn)
                    Fn = llvm::Function::Create(llvm::FunctionType::get(VoidPtrTy, {VoidPtrTy}, false),
                                                llvm::Function::ExternalLinkage, fnName, context.TheModule);
                llvm::Value* ResPtr = context.Builder.CreateCall(Fn, {MatPtr}, Name);

                llvm::Function* RowsF = context.TheModule->getFunction("flux_matrix_rows");
                if (!RowsF)
                    RowsF =
                        llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
                llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
                if (!ColsF)
                    ColsF =
                        llvm::Function::Create(llvm::FunctionType::get(Int32Ty, {VoidPtrTy}, false),
                                               llvm::Function::ExternalLinkage, "flux_matrix_cols", context.TheModule);

                llvm::Value* ResRows = context.Builder.CreateCall(RowsF, {ResPtr}, "res_rows");
                llvm::Value* ResCols = context.Builder.CreateCall(ColsF, {ResPtr}, "res_cols");

                FluxType matTy(TypeKind::Matrix);
                matTy.Dimensions = Arg.Type.Dimensions;
                llvm::StructType* MatStructTy =
                    llvm::cast<llvm::StructType>(matTy.getLLVMType(context.TheContext));
                llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
                return TypedValue(MatVal, matTy);
            }
        } else if (Arg.Type.Kind == TypeKind::Double) {
            if (Name == "abs") {
                llvm::Function* FabsF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::fabs, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FabsF, {Arg.Val}, "abstmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "floor") {
                llvm::Function* FloorF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::floor, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FloorF, {Arg.Val}, "floortmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "ceil") {
                llvm::Function* CeilF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::ceil, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(CeilF, {Arg.Val}, "ceiltmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "round") {
                llvm::Function* RoundF = llvm::Intrinsic::getDeclaration(
                    context.TheModule, llvm::Intrinsic::round, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(RoundF, {Arg.Val}, "roundtmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
        }
    }

    // 2. Handle non-argument built-ins (pi, e, etc. would go here if not externs)

    // 3. Math functions — call extern libm functions directly instead of LLVM
    //    intrinsics. LLVM intrinsics for sin/cos/sqrt/exp/log get lowered to
    //    libm calls in the backend, but under CodeModel::Small + PIC_ the
    //    generated call-site can have misaligned stack, crashing after ~100K+
    //    calls. Calling libm via the regular C ABI (extern symbol lookup)
    //    guarantees correct 16-byte stack alignment.
    if (Args.size() == 1) {
        TypedValue Arg0 = Args[0]->codegen(context);
        if (Arg0.Val && Arg0.Type.Kind == TypeKind::Double) {
            llvm::Function* F = context.TheModule->getFunction(Name);
            if (!F) {
                llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
                llvm::FunctionType* FT =
                    llvm::FunctionType::get(DoubleTy, {DoubleTy}, false);
                F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, context.TheModule);
                F->setDoesNotRecurse();
            }
            if (Name == "sqrt") {
                UnitDimensions dims = Arg0.Type.Dimensions;
                dims.mass /= 2;
                dims.length /= 2;
                dims.time /= 2;
                dims.current /= 2;
                dims.temperature /= 2;
                dims.amount /= 2;
                dims.luminous /= 2;
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "sqrttmp"),
                                  FluxType(TypeKind::Double, dims));
            }
            if (Name == "sin") {
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "sintmp"), TypeKind::Double);
            }
            if (Name == "cos") {
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "costmp"), TypeKind::Double);
            }
            if (Name == "exp") {
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "exptmp"), TypeKind::Double);
            }
            if (Name == "log" || Name == "ln") {
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "logtmp"), TypeKind::Double);
            }
            if (Name == "log10") {
                return TypedValue(context.Builder.CreateCall(F, {Arg0.Val}, "log10tmp"), TypeKind::Double);
            }
        }
    }


    // 4. Check for positional struct constructor: TypeName(args)
    if (!CalleeExpr && Args.size() > 0) {
        auto structIt = context.StructTypeIndex.find(Name);
        if (structIt != context.StructTypeIndex.end()) {
            int typeId = structIt->second;
            if (typeId >= 0 && typeId < static_cast<int>(context.StructTypes.size())) {
                auto& structInfo = context.StructTypes[typeId];
                auto* llvmStructTy = structInfo.LLVMType;
                if (!llvmStructTy) {
                    std::cerr << "Struct " << structInfo.Name << " has no LLVM type" << std::endl;
                    return TypedValue();
                }
                llvm::AllocaInst* alloca = context.Builder.CreateAlloca(llvmStructTy, nullptr, Name);
                size_t numFields = structInfo.Fields.size();
                for (size_t i = 0; i < Args.size() && i < numFields; ++i) {
                    TypedValue fieldVal = Args[i]->codegen(context);
                    if (!fieldVal.Val) {
                        std::cerr << "Failed to codegen arg " << i << " for struct " << Name << std::endl;
                        return TypedValue();
                    }
                    // Mark non-Copy variable arguments as moved
                    if (!isCopyType(fieldVal.Type)) {
                        if (auto* argVar = dynamic_cast<VariableExprAST*>(Args[i].get())) {
                            std::string srcName = argVar->getName();
                            context.MovedVariables.insert(srcName);
                        }
                    }
                    llvm::Value* gep = context.Builder.CreateStructGEP(llvmStructTy, alloca, i,
                        structInfo.Fields[i].first);
                    context.Builder.CreateStore(fieldVal.Val, gep);
                }
                llvm::Value* loaded = context.Builder.CreateLoad(llvmStructTy, alloca, Name);
                FluxType resultType = FluxType::userStruct(typeId, llvmStructTy);
                resolveUserStructType(resultType, context);
                return TypedValue(loaded, resultType);
            }
        }
    }

    // --- Codegen all argument values ---
    // Do this early so their types are available for generic monomorphization.
    std::vector<TypedValue> ArgTVs;
    std::vector<UnitDimensions> ArgDims;
    llvm::Function* CalleeF = nullptr;
    ArgTVs.reserve(Args.size());
    ArgDims.reserve(Args.size());
    for (auto& Expr : Args) {
        TypedValue ArgTV = Expr->codegen(context);
        ArgTVs.push_back(ArgTV);
        ArgDims.push_back(ArgTV.Type.Dimensions);
        // Mark non-Copy variable arguments as moved
        if (!isCopyType(ArgTV.Type)) {
            if (auto* argVar = dynamic_cast<VariableExprAST*>(Expr.get())) {
                std::string srcName = argVar->getName();
                srcName.erase(0, srcName.find_first_not_of(" "));
                srcName.erase(srcName.find_last_not_of(" ") + 1);
                context.MovedVariables.insert(srcName);
            }
        }
    }

    // If the callee wasn't found yet, check for generic function specialization.
    if (!CalleeF) {
        auto genIt = context.GenericFunctions.find(Name);
        if (genIt != context.GenericFunctions.end()) {
            FunctionAST* genericFunc = genIt->second;
            const auto& genericParams = genericFunc->getProto()->getGenericParams();
            const auto& protoArgs = genericFunc->getProto()->getArgs();

            std::map<std::string, FluxType> typeMap;

            if (hasGenericTypeArgs()) {
                if (GenericTypeArgs.size() != genericParams.size()) {
                    std::cerr << "Error: expected " << genericParams.size()
                              << " type arguments for generic function '" << Name
                              << "', got " << GenericTypeArgs.size() << "\n";
                    return TypedValue();
                }
                for (size_t i = 0; i < genericParams.size(); ++i)
                    typeMap[genericParams[i]] = GenericTypeArgs[i];
                // Argument-based inference: if explicit type args still contain
                // generic placeholders (e.g., T from an outer generic scope),
                // infer them from the argument types.
                for (size_t i = 0; i < genericParams.size(); ++i) {
                    if (typeMap[genericParams[i]].isGeneric()) {
                        for (size_t j = 0; j < protoArgs.size() && j < ArgTVs.size(); ++j) {
                            if (protoArgs[j].second.isGeneric() &&
                                protoArgs[j].second.GenericName == genericParams[i]) {
                                typeMap[genericParams[i]] = ArgTVs[j].Type;
                                break;
                            }
                        }
                    }
                }
            } else {
                for (size_t i = 0; i < genericParams.size(); ++i) {
                    for (size_t j = 0; j < protoArgs.size() && j < ArgTVs.size(); ++j) {
                        if (protoArgs[j].second.isGeneric() &&
                            protoArgs[j].second.GenericName == genericParams[i]) {
                            typeMap[genericParams[i]] = ArgTVs[j].Type;
                            break;
                        }
                    }
                }
            }

            if (!typeMap.empty()) {
                // Check trait bounds on generic parameters
                if (!checkTraitBounds(genericFunc->getProto()->getGenericParamBounds(), typeMap, context)) {
                    std::cerr << "[FLUX ERROR] Trait bounds check failed for generic function '" << Name << "'" << std::endl;
                    return TypedValue();
                }

                std::string suffix = "_";
                for (const auto& gp : genericParams) {
                    auto it = typeMap.find(gp);
                    if (it != typeMap.end()) {
                        // Resolve struct/enum types before generating suffix
                        resolveUserStructType(it->second, context);
                        resolveUserEnumType(it->second, context);
                        switch (it->second.Kind) {
                        case TypeKind::Double: suffix += "Double"; break;
                        case TypeKind::Int: suffix += "Int"; break;
                        case TypeKind::Float: suffix += "Float"; break;
                        case TypeKind::Bool: suffix += "Bool"; break;
                        case TypeKind::Matrix: suffix += "Matrix"; break;
                        case TypeKind::ComplexMatrix: suffix += "Cmat"; break;
                        case TypeKind::Complex: suffix += "Complex"; break;
                        case TypeKind::String: suffix += "String"; break;
                        case TypeKind::Vector: suffix += "Vector"; break;
                        case TypeKind::Void: suffix += "Void"; break;
                        case TypeKind::UserStruct: {
                            int sid = it->second.StructTypeId;
                            if (sid >= 0 && sid < static_cast<int>(context.StructTypes.size()))
                                suffix += context.StructTypes[sid].Name;
                            else
                                suffix += "S";
                            break;
                        }
                        case TypeKind::UserEnum: {
                            int eid = it->second.EnumTypeId;
                            if (eid >= 0 && eid < static_cast<int>(context.EnumTypes.size()))
                                suffix += context.EnumTypes[eid].Name;
                            else
                                suffix += "E";
                            break;
                        }
                        default: suffix += "T"; break;
                        }
                    }
                }

                std::string specializedName = Name + suffix;
                std::cerr << "[CALL GEN] Name=" << Name << " suffix=" << suffix << " specializedName=" << specializedName << std::endl;
                if (!GenericTypeArgs.empty()) {
                    std::cerr << "[CALL GEN] generic arg 0 kind=" << static_cast<int>(GenericTypeArgs[0].Kind)
                              << " name='" << GenericTypeArgs[0].GenericName << "'" << std::endl;
                }
                for (auto& [gp, gt] : typeMap) {
                    std::cerr << "[CALL GEN] typeMap " << gp << " -> kind=" << static_cast<int>(gt.Kind)
                              << " name='" << gt.GenericName << "'" << std::endl;
                }
                CalleeF = context.TheModule->getFunction(specializedName);

                if (!CalleeF && !context.CompiledSpecializations.count(specializedName)) {
                    context.CompiledSpecializations.insert(specializedName);
                    auto specializedProto = genericFunc->getProto()->specialize(typeMap, suffix);
                    context.FuncReturnTypes[specializedName] = specializedProto->getReturnType();
                    // Save caller's insertion point — codegenWithProto will change it
                    llvm::BasicBlock* SavedInsertBlock = context.Builder.GetInsertBlock();
                    CalleeF = genericFunc->codegenWithProto(context, specializedProto.get());
                    // Restore the caller's insertion point
                    if (SavedInsertBlock) {
                        context.Builder.SetInsertPoint(SavedInsertBlock);
                    }
                }

                if (CalleeF)
                    Callee = specializedName;
            }
        }

        if (!CalleeF) {
            // Fallback: standard function lookup + auto-declaration
            CalleeF = context.TheModule->getFunction(Callee);
            if (!CalleeF && sepPos != std::string::npos) {
                std::string unqualified = Callee.substr(sepPos + 2);
                CalleeF = context.TheModule->getFunction(unqualified);
            }
            if (!CalleeF) {
                // Check excluded symbols
                if (context.ExcludedSymbols.count(Name)) {
                    std::cerr << "Error: '" << Name << "' was not imported. "
                              << "Use a non-selective import or add it to the import list.\n";
                    return TypedValue();
                }
                // Check for registered extern function types
                auto extIt = context.ExternFuncTypes.find(Callee);
                if (extIt != context.ExternFuncTypes.end()) {
                    const auto& extRetType = extIt->second.first;
                    const auto& extArgTypes = extIt->second.second;
                    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
                    llvm::Type* RetLTy = (extRetType.Kind == TypeKind::Matrix || extRetType.Kind == TypeKind::ComplexMatrix)
                                         ? VoidPtrTy
                                         : ((extRetType.Kind == TypeKind::Void) ? llvm::Type::getVoidTy(context.TheContext)
                                                                               : extRetType.getLLVMType(context.TheContext));
                    std::vector<llvm::Type*> ExtArgLTys;
                    for (const auto& at : extArgTypes) {
                        if (at.Kind == TypeKind::Matrix || at.Kind == TypeKind::ComplexMatrix)
                            ExtArgLTys.push_back(VoidPtrTy);
                        else
                            ExtArgLTys.push_back(at.getLLVMType(context.TheContext));
                    }
                    llvm::FunctionType* ExtFT = llvm::FunctionType::get(RetLTy, ExtArgLTys, false);
                    CalleeF = llvm::Function::Create(ExtFT, llvm::Function::ExternalLinkage, Callee, context.TheModule);
                } else {
                    // Auto-declare using actual argument types (resolve structs/enums to ptr for >16 byte types)
                    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
                    std::vector<llvm::Type*> AutoArgTypes;
                    for (auto& argTv : ArgTVs) {
                        FluxType resolved = argTv.Type;
                        resolveUserStructType(resolved, context);
                        resolveUserEnumType(resolved, context);
                        if (shouldPassByPointer(resolved, context)) {
                            AutoArgTypes.push_back(VoidPtrTy);
                        } else {
                            AutoArgTypes.push_back(resolved.getLLVMType(context.TheContext));
                        }
                    }
                    llvm::FunctionType* AutoFT =
                        llvm::FunctionType::get(llvm::Type::getDoubleTy(context.TheContext), AutoArgTypes, false);
                    CalleeF = llvm::Function::Create(AutoFT, llvm::Function::ExternalLinkage, Callee, context.TheModule);
                }
            }
        }
    }

    if (!CalleeF) {
        std::cerr << "[FLUX ERROR] No callee function resolved for call to '" << Name << "'" << std::endl;
        return TypedValue();
    }

    std::vector<llvm::Value*> ArgsV;
    bool isSretCall = CalleeF->getReturnType()->isVoidTy() && CalleeF->arg_size() > 0 &&
                      CalleeF->hasParamAttribute(0, llvm::Attribute::StructRet);
    bool needsGenState = false;
    bool needsAsyncState = false;
    bool needsSret = false;

    std::cerr << "[CALL ARGS] Name=" << Name << " Callee=" << Callee << " numArgs=" << Args.size()
              << " calleeArgSz=" << CalleeF->arg_size() << " isAsyncFn=" << (CalleeF->arg_size() > (isSretCall ? 1 : 0) ? (CalleeF->arg_begin() + (isSretCall ? 1 : 0))->getName().str() : "?") << std::endl;
    if (CalleeF->arg_size() != Args.size()) {
        // Check for async state param
        {
            unsigned offset = isSretCall ? 1 : 0;
            if (CalleeF->arg_size() > offset) {
                auto ArgIt = CalleeF->arg_begin();
                if (isSretCall) ++ArgIt;
                std::cerr << "[CALL ASYNC] check arg name='" << ArgIt->getName().str() << "'" << std::endl;
                if (ArgIt->getName() == "__async_state")
                    needsAsyncState = true;
                else if (ArgIt->getName() == "__gen_state")
                    needsGenState = true;
            }
        }

        int expectedArgs = (int)Args.size();
        if (needsGenState) expectedArgs += 1;
        if (needsAsyncState) expectedArgs += 1;
        if (isSretCall) expectedArgs += 1;

        if (needsAsyncState && (int)CalleeF->arg_size() == expectedArgs) {
            // Async function call: allocate temp state on stack
            llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
            std::vector<llvm::Type*> StateTypes = {Int32Ty};
            llvm::StructType* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            llvm::Value* StatePtr = context.Builder.CreateAlloca(StateTy, nullptr, "async_temp_state");
            context.Builder.CreateStore(llvm::ConstantInt::get(Int32Ty, 0),
                                        context.Builder.CreateStructGEP(StateTy, StatePtr, 0));
            if (isSretCall) {
                // sret will be added first below, then state goes after
                ArgsV.push_back(StatePtr);
            } else {
                ArgsV.push_back(StatePtr);
            }
        } else if (needsGenState && (int)CalleeF->arg_size() == expectedArgs) {
            // Generator call: allocate temp state
            llvm::Type* Int32Ty = llvm::Type::getInt32Ty(context.TheContext);
            std::vector<llvm::Type*> StateTypes = {Int32Ty};
            llvm::StructType* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            llvm::Value* StatePtr = context.Builder.CreateAlloca(StateTy, nullptr, "gen_temp_state");
            context.Builder.CreateStore(llvm::ConstantInt::get(Int32Ty, 0),
                                        context.Builder.CreateStructGEP(StateTy, StatePtr, 0));
            ArgsV.push_back(StatePtr);
        }

        // Check if it's an sret function (expects hidden return pointer as first arg)
        if (isSretCall) {
            llvm::Type* sretTy = nullptr;
            auto retIt = context.FuncReturnTypes.find(Callee);
            if (retIt != context.FuncReturnTypes.end()) {
                FluxType resolvedRet = retIt->second;
                resolveUserStructType(resolvedRet, context);
                resolveUserEnumType(resolvedRet, context);
                sretTy = resolvedRet.getLLVMType(context.TheContext);
            } else {
                sretTy = CalleeF->getParamAttribute(0, llvm::Attribute::StructRet).getValueAsType();
            }
            if (!sretTy) {
                sretTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
            }
            llvm::Value* RetSpace = context.Builder.CreateAlloca(sretTy, nullptr, "sret_ret");
            if (needsAsyncState || needsGenState) {
                ArgsV.insert(ArgsV.begin(), RetSpace);
            } else {
                ArgsV.push_back(RetSpace);
            }
        } else if ((int)CalleeF->arg_size() != expectedArgs) {
            std::cerr << "[FLUX ERROR] Callee argument count mismatch" << std::endl;
            return TypedValue();
        }
    }

    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        llvm::Value* ArgV = ArgTVs[i].Val;
        if (!ArgV) {
            std::cerr << "[FLUX ERROR] Call argument value is null" << std::endl;
            return TypedValue();
        }

        // Account for all hidden params when indexing into callee arguments
        unsigned ParamIdx = i;
        if (isSretCall) ParamIdx += 1;
        if (needsAsyncState) ParamIdx += 1;
        if (needsGenState) ParamIdx += 1;
        llvm::Type* ArgExpectedTy =
            (ParamIdx < (unsigned)CalleeF->arg_size()) ? CalleeF->getArg(ParamIdx)->getType() : nullptr;
        if (ArgExpectedTy && ArgExpectedTy->isPointerTy() &&
            (ArgTVs[i].Type.Kind == TypeKind::Matrix || ArgTVs[i].Type.Kind == TypeKind::ComplexMatrix)) {
            ArgV = context.Builder.CreateExtractValue(ArgV, 0, "mat_ptr");
        }

        // Ensure string arguments are bitcasted to double for the new ABI
        if (ArgTVs[i].Type.Kind == TypeKind::String && ArgV->getType()->isPointerTy()) {
            llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
            ArgV = context.Builder.CreatePtrToInt(ArgV, Int64Ty, "strptr_int");
            ArgV = context.Builder.CreateBitCast(ArgV, llvm::Type::getDoubleTy(context.TheContext), "strptr_double");
        }

        // Automatic conversion from Fixed-Point to Double for standard functions (like print)
        if (ArgTVs[i].Type.Kind == TypeKind::Fixed) {
            double scale = std::pow(2.0, -ArgTVs[i].Type.Fract);
            llvm::Value* FloatVal = context.Builder.CreateSIToFP(ArgV, llvm::Type::getDoubleTy(context.TheContext));
            ArgV =
                context.Builder.CreateFMul(FloatVal, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
        }

        llvm::Type* ExpectedTy = CalleeF->getArg(ArgsV.size())->getType();
        if (shouldPassByPointer(ArgTVs[i].Type, context)) {
            if (!ArgV->getType()->isPointerTy()) {
                llvm::Value* tempAlloca = context.Builder.CreateAlloca(ArgV->getType(), nullptr, "arg_temp");
                context.Builder.CreateStore(ArgV, tempAlloca);
                ArgV = tempAlloca;
            }
        } else {
            if ((ArgTVs[i].Type.Kind == TypeKind::UserStruct || ArgTVs[i].Type.Kind == TypeKind::UserEnum) && ArgV->getType()->isPointerTy()) {
                llvm::Type* structTy = ArgTVs[i].Type.getLLVMType(context.TheContext);
                ArgV = context.Builder.CreateLoad(structTy, ArgV, "arg_loaded");
            }
        }

        // Dyn trait argument upcast: if the parameter expects a trait object
        // (fat pointer {ptr,ptr}) and the arg is a concrete struct, build the
        // fat pointer by looking up the vtable for the struct/trait pair.
        if (ArgV->getType() != ExpectedTy && ExpectedTy->isStructTy() &&
            ArgTVs[i].Type.Kind == TypeKind::UserStruct) {
            auto* ST = llvm::cast<llvm::StructType>(ExpectedTy);
            if (ST->getNumElements() == 2 &&
                ST->getElementType(0)->isPointerTy() &&
                ST->getElementType(1)->isPointerTy()) {
                // Look up the parameter's FluxType from CrossModuleParamTypes
                const std::vector<FluxType>* paramTypes = nullptr;
                auto pmit = context.CrossModuleParamTypes.find(Name);
                if (pmit != context.CrossModuleParamTypes.end())
                    paramTypes = &pmit->second;
                else {
                    pmit = context.CrossModuleParamTypes.find(Callee);
                    if (pmit != context.CrossModuleParamTypes.end())
                        paramTypes = &pmit->second;
                }
                if (paramTypes && i < paramTypes->size() && (*paramTypes)[i].isTraitObject()) {
                    int traitId = (*paramTypes)[i].TraitObjectTypeId;
                    if (traitId >= 0 && traitId < static_cast<int>(context.Traits.size())) {
                        const auto& traitInfo = context.Traits[traitId];
                        int structId = ArgTVs[i].Type.StructTypeId;
                        if (structId >= 0 && structId < static_cast<int>(context.StructTypes.size())) {
                            std::string structName = context.StructTypes[structId].Name;
                            auto vtableIdxIt = context.VTableIndex.find({traitInfo.Name, structName});
                            if (vtableIdxIt != context.VTableIndex.end()) {
                                int vtableIdx = vtableIdxIt->second;
                                if (vtableIdx >= 0 && vtableIdx < static_cast<int>(context.VTables.size())) {
                                    auto& vtableEntry = context.VTables[vtableIdx];
                                    llvm::PointerType* i8PtrTy = llvm::PointerType::get(context.TheContext, 0);
                                    llvm::Value* dataPtr = ArgV;
                                    if (!dataPtr->getType()->isPointerTy()) {
                                        dataPtr = context.Builder.CreateAlloca(dataPtr->getType(), nullptr, "tmp");
                                        context.Builder.CreateStore(ArgV, dataPtr);
                                    }
                                    if (dataPtr->getType() != i8PtrTy)
                                        dataPtr = context.Builder.CreatePointerCast(dataPtr, i8PtrTy, "data_ptr");
                                    llvm::Value* vtablePtr = context.Builder.CreatePointerCast(
                                        vtableEntry.VTableGlobal, i8PtrTy, "vtable_ptr");
                                    llvm::Value* fatPtrAlloca = context.Builder.CreateAlloca(
                                        ExpectedTy, nullptr, "fat_arg");
                                    llvm::Value* dataFieldPtr = context.Builder.CreateStructGEP(
                                        ExpectedTy, fatPtrAlloca, 0, "data_field");
                                    llvm::Value* vtableFieldPtr = context.Builder.CreateStructGEP(
                                        ExpectedTy, fatPtrAlloca, 1, "vtable_field");
                                    context.Builder.CreateStore(dataPtr, dataFieldPtr);
                                    context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                                    ArgV = context.Builder.CreateLoad(
                                        ExpectedTy, fatPtrAlloca, "fat_arg_val");
                                }
                            }
                        }
                    }
                }
            }
        }

        if (ArgV->getType() != ExpectedTy) {
            if (ExpectedTy->isPointerTy() && ArgV->getType()->isPointerTy()) {
                ArgV = context.Builder.CreatePointerCast(ArgV, ExpectedTy);
            } else if (ExpectedTy->isIntegerTy() && ArgV->getType()->isFloatingPointTy()) {
                ArgV = context.Builder.CreateFPToSI(ArgV, ExpectedTy, "cast");
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isIntegerTy()) {
                ArgV = context.Builder.CreateSIToFP(ArgV, ExpectedTy, "cast");
            } else if (ExpectedTy->isVectorTy() && !ArgV->getType()->isVectorTy()) {
                auto* VT = llvm::dyn_cast<llvm::VectorType>(ExpectedTy);
                if (VT && !VT->getElementCount().isScalable() && VT->getElementCount().getKnownMinValue() == 2 &&
                    VT->getElementType()->isDoubleTy()) {
                    ArgV = promoteToComplex(ArgTVs[i], context).Val;
                }
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isPointerTy()) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                ArgV = context.Builder.CreatePtrToInt(ArgV, Int64Ty, "ptr_to_int");
                ArgV = context.Builder.CreateBitCast(ArgV, ExpectedTy, "int_to_fp");
            } else if (ExpectedTy->isPointerTy() && ArgV->getType()->isFloatingPointTy() &&
                       ArgTVs[i].Type.Kind == TypeKind::String) {
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                ArgV = context.Builder.CreateBitCast(ArgV, Int64Ty, "double_to_int");
                ArgV = context.Builder.CreateIntToPtr(ArgV, ExpectedTy, "int_to_ptr");
            } else if (ExpectedTy->isPointerTy() && ArgV->getType()->isFloatingPointTy()) {
                auto extIt = context.ExternFuncTypes.find(Callee);
                if (extIt != context.ExternFuncTypes.end()) {
                    unsigned paramIdx2 = i;
                    if (isSretCall) paramIdx2 += 1;
                    if (needsAsyncState) paramIdx2 += 1;
                    if (needsGenState) paramIdx2 += 1;
                    if (paramIdx2 < extIt->second.second.size() &&
                        (extIt->second.second[paramIdx2].Kind == TypeKind::Matrix ||
                         extIt->second.second[paramIdx2].Kind == TypeKind::ComplexMatrix)) {
                        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                        ArgV = context.Builder.CreateBitCast(ArgV, Int64Ty, "double_to_int");
                        ArgV = context.Builder.CreateIntToPtr(ArgV, ExpectedTy, "int_to_ptr");
                    }
                }
            } else if (ExpectedTy->isFloatingPointTy() && ArgV->getType()->isStructTy() &&
                       (ArgTVs[i].Type.Kind == TypeKind::Matrix || ArgTVs[i].Type.Kind == TypeKind::ComplexMatrix)) {
                ArgV = context.Builder.CreateExtractValue(ArgV, 0, "mat_ptr");
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                ArgV = context.Builder.CreatePtrToInt(ArgV, Int64Ty, "ptr_to_int");
                ArgV = context.Builder.CreateBitCast(ArgV, ExpectedTy, "int_to_fp");
            }
        }
        ArgsV.push_back(ArgV);
    }
    auto extRetIt = context.ExternFuncTypes.find(Callee);
    bool isMatRet = extRetIt != context.ExternFuncTypes.end() &&
                    (extRetIt->second.first.Kind == TypeKind::Matrix ||
                     extRetIt->second.first.Kind == TypeKind::ComplexMatrix);
    if (isMatRet) {
        bool isComplexMatRet = extRetIt->second.first.Kind == TypeKind::ComplexMatrix;
        llvm::Value* RetPtr = context.Builder.CreateCall(CalleeF, ArgsV, "mat_ret");

        std::string rowsFnName = isComplexMatRet ? "flux_complex_matrix_rows" : "flux_matrix_rows";
        std::string colsFnName = isComplexMatRet ? "flux_complex_matrix_cols" : "flux_matrix_cols";

        llvm::Function* RowsF = context.TheModule->getFunction(rowsFnName);
        if (!RowsF)
            RowsF = llvm::Function::Create(
                llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                        {llvm::PointerType::get(context.TheContext, 0)}, false),
                llvm::Function::ExternalLinkage, rowsFnName, context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction(colsFnName);
        if (!ColsF)
            ColsF = llvm::Function::Create(
                llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                        {llvm::PointerType::get(context.TheContext, 0)}, false),
                llvm::Function::ExternalLinkage, colsFnName, context.TheModule);
        llvm::Value* RetRows = context.Builder.CreateCall(RowsF, {RetPtr}, "mat_rows");
        llvm::Value* RetCols = context.Builder.CreateCall(ColsF, {RetPtr}, "mat_cols");
        llvm::StructType* MatSTy =
            llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        llvm::Value* MatVal = llvm::PoisonValue::get(MatSTy);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetPtr, 0);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetRows, 1);
        MatVal = context.Builder.CreateInsertValue(MatVal, RetCols, 2);
        FluxType extMatType(isComplexMatRet ? TypeKind::ComplexMatrix : TypeKind::Matrix);
        // Propagate element dims from first matrix argument
        if (!ArgDims.empty()) {
            auto extIt3 = context.ExternFuncTypes.find(Callee);
            if (extIt3 != context.ExternFuncTypes.end() && !extIt3->second.second.empty() &&
                (extIt3->second.second[0].Kind == TypeKind::Matrix ||
                 extIt3->second.second[0].Kind == TypeKind::ComplexMatrix)) {
                extMatType.Dimensions = ArgDims[0];
            }
        }
        return TypedValue(MatVal, extMatType);
    }

    // User-defined function returning matrix/vector/struct/enum via sret convention
    if (isSretCall) {
        FluxType sretRetType(TypeKind::Matrix);
        auto ftIt = context.FuncReturnTypes.find(Callee);
        if (ftIt != context.FuncReturnTypes.end()) {
            sretRetType = ftIt->second;
        }
        resolveUserStructType(sretRetType, context);
        resolveUserEnumType(sretRetType, context);

        llvm::Type* sretRetLLVMTy = sretRetType.getLLVMType(context.TheContext);
        if (!sretRetLLVMTy) {
            sretRetLLVMTy = llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
        }

        // ArgsV already has the sret pointer from arg_size mismatch handling
        context.Builder.CreateCall(CalleeF, ArgsV);
        llvm::Value* RetVal = context.Builder.CreateLoad(sretRetLLVMTy, ArgsV[0], "sret_val");

        if (!ArgDims.empty() && (sretRetType.Kind == TypeKind::Matrix || sretRetType.Kind == TypeKind::ComplexMatrix)) {
            auto extIt3 = context.ExternFuncTypes.find(Callee);
            if (extIt3 != context.ExternFuncTypes.end() && !extIt3->second.second.empty() &&
                (extIt3->second.second[0].Kind == TypeKind::Matrix ||
                 extIt3->second.second[0].Kind == TypeKind::ComplexMatrix)) {
                sretRetType.Dimensions = ArgDims[0];
            }
        }
        return TypedValue(RetVal, sretRetType);
    }

    if (extRetIt != context.ExternFuncTypes.end() && extRetIt->second.first.Kind == TypeKind::Void) {
        context.Builder.CreateCall(CalleeF, ArgsV);
        return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
    }
    FluxType retType = typeFromLLVM(CalleeF->getReturnType());
    {
        auto ftIt = context.FuncReturnTypes.find(Callee);
        if (ftIt != context.FuncReturnTypes.end()) {
            // Preserve user-defined struct/enum type info from FuncReturnTypes
            if (ftIt->second.Kind == TypeKind::UserStruct || ftIt->second.Kind == TypeKind::UserEnum) {
                retType = ftIt->second;
            } else {
                retType.Dimensions = ftIt->second.Dimensions;
            }
        }
    }
    // Infer return dimensions for built-in math functions based on arg types
    if (!ArgDims.empty()) {
        static const std::set<std::string> kDimPreserve1 = {"abs",   "neg",   "sign", "floor", "ceil",
                                                            "round", "trunc", "erf",  "erfc"};
        static const std::set<std::string> kDimPreserve2 = {"min", "max"};
        static const std::set<std::string> kDimless = {"sin",   "cos",  "tan",  "asin", "acos",   "atan",
                                                       "atan2", "sinh", "cosh", "tanh", "log",    "log10",
                                                       "log2",  "ln",   "exp",  "exp2", "tgamma", "lgamma"};
        if (kDimPreserve1.count(Name)) {
            retType.Dimensions = ArgDims[0];
        } else if (Name == "sqrt" || Name == "cbrt") {
            int div = (Name == "cbrt") ? 3 : 2;
            retType.Dimensions.mass = ArgDims[0].mass / div;
            retType.Dimensions.length = ArgDims[0].length / div;
            retType.Dimensions.time = ArgDims[0].time / div;
            retType.Dimensions.current = ArgDims[0].current / div;
            retType.Dimensions.temperature = ArgDims[0].temperature / div;
            retType.Dimensions.amount = ArgDims[0].amount / div;
            retType.Dimensions.luminous = ArgDims[0].luminous / div;
        } else if (Name == "pow" && ArgDims.size() >= 2) {
            if (auto* C = llvm::dyn_cast<llvm::ConstantFP>(ArgsV[1])) {
                double exp = C->getValueAPF().convertToDouble();
                if (std::abs(exp - std::round(exp)) < 1e-10) {
                    int e = static_cast<int>(std::round(exp));
                    retType.Dimensions.mass = ArgDims[0].mass * e;
                    retType.Dimensions.length = ArgDims[0].length * e;
                    retType.Dimensions.time = ArgDims[0].time * e;
                    retType.Dimensions.current = ArgDims[0].current * e;
                    retType.Dimensions.temperature = ArgDims[0].temperature * e;
                    retType.Dimensions.amount = ArgDims[0].amount * e;
                    retType.Dimensions.luminous = ArgDims[0].luminous * e;
                } else if (std::abs(exp - 0.5) < 1e-10) {
                    retType.Dimensions.mass = ArgDims[0].mass / 2;
                    retType.Dimensions.length = ArgDims[0].length / 2;
                    retType.Dimensions.time = ArgDims[0].time / 2;
                    retType.Dimensions.current = ArgDims[0].current / 2;
                    retType.Dimensions.temperature = ArgDims[0].temperature / 2;
                    retType.Dimensions.amount = ArgDims[0].amount / 2;
                    retType.Dimensions.luminous = ArgDims[0].luminous / 2;
                }
            }
        } else if (Name == "hypot" && ArgDims.size() >= 2) {
            if (ArgDims[0] == ArgDims[1])
                retType.Dimensions = ArgDims[0];
        } else if (kDimPreserve2.count(Name) && ArgDims.size() >= 2) {
            retType.Dimensions = ArgDims[0];
        } else if (kDimless.count(Name)) {
            retType.Dimensions = UnitDimensions();
        }
    }
    // Infer return dimensions for matrix-to-scalar reductions
    if (retType.Kind == TypeKind::Double && !ArgDims.empty() && !Args.empty()) {
        static const std::set<std::string> kMatToScalar = {
            "matrix_get", "matrix_sum", "matrix_mean", "matrix_trace"};
        if (kMatToScalar.count(Callee)) {
            auto extIt3 = context.ExternFuncTypes.find(Callee);
            if (extIt3 != context.ExternFuncTypes.end() && !extIt3->second.second.empty() &&
                (extIt3->second.second[0].Kind == TypeKind::Matrix ||
                 extIt3->second.second[0].Kind == TypeKind::ComplexMatrix)) {
                retType.Dimensions = ArgDims[0];
            }
        }
    }
    return TypedValue(context.Builder.CreateCall(CalleeF, ArgsV, "calltmp"), retType);
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
        llvm::DIScope* parentScope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
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
    if (context.DebugEnabled) context.LexicalBlocks.pop_back();

    // Generate Else block
    context.Builder.SetInsertPoint(ElseBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
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
    if (context.DebugEnabled) context.LexicalBlocks.pop_back();

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
    llvm::AllocaInst* LoopAlloca = context.Builder.CreateAlloca(
        llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
    context.Builder.CreateStore(StartTV.Val, LoopAlloca);
    context.NamedValues[VarName] = LoopAlloca;

    context.Builder.CreateBr(LoopBB);
    context.Builder.SetInsertPoint(LoopBB);
    if (context.DebugEnabled) {
        llvm::DIScope* parentScope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] For-loop body failed to codegen" << std::endl;
        return TypedValue();
    }
    // Load current value, add step, store back
    llvm::Value* CurrentVar = context.Builder.CreateLoad(
        llvm::Type::getDoubleTy(context.TheContext), LoopAlloca, VarName.c_str());
    llvm::Value* NextVar = context.Builder.CreateFAdd(CurrentVar, StepTV.Val, "nextvar");
    context.Builder.CreateStore(NextVar, LoopAlloca);
    if (context.DebugEnabled) context.LexicalBlocks.pop_back();
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
        llvm::DIScope* parentScope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
        auto* block = context.DebugBuilder->createLexicalBlockFile(parentScope, context.DebugFile);
        context.LexicalBlocks.push_back(block);
    }
    TypedValue BodyTV = Body->codegen(context);
    if (!BodyTV.Val) {
        std::cerr << "[FLUX ERROR] While body failed to codegen" << std::endl;
        return TypedValue();
    }
    if (context.DebugEnabled) context.LexicalBlocks.pop_back();
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
                ? context.StructTypes[InitTV.Type.StructTypeId].Name : "";

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

                        llvm::Value* vtablePtr = context.Builder.CreatePointerCast(
                            vtableEntry.VTableGlobal, i8PtrTy, "vtable_ptr");

                        llvm::Value* fatPtrAlloca = context.Builder.CreateAlloca(
                            Type.getLLVMType(context.TheContext), nullptr, VarName + "_fat");
                        llvm::Value* dataFieldPtr = context.Builder.CreateStructGEP(
                            Type.getLLVMType(context.TheContext), fatPtrAlloca, 0, "data_field");
                        llvm::Value* vtableFieldPtr = context.Builder.CreateStructGEP(
                            Type.getLLVMType(context.TheContext), fatPtrAlloca, 1, "vtable_field");
                        context.Builder.CreateStore(dataPtr, dataFieldPtr);
                        context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                        InitTV.Val = context.Builder.CreateLoad(
                            Type.getLLVMType(context.TheContext), fatPtrAlloca, VarName + "_fat_val");
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
            InitV = context.Builder.CreateFCmpONE(InitV, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0),
                                                  "dbltobool");
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
            llvm::DIScope* scope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
            auto* DbgVar = context.DebugBuilder->createAutoVariable(scope, VarName, context.DebugFile, Init->getLine(), DbgTy);
            auto* DbgLoc = llvm::DILocation::get(context.TheContext, Init->getLine(), Init->getCol(), scope);
            context.DebugBuilder->insertDeclare(Alloca, DbgVar, context.DebugBuilder->createExpression(), DbgLoc, context.Builder.GetInsertBlock());
        }

        llvm::DIScope* parentScope = context.LexicalBlocks.empty() ? context.DebugCompileUnit : context.LexicalBlocks.back();
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
                                        selfArg = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, "self_ptr");
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
                                    llvm::Value* loadedVal = context.Builder.CreateLoad(curLLVMType, selfArg, "self_destroy");
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

    if (context.DebugEnabled) context.LexicalBlocks.pop_back();
    context.NamedValues[VarName] = OldVal;
    context.NamedTypes[VarName] = OldType;
    if (OldWasMoved) context.MovedVariables.insert(VarName);
    else context.MovedVariables.erase(VarName);
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
            if (!ElemTV.Val) continue;
            if (ElemTV.Type.Kind == TypeKind::Vector) {
                // Allocate heap space for inner vector struct, store it, store pointer
                llvm::Value* HeapVec = context.Builder.CreateCall(MallocF,
                    llvm::ConstantInt::get(Int64Ty, 16), "inner_vec_heap");
                llvm::Value* VecPtr = context.Builder.CreatePointerCast(HeapVec,
                    llvm::PointerType::get(context.TheContext, 0), "inner_vec_ptr");
                context.Builder.CreateStore(ElemTV.Val, VecPtr);
                // Bitcast pointer to double for storage in the double data array
                llvm::Value* PtrAsInt = context.Builder.CreatePtrToInt(HeapVec, Int64Ty, "heap_i64");
                llvm::Value* PtrAsDouble = context.Builder.CreateBitCast(PtrAsInt, DoubleTy, "ptr_as_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr,
                    {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
                context.Builder.CreateStore(PtrAsDouble, ElemPtr);
            } else {
                // Scalar element in mixed vector — convert to double and store
                llvm::Value* Val = ElemTV.Val;
                if (Val->getType()->isIntegerTy())
                    Val = context.Builder.CreateSIToFP(Val, DoubleTy, "elem_dbl");
                else if (Val->getType()->isFloatTy())
                    Val = context.Builder.CreateFPExt(Val, DoubleTy, "elem_dbl");
                llvm::Value* ElemPtr = context.Builder.CreateInBoundsGEP(DoubleTy, DataPtr,
                    {llvm::ConstantInt::get(Int64Ty, i)}, "elem_ptr");
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
    DataPtr = context.Builder.CreateBitCast(DataPtr, llvm::PointerType::get(ElemTy->getContext(), 0));

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
        if (!IdxTV.Val) return TypedValue();
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
            llvm::StructType* MatSTy =
                llvm::cast<llvm::StructType>(sliceMatType.getLLVMType(context.TheContext));
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
            RowsF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                                     {llvm::PointerType::get(context.TheContext, 0)},
                                                                     false),
                                           llvm::Function::ExternalLinkage, "flux_matrix_rows", context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction("flux_matrix_cols");
        if (!ColsF)
            ColsF = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                                     {llvm::PointerType::get(context.TheContext, 0)},
                                                                     false),
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

llvm::Function* PrototypeAST::codegen(CodegenContext& context)
{
    // Resolve user-defined type in return type before checking shouldReturnBySRet
    resolveUserStructType(ReturnType, context);
    resolveUserEnumType(ReturnType, context);

    // Specialize generic enum return type
    if (ReturnType.Kind == TypeKind::UserEnum && !ReturnType.GenericName.empty()
        && !ReturnType.GenericArgs.empty() && context.GenericEnums.count(ReturnType.GenericName)) {
        std::string mangledName = specializeGenericEnum(ReturnType.GenericName, ReturnType.GenericArgs, context);
        if (!mangledName.empty()) {
            auto enumIdxIt = context.EnumTypeIndex.find(mangledName);
            if (enumIdxIt != context.EnumTypeIndex.end()) {
                int enumTypeId = enumIdxIt->second;
                if (enumTypeId >= 0 && enumTypeId < static_cast<int>(context.EnumTypes.size())) {
                    auto& enumInfo = context.EnumTypes[enumTypeId];
                    ReturnType = FluxType::userEnum(enumTypeId, enumInfo.LLVMType);
                }
            }
        }
    }

    std::vector<llvm::Type*> ArgTypes;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
    llvm::Type* DoublePtrTy = llvm::PointerType::get(DoubleTy->getContext(), 0);

    const bool useSRet = shouldReturnBySRet(ReturnType, context);

    if (useSRet) {
        // sret: hidden first parameter is pointer to return struct
        ArgTypes.push_back(VoidPtrTy);
    }

    if (IsGenerator) {
        // Generators take a hidden pointer to their state struct
        ArgTypes.push_back(VoidPtrTy);
    }

    if (IsAsync) {
        // Async functions take a hidden pointer to their state struct
        ArgTypes.push_back(VoidPtrTy);
    }

    const bool isUpdateLike = Name == "update" || (Name.rfind("update_", 0) == 0);

    for (auto& Arg : Args) {
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
            resolveUserStructType(Arg.second, context);
            resolveUserEnumType(Arg.second, context);
            if (shouldPassByPointer(Arg.second, context)) {
                ArgTypes.push_back(VoidPtrTy);
            } else {
                ArgTypes.push_back(Arg.second.getLLVMType(context.TheContext));
            }
        }
    }

    // sret returns use void return type with parameter
    llvm::Type* RetTy =
        useSRet ? llvm::Type::getVoidTy(context.TheContext)
                : (ReturnType.Kind == TypeKind::String ? DoubleTy : ReturnType.getLLVMType(context.TheContext));

    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, context.TheModule);
    F->addFnAttr("stackrealign");

    // Mark sret parameter with attribute
    if (useSRet) {
        llvm::Type* sretTy = nullptr;
        if (ReturnType.Kind == TypeKind::Matrix || ReturnType.Kind == TypeKind::ComplexMatrix) {
            sretTy = FluxType(TypeKind::Matrix).getLLVMType(context.TheContext);
        } else if (ReturnType.Kind == TypeKind::Vector) {
            sretTy = FluxType(TypeKind::Vector).getLLVMType(context.TheContext);
        } else {
            sretTy = ReturnType.getLLVMType(context.TheContext);
        }
        F->addParamAttr(0, llvm::Attribute::getWithStructRetType(context.TheContext, sretTy));
        F->addParamAttr(0, llvm::Attribute::get(context.TheContext, llvm::Attribute::NoAlias));
    }

    // Mark non-self large parameters with readonly attribute
    unsigned AttrIdx = 0;
    if (useSRet) {
        AttrIdx++; // skip sret
    }
    if (IsGenerator) {
        AttrIdx++; // skip gen state
    }
    if (IsAsync) {
        AttrIdx++; // skip async state
    }
    for (size_t i = 0; i < Args.size(); ++i) {
        // Skip readonly for the self parameter (first arg) to allow mutation
        if (shouldPassByPointer(Args[i].second, context) && i > 0) {
            F->addParamAttr(AttrIdx, llvm::Attribute::get(context.TheContext, llvm::Attribute::ReadOnly));
        }
        AttrIdx++;
    }

    unsigned Idx = 0;
    auto ArgIt = F->arg_begin();
    if (useSRet) {
        ArgIt->setName("sret");
        ++ArgIt;
    }
    if (IsGenerator) {
        ArgIt->setName("__gen_state");
        ++ArgIt;
    }
    if (IsAsync) {
        ArgIt->setName("__async_state");
        ++ArgIt;
    }
    for (; ArgIt != F->arg_end(); ++ArgIt) {
        ArgIt->setName(Args[Idx++].first);
    }
    return F;
}

// ============================================================================
// Struct type codegen
// ============================================================================

void StructDeclAST::codegen(CodegenContext& context)
{
    // Check if already registered
    auto it = context.StructTypeIndex.find(Name);
    if (it != context.StructTypeIndex.end())
        return; // Already registered — skip

    // Prepend parent fields if inheritance is used
    if (!ParentName.empty()) {
        auto parentIt = context.StructTypeIndex.find(ParentName);
        if (parentIt != context.StructTypeIndex.end()) {
            int parentId = parentIt->second;
            if (parentId >= 0 && parentId < static_cast<int>(context.StructTypes.size())) {
                const auto& parentFields = context.StructTypes[parentId].Fields;
                Fields.insert(Fields.begin(), parentFields.begin(), parentFields.end());
            }
        }
    }

    // Collect field types — resolve any unresolved struct types first
    std::vector<llvm::Type*> fieldTypes;
    for (auto& [fieldName, fieldType] : Fields) {
        resolveUserStructType(fieldType, context);
        resolveUserEnumType(fieldType, context);
        fieldTypes.push_back(fieldType.getLLVMType(context.TheContext));
    }

    // Create LLVM struct type
    auto* llvmStructTy = llvm::StructType::create(context.TheContext, fieldTypes, Name);
    int structId = static_cast<int>(context.StructTypes.size());

    // Register in context
    CodegenContext::StructTypeInfo info;
    info.Name = Name;
    info.ParentName = ParentName;
    info.Fields = Fields;
    info.LLVMType = llvmStructTy;
    info.isCopy = !IsNoCopy;
    context.StructTypes.push_back(std::move(info));
    context.StructTypeIndex[Name] = structId;
    setStructTypeId(structId);
}

// ============================================================================
// Struct construction codegen
// ============================================================================

TypedValue StructConstructExprAST::codegen(CodegenContext& context)
{


    // Handle generic struct instantiation or generic enum braced payload struct
    std::string resolvedName = StructName;
    int typeId = StructTypeId;

    if (!GenericTypeArgs.empty()) {
        // Look up generic struct definition
        auto genIt = context.GenericStructs.find(StructName);
        if (genIt == context.GenericStructs.end()) {
            std::cerr << "Unknown generic struct: " << StructName << std::endl;
            return TypedValue();
        }
        StructDeclAST* genericStruct = genIt->second;
        const auto& genericParams = genericStruct->getGenericParams();
        const auto& genericFields = genericStruct->getFields();

        if (GenericTypeArgs.size() != genericParams.size()) {
            std::cerr << "Generic struct " << StructName << " expects " << genericParams.size()
                      << " type args, got " << GenericTypeArgs.size() << std::endl;
            return TypedValue();
        }

        // Recursively specialize nested generic type args (e.g., Pair[Pair[Double, Double], Double])
        for (auto& arg : GenericTypeArgs)
            ensureGenericTypeArgSpecialized(arg, context);

        // Build type map from the now-fully-specialized type args
        std::map<std::string, FluxType> typeMap;
        for (size_t i = 0; i < genericParams.size(); ++i)
            typeMap[genericParams[i]] = GenericTypeArgs[i];

        // Build suffix using the fully resolved type args
        std::string suffix = "_" + buildGenericTypeSuffix(GenericTypeArgs, context);
        resolvedName = StructName + suffix;

        // Check if already specialized
        auto existingIt = context.StructTypeIndex.find(resolvedName);
        if (existingIt != context.StructTypeIndex.end()) {
            typeId = existingIt->second;
        } else if (!context.CompiledStructSpecializations.count(resolvedName)) {
            context.CompiledStructSpecializations.insert(resolvedName);

            // Substitute concrete types in fields
            std::vector<std::pair<std::string, FluxType>> concreteFields;
            // Prepend parent fields if inheritance is used
            std::string parentName = genericStruct->getParentName();
            if (!parentName.empty()) {
                auto parentIt = context.StructTypeIndex.find(parentName);
                if (parentIt != context.StructTypeIndex.end()) {
                    int parentId = parentIt->second;
                    if (parentId >= 0 && parentId < static_cast<int>(context.StructTypes.size())) {
                        const auto& parentFields = context.StructTypes[parentId].Fields;
                        concreteFields.insert(concreteFields.begin(), parentFields.begin(), parentFields.end());
                    }
                }
            }

            for (auto& [fName, fType] : genericFields) {
                FluxType concreteType = fType;
                if (fType.isGeneric()) {
                    auto mapIt = typeMap.find(fType.GenericName);
                    if (mapIt != typeMap.end()) {
                        concreteType = mapIt->second;
                    }
                }
                concreteFields.push_back({fName, concreteType});
            }

            // Create LLVM struct type
            std::vector<llvm::Type*> fieldTypes;
            for (auto& [_, ft] : concreteFields)
                fieldTypes.push_back(ft.getLLVMType(context.TheContext));

            auto* llvmStructTy = llvm::StructType::create(context.TheContext, fieldTypes, resolvedName);
            typeId = static_cast<int>(context.StructTypes.size());

            CodegenContext::StructTypeInfo info;
            info.Name = resolvedName;
            info.ParentName = parentName;
            info.Fields = concreteFields;
            info.LLVMType = llvmStructTy;
            context.StructTypes.push_back(std::move(info));
            context.StructTypeIndex[resolvedName] = typeId;

            std::cerr << "DEBUG SPECIALIZATION: resolvedName=" << resolvedName << " StructName=" << StructName << " inGenericImpls=" << context.GenericImpls.count(StructName) << std::endl;
            // Monomorphize generic methods if a generic impl exists
            auto implIt = context.GenericImpls.find(StructName);
            if (implIt != context.GenericImpls.end()) {
                ImplDeclAST* genericImpl = implIt->second;
                FluxType concreteSelfType = FluxType::userStruct(typeId, llvmStructTy);
                for (auto& method : genericImpl->getMethods()) {
                    auto* proto = method->getProto();
                    auto specializedProto = proto->specialize(typeMap, suffix);

                    // Force the self argument to be the concrete self type
                    if (!specializedProto->getArgs().empty()) {
                        specializedProto->setArgType(0, concreteSelfType);
                    }

                    // Prepend resolvedName to ensure uniqueness in LLVM
                    std::string unmangledMethodName = proto->getName();
                    if (unmangledMethodName.rfind(StructName + ".", 0) == 0) {
                        unmangledMethodName = unmangledMethodName.substr(StructName.length() + 1);
                    }
                    std::string mangledName = resolvedName + "." + unmangledMethodName;
                    specializedProto->setName(mangledName);

                    // Save caller's insertion point — codegenWithProto will change it
                    llvm::BasicBlock* SavedInsertBlock = context.Builder.GetInsertBlock();

                    // Codegen the specialized method body
                    llvm::Function* specializedFunc = method->codegenWithProto(context, specializedProto.get());

                    // Restore the caller's insertion point
                    if (SavedInsertBlock) {
                        context.Builder.SetInsertPoint(SavedInsertBlock);
                    }

                    // Register in TypeMethods under resolvedName and unmangledMethodName
                    context.TypeMethods[resolvedName][unmangledMethodName] = specializedFunc;
                }
            }

            // Inherit parent methods if the generic struct has a parent
            if (!parentName.empty()) {
                auto parentIt = context.TypeMethods.find(parentName);
                if (parentIt != context.TypeMethods.end()) {
                    for (auto& [methodName, func] : parentIt->second) {
                        if (context.TypeMethods[resolvedName].find(methodName) == context.TypeMethods[resolvedName].end()) {
                            context.TypeMethods[resolvedName][methodName] = func;
                        }
                    }
                }
            }
        } else {
            std::cerr << "Failed to create struct specialization: " << resolvedName << std::endl;
            return TypedValue();
        }
    } else {
        // Resolve struct type ID from name if placeholder
        if (typeId < 0) {
            auto it = context.StructTypeIndex.find(StructName);
            if (it == context.StructTypeIndex.end()) {
                std::cerr << "Unknown struct type: " << StructName << std::endl;
                return TypedValue();
            }
            typeId = it->second;
        }
    }

    if (typeId < 0 || typeId >= static_cast<int>(context.StructTypes.size())) {
        std::cerr << "Invalid struct type ID: " << typeId << std::endl;
        return TypedValue();
    }

    auto& structInfo = context.StructTypes[typeId];
    auto* llvmStructTy = structInfo.LLVMType;
    if (!llvmStructTy) {
        std::cerr << "Struct " << structInfo.Name << " has no LLVM type (id=" << typeId << ")" << std::endl;
        return TypedValue();
    }

    // Allocate struct on the stack
    llvm::AllocaInst* alloca = context.Builder.CreateAlloca(llvmStructTy, nullptr, resolvedName);

    // Initialize each field (default: zero)
    for (size_t i = 0; i < structInfo.Fields.size(); ++i) {
        auto& [fieldName, fieldType] = structInfo.Fields[i];

        // Check if this field was provided
        auto valIt = FieldValues.find(fieldName);
        if (valIt != FieldValues.end()) {
            // Evaluate field value expression
            TypedValue fieldVal = valIt->second->codegen(context);
            if (!fieldVal.Val) {
                std::cerr << "Failed to codegen field '" << fieldName << "' in struct " << resolvedName << std::endl;
                return TypedValue();
            }

            // Get pointer to field
            llvm::Value* gep = context.Builder.CreateStructGEP(llvmStructTy, alloca, i, fieldName);
            context.Builder.CreateStore(fieldVal.Val, gep);
        }
        // Fields not explicitly initialized are zero-initialized (alloca already zeros them)
    }

    // Check if onInit method exists (including parent classes) and call them in base-to-derived order
    std::vector<std::string> initChain;
    std::string currentInitType = resolvedName;
    while (!currentInitType.empty()) {
        initChain.push_back(currentInitType);
        auto indexIt = context.StructTypeIndex.find(currentInitType);
        if (indexIt != context.StructTypeIndex.end()) {
            int curId = indexIt->second;
            if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                currentInitType = context.StructTypes[curId].ParentName;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    for (auto itChain = initChain.rbegin(); itChain != initChain.rend(); ++itChain) {
        std::string chainTypeName = *itChain;
        auto typeIt = context.TypeMethods.find(chainTypeName);
        if (typeIt != context.TypeMethods.end()) {
            auto methodIt = typeIt->second.find("onInit");
            if (methodIt != typeIt->second.end()) {
                llvm::Function* initFn = methodIt->second;
                if (initFn) {
                    auto indexIt = context.StructTypeIndex.find(chainTypeName);
                    if (indexIt != context.StructTypeIndex.end()) {
                        int curId = indexIt->second;
                        if (curId >= 0 && curId < static_cast<int>(context.StructTypes.size())) {
                            auto& structInfo = context.StructTypes[curId];
                            llvm::StructType* curLLVMType = structInfo.LLVMType;
                            FluxType curStructType = FluxType::userStruct(curId, curLLVMType);

                            llvm::Value* selfArg = alloca;
                            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(alloca)) {
                                if (allocaInst->getAllocatedType()->isPointerTy()) {
                                    selfArg = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst, "self_ptr");
                                }
                            }
                            llvm::Type* expectedType = initFn->getFunctionType()->getParamType(0);
                            if (selfArg->getType() != expectedType) {
                                selfArg = context.Builder.CreateBitCast(selfArg, expectedType);
                            }

                            std::vector<llvm::Value*> initArgs;
                            if (shouldPassByPointer(curStructType, context)) {
                                initArgs.push_back(selfArg);
                            } else {
                                llvm::Value* loadedVal = context.Builder.CreateLoad(curLLVMType, selfArg, "self_init");
                                initArgs.push_back(loadedVal);
                            }
                            context.Builder.CreateCall(initFn, initArgs);
                        }
                    }
                }
            }
        }
    }

    // Load the struct value (e.g. for passing to functions)
    llvm::Value* loaded = context.Builder.CreateLoad(llvmStructTy, alloca, resolvedName);
    FluxType resultType = FluxType::userStruct(typeId, llvmStructTy);
    resolveUserStructType(resultType, context);
    return TypedValue(loaded, resultType);
}

// ============================================================================
// Enum type codegen
// ============================================================================

void EnumDeclAST::codegen(CodegenContext& context)
{
    auto it = context.EnumTypeIndex.find(Name);
    if (it != context.EnumTypeIndex.end())
        return;

    int enumId = static_cast<int>(context.EnumTypes.size());
    CodegenContext::EnumTypeInfo info;
    info.Name = Name;
    info.Variants = Variants;
    // Create the LLVM tagged union type if any variant has a payload
    llvm::StructType* taggedUnionTy = nullptr;
    bool hasPayload = false;
    for (auto& pt : VariantPayloads) {
        if (pt.Kind != TypeKind::Void) {
            hasPayload = true;
            break;
        }
    }
    if (hasPayload) {
        llvm::SmallVector<llvm::Type*, 4> memberTypes;
        memberTypes.push_back(llvm::Type::getInt32Ty(context.TheContext)); // tag
        info.VariantIsBoxed.resize(VariantPayloads.size(), false);
        // Compute the largest payload type (considering boxed payloads as pointer types)
        llvm::Type* largestTy = llvm::Type::getInt8Ty(context.TheContext); // minimum 1 byte
        for (size_t i = 0; i < VariantPayloads.size(); ++i) {
            auto& pt = VariantPayloads[i];
            if (pt.Kind != TypeKind::Void) {
                // Specialize generic struct/enum payload types so the stored
                // VariantPayloads resolve to concrete types (e.g., Pair_Double_Double
                // instead of the generic Pair template). This ensures later
                // match-payload bindings get correct field types.
                ensureGenericTypeArgSpecialized(pt, context);
                // Resolve anonymous struct/enum payload types
                resolveUserStructType(pt, context);
                resolveUserEnumType(pt, context);
                llvm::Type* payloadLLVM = pt.getLLVMType(context.TheContext);
                uint64_t sizeInBits = context.TheModule->getDataLayout().getTypeSizeInBits(payloadLLVM);

                llvm::Type* effectiveLLVMTy = payloadLLVM;
                if (sizeInBits > 128) { // Boxing threshold: 16 bytes (128 bits)
                    info.VariantIsBoxed[i] = true;
                    effectiveLLVMTy = llvm::PointerType::get(context.TheContext, 0); // i8* pointer
                }

                if (context.TheModule->getDataLayout().getTypeSizeInBits(effectiveLLVMTy) >
                    context.TheModule->getDataLayout().getTypeSizeInBits(largestTy))
                    largestTy = effectiveLLVMTy;
            }
        }
        // Copy resolved variant payloads into info after specialization/resolution
        info.VariantPayloads = VariantPayloads;
        memberTypes.push_back(largestTy);
        taggedUnionTy = llvm::StructType::create(context.TheContext, memberTypes, Name);
        info.LLVMType = taggedUnionTy;
    } else {
        // No payloads — copy as-is (all Void)
        info.VariantPayloads = VariantPayloads;
    }

    info.isCopy = !IsNoCopy;
    context.EnumTypes.push_back(std::move(info));
    context.EnumTypeIndex[Name] = enumId;
    setEnumTypeId(enumId);
}

void ImplDeclAST::codegen(CodegenContext& context)
{
    // Find the struct type for self
    FluxType selfType(TypeKind::Double); // fallback
    auto structIt = context.StructTypeIndex.find(TypeName);
    if (structIt != context.StructTypeIndex.end()) {
        int typeId = structIt->second;
        if (typeId >= 0 && typeId < static_cast<int>(context.StructTypes.size())) {
            auto& structInfo = context.StructTypes[typeId];
            selfType = FluxType::userStruct(typeId, structInfo.LLVMType);
        }
    }

    for (auto& method : Methods) {
        auto* proto = method->getProto();
        std::string unmangledName = proto->getName();
        std::string mangledName = TypeName + "." + unmangledName;
        proto->setName(mangledName);

        // Set the first argument (self) to the struct type
        if (!proto->getArgs().empty()) {
            proto->setArgType(0, selfType);
        }
        proto->codegen(context);

        // Register method with the UNMANGLED name
        context.TypeMethods[TypeName][unmangledName] = nullptr;
    }

    // Inherit parent methods if ParentName is not empty
    if (!ParentName.empty()) {
        auto parentIt = context.TypeMethods.find(ParentName);
        if (parentIt != context.TypeMethods.end()) {
            for (auto& [methodName, func] : parentIt->second) {
                // Copy parent's method if child doesn't override it
                if (context.TypeMethods[TypeName].find(methodName) == context.TypeMethods[TypeName].end()) {
                    context.TypeMethods[TypeName][methodName] = func;
                }
            }
        }
    }

    // Register all method function pointers (so forward references between methods work)
    for (auto& method : Methods) {
        auto* proto = method->getProto();
        std::string mangledName = proto->getName();
        auto func = context.TheModule->getFunction(mangledName);
        if (func) {
            std::string unmangledName = mangledName.substr(TypeName.length() + 1);
            context.TypeMethods[TypeName][unmangledName] = func;
        }
    }

    // Generate code for each method (third pass — function bodies)
    for (auto& method : Methods) {
        auto* proto = method->getProto();
        std::string mangledName = proto->getName();
        std::string unmangledName = mangledName.substr(TypeName.length() + 1);

        auto func = context.TheModule->getFunction(mangledName);
        if (func) {
            llvm::Function* newFunc = method->codegen(context);
            if (newFunc) {
                context.TypeMethods[TypeName][unmangledName] = newFunc;
            }
        }
    }

    // Register trait implementation if this is a trait impl
    if (isTraitImpl()) {
        // Validate that the trait exists
        auto traitIt = context.TraitIndex.find(TraitName);
        if (traitIt == context.TraitIndex.end()) {
            llvm::errs() << "error: trait '" << TraitName << "' not found\n";
            return;
        }
        // Validate that all trait methods are implemented
        int traitId = traitIt->second;
        if (traitId >= 0 && traitId < static_cast<int>(context.Traits.size())) {
            const auto& traitInfo = context.Traits[traitId];
            for (const auto& sig : traitInfo.Methods) {
                if (context.TypeMethods[TypeName].find(sig.Name) == context.TypeMethods[TypeName].end()) {
                    llvm::errs() << "error: missing method '" << sig.Name
                                 << "' in implementation of trait '" << TraitName
                                 << "' for type '" << TypeName << "'\n";
                    return;
                }
            }
            // Validate that all associated types are bound
            for (const auto& assocType : traitInfo.AssociatedTypes) {
                if (AssociatedTypeMappings.find(assocType) == AssociatedTypeMappings.end()) {
                    llvm::errs() << "error: missing associated type '" << assocType
                                 << "' in implementation of trait '" << TraitName
                                 << "' for type '" << TypeName << "'\n";
                    return;
                }
            }
        }
        context.TraitImplementations.insert({TraitName, TypeName});
    }

    // Generate vtable + thunks for this trait implementation
    if (isTraitImpl() && !TraitName.empty() && !TypeName.empty()) {
        auto traitIdxIt = context.TraitIndex.find(TraitName);
        if (traitIdxIt != context.TraitIndex.end()) {
            int traitId = traitIdxIt->second;
            if (traitId >= 0 && traitId < static_cast<int>(context.Traits.size())) {
                const auto& traitInfo = context.Traits[traitId];

                auto typeMethIt = context.TypeMethods.find(TypeName);
                if (typeMethIt == context.TypeMethods.end() || typeMethIt->second.empty()) {
                    llvm::errs() << "DEBUG: vtable skip " << TraitName << " for " << TypeName << " (no methods)\n";
                    goto end_vtable;
                }

                {
                std::vector<llvm::Type*> vtableFieldTypes;
                llvm::PointerType* i8PtrTy = llvm::PointerType::get(context.TheContext, 0);
                vtableFieldTypes.push_back(i8PtrTy);
                std::vector<llvm::Constant*> vtableInitValues;
                vtableInitValues.push_back(llvm::ConstantPointerNull::get(i8PtrTy));

                for (const auto& sig : traitInfo.Methods) {
                    auto mIt = typeMethIt->second.find(sig.Name);
                    llvm::Function* realFn = (mIt != typeMethIt->second.end()) ? mIt->second : nullptr;
                    if (!realFn) {
                        vtableFieldTypes.push_back(i8PtrTy);
                        vtableInitValues.push_back(llvm::ConstantPointerNull::get(i8PtrTy));
                        continue;
                    }

                    // Build a thunk function: takes i8* (data ptr) + extra args (from trait sig), calls realFn
                    std::string thunkName = "__thunk_" + TypeName + "_" + TraitName + "_" + sig.Name;
                    llvm::Function* existingThunk = context.TheModule->getFunction(thunkName);
                    if (existingThunk) {
                        llvm::Constant* thunkPtr = llvm::ConstantExpr::getPointerBitCastOrAddrSpaceCast(
                            existingThunk, i8PtrTy);
                        vtableFieldTypes.push_back(i8PtrTy);
                        vtableInitValues.push_back(thunkPtr);
                        continue;
                    }

                    llvm::Type* selfLLVMTy = selfType.getLLVMType(context.TheContext);
                    bool passSelfByPointer = shouldPassByPointer(selfType, context);
                    bool realIsSret = realFn->getReturnType()->isVoidTy() && realFn->arg_size() > 0 &&
                                      realFn->hasParamAttribute(0, llvm::Attribute::StructRet);

                    // Thunk: takes i8* self + trait-sig args, calls realFn with correct struct types
                    std::vector<llvm::Type*> thunkArgTypes;
                    thunkArgTypes.push_back(i8PtrTy);
                    for (size_t ai = 1; ai < sig.Args.size(); ++ai) {
                        FluxType argType = sig.Args[ai].second;
                        resolveUserStructType(argType, context);
                        resolveUserEnumType(argType, context);
                        thunkArgTypes.push_back(argType.getLLVMType(context.TheContext));
                    }
                    FluxType traitRetType = sig.ReturnType;
                    resolveUserStructType(traitRetType, context);
                    resolveUserEnumType(traitRetType, context);
                    llvm::Type* traitRetTy = traitRetType.getLLVMType(context.TheContext);
                    llvm::FunctionType* thunkFT = llvm::FunctionType::get(traitRetTy, thunkArgTypes, false);

                    llvm::Function* thunk = llvm::Function::Create(thunkFT,
                        llvm::Function::InternalLinkage, thunkName, context.TheModule);
                    thunk->setGC("shadow-stack");

                    auto thunkBB = llvm::BasicBlock::Create(context.TheContext, "entry", thunk);
                    llvm::IRBuilder<> thunkBuilder(thunkBB);

                    // Load self from i8* data ptr, cast to struct type
                    auto thArgIt = thunk->arg_begin();
                    llvm::Value* dataPtr = thArgIt;
                    llvm::Value* selfPtr = thunkBuilder.CreatePointerCast(dataPtr,
                        llvm::PointerType::get(selfLLVMTy, 0), "self_ptr");
                    llvm::Value* selfVal;
                    if (passSelfByPointer) {
                        selfVal = selfPtr;
                    } else {
                        selfVal = thunkBuilder.CreateLoad(selfLLVMTy, selfPtr, "self_loaded");
                    }

                    // Build call to realFn, casting args to realFn's expected types
                    std::vector<llvm::Value*> callArgs;
                    llvm::Value* sretAlloca = nullptr;
                    if (realIsSret) {
                        sretAlloca = thunkBuilder.CreateAlloca(selfLLVMTy, nullptr, "sret_tmp");
                        callArgs.push_back(sretAlloca);
                    }
                    callArgs.push_back(selfVal);

                    unsigned thunkArgIdx = 1;
                    unsigned realArgIdx = (realIsSret ? 1 : 0) + 1; // skip sret + self
                    for (++thArgIt; thArgIt != thunk->arg_end(); ++thArgIt, ++thunkArgIdx, ++realArgIdx) {
                        llvm::Value* thArg = thArgIt;
                        if (realArgIdx < realFn->arg_size()) {
                            llvm::Type* expectedTy = realFn->getArg(realArgIdx)->getType();
                            if (thArg->getType() != expectedTy) {
                                if (thArg->getType()->isPointerTy() && expectedTy->isPointerTy())
                                    thArg = thunkBuilder.CreatePointerCast(thArg, expectedTy);
                                else if (thArg->getType()->isStructTy() && expectedTy->isPointerTy()) {
                                    llvm::Value* allocaSlot = thunkBuilder.CreateAlloca(thArg->getType(), nullptr, "byref_arg");
                                    thunkBuilder.CreateStore(thArg, allocaSlot);
                                    thArg = allocaSlot;
                                } else if (thArg->getType()->isVectorTy() && expectedTy->isStructTy()) {
                                    // Convert <2 x double> to { double, double }
                                    llvm::Value* v0 = thunkBuilder.CreateExtractElement(thArg, (uint64_t)0, "ext0");
                                    llvm::Value* v1 = thunkBuilder.CreateExtractElement(thArg, (uint64_t)1, "ext1");
                                    llvm::Value* st = llvm::UndefValue::get(expectedTy);
                                    st = thunkBuilder.CreateInsertValue(st, v0, 0, "ins0");
                                    st = thunkBuilder.CreateInsertValue(st, v1, 1, "ins1");
                                    thArg = st;
                                } else if (thArg->getType()->isStructTy() && expectedTy->isVectorTy()) {
                                    // Convert { double, double } to <2 x double>
                                    llvm::Value* v0 = thunkBuilder.CreateExtractValue(thArg, 0, "ext0");
                                    llvm::Value* v1 = thunkBuilder.CreateExtractValue(thArg, 1, "ext1");
                                    llvm::Value* vec = llvm::UndefValue::get(expectedTy);
                                    vec = thunkBuilder.CreateInsertElement(vec, v0, (uint64_t)0, "ins0");
                                    vec = thunkBuilder.CreateInsertElement(vec, v1, (uint64_t)1, "ins1");
                                    thArg = vec;
                                }
                            }
                        }
                        callArgs.push_back(thArg);
                    }

                    if (realIsSret) {
                        thunkBuilder.CreateCall(realFn, callArgs);
                        llvm::Value* result = thunkBuilder.CreateLoad(selfLLVMTy, sretAlloca, "sret_val");
                        if (result->getType() != thunk->getReturnType()) {
                            if (result->getType()->isStructTy() && thunk->getReturnType()->isVectorTy()) {
                                llvm::Value* v0 = thunkBuilder.CreateExtractValue(result, 0, "ext0");
                                llvm::Value* v1 = thunkBuilder.CreateExtractValue(result, 1, "ext1");
                                llvm::Value* vec = llvm::UndefValue::get(thunk->getReturnType());
                                vec = thunkBuilder.CreateInsertElement(vec, v0, (uint64_t)0, "ins0");
                                vec = thunkBuilder.CreateInsertElement(vec, v1, (uint64_t)1, "ins1");
                                result = vec;
                            } else if (result->getType()->isVectorTy() && thunk->getReturnType()->isStructTy()) {
                                llvm::Value* v0 = thunkBuilder.CreateExtractElement(result, (uint64_t)0, "ext0");
                                llvm::Value* v1 = thunkBuilder.CreateExtractElement(result, (uint64_t)1, "ext1");
                                llvm::Value* st = llvm::UndefValue::get(thunk->getReturnType());
                                st = thunkBuilder.CreateInsertValue(st, v0, 0, "ins0");
                                st = thunkBuilder.CreateInsertValue(st, v1, 1, "ins1");
                                result = st;
                            } else if (result->getType()->isIntegerTy(1) && thunk->getReturnType()->isDoubleTy()) {
                                result = thunkBuilder.CreateUIToFP(result, thunk->getReturnType(), "ret_cast");
                            } else if (result->getType()->isDoubleTy() && thunk->getReturnType()->isIntegerTy(1)) {
                                llvm::Value* zero = llvm::ConstantFP::get(result->getType(), 0.0);
                                llvm::Value* cmp = thunkBuilder.CreateFCmpONE(result, zero, "ret_cast");
                                result = cmp;
                            } else {
                                result = thunkBuilder.CreateBitCast(result, thunk->getReturnType(), "ret_cast");
                            }
                        }
                        thunkBuilder.CreateRet(result);
                    } else {
                        llvm::Value* result = thunkBuilder.CreateCall(realFn, callArgs, "thunk_result");
                        if (result->getType() != thunk->getReturnType()) {
                            if (result->getType()->isStructTy() && thunk->getReturnType()->isVectorTy()) {
                                // Convert { double, double } to <2 x double>
                                llvm::Value* v0 = thunkBuilder.CreateExtractValue(result, 0, "ext0");
                                llvm::Value* v1 = thunkBuilder.CreateExtractValue(result, 1, "ext1");
                                llvm::Value* vec = llvm::UndefValue::get(thunk->getReturnType());
                                vec = thunkBuilder.CreateInsertElement(vec, v0, (uint64_t)0, "ins0");
                                vec = thunkBuilder.CreateInsertElement(vec, v1, (uint64_t)1, "ins1");
                                result = vec;
                            } else if (result->getType()->isVectorTy() && thunk->getReturnType()->isStructTy()) {
                                // Convert <2 x double> to { double, double }
                                llvm::Value* v0 = thunkBuilder.CreateExtractElement(result, (uint64_t)0, "ext0");
                                llvm::Value* v1 = thunkBuilder.CreateExtractElement(result, (uint64_t)1, "ext1");
                                llvm::Value* st = llvm::UndefValue::get(thunk->getReturnType());
                                st = thunkBuilder.CreateInsertValue(st, v0, 0, "ins0");
                                st = thunkBuilder.CreateInsertValue(st, v1, 1, "ins1");
                                result = st;
                            } else if (result->getType()->isIntegerTy(1) && thunk->getReturnType()->isDoubleTy()) {
                                result = thunkBuilder.CreateUIToFP(result, thunk->getReturnType(), "ret_cast");
                            } else if (result->getType()->isDoubleTy() && thunk->getReturnType()->isIntegerTy(1)) {
                                llvm::Value* zero = llvm::ConstantFP::get(result->getType(), 0.0);
                                llvm::Value* cmp = thunkBuilder.CreateFCmpONE(result, zero, "ret_cast");
                                result = cmp;
                            } else {
                                result = thunkBuilder.CreateBitCast(result, thunk->getReturnType(), "ret_cast");
                            }
                        }
                        thunkBuilder.CreateRet(result);
                    }

                    llvm::Constant* thunkPtr = llvm::ConstantExpr::getPointerBitCastOrAddrSpaceCast(
                        thunk, i8PtrTy);
                    vtableFieldTypes.push_back(i8PtrTy);
                    vtableInitValues.push_back(thunkPtr);
                }

                if (vtableFieldTypes.size() > 1) {
                    llvm::StructType* vtableSTy = llvm::StructType::create(context.TheContext, vtableFieldTypes,
                        "__vtable_" + TraitName + "_for_" + TypeName);
                    llvm::Constant* vtableInit = llvm::ConstantStruct::get(vtableSTy, vtableInitValues);

                    auto* vtableGV = new llvm::GlobalVariable(*context.TheModule, vtableSTy, true,
                        llvm::GlobalValue::InternalLinkage, vtableInit,
                        "__vtable_" + TraitName + "_for_" + TypeName);

                    CodegenContext::VTableEntry ve;
                    ve.TraitName = TraitName;
                    ve.TypeName = TypeName;
                    ve.VTableGlobal = vtableGV;
                    ve.VTableType = vtableSTy;
                    context.VTableIndex[{TraitName, TypeName}] = static_cast<int>(context.VTables.size());
                    context.VTables.push_back(std::move(ve));
                }
                }
            }
        }
    }
    end_vtable:;
}

void TraitDeclAST::codegen(CodegenContext& context)
{
    // Check that super-traits exist
    for (const auto& st : SuperTraits) {
        if (context.TraitIndex.find(st) == context.TraitIndex.end()) {
            llvm::errs() << "error: super-trait '" << st << "' not found for trait '" << Name << "'\n";
            return;
        }
    }

    CodegenContext::TraitInfo info;
    info.Name = Name;
    info.SuperTraits = SuperTraits;
    info.AssociatedTypes = AssociatedTypes;

    for (const auto& m : Methods) {
        CodegenContext::TraitInfo::MethodSig sig;
        sig.Name = m.Name;
        sig.Args = m.Args;
        sig.ReturnType = m.ReturnType;
        info.Methods.push_back(std::move(sig));
    }

    context.Traits.push_back(std::move(info));
    context.TraitIndex[Name] = static_cast<int>(context.Traits.size()) - 1;
}

llvm::Function* FunctionAST::codegen(CodegenContext& context)
{
    std::cerr << "[CODEGEN] " << Proto->getName() << " isAsync=" << Proto->isAsync() << " isGenerator=" << Body->containsYield() << std::endl;
    bool isGenerator = Body->containsYield();
    if (isGenerator)
        Proto->setGenerator(true);
    bool isAsync = Proto->isAsync();

    const bool useSRet = shouldReturnBySRet(Proto->getReturnType(), context);

    llvm::Function* TheFunction = context.TheModule->getFunction(Proto->getName());
    if (TheFunction) {
        // For sret functions, the declared return type is void but the actual
        // struct type is determined by the sret parameter attribute.
        llvm::Type* ExpectedRetTy = useSRet ? llvm::Type::getVoidTy(context.TheContext)
                                            : Proto->getReturnType().getLLVMType(context.TheContext);
        bool typeMismatch = (TheFunction->getReturnType() != ExpectedRetTy);
        if (!typeMismatch) {
            auto protoArgs = Proto->getArgs();
            size_t expectedNumArgs = protoArgs.size();
            if (useSRet)
                expectedNumArgs += 1; // sret param
            if (isGenerator)
                expectedNumArgs += 1; // gen state param
            if (isAsync)
                expectedNumArgs += 1; // async state param
            if (TheFunction->arg_size() == expectedNumArgs) {
                auto funcArgIt = TheFunction->arg_begin();
                if (useSRet)
                    ++funcArgIt; // skip sret
                if (isGenerator)
                    ++funcArgIt; // skip gen state
                if (isAsync)
                    ++funcArgIt; // skip async state
                for (size_t i = 0; i < protoArgs.size(); ++i, ++funcArgIt) {
                    FluxType pa = protoArgs[i].second;
                    resolveUserStructType(pa, context);
                    resolveUserEnumType(pa, context);
                    llvm::Type* ExpectedArgTy = pa.getLLVMType(context.TheContext);
                    if (funcArgIt->getType() != ExpectedArgTy) {
                        typeMismatch = true;
                        break;
                    }
                }
            } else {
                typeMismatch = true;
            }
        }
        if (typeMismatch) {
            // Function was auto-declared with wrong types — erase and recreate
            TheFunction->eraseFromParent();
            TheFunction = nullptr;
        }
    }
    if (!TheFunction)
        TheFunction = Proto->codegen(context);
    if (!TheFunction)
        return nullptr;

    context.FuncReturnTypes[Proto->getName()] = Proto->getReturnType();

    llvm::DISubprogram* subprogram = nullptr;
    if (context.DebugBuilder && context.DebugCompileUnit && context.DebugFile) {
        subprogram = TheFunction->getSubprogram();
        if (!subprogram) {
            auto typeArray = context.DebugBuilder->getOrCreateTypeArray({});
            auto* subroutineType = context.DebugBuilder->createSubroutineType(typeArray);
            subprogram = context.DebugBuilder->createFunction(context.DebugFile, Proto->getName(), Proto->getName(),
                                                              context.DebugFile, 1, subroutineType, 1,
                                                              llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagDefinition);
            TheFunction->setSubprogram(subprogram);
        }
    }

    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    llvm::BasicBlock* ReturnBB = llvm::BasicBlock::Create(context.TheContext, "return");
    context.Builder.SetInsertPoint(EntryBB);
    context.Builder.SetCurrentDebugLocation(llvm::DebugLoc());

    // Save outer context fields (async/gen state) for restoration after nested codegen
    auto SavedCatchBB = context.CurrentCatchBB;
    auto SavedExAlloca = context.CurrentExceptionAlloca;
    auto SavedGenState = context.GeneratorStateAlloca;
    auto SavedGenStructTy = context.GeneratorStateStructTy;
    auto SavedGenDisp = context.GeneratorDispatcherBB;
    auto SavedYieldTgts = std::move(context.YieldTargets);
    auto SavedAsyncState = context.AsyncStateAlloca;
    auto SavedAsyncStructTy = context.AsyncStateStructTy;
    auto SavedAsyncResult = context.AsyncResultAlloca;
    auto SavedAwaitTgts = std::move(context.AwaitResumeTargets);
    auto SavedAsyncDisp = context.AsyncDispatcherBB;

    // Initialize context for this function
    context.CurrentCatchBB = nullptr;
    context.CurrentExceptionAlloca = nullptr;
    context.GeneratorStateAlloca = nullptr;
    context.GeneratorStateStructTy = nullptr;
    context.GeneratorDispatcherBB = nullptr;
    context.YieldTargets.clear();
    context.AsyncStateAlloca = nullptr;
    context.AsyncStateStructTy = nullptr;
    context.AsyncResultAlloca = nullptr;
    context.AwaitResumeTargets.clear();
    context.AsyncDispatcherBB = nullptr;
    context.LexicalBlocks.clear();

    llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);

    // For sret returns, use the sret parameter as the return value pointer.
    // For other types, create a stack alloca.
    llvm::Value* RetValAlloca = nullptr;
    if (useSRet) {
        // sret parameter is the first argument
        RetValAlloca = &(*TheFunction->arg_begin());
    } else if (!RetTy->isVoidTy()) {
        RetValAlloca = context.Builder.CreateAlloca(RetTy, nullptr, "retval");
        context.Builder.CreateStore(llvm::Constant::getNullValue(RetTy), RetValAlloca);
    }

    llvm::BasicBlock* SavedRetBB = context.CurrentReturnBB;
    llvm::Value* SavedRetAlloca = context.CurrentReturnValueAlloca;
    context.CurrentReturnBB = ReturnBB;
    context.CurrentReturnValueAlloca = RetValAlloca;

    if (isGenerator) {
        // Generator state struct: { i32 state_index }
        std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
        llvm::Type* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
        context.GeneratorStateStructTy = StateTy;
        llvm::Value* StatePtr = &(*TheFunction->arg_begin());

        context.GeneratorStateAlloca = StatePtr;
        context.GeneratorDispatcherBB = llvm::BasicBlock::Create(context.TheContext, "gen_dispatch", TheFunction);

        context.Builder.CreateBr(context.GeneratorDispatcherBB);
        context.Builder.SetInsertPoint(context.GeneratorDispatcherBB);

        // Initial entry block (state 0)
        llvm::BasicBlock* StartBB = llvm::BasicBlock::Create(context.TheContext, "gen_start", TheFunction);
        context.YieldTargets.push_back(StartBB);
    }

    if (isAsync) {
        // Async state struct: { i32 state_index }
        std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
        llvm::Type* StateTy = llvm::StructType::get(context.TheContext, StateTypes);
        context.AsyncStateStructTy = StateTy;
        // For async, the hidden state param is the second hidden param (after sret if any)
        auto ArgIt = TheFunction->arg_begin();
        if (useSRet)
            ++ArgIt;
        llvm::Value* StatePtr = &(*ArgIt);

        context.AsyncStateAlloca = StatePtr;
        context.AsyncResultAlloca = context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, "async_result");
        context.AsyncDispatcherBB = llvm::BasicBlock::Create(context.TheContext, "async_dispatch", TheFunction);

        context.Builder.CreateBr(context.AsyncDispatcherBB);
        context.Builder.SetInsertPoint(context.AsyncDispatcherBB);

        // Initial entry block (state 0)
        llvm::BasicBlock* StartBB = llvm::BasicBlock::Create(context.TheContext, "async_start", TheFunction);
        context.AwaitResumeTargets.push_back(StartBB);
    }

    if (subprogram) {
        context.Builder.SetCurrentDebugLocation(llvm::DILocation::get(context.TheContext, 1, 1, subprogram));
        context.LexicalBlocks.push_back(subprogram);
    }
    context.NamedValues.clear();
    context.MovedVariables.clear();
    const auto& ArgTypes = Proto->getArgs();
    unsigned Idx = 0;
    auto ArgIt = TheFunction->arg_begin();
    if (useSRet)
        ++ArgIt; // Skip sret pointer
    if (isGenerator)
        ++ArgIt; // Skip state pointer
    if (isAsync)
        ++ArgIt; // Skip async state pointer
    const bool isUpdateLike = Proto->getName() == "update" || (Proto->getName().rfind("update_", 0) == 0);

    for (; ArgIt != TheFunction->arg_end(); ++ArgIt) {
        // Resolve type on a mutable copy first to get the correct LLVM type
        FluxType argType = ArgTypes[Idx].second;
        resolveUserStructType(argType, context);
        resolveUserEnumType(argType, context);
        llvm::Type* ExpectedTy = argType.getLLVMType(context.TheContext);

        // Use the prototype's arg name, not the LLVM function's arg name,
        // because when a function is reused from a prior declaration
        // (e.g. auto-import vs. main-file definition) the LLVM arg name
        // may differ from the current prototype's parameter name.
        const std::string argName = (Idx < ArgTypes.size()) ? ArgTypes[Idx].first
                                                            : std::string(ArgIt->getName());
        if (isUpdateLike && (argName == "inputs" || argName == "outputs" || argName == "state")) {
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

        llvm::Type* AllocaTy = ExpectedTy;
        if (shouldPassByPointer(argType, context)) {
            AllocaTy = llvm::PointerType::get(context.TheContext, 0);
        }
        llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(AllocaTy, nullptr, argName);
        if (shouldPassByPointer(argType, context)) {
            llvm::Value* CastArgVal = context.Builder.CreateBitCast(ArgVal, AllocaTy);
            context.Builder.CreateStore(CastArgVal, Alloca);
        } else {
            context.Builder.CreateStore(ArgVal, Alloca);
        }
        context.NamedValues[argName] = Alloca;
        context.NamedTypes[argName] = argType;
        Idx++;
    }

    if (isGenerator) {
        context.Builder.SetInsertPoint(context.YieldTargets[0]);
    }

    if (isAsync) {
        context.Builder.SetInsertPoint(context.AwaitResumeTargets[0]);
    }

    // Codegen local enum and anonymous struct declarations
    for (auto& S : LocalAnonStructs)
        S->codegen(context);
    for (auto& E : LocalEnums)
        E->codegen(context);

    if (TypedValue RetTV = Body->codegen(context)) {
        llvm::Value* RetVal = RetTV.Val;
        llvm::Type* RetTy = Proto->getReturnType().getLLVMType(context.TheContext);


        {
            const auto& retDims = Proto->getReturnType().Dimensions;
            const auto& bodyDims = RetTV.Type.Dimensions;
            if (retDims.mass != bodyDims.mass || retDims.length != bodyDims.length || retDims.time != bodyDims.time ||
                retDims.current != bodyDims.current || retDims.temperature != bodyDims.temperature ||
                retDims.amount != bodyDims.amount || retDims.luminous != bodyDims.luminous) {
                llvm::errs() << "[Flux] Unit mismatch in return value: function returns " << retDims.toString()
                             << " but body produces " << bodyDims.toString() << "\n";
                if (!retDims.isDimensionless()) {
                    llvm::errs() << "[Flux] error: declared return type dimensions do not match body\n";
                    return nullptr;
                }
            }
        }

        if (isGenerator) {
            // End of generator: set state to -1
            llvm::Type* StateTy = context.GeneratorStateStructTy;
            if (!StateTy) {
                std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
                StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            }
            std::cerr << "[CODEGEN GEP] gen_end for " << Proto->getName() << std::endl;
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.GeneratorStateAlloca, 0);
            context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), -1),
                                        IndexPtr);
        }

        if (isAsync) {
            // End of async function: set state to -1
            llvm::Type* StateTy = context.AsyncStateStructTy;
            if (!StateTy) {
                std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
                StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            }
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.AsyncStateAlloca, 0);
            context.Builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), -1),
                                        IndexPtr);
        }

        if (RetVal && RetVal->getType() != RetTy) {
            // Complex (<2 x double>) → double: extract real part
            if (RetTy->isDoubleTy() && RetVal->getType()->isVectorTy()) {
                RetVal = context.Builder.CreateExtractElement(RetVal, (uint64_t)0, "ret_real");
            } else if (RetTy->isIntegerTy() && RetVal->getType()->isFloatingPointTy())
                RetVal = context.Builder.CreateFPToSI(RetVal, RetTy, "cast_final");
            else if (RetTy->isFloatingPointTy() && RetVal->getType()->isIntegerTy()) {
                if (RetVal->getType()->isIntegerTy(1))
                    RetVal = context.Builder.CreateUIToFP(RetVal, RetTy, "bool_cast_final");
                else
                    RetVal = context.Builder.CreateSIToFP(RetVal, RetTy, "cast_final");
            } else if (RetTy->isFloatingPointTy() && RetVal->getType()->isPointerTy()) {
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
                if (RetVal)
                    context.Builder.CreateStore(RetVal, RetValAlloca);
                context.Builder.CreateBr(ReturnBB);
            } else {
                context.Builder.CreateBr(ReturnBB);
            }
        }

        // Generate Return Block
        TheFunction->insert(TheFunction->end(), ReturnBB);
        context.Builder.SetInsertPoint(ReturnBB);
        if (RetValAlloca) {
            if (useSRet) {
                // sret: result already stored via the hidden pointer parameter
                context.Builder.CreateRetVoid();
            } else {
                llvm::Value* FinalRetVal = context.Builder.CreateLoad(RetTy, RetValAlloca, "ret_final");
                context.Builder.CreateRet(FinalRetVal);
            }
        } else {
            context.Builder.CreateRetVoid();
        }

        if (isGenerator) {
            // Generate dispatcher switch
            context.Builder.SetInsertPoint(context.GeneratorDispatcherBB);
            llvm::Type* StateTy = context.GeneratorStateStructTy;
            if (!StateTy) {
                std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
                StateTy = llvm::StructType::get(context.TheContext, StateTypes);
            }
            std::cerr << "[CODEGEN GEP] gen_dispatch for " << Proto->getName() << std::endl;
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.GeneratorStateAlloca, 0);
            llvm::Value* StateIndex = context.Builder.CreateLoad(llvm::Type::getInt32Ty(context.TheContext), IndexPtr);

            llvm::SwitchInst* Switch = context.Builder.CreateSwitch(StateIndex, context.YieldTargets[0],
                                                                    (unsigned)context.YieldTargets.size());
            for (size_t i = 1; i < context.YieldTargets.size(); i++) {
                Switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), (int)i),
                                context.YieldTargets[i]);
            }
        }

        if (isAsync) {
            // Generate async dispatcher switch
            context.Builder.SetInsertPoint(context.AsyncDispatcherBB);
            llvm::Type* StateTy = context.AsyncStateStructTy;
            if (!StateTy) {
                std::vector<llvm::Type*> StateTypes = {llvm::Type::getInt32Ty(context.TheContext)};
                StateTy = llvm::StructType::get(context.TheContext, StateTypes);
                context.AsyncStateStructTy = StateTy;
            }
            llvm::Value* IndexPtr = context.Builder.CreateStructGEP(StateTy, context.AsyncStateAlloca, 0);
            llvm::Value* StateIndex = context.Builder.CreateLoad(llvm::Type::getInt32Ty(context.TheContext), IndexPtr);

            llvm::SwitchInst* Switch = context.Builder.CreateSwitch(StateIndex, context.AwaitResumeTargets[0],
                                                                    (unsigned)context.AwaitResumeTargets.size());
            for (size_t i = 1; i < context.AwaitResumeTargets.size(); i++) {
                Switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), (int)i),
                                context.AwaitResumeTargets[i]);
            }
        }

        // Restore context (after dispatchers, which still need current function's values)
        context.CurrentReturnBB = SavedRetBB;
        context.CurrentReturnValueAlloca = SavedRetAlloca;
        context.CurrentCatchBB = SavedCatchBB;
        context.CurrentExceptionAlloca = SavedExAlloca;
        context.GeneratorStateAlloca = SavedGenState;
        context.GeneratorStateStructTy = SavedGenStructTy;
        context.GeneratorDispatcherBB = SavedGenDisp;
        context.YieldTargets = std::move(SavedYieldTgts);
        context.AsyncStateAlloca = SavedAsyncState;
        context.AsyncStateStructTy = SavedAsyncStructTy;
        context.AsyncResultAlloca = SavedAsyncResult;
        context.AwaitResumeTargets = std::move(SavedAwaitTgts);
        context.AsyncDispatcherBB = SavedAsyncDisp;

        if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
            std::cerr << "LLVM IR Verification FAILED for standard function. Dumping IR:" << std::endl;
            TheFunction->print(llvm::errs());
        }
        if (subprogram) context.LexicalBlocks.pop_back();
        return TheFunction;
    }

    // Restore context on failure path too
    context.CurrentReturnBB = SavedRetBB;
    context.CurrentReturnValueAlloca = SavedRetAlloca;
    context.CurrentCatchBB = SavedCatchBB;
    context.CurrentExceptionAlloca = SavedExAlloca;
    context.GeneratorStateAlloca = SavedGenState;
    context.GeneratorStateStructTy = SavedGenStructTy;
    context.GeneratorDispatcherBB = SavedGenDisp;
    context.YieldTargets = std::move(SavedYieldTgts);
    context.AsyncStateAlloca = SavedAsyncState;
    context.AsyncStateStructTy = SavedAsyncStructTy;
    context.AsyncResultAlloca = SavedAsyncResult;
    context.AwaitResumeTargets = std::move(SavedAwaitTgts);
    context.AsyncDispatcherBB = SavedAsyncDisp;
    if (subprogram) context.LexicalBlocks.pop_back();
    TheFunction->eraseFromParent();
    return nullptr;
}

llvm::Function* FunctionAST::codegenWithProto(CodegenContext& context, PrototypeAST* customProto)
{
    // Temporarily replace Proto with the specialized prototype so
    // codegen() uses the concrete types from the specialization.
    auto savedProto = std::move(Proto);
    Proto = std::unique_ptr<PrototypeAST>(customProto);
    bool isGenerator = Body->containsYield();
    if (isGenerator)
        customProto->setGenerator(true);
    // Save outer scope — codegen() will clear NamedValues & LexicalBlocks
    auto SavedNamedValues = context.NamedValues;
    auto SavedNamedTypes = context.NamedTypes;
    auto SavedLexicalBlocks = context.LexicalBlocks;
    auto SavedDebugLoc = context.Builder.getCurrentDebugLocation();
    llvm::Function* result = codegen(context);
    // Restore outer scope after specialization
    context.NamedValues = std::move(SavedNamedValues);
    context.NamedTypes = std::move(SavedNamedTypes);
    context.LexicalBlocks = std::move(SavedLexicalBlocks);
    context.Builder.SetCurrentDebugLocation(SavedDebugLoc);
    // Release without deleting (customProto is owned by the caller)
    Proto.release();
    Proto = std::move(savedProto);
    return result;
}

TypedValue VoltageExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetVF = context.TheModule->getFunction("flux_get_voltage");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetVF) {
        GetVF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_voltage", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(NodeName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetVF, {PtrDouble}, "v"), TypeKind::Double);
}

TypedValue CurrentExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetIF = context.TheModule->getFunction("flux_get_current");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetIF) {
        GetIF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_current", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(BranchName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetIF, {PtrDouble}, "i"), TypeKind::Double);
}

TypedValue ParameterExprAST::codegen(CodegenContext& context)
{
    llvm::Function* GetPF = context.TheModule->getFunction("flux_get_parameter");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    if (!GetPF) {
        GetPF = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                       llvm::Function::ExternalLinkage, "flux_get_parameter", context.TheModule);
    }

    llvm::Value* StrPtr = context.Builder.CreateGlobalString(ParamName, "ptr_double");
    llvm::Value* IntPtr = context.Builder.CreatePtrToInt(StrPtr, llvm::Type::getInt64Ty(context.TheContext));
    llvm::Value* PtrDouble = context.Builder.CreateBitCast(IntPtr, DoubleTy, "ptr_double");

    return TypedValue(context.Builder.CreateCall(GetPF, {PtrDouble}, "p"), TypeKind::Double);
}

// ============================================================================
// SPICE Time-Domain Simulation Codegen
// ============================================================================

TypedValue BuiltinVarExprAST::codegen(CodegenContext& context)
{
    // Check local scope first — user-defined parameter/variable shadows the built-in
    auto localIt = context.NamedValues.find(Name);
    if (localIt != context.NamedValues.end()) {
        llvm::Value* V = localIt->second;
        if (auto* Alloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
            llvm::Type* Ty = Alloca->getAllocatedType();
            return TypedValue(context.Builder.CreateLoad(Ty, Alloca, Name.c_str()), TypeKind::Double);
        }
        FluxType FTy = typeFromLLVM(V->getType());
        return TypedValue(V, FTy);
    }

    // Generate calls for built-in variables: time, dt, temp
    std::string FuncName;
    if (Name == "time")
        FuncName = "flux_get_time";
    else if (Name == "dt")
        FuncName = "flux_get_dt";
    else if (Name == "temp")
        FuncName = "flux_get_temp";
    else {
        std::cerr << "Unknown built-in variable: " << Name << std::endl;
        return TypedValue();
    }

    llvm::Function* GetFunc = context.TheModule->getFunction(FuncName);
    if (!GetFunc) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        GetFunc = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {}, false), llvm::Function::ExternalLinkage,
                                         FuncName, context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(GetFunc, {}, Name.c_str()), TypeKind::Double);
}

TypedValue UpdateFuncAST::codegen(CodegenContext& context)
{
    // Generate update(t, inputs) or update(t, inputs, outputs, state) function
    std::vector<llvm::Type*> ArgTypes;
    std::vector<std::string> ArgNames;

    // Time argument (double)
    ArgTypes.push_back(llvm::Type::getDoubleTy(context.TheContext));
    ArgNames.push_back(TimeVar);

    // Inputs argument (pointer to inputs array)
    llvm::Type* DoublePtrTy = llvm::PointerType::get(context.TheContext, 0);
    ArgTypes.push_back(DoublePtrTy);
    ArgNames.push_back(InputsVar);

    // Optional outputs argument (pointer to outputs array)
    if (!OutputsVar.empty()) {
        ArgTypes.push_back(DoublePtrTy);
        ArgNames.push_back(OutputsVar);
    }

    // Optional state argument (i8* for opaque state pointer)
    if (!StateVar.empty()) {
        ArgTypes.push_back(llvm::PointerType::get(context.TheContext, 0));
        ArgNames.push_back(StateVar);
    }

    // Return type is double (output value)
    llvm::Type* RetTy = llvm::Type::getDoubleTy(context.TheContext);

    llvm::FunctionType* FT = llvm::FunctionType::get(RetTy, ArgTypes, false);
    llvm::Function* TheFunction =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "update", context.TheModule);

    // Create entry block and return block
    llvm::BasicBlock* EntryBB = llvm::BasicBlock::Create(context.TheContext, "entry", TheFunction);
    llvm::BasicBlock* ReturnBB = llvm::BasicBlock::Create(context.TheContext, "return");
    context.Builder.SetInsertPoint(EntryBB);

    // Create return value alloca
    llvm::Value* RetValAlloca = context.Builder.CreateAlloca(RetTy, nullptr, "retval");
    context.Builder.CreateStore(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), RetValAlloca);

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
        if (!RetVal)
            RetVal = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0);
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

TypedValue BSourceExprAST::codegen(CodegenContext& context)
{
    // Generate B-source netlist entry
    // Bxxx n+ n- V=<expression> or I=<expression>
    std::string SourceType = IsCurrent ? "I" : "V";
    std::string NetlistEntry = "B" + Name + " " + PositiveNode + " " + NegativeNode + " " + SourceType + "=";

    // The expression will be evaluated and stored as a string
    // For now, we generate a placeholder and mark it for later netlist generation
    llvm::Function* RegisterBSource = context.TheModule->getFunction("flux_register_bsource");
    if (!RegisterBSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterBSource = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy, CharPtrTy, CharPtrTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_bsource", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterBSource, {context.Builder.CreateGlobalString(Name),
                                                                   context.Builder.CreateGlobalString(PositiveNode),
                                                                   context.Builder.CreateGlobalString(NegativeNode),
                                                                   context.Builder.CreateGlobalString(SourceType)}),
                      TypeKind::Double);
}

TypedValue ESourceExprAST::codegen(CodegenContext& context)
{
    // Generate E-source (VCVS) netlist entry
    // Exxx n+ n- nc+ nc- <gain>
    llvm::Function* RegisterESource = context.TheModule->getFunction("flux_register_esource");
    if (!RegisterESource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
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

    return TypedValue(context.Builder.CreateCall(RegisterESource,
                                                 {context.Builder.CreateGlobalString(Name),
                                                  context.Builder.CreateGlobalString(PositiveNode),
                                                  context.Builder.CreateGlobalString(NegativeNode),
                                                  context.Builder.CreateGlobalString(ControlPosNode),
                                                  context.Builder.CreateGlobalString(ControlNegNode),
                                                  llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), GainVal)}),
                      TypeKind::Double);
}

TypedValue FSourceExprAST::codegen(CodegenContext& context)
{
    // Generate F-source (CCCS) netlist entry
    // Fxxx n+ n- <vsource_name> <gain>
    llvm::Function* RegisterFSource = context.TheModule->getFunction("flux_register_fsource");
    if (!RegisterFSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
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

    return TypedValue(context.Builder.CreateCall(RegisterFSource,
                                                 {context.Builder.CreateGlobalString(Name),
                                                  context.Builder.CreateGlobalString(PositiveNode),
                                                  context.Builder.CreateGlobalString(NegativeNode),
                                                  context.Builder.CreateGlobalString(VoltageSourceName),
                                                  llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), GainVal)}),
                      TypeKind::Double);
}

TypedValue GSourceExprAST::codegen(CodegenContext& context)
{
    // Generate G-source (VCCS) netlist entry
    // Gxxx n+ n- nc+ nc- <transconductance>
    llvm::Function* RegisterGSource = context.TheModule->getFunction("flux_register_gsource");
    if (!RegisterGSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
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

    return TypedValue(
        context.Builder.CreateCall(
            RegisterGSource,
            {context.Builder.CreateGlobalString(Name), context.Builder.CreateGlobalString(PositiveNode),
             context.Builder.CreateGlobalString(NegativeNode), context.Builder.CreateGlobalString(ControlPosNode),
             context.Builder.CreateGlobalString(ControlNegNode),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TranscondVal)}),
        TypeKind::Double);
}

TypedValue HSourceExprAST::codegen(CodegenContext& context)
{
    // Generate H-source (CCVS) netlist entry
    // Hxxx n+ n- <vsource_name> <transresistance>
    llvm::Function* RegisterHSource = context.TheModule->getFunction("flux_register_hsource");
    if (!RegisterHSource) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
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

    return TypedValue(
        context.Builder.CreateCall(
            RegisterHSource,
            {context.Builder.CreateGlobalString(Name), context.Builder.CreateGlobalString(PositiveNode),
             context.Builder.CreateGlobalString(NegativeNode), context.Builder.CreateGlobalString(VoltageSourceName),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), TransresVal)}),
        TypeKind::Double);
}

// ============================================================================
// SPICE Analysis Control Codegen
// ============================================================================

void AnalysisExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue AnalysisExprAST::codegen(CodegenContext& context)
{
    std::string AnalysisName;
    switch (Type) {
    case AnalysisType::TRAN:
        AnalysisName = "tran";
        break;
    case AnalysisType::DC:
        AnalysisName = "dc";
        break;
    case AnalysisType::AC:
        AnalysisName = "ac";
        break;
    case AnalysisType::NOISE:
        AnalysisName = "noise";
        break;
    case AnalysisType::OP:
        AnalysisName = "op";
        break;
    case AnalysisType::TF:
        AnalysisName = "tf";
        break;
    case AnalysisType::SENS:
        AnalysisName = "sens";
        break;
    case AnalysisType::FOURIER:
        AnalysisName = "fourier";
        break;
    }

    llvm::Function* RegisterAnalysis = context.TheModule->getFunction("flux_register_analysis");
    if (!RegisterAnalysis) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterAnalysis =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_analysis", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterAnalysis, {context.Builder.CreateGlobalString(AnalysisName)}),
                      TypeKind::Double);
}

void MeasureExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue MeasureExprAST::codegen(CodegenContext& context)
{
    std::string MeasureName;
    switch (Type) {
    case MeasureType::MAX:
        MeasureName = "MAX";
        break;
    case MeasureType::MIN:
        MeasureName = "MIN";
        break;
    case MeasureType::AVG:
        MeasureName = "AVG";
        break;
    case MeasureType::RMS:
        MeasureName = "RMS";
        break;
    case MeasureType::TRIG:
        MeasureName = "TRIG";
        break;
    case MeasureType::TARG:
        MeasureName = "TARG";
        break;
    case MeasureType::WHEN:
        MeasureName = "WHEN";
        break;
    case MeasureType::FIND:
        MeasureName = "FIND";
        break;
    case MeasureType::DERIV:
        MeasureName = "DERIV";
        break;
    case MeasureType::INTEG:
        MeasureName = "INTEG";
        break;
    }

    llvm::Function* RegisterMeasure = context.TheModule->getFunction("flux_register_measure");
    if (!RegisterMeasure) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterMeasure =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_measure", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterMeasure, {context.Builder.CreateGlobalString(Name),
                                                                   context.Builder.CreateGlobalString(MeasureName)}),
                      TypeKind::Double);
}

TypedValue ProbeExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterProbe = context.TheModule->getFunction("flux_register_probe");
    if (!RegisterProbe) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterProbe =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, CharPtrTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_probe", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterProbe, {context.Builder.CreateGlobalString(VariableName),
                                                                 context.Builder.CreateGlobalString(
                                                                     OutputName.empty() ? VariableName : OutputName)}),
                      TypeKind::Double);
}

TypedValue SaveExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterSave = context.TheModule->getFunction("flux_register_save");
    if (!RegisterSave) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterSave = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy}, false),
                                              llvm::Function::ExternalLinkage, "flux_register_save", context.TheModule);
    }

    return TypedValue(context.Builder.CreateCall(RegisterSave, {context.Builder.CreateGlobalString(VariableName)}),
                      TypeKind::Double);
}

// ============================================================================
// SPICE Subcircuit and Model Codegen
// ============================================================================

void SubcktExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters.push_back({Name, std::move(Value)});
}

void SubcktExprAST::addStatement(std::unique_ptr<ExprAST> Stmt)
{
    Body.push_back(std::move(Stmt));
}

void SubcktExprAST::addFunction(std::unique_ptr<FunctionAST> Func)
{
    Functions.push_back(std::move(Func));
}

TypedValue SubcktExprAST::codegen(CodegenContext& context)
{
    // Build pin list string
    std::string PinsStr;
    for (size_t i = 0; i < Pins.size(); ++i) {
        if (i > 0)
            PinsStr += " ";
        PinsStr += Pins[i];
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    llvm::Function* RegisterSubckt = context.TheModule->getFunction("flux_register_subckt");
    if (!RegisterSubckt) {
        // Updated signature: double flux_register_subckt(double name_ptr_double, double pins_ptr_double)
        RegisterSubckt =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_subckt", context.TheModule);
    }

    // Use bitcast pattern for Name
    llvm::Value* NameStrPtr = context.Builder.CreateGlobalString(Name, "name_str");
    llvm::Value* NameIntPtr = context.Builder.CreatePtrToInt(NameStrPtr, Int64Ty);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameIntPtr, DoubleTy, "name_double");

    // Use bitcast pattern for PinsStr
    llvm::Value* PinsStrPtr = context.Builder.CreateGlobalString(PinsStr, "pins_str");
    llvm::Value* PinsIntPtr = context.Builder.CreatePtrToInt(PinsStrPtr, Int64Ty);
    llvm::Value* PinsDouble = context.Builder.CreateBitCast(PinsIntPtr, DoubleTy, "pins_double");

    // Register the subcircuit
    TypedValue Result =
        TypedValue(context.Builder.CreateCall(RegisterSubckt, {NameDouble, PinsDouble}), TypeKind::Double);

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

void ModelExprAST::addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value)
{
    Parameters[Name] = std::move(Value);
}

TypedValue ModelExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterModel = context.TheModule->getFunction("flux_register_model");
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);

    if (!RegisterModel) {
        RegisterModel =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_model", context.TheModule);
    }

    // Use bitcast pattern for Name
    llvm::Value* NameStrPtr = context.Builder.CreateGlobalString(Name, "name_str");
    llvm::Value* NameIntPtr = context.Builder.CreatePtrToInt(NameStrPtr, Int64Ty);
    llvm::Value* NameDouble = context.Builder.CreateBitCast(NameIntPtr, DoubleTy, "name_double");

    // Use bitcast pattern for ModelType
    llvm::Value* TypeStrPtr = context.Builder.CreateGlobalString(ModelType, "type_str");
    llvm::Value* TypeIntPtr = context.Builder.CreatePtrToInt(TypeStrPtr, Int64Ty);
    llvm::Value* TypeDouble = context.Builder.CreateBitCast(TypeIntPtr, DoubleTy, "type_double");

    return TypedValue(context.Builder.CreateCall(RegisterModel, {NameDouble, TypeDouble}), TypeKind::Double);
}

TypedValue ParamExprAST::codegen(CodegenContext& context)
{
    // Parameters are handled at the simulation setup level
    // This just marks the parameter as declared
    llvm::Function* RegisterParam = context.TheModule->getFunction("flux_register_param");
    if (!RegisterParam) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterParam =
            llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
                                   llvm::Function::ExternalLinkage, "flux_register_param", context.TheModule);
    }

    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }

    return TypedValue(
        context.Builder.CreateCall(RegisterParam, {context.Builder.CreateGlobalString(Name),
                                                   llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Val)}),
        TypeKind::Double);
}

TypedValue ICExprAST::codegen(CodegenContext& context)
{
    llvm::Function* RegisterIC = context.TheModule->getFunction("flux_register_ic");
    if (!RegisterIC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        RegisterIC = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {CharPtrTy, DoubleTy}, false),
                                            llvm::Function::ExternalLinkage, "flux_register_ic", context.TheModule);
    }

    double Val = 0.0;
    if (auto* NumVal = dynamic_cast<NumberExprAST*>(Value.get())) {
        Val = NumVal->getValue();
    }

    return TypedValue(
        context.Builder.CreateCall(RegisterIC, {context.Builder.CreateGlobalString(NodeName),
                                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), Val)}),
        TypeKind::Double);
}

TypedValue WorstCaseExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings for components
    std::string namesStr, nominalsStr, tolerancesStr;
    for (auto it = ComponentNominal.begin(); it != ComponentNominal.end(); ++it) {
        if (it != ComponentNominal.begin()) {
            namesStr += ",";
            nominalsStr += ",";
            tolerancesStr += ",";
        }
        namesStr += it->first;
        nominalsStr += std::to_string(it->second);
        auto tolIt = ComponentTolerance.find(it->first);
        tolerancesStr += (tolIt != ComponentTolerance.end()) ? std::to_string(tolIt->second) : "0.0";
    }

    llvm::Function* RegisterWC = TheModule->getFunction("flux_register_worst_case");
    if (!RegisterWC) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        RegisterWC = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_register_worst_case", TheModule);
    }

    // Convert string pointers to doubles (JIT ABI convention)
    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        RegisterWC, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(nominalsStr),
                     stringToDouble(tolerancesStr)},
        "wc_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue StabilityExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_stability_run");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                    llvm::Function::ExternalLinkage,
                                    "flux_stability_run", TheModule);
    }

    // Convert string pointer to double for JIT ABI
    llvm::Value* Ptr = context.Builder.CreateGlobalString(OutputName, "stab_output");
    llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, llvm::Type::getInt64Ty(Ctx), "str_int");
    llvm::Value* StrDbl = context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");

    llvm::Value* Res = context.Builder.CreateCall(Fn, {StrDbl}, "stab_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue SensitivityExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_sensitivity_run");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy}, false),
                                    llvm::Function::ExternalLinkage,
                                    "flux_sensitivity_run", TheModule);
    }

    llvm::Value* Ptr = context.Builder.CreateGlobalString(OutputName, "sens_output");
    llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, llvm::Type::getInt64Ty(Ctx), "str_int");
    llvm::Value* StrDbl = context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");

    llvm::Value* Res = context.Builder.CreateCall(Fn, {StrDbl}, "sens_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue OptimizationExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings
    std::string namesStr, initsStr, minsStr, maxsStr;
    for (size_t i = 0; i < VarNames.size(); i++) {
        if (i > 0) {
            namesStr += ",";
            initsStr += ",";
            minsStr += ",";
            maxsStr += ",";
        }
        namesStr += VarNames[i];
        initsStr += (i < InitVals.size()) ? std::to_string(InitVals[i]) : "0.0";
        minsStr += (i < MinVals.size()) ? std::to_string(MinVals[i]) : "0.0";
        maxsStr += (i < MaxVals.size()) ? std::to_string(MaxVals[i]) : "0.0";
    }

    llvm::Function* Fn = TheModule->getFunction("flux_optimize_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_optimize_analyze", TheModule);
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(initsStr),
             stringToDouble(minsStr), stringToDouble(maxsStr)},
        "opt_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue BodeExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    llvm::Function* Fn = TheModule->getFunction("flux_bode_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_bode_analyze", TheModule);
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), FreqStart),
             llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), FreqEnd),
             llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), PointsPerDecade)},
        "bode_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue PlotExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;

    // plot(x, y) or plot(y) — first arg is x (optional), last is y
    auto xTV = Args[0]->codegen(context);
    if (!xTV.Val) {
        std::cerr << "[FLUX ERROR] Plot argument expression failed to codegen" << std::endl;
        return TypedValue();
    }
    auto yTV = Args.size() > 1 ? Args[1]->codegen(context) : TypedValue();

    // Verify types
    auto& primaryTV = Args.size() > 1 ? yTV : xTV;
    if (primaryTV.Type.Kind != TypeKind::Matrix) {
        std::cerr << "[FLUX ERROR] Plot requires matrix argument" << std::endl;
        context.TheModule = nullptr;
        return TypedValue();
    }

    // If only one arg, treat as y (x is implicit)
    // If two args, first is x, second is y
    llvm::Value* yPtr;
    llvm::Value* yRows;
    llvm::Value* yCols;
    llvm::Value* xPtr;

    if (Args.size() > 1) {
        // xTV = first arg (x), yTV = second arg (y)
        if (xTV.Type.Kind != TypeKind::Matrix || yTV.Type.Kind != TypeKind::Matrix) {
            std::cerr << "[FLUX ERROR] Plot requires matrix arguments for both x and y" << std::endl;
            context.TheModule = nullptr;
            return TypedValue();
        }
        yPtr = context.Builder.CreateExtractValue(yTV.Val, 0, "y_ptr");
        yRows = context.Builder.CreateExtractValue(yTV.Val, 1, "y_rows");
        yCols = context.Builder.CreateExtractValue(yTV.Val, 2, "y_cols");
        xPtr = context.Builder.CreateExtractValue(xTV.Val, 0, "x_ptr");
    } else {
        yPtr = context.Builder.CreateExtractValue(xTV.Val, 0, "y_ptr");
        yRows = context.Builder.CreateExtractValue(xTV.Val, 1, "y_rows");
        yCols = context.Builder.CreateExtractValue(xTV.Val, 2, "y_cols");
        xPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(Ctx, 0));
    }

    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Function* Fn = context.TheModule->getFunction("flux_plot_data");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        llvm::Type* PtrTy = llvm::PointerType::get(Ctx, 0);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {PtrTy, Int32Ty, Int32Ty, PtrTy, Int32Ty, DoubleTy}, false),
            llvm::Function::ExternalLinkage, "flux_plot_data", context.TheModule);
    }

    llvm::Value* yRowsI32 = context.Builder.CreateTrunc(yRows, llvm::Type::getInt32Ty(Ctx), "y_rows_i32");
    llvm::Value* yColsI32 = context.Builder.CreateTrunc(yCols, llvm::Type::getInt32Ty(Ctx), "y_cols_i32");

    (void)context.Builder.CreateCall(
        Fn, {yPtr, yRowsI32, yColsI32, xPtr,
             xTV.Val ? llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 1)
                     : llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), 0),
             stringToDouble("")},
        "plot_res");
    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 1.0), TypeKind::Double);
}

TypedValue FFTExprAST::codegen(CodegenContext& context)
{
    llvm::Function* Fn = context.TheModule->getFunction("flux_fft_analyze");
    if (!Fn) {
        llvm::Type* IntTy = llvm::Type::getInt32Ty(context.TheContext);
        llvm::Type* CharPtrTy = llvm::PointerType::get(context.TheContext, 0);
        Fn = llvm::Function::Create(llvm::FunctionType::get(IntTy, {CharPtrTy}, false), llvm::Function::ExternalLinkage,
                                    "flux_fft_analyze", context.TheModule);
    }
    return TypedValue(context.Builder.CreateCall(Fn, {context.Builder.CreateGlobalString(SignalName)}), TypeKind::Int);
}

// ============================================================================
// Statement-based Control Flow Codegen (void-returning, no PHI nodes)
// ============================================================================

TypedValue IfStmtAST::codegen(CodegenContext& context)
{
    llvm::Function* TheFunction = context.Builder.GetInsertBlock()->getParent();
    llvm::LLVMContext& Ctx = context.TheContext;

    // Generate condition
    TypedValue CondTV = Cond->codegen(context);
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] If-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "ifcond");
    }

    // Create basic blocks
    llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(Ctx, "then", TheFunction);
    llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(Ctx, "else", TheFunction);
    llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Ctx, "ifcont");

    context.Builder.CreateCondBr(IsTrue, ThenBB, ElseBB);

    // Generate then block
    context.Builder.SetInsertPoint(ThenBB);
    TypedValue ThenTV(nullptr, TypeKind::Void);
    for (auto& Stmt : ThenBody) {
        if (context.Builder.GetInsertBlock()->getTerminator())
            break;
        ThenTV = Stmt->codegen(context);
    }
    bool thenTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!thenTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ThenBB = context.Builder.GetInsertBlock();

    // Generate else block
    context.Builder.SetInsertPoint(ElseBB);
    TypedValue ElseTV(nullptr, TypeKind::Void);
    for (auto& Stmt : ElseBody) {
        if (context.Builder.GetInsertBlock()->getTerminator())
            break;
        ElseTV = Stmt->codegen(context);
    }
    bool elseTerminated = context.Builder.GetInsertBlock()->getTerminator() != nullptr;
    if (!elseTerminated) {
        context.Builder.CreateBr(MergeBB);
    }
    ElseBB = context.Builder.GetInsertBlock();

    // Determine if we need to continue after the if
    if (thenTerminated && elseTerminated) {
        delete MergeBB;
        return ThenTV.Val ? ThenTV : TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
    }

    // Continue at merge
    TheFunction->insert(TheFunction->end(), MergeBB);
    context.Builder.SetInsertPoint(MergeBB);

    // PHI node to merge values from reachable paths
    if (ThenTV.Type.Kind == TypeKind::Void && ElseTV.Type.Kind == TypeKind::Void) {
        return ThenTV;
    }

    TypedValue ResultTV = ThenTV.Val ? ThenTV : ElseTV;
    llvm::Type* PhiTy = ResultTV.Val ? ResultTV.Val->getType() : llvm::Type::getDoubleTy(Ctx);
    llvm::PHINode* PN = context.Builder.CreatePHI(PhiTy, 2, "ifphi");

    if (!thenTerminated) {
        llvm::Value* TV = ThenTV.Val;
        if (!TV)
            TV = llvm::Constant::getNullValue(PhiTy);
        else if (TV->getType() != PhiTy)
            TV = context.Builder.CreateSIToFP(TV, PhiTy, "cast_then");
        PN->addIncoming(TV, ThenBB);
    }
    if (!elseTerminated) {
        llvm::Value* EV = ElseTV.Val;
        if (!EV)
            EV = llvm::Constant::getNullValue(PhiTy);
        else if (EV->getType() != PhiTy)
            EV = context.Builder.CreateSIToFP(EV, PhiTy, "cast_else");
        PN->addIncoming(EV, ElseBB);
    }

    return TypedValue(PN, ResultTV.Type);
}

TypedValue ForStmtAST::codegen(CodegenContext& context)
{
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
                    llvm::AllocaInst* Alloca = context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext),
                                                                            nullptr, VarName.c_str());
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
                llvm::AllocaInst* Alloca =
                    context.Builder.CreateAlloca(llvm::Type::getDoubleTy(context.TheContext), nullptr, VarName.c_str());
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
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] For-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "forcond");
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

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue WhileStmtAST::codegen(CodegenContext& context)
{
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
    if (!CondTV.Val) {
        std::cerr << "[FLUX ERROR] While-statement condition failed to codegen" << std::endl;
        return TypedValue();
    }

    // If the condition is already a boolean (i1), use it directly
    llvm::Value* IsTrue;
    if (CondTV.Val->getType()->isIntegerTy(1)) {
        IsTrue = CondTV.Val;
    } else {
        // Otherwise compare to non-zero
        IsTrue = context.Builder.CreateFCmpONE(CondTV.Val, llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), "whilecond");
    }
    context.Builder.CreateCondBr(IsTrue, BodyBB, AfterBB);

    // Body
    context.CurrentLoopEnd = AfterBB;
    context.CurrentLoopCont = BodyBB; // continue jumps to condition check
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

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue TrainExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Inputs->codegen(context);
    TypedValue OutTV = Outputs->codegen(context);

    if (!ModelTV.Val || !InTV.Val || !OutTV.Val) {
        std::cerr << "[FLUX ERROR] Train expression failed to codegen model/inputs/outputs" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inDim, int outDim,
    // int epochs)
    llvm::Function* TrainF = TheModule->getFunction("flux_nn_train");
    if (!TrainF) {
        llvm::Type* Params[] = {VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty, Int32Ty, Int32Ty};
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

    context.Builder.CreateCall(
        TrainF, {ModelPtr, InPtr, OutPtr, InRows, InCols, OutCols, llvm::ConstantInt::get(Int32Ty, Epochs)});

    return ModelTV;
}

TypedValue PredictExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ModelTV = Model->codegen(context);
    TypedValue InTV = Input->codegen(context);

    if (!ModelTV.Val || !InTV.Val) {
        std::cerr << "[FLUX ERROR] Predict expression failed to codegen model/input" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_nn_predict(void* nn, const double* input, double* output, int inDim, int outDim)
    // We'll return a new Vector

    // For now, let's assume output is a vector of size 1 for simplicity
    llvm::Function* PredictF = TheModule->getFunction("flux_nn_predict");
    if (!PredictF) {
        llvm::Type* Params[] = {VoidPtrTy, VoidPtrTy, VoidPtrTy, Int32Ty, Int32Ty};
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

TypedValue GoalExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue ExprTV = Expression->codegen(context);
    TypedValue TargetTV = Target->codegen(context);

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    llvm::Function* Fn = TheModule->getFunction("flux_register_goal");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_register_goal", TheModule);
    }

    context.Builder.CreateCall(Fn, {ExprTV.Val, TargetTV.Val});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 0.0), TypeKind::Double);
}

TypedValue ThermalBlockAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue PowerTV = Power->codegen(context);
    TypedValue ResTV = Resistance->codegen(context);
    TypedValue CapTV = Capacitance->codegen(context);

    if (!PowerTV.Val || !ResTV.Val || !CapTV.Val) {
        std::cerr << "[FLUX ERROR] Thermal block expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // Call flux_thermal_step(double power, double resistance, double capacitance)
    // Returns current temperature
    llvm::Function* Fn = TheModule->getFunction("flux_thermal_step");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_thermal_step", TheModule);
    }

    llvm::Value* Temp = context.Builder.CreateCall(Fn, {PowerTV.Val, ResTV.Val, CapTV.Val}, "temp");

    return TypedValue(Temp, TypeKind::Double);
}

TypedValue MonteCarloExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    // Build comma-separated strings for components
    std::string namesStr, nominalsStr, tolerancesStr;
    for (size_t i = 0; i < ComponentNames.size(); i++) {
        if (i > 0) {
            namesStr += ",";
            nominalsStr += ",";
            tolerancesStr += ",";
        }
        namesStr += ComponentNames[i];
        nominalsStr += (i < Nominals.size()) ? std::to_string(Nominals[i]) : "0.0";
        tolerancesStr += (i < Tolerances.size()) ? std::to_string(Tolerances[i]) : "0.01";
    }

    llvm::Function* Fn = TheModule->getFunction("flux_monte_carlo_analyze");
    if (!Fn) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
        llvm::Type* Int32Ty = llvm::Type::getInt32Ty(Ctx);
        Fn = llvm::Function::Create(
            llvm::FunctionType::get(DoubleTy, {DoubleTy, DoubleTy, DoubleTy, DoubleTy, Int32Ty}, false),
            llvm::Function::ExternalLinkage, "flux_monte_carlo_analyze", TheModule);
    }

    // Convert string pointers to doubles (JIT ABI convention)
    auto stringToDouble = [&](const std::string& s) -> llvm::Value* {
        llvm::Value* Ptr = context.Builder.CreateGlobalString(s);
        llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
        llvm::Value* IntVal = context.Builder.CreatePtrToInt(Ptr, Int64Ty, "str_int");
        return context.Builder.CreateBitCast(IntVal, llvm::Type::getDoubleTy(Ctx), "str_dbl");
    };

    llvm::Value* Res = context.Builder.CreateCall(
        Fn, {stringToDouble(OutputName), stringToDouble(namesStr), stringToDouble(nominalsStr),
             stringToDouble(tolerancesStr), llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), Iterations)},
        "mc_res");
    return TypedValue(Res, TypeKind::Double);
}

TypedValue VirtualProbeExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue SigTV = Signal->codegen(context);
    if (!SigTV.Val) {
        std::cerr << "[FLUX ERROR] Virtual probe signal expression failed to codegen" << std::endl;
        return TypedValue();
    }

    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // Call flux_virtual_probe(double value, const char* name)
    llvm::Function* Fn = TheModule->getFunction("flux_virtual_probe");
    if (!Fn) {
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {DoubleTy, VoidPtrTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_virtual_probe", TheModule);
    }

    context.Builder.CreateCall(Fn, {SigTV.Val, context.Builder.CreateGlobalString(ProbeName)});

    return TypedValue(SigTV.Val, TypeKind::Double);
}

TypedValue HotSwapExprAST::codegen(CodegenContext& context)
{
    emitLocation(this, context);
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Module* TheModule = context.TheModule;

    TypedValue NameTV = SubcktName->codegen(context);
    TypedValue ModelTV = Model->codegen(context);

    if (!NameTV.Val || !ModelTV.Val) {
        std::cerr << "[FLUX ERROR] Hot-swap subcircuit or model expression failed to codegen" << std::endl;
        return TypedValue();
    }

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
        Fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {VoidPtrTy, DoubleTy}, false),
                                    llvm::Function::ExternalLinkage, "flux_hot_swap", TheModule);
    }

    context.Builder.CreateCall(Fn, {NamePtr, ModelTV.Val});

    return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(Ctx), 1.0), TypeKind::Double);
}

TypedValue SpawnExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);
    llvm::Type* Int64Ty = llvm::Type::getInt64Ty(Ctx);
    llvm::Type* VoidPtrTy = llvm::PointerType::get(Ctx, 0);

    // 1. Evaluate all arguments
    size_t nargs = Args.size();
    std::vector<llvm::Value*> argVals;
    argVals.reserve(nargs);
    for (auto& arg : Args) {
        TypedValue tv = arg->codegen(context);
        if (!tv.Val) return TypedValue();
        argVals.push_back(tv.Val);
    }

    // 2. Get the LLVM function pointer for the callee
    llvm::Function* calleeFn = context.TheModule->getFunction(Callee);
    if (!calleeFn) {
        std::vector<llvm::Type*> paramTys(nargs, DoubleTy);
        auto* ft = llvm::FunctionType::get(DoubleTy, paramTys, false);
        calleeFn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, Callee, context.TheModule);
    }

    // 3. Create a double[] array on the stack to hold argument values
    llvm::AllocaInst* argsArray = context.Builder.CreateAlloca(
        llvm::ArrayType::get(DoubleTy, nargs), nullptr, "spawn_args");
    for (size_t i = 0; i < nargs; i++) {
        llvm::Value* idx0 = llvm::ConstantInt::get(Int64Ty, 0);
        llvm::Value* idx1 = llvm::ConstantInt::get(Int64Ty, static_cast<uint64_t>(i));
        llvm::Value* gep = context.Builder.CreateGEP(
            argsArray->getAllocatedType(), argsArray, {idx0, idx1}, "spawn_arg");
        context.Builder.CreateStore(argVals[i], gep);
    }

    // 4. Cast function pointer to void*
    llvm::Value* fnPtr = context.Builder.CreateBitCast(calleeFn, VoidPtrTy, "spawn_fn_ptr");

    // 5. Cast args array to void*
    llvm::Value* argsPtr = context.Builder.CreateBitCast(argsArray, VoidPtrTy, "spawn_args_ptr");

    // 6. Declare / get flux_spawn(void*, void*, i64) -> double
    llvm::Function* spawnFn = context.TheModule->getFunction("flux_spawn");
    if (!spawnFn) {
        auto* spawnFT = llvm::FunctionType::get(DoubleTy, {VoidPtrTy, VoidPtrTy, Int64Ty}, false);
        spawnFn = llvm::Function::Create(spawnFT, llvm::Function::ExternalLinkage, "flux_spawn", context.TheModule);
    }

    llvm::Value* nargsVal = llvm::ConstantInt::get(Int64Ty, static_cast<uint64_t>(nargs));
    llvm::Value* handle = context.Builder.CreateCall(spawnFn, {fnPtr, argsPtr, nargsVal}, "spawn_handle");

    return TypedValue(handle, FluxType(TypeKind::Double));
}

TypedValue JoinExprAST::codegen(CodegenContext& context)
{
    llvm::LLVMContext& Ctx = context.TheContext;
    llvm::Type* DoubleTy = llvm::Type::getDoubleTy(Ctx);

    // 1. Evaluate the handle expression
    TypedValue handleTV = Handle->codegen(context);
    if (!handleTV.Val) return TypedValue();

    // 2. Declare / get flux_join(double) -> double
    llvm::Function* joinFn = context.TheModule->getFunction("flux_join");
    if (!joinFn) {
        auto* joinFT = llvm::FunctionType::get(DoubleTy, {DoubleTy}, false);
        joinFn = llvm::Function::Create(joinFT, llvm::Function::ExternalLinkage, "flux_join", context.TheModule);
    }

    llvm::Value* result = context.Builder.CreateCall(joinFn, {handleTV.Val}, "join_result");
    return TypedValue(result, FluxType(TypeKind::Double));
}

} // namespace Flux
