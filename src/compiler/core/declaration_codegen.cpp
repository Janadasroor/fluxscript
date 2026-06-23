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
    llvm::Type* DoublePtrTy = llvm::PointerType::get(context.TheContext, 0);

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
                        llvm::PointerType::get(context.TheContext, 0), "self_ptr");
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
    // Debug: std::cerr << "[CODEGEN] " << Proto->getName() << " isAsync=" << Proto->isAsync() << " isGenerator=" << Body->containsYield() << std::endl;
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
            // Debug: std::cerr << "[CODEGEN GEP] gen_end for " << Proto->getName() << std::endl;
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
            // Debug: std::cerr << "[CODEGEN GEP] gen_dispatch for " << Proto->getName() << std::endl;
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
    (void)Proto.release();
    Proto = std::move(savedProto);
    return result;
}


} // namespace Flux
