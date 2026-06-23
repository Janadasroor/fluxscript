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

#ifndef FLUX_COMPILER_CODEGEN_CONTEXT_H
#define FLUX_COMPILER_CODEGEN_CONTEXT_H

#include "flux/compiler/flux_type.h"
#include <functional>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Flux {

class FunctionAST;
class StructDeclAST;
class EnumDeclAST;
class ImplDeclAST;

class CodegenContext
{
public:
    std::unique_ptr<llvm::LLVMContext> OwnedContext;
    llvm::LLVMContext& TheContext;
    llvm::IRBuilder<> Builder;
    std::unique_ptr<llvm::Module> OwnedModule;
    llvm::Module* TheModule = nullptr;
    std::map<std::string, llvm::Value*> NamedValues;
    std::map<std::string, FluxType> NamedTypes;
    std::set<std::string> MovedVariables;
    std::map<std::string, std::pair<FluxType, std::vector<FluxType>>> ExternFuncTypes;

    // For return statements
    llvm::BasicBlock* CurrentReturnBB = nullptr;
    llvm::Value* CurrentReturnValueAlloca = nullptr;

    std::unique_ptr<llvm::DIBuilder> DebugBuilder;
    llvm::DICompileUnit* DebugCompileUnit = nullptr;
    llvm::DIFile* DebugFile = nullptr;
    bool DebugEnabled = false;
    std::vector<llvm::DIScope*> LexicalBlocks;

    // Control flow context for break/continue
    llvm::BasicBlock* CurrentLoopEnd = nullptr;   // Target for break in loops
    llvm::BasicBlock* CurrentLoopCont = nullptr;  // Target for continue in loops
    llvm::BasicBlock* CurrentSwitchEnd = nullptr; // Target for break in switch

    // Exception handling context
    llvm::BasicBlock* CurrentCatchBB = nullptr;    // Target for throw
    llvm::Value* CurrentExceptionAlloca = nullptr; // Where to store the thrown value

    // Generator context
    llvm::Value* GeneratorStateAlloca = nullptr;       // Struct containing state index and locals
    llvm::Type* GeneratorStateStructTy = nullptr;      // Struct type for generator state
    std::vector<llvm::BasicBlock*> YieldTargets;       // Blocks to resume from
    llvm::BasicBlock* GeneratorDispatcherBB = nullptr; // Entry dispatcher

    // Async context
    llvm::Value* AsyncStateAlloca = nullptr;           // Struct containing state index
    llvm::Type* AsyncStateStructTy = nullptr;          // Struct type for async state
    llvm::Value* AsyncResultAlloca = nullptr;          // Alloca for await result value
    std::vector<llvm::BasicBlock*> AwaitResumeTargets; // Blocks to resume from after await
    llvm::BasicBlock* AsyncDispatcherBB = nullptr;     // Entry dispatcher

    // Module import callback — called by ImportExprAST::codegen for function-body imports.
    // Parameters: moduleName, alias, symbols list
    // Returns true on success, stores error string on failure.
    std::function<bool(const std::string&, const std::string&, const std::vector<std::string>&)> importModuleFn;

    // Set of imported module namespace names (e.g., "defs" for `import defs`).
    // These survive NamedValues.clear() in FunctionAST::codegen so that
    // namespace-qualified calls like defs.func() can be resolved.
    std::set<std::string> ModuleNamespaces;

    // Names of functions that exist in selectively-imported modules but
    // were not selected. Used to produce clear error messages when the
    // user tries to call a non-imported function.
    std::set<std::string> ExcludedSymbols;

    // Declared return type dimensions for user/extern functions, keyed by name.
    // Populated during compilation so CallExprAST can propagate UnitDimensions
    // through function calls.
    std::map<std::string, FluxType> FuncReturnTypes;

    // Cross-module parameter types for user-defined functions, keyed by
    // function name -> vector of param types. Populated during compilation
    // so that inferParamTypes can see callee param types from imported modules.
    std::map<std::string, std::vector<FluxType>> CrossModuleParamTypes;

    // Struct type definitions: maps struct name -> field list
    // Index in the vector serves as StructTypeId.
    struct StructTypeInfo {
        std::string Name;
        std::string ParentName;
        std::vector<std::pair<std::string, FluxType>> Fields;
        llvm::StructType* LLVMType = nullptr;
        bool isCopy = true;
    };
    std::vector<StructTypeInfo> StructTypes;
    std::map<std::string, int> StructTypeIndex; // name -> index

    // Enum type definitions
    struct EnumTypeInfo {
        std::string Name;
        std::vector<std::string> Variants; // variant name -> discriminant = index
        std::vector<FluxType> VariantPayloads; // payload type per variant (Void if none)
        llvm::StructType* LLVMType = nullptr; // { i32, union_of_payloads }
        std::vector<bool> VariantIsBoxed; // whether variant payload is heap-allocated (boxed)
        bool isCopy = true;
    };
    std::vector<EnumTypeInfo> EnumTypes;
    std::map<std::string, int> EnumTypeIndex; // name -> index

    // Generic function definitions: original (un-specialized) function name -> FunctionAST.
    // Populated during parse phase, used during codegen for monomorphization.
    std::map<std::string, FunctionAST*> GenericFunctions;

    // Set of already-specialized generic function suffixes, to avoid
    // re-compiling the same specialization.
    std::set<std::string> CompiledSpecializations;

    // Owning storage for generic function definitions.
    // NOLINTBEGIN(clang-diagnostic-error) — unique_ptr with incomplete types is valid here
    std::vector<std::unique_ptr<FunctionAST>> GenericFunctionDefs;

    // Generic struct definitions
    std::map<std::string, StructDeclAST*> GenericStructs;
    std::vector<std::unique_ptr<StructDeclAST>> GenericStructDefs;

    // Generic enum definitions
    std::map<std::string, EnumDeclAST*> GenericEnums;
    std::vector<std::unique_ptr<EnumDeclAST>> GenericEnumDefs;

    // Generic impl definitions
    std::map<std::string, ImplDeclAST*> GenericImpls;
    std::vector<std::unique_ptr<ImplDeclAST>> GenericImplDefs;
    // NOLINTEND(clang-diagnostic-error)

    // Trait definitions
    struct TraitInfo {
        std::string Name;
        std::vector<std::string> SuperTraits;
        std::vector<std::string> AssociatedTypes;
        struct MethodSig {
            std::string Name;
            std::vector<std::pair<std::string, FluxType>> Args;
            FluxType ReturnType;
        };
        std::vector<MethodSig> Methods;
    };
    std::vector<TraitInfo> Traits;
    std::map<std::string, int> TraitIndex; // trait name -> index

    // Track trait implementations: (trait name, type name) pair
    std::set<std::pair<std::string, std::string>> TraitImplementations;

    // VTable infrastructure for trait objects
    struct VTableEntry {
        std::string TraitName;
        std::string TypeName;
        llvm::GlobalVariable* VTableGlobal = nullptr; // the vtable instance
        llvm::StructType* VTableType = nullptr;       // the vtable struct type
    };
    std::vector<VTableEntry> VTables;
    // Map: (trait_name, type_name) -> index into VTables
    std::map<std::pair<std::string, std::string>, int> VTableIndex;

    // Already-specialized struct/enum type suffixes
    std::set<std::string> CompiledStructSpecializations;
    std::set<std::string> CompiledEnumSpecializations;

    // Method dispatch: type name -> { method name -> Function }
    std::map<std::string, std::map<std::string, llvm::Function*>> TypeMethods;

    CodegenContext()
        : OwnedContext(std::make_unique<llvm::LLVMContext>()), TheContext(*OwnedContext), Builder(TheContext)
    {
        llvm::FastMathFlags FMF;
        FMF.setFast();
        Builder.setFastMathFlags(FMF);
    }

    CodegenContext(llvm::LLVMContext& Ctx, llvm::Module* Mod) : TheContext(Ctx), Builder(Ctx), TheModule(Mod)
    {
        llvm::FastMathFlags FMF;
        FMF.setFast();
        Builder.setFastMathFlags(FMF);
    }
};

struct TypedValue
{
    llvm::Value* Val;
    FluxType Type;

    TypedValue(llvm::Value* V = nullptr, FluxType T = TypeKind::Double) : Val(V), Type(T) {}

    operator llvm::Value*() const { return Val; }
};

} // namespace Flux

#endif // FLUX_COMPILER_CODEGEN_CONTEXT_H
