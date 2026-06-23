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

#ifndef FLUX_COMPILER_CODEGEN_HELPERS_H
#define FLUX_COMPILER_CODEGEN_HELPERS_H

#include "flux/compiler/ast.h"
#include <iostream>

namespace Flux {

inline bool isComplexType(TypedValue V)
{
    return V.Type.Kind == TypeKind::Complex;
}

inline FluxType typeFromLLVM(llvm::Type* Ty)
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

inline TypedValue promoteToComplex(TypedValue V, CodegenContext& context)
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

    llvm::Value* ZeroVec = llvm::ConstantAggregateZero::get(ComplexTy);
    llvm::Value* ComplexVal = context.Builder.CreateInsertElement(ZeroVec, Val, (uint64_t)0, "to_complex");
    return TypedValue(ComplexVal, TypeKind::Complex);
}

inline void emitLocation(ExprAST* ast, CodegenContext& context)
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

inline void resolveUserStructType(FluxType& type, CodegenContext& context)
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

inline void resolveUserEnumType(FluxType& type, CodegenContext& context)
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

inline bool shouldPassByPointer(const FluxType& type, CodegenContext& context)
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

inline bool isCopyType(const FluxType& type)
{
    if (type.Kind == TypeKind::UserStruct || type.Kind == TypeKind::UserEnum)
        return type.isCopy;
    return true;
}

inline bool shouldReturnBySRet(const FluxType& type, CodegenContext& context)
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
        return (size != 1 && size != 2 && size != 4 && size != 8);
#else
        return size > 16;
#endif
    }

    return false;
}

inline bool checkTraitBounds(
    const std::map<std::string, std::vector<std::string>>& genericParamBounds,
    const std::map<std::string, FluxType>& typeMap,
    CodegenContext& context)
{
    for (const auto& [paramName, boundTraits] : genericParamBounds) {
        auto typeIt = typeMap.find(paramName);
        if (typeIt == typeMap.end())
            continue;
        const FluxType& concreteType = typeIt->second;

        std::string typeName;
        if (concreteType.isStruct()) {
            int id = concreteType.StructTypeId;
            if (id >= 0 && id < static_cast<int>(context.StructTypes.size()))
                typeName = context.StructTypes[id].Name;
        } else if (concreteType.isEnum()) {
            int id = concreteType.EnumTypeId;
            if (id >= 0 && id < static_cast<int>(context.EnumTypes.size()))
                typeName = context.EnumTypes[id].Name;
        } else if (concreteType.Kind == TypeKind::Double) typeName = "Double";
        else if (concreteType.Kind == TypeKind::Int) typeName = "Int";
        else if (concreteType.Kind == TypeKind::Float) typeName = "Float";
        else if (concreteType.Kind == TypeKind::Bool) typeName = "Bool";
        else if (concreteType.Kind == TypeKind::String) typeName = "String";
        else if (concreteType.Kind == TypeKind::Matrix) typeName = "Matrix";
        else if (concreteType.Kind == TypeKind::Complex) typeName = "Complex";
        else if (concreteType.Kind == TypeKind::Vector) typeName = "Vector";

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

// Forward declarations
std::string buildGenericTypeSuffix(const std::vector<FluxType>& genericArgs, CodegenContext& context);
std::string specializeGenericEnum(const std::string& enumName, const std::vector<FluxType>& genericArgs, CodegenContext& context);

inline void ensureGenericTypeArgSpecialized(FluxType& typeArg, CodegenContext& context)
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

} // namespace Flux

#endif // FLUX_COMPILER_CODEGEN_HELPERS_H
