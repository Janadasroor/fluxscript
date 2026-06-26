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
                                        std::cerr
                                            << "[FLUX ERROR] Specialized enum tagged union has no LLVM struct type"
                                            << std::endl;
                                        return TypedValue();
                                    }
                                    llvm::Value* structPtr = context.Builder.CreateAlloca(
                                        taggedTy, nullptr, mangledEnumName + "." + variantName);
                                    llvm::Value* tagPtr =
                                        context.Builder.CreateStructGEP(taggedTy, structPtr, 0, "tag");
                                    context.Builder.CreateStore(
                                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), disc),
                                        tagPtr);
                                    llvm::Value* payloadVal = nullptr;
                                    if (payloadType.Kind == TypeKind::UserStruct) {
                                        resolveUserStructType(payloadType, context);
                                        int anonStructId = payloadType.StructTypeId;
                                        if (anonStructId < 0) {
                                            std::cerr
                                                << "[FLUX ERROR] Anonymous struct type ID is invalid for enum payload"
                                                << std::endl;
                                            return TypedValue();
                                        }
                                        auto& anonInfo = context.StructTypes[anonStructId];
                                        llvm::Type* anonTy = anonInfo.LLVMType;
                                        if (!anonTy) {
                                            std::cerr
                                                << "[FLUX ERROR] Anonymous struct has no LLVM type for enum payload"
                                                << std::endl;
                                            return TypedValue();
                                        }
                                        if (Args.size() == 1) {
                                            TypedValue argTV = Args[0]->codegen(context);
                                            if (argTV.Val && argTV.Type.Kind == TypeKind::UserStruct) {
                                                payloadVal = argTV.Val;
                                            }
                                        }
                                        if (!payloadVal) {
                                            llvm::AllocaInst* anonAlloca =
                                                context.Builder.CreateAlloca(anonTy, nullptr, "payload_struct");
                                            for (size_t ai = 0; ai < Args.size() && ai < anonInfo.Fields.size(); ++ai) {
                                                TypedValue fv = Args[ai]->codegen(context);
                                                if (!fv.Val) {
                                                    std::cerr << "[FLUX ERROR] Anonymous struct field expression "
                                                                 "failed to codegen"
                                                              << std::endl;
                                                    return TypedValue();
                                                }
                                                llvm::Value* fptr = context.Builder.CreateStructGEP(
                                                    anonTy, anonAlloca, ai, anonInfo.Fields[ai].first);
                                                context.Builder.CreateStore(fv.Val, fptr);
                                            }
                                            payloadVal =
                                                context.Builder.CreateLoad(anonTy, anonAlloca, "payload_loaded");
                                        }
                                    } else {
                                        TypedValue argTV = Args[0]->codegen(context);
                                        if (!argTV.Val) {
                                            std::cerr << "[FLUX ERROR] Direct enum payload expression failed to codegen"
                                                      << std::endl;
                                            return TypedValue();
                                        }
                                        payloadVal = argTV.Val;
                                    }
                                    resolveUserStructType(payloadType, context);
                                    resolveUserEnumType(payloadType, context);
                                    llvm::Type* concretePayloadTy = payloadType.getLLVMType(context.TheContext);
                                    if (payloadVal->getType()->isStructTy() && !concretePayloadTy->isStructTy()) {
                                        if (llvm::StructType* st =
                                                llvm::dyn_cast<llvm::StructType>(payloadVal->getType())) {
                                            if (st->getNumElements() == 1)
                                                payloadVal =
                                                    context.Builder.CreateExtractValue(payloadVal, {0}, "payload_val");
                                        }
                                    }
                                    if (payloadVal->getType() != concretePayloadTy) {
                                        if (concretePayloadTy->isFloatingPointTy() &&
                                            payloadVal->getType()->isIntegerTy()) {
                                            payloadVal = context.Builder.CreateSIToFP(payloadVal, concretePayloadTy,
                                                                                      "payload_cast");
                                        } else if (concretePayloadTy->isIntegerTy() &&
                                                   payloadVal->getType()->isFloatingPointTy()) {
                                            payloadVal = context.Builder.CreateFPToSI(payloadVal, concretePayloadTy,
                                                                                      "payload_cast");
                                        } else if (concretePayloadTy->isPointerTy() &&
                                                   payloadVal->getType()->isPointerTy()) {
                                            payloadVal = context.Builder.CreatePointerCast(
                                                payloadVal, concretePayloadTy, "payload_cast");
                                        } else {
                                            payloadVal = context.Builder.CreateBitCast(payloadVal, concretePayloadTy,
                                                                                       "payload_cast");
                                        }
                                    }
                                    bool isBoxed = disc < static_cast<int>(enumInfo.VariantIsBoxed.size())
                                                       ? enumInfo.VariantIsBoxed[disc]
                                                       : false;
                                    llvm::Value* payloadPtr =
                                        context.Builder.CreateStructGEP(taggedTy, structPtr, 1, "payload");
                                    if (isBoxed) {
                                        llvm::Function* mallocFn = context.TheModule->getFunction("flux_malloc");
                                        if (!mallocFn) {
                                            llvm::Type* sizeTy = llvm::Type::getInt64Ty(context.TheContext);
                                            mallocFn = llvm::Function::Create(
                                                llvm::FunctionType::get(llvm::PointerType::get(context.TheContext, 0),
                                                                        {sizeTy}, false),
                                                llvm::Function::ExternalLinkage, "flux_malloc", context.TheModule);
                                        }
                                        uint64_t sizeInBytes =
                                            context.TheModule->getDataLayout().getTypeStoreSize(concretePayloadTy);
                                        llvm::Value* sizeVal = llvm::ConstantInt::get(
                                            llvm::Type::getInt64Ty(context.TheContext), sizeInBytes);
                                        llvm::Value* heapAlloc =
                                            context.Builder.CreateCall(mallocFn, {sizeVal}, "heap_alloc");
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            heapAlloc, llvm::PointerType::get(context.TheContext, 0),
                                            "concrete_heap_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);
                                        llvm::Value* unionPayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0),
                                            "union_payload_ptr");
                                        context.Builder.CreateStore(heapAlloc, unionPayloadPtr);
                                    } else {
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0),
                                            "concrete_payload_ptr");
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
                                        std::cerr << "[FLUX ERROR] Enum variant tagged union has no LLVM struct type"
                                                  << std::endl;
                                        return TypedValue();
                                    }
                                    llvm::Value* structPtr =
                                        context.Builder.CreateAlloca(taggedTy, nullptr, enumName + "." + variantName);
                                    llvm::Value* tagPtr =
                                        context.Builder.CreateStructGEP(taggedTy, structPtr, 0, "tag");
                                    context.Builder.CreateStore(
                                        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), disc),
                                        tagPtr);
                                    llvm::Value* payloadVal = nullptr;
                                    if (payloadType.Kind == TypeKind::UserStruct) {
                                        resolveUserStructType(payloadType, context);
                                        int anonStructId = payloadType.StructTypeId;
                                        if (anonStructId < 0) {
                                            std::cerr
                                                << "[FLUX ERROR] Anonymous struct type ID is invalid for enum payload"
                                                << std::endl;
                                            return TypedValue();
                                        }
                                        auto& anonInfo = context.StructTypes[anonStructId];
                                        llvm::Type* anonTy = anonInfo.LLVMType;
                                        if (!anonTy) {
                                            std::cerr
                                                << "[FLUX ERROR] Anonymous struct has no LLVM type for enum payload"
                                                << std::endl;
                                            return TypedValue();
                                        }

                                        bool isDirectStruct = false;
                                        if (Args.size() == 1) {
                                            TypedValue argTV = Args[0]->codegen(context);
                                            if (argTV.Val && argTV.Type.Kind == TypeKind::UserStruct &&
                                                argTV.Type.StructTypeId == anonStructId) {
                                                payloadVal = argTV.Val;
                                                isDirectStruct = true;
                                            }
                                        }

                                        if (!isDirectStruct) {
                                            llvm::AllocaInst* anonAlloca =
                                                context.Builder.CreateAlloca(anonTy, nullptr, "payload_struct");
                                            for (size_t ai = 0; ai < Args.size() && ai < anonInfo.Fields.size(); ++ai) {
                                                TypedValue fv = Args[ai]->codegen(context);
                                                if (!fv.Val) {
                                                    std::cerr << "[FLUX ERROR] Anonymous struct field expression "
                                                                 "failed to codegen"
                                                              << std::endl;
                                                    return TypedValue();
                                                }
                                                llvm::Value* fptr = context.Builder.CreateStructGEP(
                                                    anonTy, anonAlloca, ai, anonInfo.Fields[ai].first);
                                                context.Builder.CreateStore(fv.Val, fptr);
                                            }
                                            payloadVal =
                                                context.Builder.CreateLoad(anonTy, anonAlloca, "payload_loaded");
                                        }
                                    } else {
                                        TypedValue argTV = Args[0]->codegen(context);
                                        if (!argTV.Val) {
                                            std::cerr << "[FLUX ERROR] Direct enum payload expression failed to codegen"
                                                      << std::endl;
                                            return TypedValue();
                                        }
                                        payloadVal = argTV.Val;
                                    }
                                    resolveUserStructType(payloadType, context);
                                    resolveUserEnumType(payloadType, context);
                                    llvm::Type* concretePayloadTy = payloadType.getLLVMType(context.TheContext);

                                    if (payloadVal->getType() != concretePayloadTy) {
                                        if (concretePayloadTy->isFloatingPointTy() &&
                                            payloadVal->getType()->isIntegerTy()) {
                                            payloadVal = context.Builder.CreateSIToFP(payloadVal, concretePayloadTy,
                                                                                      "payload_cast");
                                        } else if (concretePayloadTy->isIntegerTy() &&
                                                   payloadVal->getType()->isFloatingPointTy()) {
                                            payloadVal = context.Builder.CreateFPToSI(payloadVal, concretePayloadTy,
                                                                                      "payload_cast");
                                        } else if (concretePayloadTy->isPointerTy() &&
                                                   payloadVal->getType()->isPointerTy()) {
                                            payloadVal = context.Builder.CreatePointerCast(
                                                payloadVal, concretePayloadTy, "payload_cast");
                                        } else {
                                            payloadVal = context.Builder.CreateBitCast(payloadVal, concretePayloadTy,
                                                                                       "payload_cast");
                                        }
                                    }

                                    bool isBoxed = disc < static_cast<int>(enumInfo.VariantIsBoxed.size())
                                                       ? enumInfo.VariantIsBoxed[disc]
                                                       : false;
                                    llvm::Value* payloadPtr =
                                        context.Builder.CreateStructGEP(taggedTy, structPtr, 1, "payload");

                                    if (isBoxed) {
                                        llvm::Function* mallocFn = context.TheModule->getFunction("flux_malloc");
                                        if (!mallocFn) {
                                            llvm::Type* sizeTy = llvm::Type::getInt64Ty(context.TheContext);
                                            mallocFn = llvm::Function::Create(
                                                llvm::FunctionType::get(llvm::PointerType::get(context.TheContext, 0),
                                                                        {sizeTy}, false),
                                                llvm::Function::ExternalLinkage, "flux_malloc", context.TheModule);
                                        }

                                        uint64_t sizeInBytes =
                                            context.TheModule->getDataLayout().getTypeStoreSize(concretePayloadTy);
                                        llvm::Value* sizeVal = llvm::ConstantInt::get(
                                            llvm::Type::getInt64Ty(context.TheContext), sizeInBytes);

                                        llvm::Value* heapAlloc =
                                            context.Builder.CreateCall(mallocFn, {sizeVal}, "heap_alloc");

                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            heapAlloc, llvm::PointerType::get(context.TheContext, 0),
                                            "concrete_heap_ptr");
                                        context.Builder.CreateStore(payloadVal, concretePayloadPtr);

                                        llvm::Value* unionPayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0),
                                            "union_payload_ptr");
                                        context.Builder.CreateStore(heapAlloc, unionPayloadPtr);
                                    } else {
                                        llvm::Value* concretePayloadPtr = context.Builder.CreatePointerCast(
                                            payloadPtr, llvm::PointerType::get(context.TheContext, 0),
                                            "concrete_payload_ptr");
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
                            selfAllocaPtr = context.Builder.CreateLoad(allocaInst->getAllocatedType(), allocaInst,
                                                                       "self_loaded_ptr");
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
                    FluxType retType = context.FuncReturnTypes.count(fnName) ? context.FuncReturnTypes[fnName]
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
                            sretLLVMTy = llvm::cast<llvm::StructType>(
                                FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
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
                        llvm::Value* result =
                            context.Builder.CreateLoad(retType.getLLVMType(context.TheContext), sretPtr, "sret_val");
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
                    std::cerr << "Method '" << methodName << "' not found in trait '" << traitInfo.Name << "'"
                              << std::endl;
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
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context.TheContext), 1 + methodIdx), "fn_ptr_gep");
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
                llvm::Value* typedFnPtr =
                    context.Builder.CreatePointerCast(fnPtr, llvm::PointerType::get(context.TheContext, 0), "typed_fn");

                // Build call arguments
                std::vector<llvm::Value*> callArgs;
                bool isSret = fnRetTy->isVoidTy();
                llvm::Value* sretAlloca = nullptr;
                if (isSret) {
                    sretAlloca = context.Builder.CreateAlloca(sig.ReturnType.getLLVMType(context.TheContext), nullptr,
                                                              "sret_tmp");
                    callArgs.push_back(sretAlloca);
                    callArgs.push_back(dataPtr);
                } else {
                    callArgs.push_back(dataPtr);
                }

                for (auto& Arg : Args) {
                    TypedValue argVal = Arg->codegen(context);
                    if (!argVal.Val)
                        return TypedValue();
                    callArgs.push_back(argVal.Val);
                }

                llvm::CallInst* call = context.Builder.CreateCall(fnFT, typedFnPtr, callArgs, methodName);
                if (isSret) {
                    llvm::Value* result = context.Builder.CreateLoad(sig.ReturnType.getLLVMType(context.TheContext),
                                                                     sretAlloca, "sret_val");
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
                std::cerr << "Method '" << methodName << "' on type '" << typeName << "' has no codegen function"
                          << std::endl;
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
                    sretLLVMTy =
                        llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
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
                        llvm::Value* tempAlloca =
                            context.Builder.CreateAlloca(selfVal->getType(), nullptr, "self_temp");
                        context.Builder.CreateStore(selfVal, tempAlloca);
                        selfVal = tempAlloca;
                    }
                } else {
                    if ((objVal.Type.Kind == TypeKind::UserStruct || objVal.Type.Kind == TypeKind::UserEnum) &&
                        selfVal->getType()->isPointerTy()) {
                        llvm::Type* selfLLVMTy = objVal.Type.getLLVMType(context.TheContext);
                        selfVal = context.Builder.CreateLoad(selfLLVMTy, selfVal, "self_loaded");
                    }
                }

                unsigned firstArgIdx = isSretCall ? 1 : 0;
                if (calleeFn->arg_size() > firstArgIdx &&
                    selfVal->getType() != calleeFn->getArg(firstArgIdx)->getType()) {
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
                            llvm::Value* tempAlloca =
                                context.Builder.CreateAlloca(argVal.Val->getType(), nullptr, "arg_temp");
                            context.Builder.CreateStore(argVal.Val, tempAlloca);
                            argVal.Val = tempAlloca;
                        }
                    } else {
                        if ((argVal.Type.Kind == TypeKind::UserStruct || argVal.Type.Kind == TypeKind::UserEnum) &&
                            argVal.Val->getType()->isPointerTy()) {
                            llvm::Type* structTy = argVal.Type.getLLVMType(context.TheContext);
                            argVal.Val = context.Builder.CreateLoad(structTy, argVal.Val, "arg_loaded");
                        }
                    }

                    // Dyn trait argument upcast for method calls
                    if (argVal.Val->getType() != expectedTy && expectedTy->isStructTy() &&
                        argVal.Type.Kind == TypeKind::UserStruct) {
                        auto* ST = llvm::cast<llvm::StructType>(expectedTy);
                        if (ST->getNumElements() == 2 && ST->getElementType(0)->isPointerTy() &&
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
                                            if (vtableIdx >= 0 &&
                                                vtableIdx < static_cast<int>(context.VTables.size())) {
                                                auto& vtableEntry = context.VTables[vtableIdx];
                                                llvm::PointerType* i8PtrTy =
                                                    llvm::PointerType::get(context.TheContext, 0);
                                                llvm::Value* dataPtr = argVal.Val;
                                                if (!dataPtr->getType()->isPointerTy()) {
                                                    dataPtr = context.Builder.CreateAlloca(dataPtr->getType(), nullptr,
                                                                                           "tmp");
                                                    context.Builder.CreateStore(argVal.Val, dataPtr);
                                                }
                                                if (dataPtr->getType() != i8PtrTy)
                                                    dataPtr =
                                                        context.Builder.CreatePointerCast(dataPtr, i8PtrTy, "data_ptr");
                                                llvm::Value* vtablePtr = context.Builder.CreatePointerCast(
                                                    vtableEntry.VTableGlobal, i8PtrTy, "vtable_ptr");
                                                llvm::Value* fatPtrAlloca =
                                                    context.Builder.CreateAlloca(expectedTy, nullptr, "fat_arg");
                                                llvm::Value* dataFieldPtr = context.Builder.CreateStructGEP(
                                                    expectedTy, fatPtrAlloca, 0, "data_field");
                                                llvm::Value* vtableFieldPtr = context.Builder.CreateStructGEP(
                                                    expectedTy, fatPtrAlloca, 1, "vtable_field");
                                                context.Builder.CreateStore(dataPtr, dataFieldPtr);
                                                context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                                                argVal.Val =
                                                    context.Builder.CreateLoad(expectedTy, fatPtrAlloca, "fat_arg_val");
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
                    sretLLVMTy =
                        llvm::cast<llvm::StructType>(FluxType(TypeKind::Matrix).getLLVMType(context.TheContext));
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
                    Val = context.Builder.CreateFMul(
                        FloatVal, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
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

    // Handle print_sep(a, b, sep) — print a and b separated by sep
    if (Name == "print_sep" && Args.size() == 3) {
        TypedValue ArgA = Args[0]->codegen(context);
        TypedValue ArgB = Args[1]->codegen(context);
        TypedValue ArgSep = Args[2]->codegen(context);
        if (!ArgA.Val || !ArgB.Val || !ArgSep.Val)
            return TypedValue();

        // Convert to double if needed
        llvm::Value* ValA = ArgA.Val;
        if (ArgA.Type.Kind == TypeKind::String) {
            // String is already a double (bitcasted pointer)
        } else if (!ValA->getType()->isDoubleTy()) {
            ValA = context.Builder.CreateSIToFP(ValA, llvm::Type::getDoubleTy(context.TheContext));
        }
        llvm::Value* ValB = ArgB.Val;
        if (ArgB.Type.Kind == TypeKind::String) {
        } else if (!ValB->getType()->isDoubleTy()) {
            ValB = context.Builder.CreateSIToFP(ValB, llvm::Type::getDoubleTy(context.TheContext));
        }

        llvm::Function* PrintSepFn = context.TheModule->getFunction("flux_print_sep");
        if (PrintSepFn) {
            context.Builder.CreateCall(PrintSepFn, {ValA, ValB, ArgSep.Val});
        }
        return TypedValue(llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), 0.0), TypeKind::Double);
    }

    // Handle print_end(x, end) — print x followed by custom end string
    if (Name == "print_end" && Args.size() == 2) {
        TypedValue ArgX = Args[0]->codegen(context);
        TypedValue ArgEnd = Args[1]->codegen(context);
        if (!ArgX.Val || !ArgEnd.Val)
            return TypedValue();

        llvm::Value* ValX = ArgX.Val;
        if (ArgX.Type.Kind == TypeKind::String) {
        } else if (!ValX->getType()->isDoubleTy()) {
            ValX = context.Builder.CreateSIToFP(ValX, llvm::Type::getDoubleTy(context.TheContext));
        }

        llvm::Function* PrintEndFn = context.TheModule->getFunction("flux_print_end");
        if (PrintEndFn) {
            context.Builder.CreateCall(PrintEndFn, {ValX, ArgEnd.Val});
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
                llvm::StructType* MatStructTy = llvm::cast<llvm::StructType>(matTy.getLLVMType(context.TheContext));
                llvm::Value* MatVal = llvm::UndefValue::get(MatStructTy);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResPtr, 0);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResRows, 1);
                MatVal = context.Builder.CreateInsertValue(MatVal, ResCols, 2);
                return TypedValue(MatVal, matTy);
            }
        } else if (Arg.Type.Kind == TypeKind::Double) {
            if (Name == "abs") {
                llvm::Function* FabsF = llvm::Intrinsic::getOrInsertDeclaration(
                    context.TheModule, llvm::Intrinsic::fabs, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FabsF, {Arg.Val}, "abstmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "floor") {
                llvm::Function* FloorF = llvm::Intrinsic::getOrInsertDeclaration(
                    context.TheModule, llvm::Intrinsic::floor, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(FloorF, {Arg.Val}, "floortmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "ceil") {
                llvm::Function* CeilF = llvm::Intrinsic::getOrInsertDeclaration(
                    context.TheModule, llvm::Intrinsic::ceil, {llvm::Type::getDoubleTy(context.TheContext)});
                return TypedValue(context.Builder.CreateCall(CeilF, {Arg.Val}, "ceiltmp"),
                                  FluxType(TypeKind::Double, Arg.Type.Dimensions));
            }
            if (Name == "round") {
                llvm::Function* RoundF = llvm::Intrinsic::getOrInsertDeclaration(
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
                llvm::FunctionType* FT = llvm::FunctionType::get(DoubleTy, {DoubleTy}, false);
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
                    llvm::Value* gep =
                        context.Builder.CreateStructGEP(llvmStructTy, alloca, i, structInfo.Fields[i].first);
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
                    std::cerr << "Error: expected " << genericParams.size() << " type arguments for generic function '"
                              << Name << "', got " << GenericTypeArgs.size() << "\n";
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
                        if (protoArgs[j].second.isGeneric() && protoArgs[j].second.GenericName == genericParams[i]) {
                            typeMap[genericParams[i]] = ArgTVs[j].Type;
                            break;
                        }
                    }
                }
            }

            if (!typeMap.empty()) {
                // Check trait bounds on generic parameters
                if (!checkTraitBounds(genericFunc->getProto()->getGenericParamBounds(), typeMap, context)) {
                    std::cerr << "[FLUX ERROR] Trait bounds check failed for generic function '" << Name << "'"
                              << std::endl;
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
                        case TypeKind::Double:
                            suffix += "Double";
                            break;
                        case TypeKind::Int:
                            suffix += "Int";
                            break;
                        case TypeKind::Float:
                            suffix += "Float";
                            break;
                        case TypeKind::Bool:
                            suffix += "Bool";
                            break;
                        case TypeKind::Matrix:
                            suffix += "Matrix";
                            break;
                        case TypeKind::ComplexMatrix:
                            suffix += "Cmat";
                            break;
                        case TypeKind::Complex:
                            suffix += "Complex";
                            break;
                        case TypeKind::String:
                            suffix += "String";
                            break;
                        case TypeKind::Vector:
                            suffix += "Vector";
                            break;
                        case TypeKind::Void:
                            suffix += "Void";
                            break;
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
                        default:
                            suffix += "T";
                            break;
                        }
                    }
                }

                std::string specializedName = Name + suffix;
                std::cerr << "[CALL GEN] Name=" << Name << " suffix=" << suffix
                          << " specializedName=" << specializedName << std::endl;
                if (!GenericTypeArgs.empty()) {
                    std::cerr << "[CALL GEN] generic arg 0 kind=" << static_cast<int>(GenericTypeArgs[0].Kind)
                              << " name='" << GenericTypeArgs[0].GenericName << "'" << std::endl;
                }
                for (auto& [gp, gt] : typeMap) {
                    std::cerr << "[CALL GEN] typeMap " << gp << " -> kind=" << static_cast<int>(gt.Kind) << " name='"
                              << gt.GenericName << "'" << std::endl;
                }
                CalleeF = context.TheModule->getFunction(specializedName);

                if (!CalleeF && !context.CompiledSpecializations.count(specializedName)) {
                    static constexpr size_t MAX_MONOMORPHIZATIONS = 1024;
                    if (context.CompiledSpecializations.size() >= MAX_MONOMORPHIZATIONS) {
                        std::cerr << "[FLUX ERROR] Maximum number of generic specializations (" << MAX_MONOMORPHIZATIONS
                                  << ") exceeded. "
                                  << "This may indicate a code bloat attack or excessive generic usage." << std::endl;
                        return TypedValue();
                    }
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
            // Check if callee is a local variable containing a function pointer
            llvm::Value* localVar = nullptr;
            {
                auto nvIt = context.NamedValues.find(Callee);
                if (nvIt != context.NamedValues.end() && nvIt->second) {
                    localVar = nvIt->second;
                }
            }
            if (localVar && localVar->getType()->isPointerTy()) {
                // Indirect call through function pointer variable
                llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
                llvm::Type* Int64Ty = llvm::Type::getInt64Ty(context.TheContext);
                llvm::Type* VoidPtrTy = llvm::PointerType::get(context.TheContext, 0);
                std::vector<llvm::Type*> IndirectArgTys;
                for (size_t i = 0; i < Args.size(); ++i)
                    IndirectArgTys.push_back(DoubleTy);
                llvm::FunctionType* IndirectFT = llvm::FunctionType::get(DoubleTy, IndirectArgTys, false);

                std::vector<llvm::Value*> ArgsV;
                llvm::Value* fnDouble = context.Builder.CreateLoad(DoubleTy, localVar, "fnptr_val");
                // Convert double back to function pointer: double → i64 → ptr
                llvm::Value* fnInt = context.Builder.CreateBitCast(fnDouble, Int64Ty, "fnptr_int");
                llvm::Value* fnPtr =
                    context.Builder.CreateIntToPtr(fnInt, llvm::PointerType::get(context.TheContext, 0), "fnptr_cast");

                for (auto& arg : Args) {
                    TypedValue argTV = arg->codegen(context);
                    if (!argTV.Val)
                        return TypedValue();
                    ArgsV.push_back(argTV.Val);
                }
                llvm::Value* ret = context.Builder.CreateCall(IndirectFT, fnPtr, ArgsV, "indirect_call");
                return TypedValue(ret, TypeKind::Double);
            }
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
                    llvm::Type* RetLTy =
                        (extRetType.Kind == TypeKind::Matrix || extRetType.Kind == TypeKind::ComplexMatrix)
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
                    CalleeF =
                        llvm::Function::Create(AutoFT, llvm::Function::ExternalLinkage, Callee, context.TheModule);
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

    // Debug: std::cerr << "[CALL ARGS] Name=" << Name << " Callee=" << Callee << " numArgs=" << Args.size()
    //           << " calleeArgSz=" << CalleeF->arg_size() << " isAsyncFn=" << (CalleeF->arg_size() > (isSretCall ? 1 :
    //           0) ? (CalleeF->arg_begin() + (isSretCall ? 1 : 0))->getName().str() : "?") << std::endl;
    if (CalleeF->arg_size() != Args.size()) {
        // Check for async state param
        {
            unsigned offset = isSretCall ? 1 : 0;
            if (CalleeF->arg_size() > offset) {
                auto ArgIt = CalleeF->arg_begin();
                if (isSretCall)
                    ++ArgIt;
                // Debug: std::cerr << "[CALL ASYNC] check arg name='" << ArgIt->getName().str() << "'" << std::endl;
                if (ArgIt->getName() == "__async_state")
                    needsAsyncState = true;
                else if (ArgIt->getName() == "__gen_state")
                    needsGenState = true;
            }
        }

        int expectedArgs = (int)Args.size();
        if (needsGenState)
            expectedArgs += 1;
        if (needsAsyncState)
            expectedArgs += 1;
        if (isSretCall)
            expectedArgs += 1;

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
        if (isSretCall)
            ParamIdx += 1;
        if (needsAsyncState)
            ParamIdx += 1;
        if (needsGenState)
            ParamIdx += 1;
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
            ArgV = context.Builder.CreateFMul(
                FloatVal, llvm::ConstantFP::get(llvm::Type::getDoubleTy(context.TheContext), scale));
        }

        llvm::Type* ExpectedTy = CalleeF->getArg(ArgsV.size())->getType();
        if (shouldPassByPointer(ArgTVs[i].Type, context)) {
            if (!ArgV->getType()->isPointerTy()) {
                llvm::Value* tempAlloca = context.Builder.CreateAlloca(ArgV->getType(), nullptr, "arg_temp");
                context.Builder.CreateStore(ArgV, tempAlloca);
                ArgV = tempAlloca;
            }
        } else {
            if ((ArgTVs[i].Type.Kind == TypeKind::UserStruct || ArgTVs[i].Type.Kind == TypeKind::UserEnum) &&
                ArgV->getType()->isPointerTy()) {
                llvm::Type* structTy = ArgTVs[i].Type.getLLVMType(context.TheContext);
                ArgV = context.Builder.CreateLoad(structTy, ArgV, "arg_loaded");
            }
        }

        // Dyn trait argument upcast: if the parameter expects a trait object
        // (fat pointer {ptr,ptr}) and the arg is a concrete struct, build the
        // fat pointer by looking up the vtable for the struct/trait pair.
        if (ArgV->getType() != ExpectedTy && ExpectedTy->isStructTy() && ArgTVs[i].Type.Kind == TypeKind::UserStruct) {
            auto* ST = llvm::cast<llvm::StructType>(ExpectedTy);
            if (ST->getNumElements() == 2 && ST->getElementType(0)->isPointerTy() &&
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
                                    llvm::Value* vtablePtr = context.Builder.CreatePointerCast(vtableEntry.VTableGlobal,
                                                                                               i8PtrTy, "vtable_ptr");
                                    llvm::Value* fatPtrAlloca =
                                        context.Builder.CreateAlloca(ExpectedTy, nullptr, "fat_arg");
                                    llvm::Value* dataFieldPtr =
                                        context.Builder.CreateStructGEP(ExpectedTy, fatPtrAlloca, 0, "data_field");
                                    llvm::Value* vtableFieldPtr =
                                        context.Builder.CreateStructGEP(ExpectedTy, fatPtrAlloca, 1, "vtable_field");
                                    context.Builder.CreateStore(dataPtr, dataFieldPtr);
                                    context.Builder.CreateStore(vtablePtr, vtableFieldPtr);
                                    ArgV = context.Builder.CreateLoad(ExpectedTy, fatPtrAlloca, "fat_arg_val");
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
                    if (isSretCall)
                        paramIdx2 += 1;
                    if (needsAsyncState)
                        paramIdx2 += 1;
                    if (needsGenState)
                        paramIdx2 += 1;
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
    bool isMatRet =
        extRetIt != context.ExternFuncTypes.end() &&
        (extRetIt->second.first.Kind == TypeKind::Matrix || extRetIt->second.first.Kind == TypeKind::ComplexMatrix);
    if (isMatRet) {
        bool isComplexMatRet = extRetIt->second.first.Kind == TypeKind::ComplexMatrix;
        llvm::Value* RetPtr = context.Builder.CreateCall(CalleeF, ArgsV, "mat_ret");

        std::string rowsFnName = isComplexMatRet ? "flux_complex_matrix_rows" : "flux_matrix_rows";
        std::string colsFnName = isComplexMatRet ? "flux_complex_matrix_cols" : "flux_matrix_cols";

        llvm::Function* RowsF = context.TheModule->getFunction(rowsFnName);
        if (!RowsF)
            RowsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
                                                               {llvm::PointerType::get(context.TheContext, 0)}, false),
                                       llvm::Function::ExternalLinkage, rowsFnName, context.TheModule);
        llvm::Function* ColsF = context.TheModule->getFunction(colsFnName);
        if (!ColsF)
            ColsF =
                llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context.TheContext),
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
        static const std::set<std::string> kMatToScalar = {"matrix_get", "matrix_sum", "matrix_mean", "matrix_trace"};
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

}; // namespace Flux
