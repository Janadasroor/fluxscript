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

#ifndef FLUX_COMPILER_COMPILER_INSTANCE_H
#define FLUX_COMPILER_COMPILER_INSTANCE_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <llvm/ADT/StringRef.h>

#include "flux/compiler/ast.h"
#include "flux/compiler/module_loader.h"
#include "flux/jit/flux_jit.h"

namespace Flux {

class Parser;

struct TokenInfo
{
    int token = 0;
    std::string spelling;
    int line = 0;
    int column = 0;
    size_t offset = 0;
    size_t length = 0;
    std::string rawText;
};

/// Progress callback for long compilations.
/// stage: human-readable stage name (e.g. "Parsing", "Codegen", "Object emission")
/// current: zero-based index within the stage (0 when the stage begins)
/// total: total items in this stage (0 when unknown)
/// Returns true to continue, false to cancel.
using CompileProgressCallback = std::function<bool(const std::string& stage, int current, int total)>;

struct CompilerOptions
{
    std::string inputName = "<stdin>";
    std::string moduleName = "Flux Module";
    OptimizationLevel optimizationLevel = OptimizationLevel::O2;
    bool injectStdlib = true;
    bool debugInfo = false;

    /// Optional progress callback for AOT compilation of large projects.
    /// Set before calling compileToIR() / emitArtifact().
    CompileProgressCallback progressCallback = nullptr;

    /// Number of parallel jobs for function codegen (default 1 = sequential).
    int numJobs = 1;
};

struct CompileArtifacts
{
    std::unique_ptr<CodegenContext> codegenContext;
    std::map<std::string, FluxType> functionReturnTypes;
    CompileProgressCallback compileProgress = nullptr;
};

struct ParsedAST
{
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::vector<std::unique_ptr<StructDeclAST>> structs;
    std::vector<std::unique_ptr<EnumDeclAST>> enums;
    std::vector<std::unique_ptr<ImplDeclAST>> impls;
    std::vector<std::unique_ptr<TraitDeclAST>> traits;
    std::vector<std::unique_ptr<SubcktAST>> subckts;
    std::vector<std::unique_ptr<ModelAST>> models;
    std::vector<std::unique_ptr<AnalysisExprAST>> analyses;
    std::vector<std::unique_ptr<MeasureExprAST>> measures;
    std::vector<std::unique_ptr<ExprAST>> params;
    std::vector<std::unique_ptr<ExprAST>> ics;
    std::vector<std::unique_ptr<ExprAST>> probes;
    std::vector<std::unique_ptr<ExprAST>> saves;
    std::vector<std::unique_ptr<ExprAST>> bsources;
    std::vector<std::unique_ptr<ExprAST>> esources;
    std::vector<std::unique_ptr<ExprAST>> fsources;
    std::vector<std::unique_ptr<ExprAST>> gsources;
    std::vector<std::unique_ptr<ExprAST>> hsources;
};

class CompilerInstance
{
public:
    explicit CompilerInstance(CompilerOptions options = {});

    std::vector<TokenInfo> tokenize(const std::string& code, std::string* error = nullptr) const;
    std::unique_ptr<ParsedAST> parse(const std::string& code, std::string* error = nullptr) const;
    std::unique_ptr<CompileArtifacts> compileToIR(const std::string& code, std::string* error = nullptr);
    std::string emitLLVMIR(const std::string& code, std::string* error = nullptr);

private:
    std::unique_ptr<CodegenContext> createCodegenContext() const;
    void injectStandardLibrary(CodegenContext& context, std::map<std::string, FluxType>& returnTypes) const;
    bool compileParser(Parser& parser, CodegenContext& context, std::map<std::string, FluxType>& returnTypes,
                       std::string& error, std::map<std::string, bool>& importedModules,
                       const std::vector<std::string>& symbols = {},
                       std::vector<std::unique_ptr<FunctionAST>>* preParsedFunctions = nullptr) const;
    bool importModule(const std::string& moduleName, CodegenContext& context,
                      std::map<std::string, FluxType>& returnTypes, std::string* error,
                      std::map<std::string, bool>& importedModules, const std::vector<std::string>& symbols = {}) const;
    bool collectImportFunctions(const std::string& moduleName,
                                std::vector<std::unique_ptr<FunctionAST>>& outFunctions,
                                CodegenContext& context, std::map<std::string, FluxType>& returnTypes,
                                std::string* error, std::map<std::string, bool>& importedModules,
                                const std::vector<std::string>& symbols = {}) const;
    std::string resolveImportPath(const std::string& moduleName) const;
    bool loadAndLinkBitcodeModule(const std::string& bcPath, CodegenContext& context,
                                  std::map<std::string, FluxType>& returnTypes,
                                  std::string* error = nullptr) const;

    CompilerOptions m_options;
    mutable ModuleLoader m_moduleLoader;
};

} // namespace Flux

#endif // FLUX_COMPILER_COMPILER_INSTANCE_H
