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

#include "flux/compiler/compiler_instance.h"

#include <cstdlib>

#include <llvm/IR/Verifier.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Path.h>

#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"

#include <sstream>
#include <iostream>

namespace Flux {

namespace {

std::string tokenSpelling(const Lexer& lexer, int token) {
    if (token == static_cast<int>(TokenType::tok_identifier))
        return lexer.IdentifierStr;
    if (token == static_cast<int>(TokenType::tok_string))
        return "\"" + lexer.StringVal + "\"";
    if (token == static_cast<int>(TokenType::tok_number) ||
        token == static_cast<int>(TokenType::tok_imaginary)) {
        std::ostringstream os;
        os << lexer.NumVal;
        if (token == static_cast<int>(TokenType::tok_imaginary))
            os << "j";
        return os.str();
    }
    if (token >= 0 && token < 128)
        return std::string(1, static_cast<char>(token));

    switch (static_cast<TokenType>(token)) {
        case TokenType::tok_eof: return "eof";
        case TokenType::tok_def: return "def";
        case TokenType::tok_extern: return "extern";
        case TokenType::tok_return: return "return";
        case TokenType::tok_var: return "var";
        case TokenType::tok_if: return "if";
        case TokenType::tok_then: return "then";
        case TokenType::tok_else: return "else";
        case TokenType::tok_for: return "for";
        case TokenType::tok_in: return "in";
        case TokenType::tok_do: return "do";
        case TokenType::tok_while: return "while";
        case TokenType::tok_let: return "let";
        case TokenType::tok_fn: return "fn";
        case TokenType::tok_import: return "import";
        case TokenType::tok_type_float: return "float";
        case TokenType::tok_type_double: return "double";
        case TokenType::tok_type_int: return "int";
        case TokenType::tok_type_void: return "void";
        case TokenType::tok_type_complex: return "complex";
        case TokenType::tok_type_string: return "string";
        case TokenType::tok_type_vector: return "vector";
        case TokenType::tok_type_matrix: return "matrix";
        default: return "token(" + std::to_string(token) + ")";
    }
}

} // namespace

CompilerInstance::CompilerInstance(CompilerOptions options)
    : m_options(std::move(options)) {}

std::unique_ptr<CodegenContext> CompilerInstance::createCodegenContext() const {
    auto context = std::make_unique<CodegenContext>();
    context->TheModule = std::make_unique<llvm::Module>(m_options.moduleName, context->TheContext);

    if (m_options.debugInfo) {
        namespace path = llvm::sys::path;
        llvm::SmallString<256> fullPath(m_options.inputName.empty() ? "<stdin>" : m_options.inputName);
        llvm::SmallString<256> directory(fullPath);
        path::remove_filename(directory);
        const llvm::StringRef fileName = path::filename(fullPath);

        context->DebugEnabled = true;
        context->TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                                          llvm::DEBUG_METADATA_VERSION);
        context->TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
        context->DebugBuilder = std::make_unique<llvm::DIBuilder>(*context->TheModule);
        context->DebugFile = context->DebugBuilder->createFile(
            fileName.empty() ? llvm::StringRef("<stdin>") : fileName,
            directory.empty() ? llvm::StringRef(".") : llvm::StringRef(directory));
        context->DebugCompileUnit = context->DebugBuilder->createCompileUnit(
            llvm::dwarf::DW_LANG_C_plus_plus,
            context->DebugFile,
            "FluxScript",
            false,
            "",
            0);
    }

    return context;
}

void CompilerInstance::injectStandardLibrary(CodegenContext& context,
                                             std::map<std::string, FluxType>& returnTypes) const {
    auto inject = [&](const std::string& name, int args) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i)
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});

        PrototypeAST proto(name, params, FluxType(TypeKind::Double));
        proto.codegen(context);
        returnTypes[name] = FluxType(TypeKind::Double);
    };

    inject("sin", 1);
    inject("cos", 1);
    inject("tan", 1);
    inject("asin", 1);
    inject("acos", 1);
    inject("atan", 1);
    inject("atan2", 2);
    inject("sinh", 1);
    inject("cosh", 1);
    inject("tanh", 1);
    inject("sqrt", 1);
    inject("exp", 1);
    inject("log", 1);
    inject("log10", 1);
    inject("abs", 1);
    inject("floor", 1);
    inject("ceil", 1);
    inject("round", 1);
    inject("pow", 2);
    inject("pi", 0);
    inject("e", 0);

    // EDA functions (strings as arguments)
    auto injectStr = [&](const std::string& name, int args) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i)
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});
        PrototypeAST proto(name, params, FluxType(TypeKind::Double));
        proto.codegen(context);
        returnTypes[name] = FluxType(TypeKind::Double);
    };

    injectStr("net", 1);
    injectStr("branch", 1);
    injectStr("p", 1); // Alias for get_parameter if needed, but we have MemberExpr
    injectStr("state_get", 2);
    injectStr("state_set", 2);
    injectStr("print", 1);
    injectStr("println", 1);

    inject("sim_run", 0);
    inject("sim_stop", 0);
    inject("sim_pause", 0);
    inject("erc_check", 0);
}

std::string CompilerInstance::resolveImportPath(const std::string& moduleName) const {
    namespace path = llvm::sys::path;
    namespace fs = llvm::sys::fs;

    llvm::SmallString<256> resolved;

    // Try 1: Same directory as input file
    if (!m_options.inputName.empty() && m_options.inputName != "-") {
        resolved = m_options.inputName;
        path::remove_filename(resolved);
        path::append(resolved, moduleName + ".flux");
        if (fs::exists(resolved)) return std::string(resolved.str());
    }

    // Try 2: Current working directory
    resolved.clear();
    path::append(resolved, moduleName + ".flux");
    if (fs::exists(resolved)) return std::string(resolved.str());

    // Try 3: FLUX_MODULE_PATH environment variable
    if (const char* modPath = std::getenv("FLUX_MODULE_PATH")) {
        resolved.clear();
        path::append(resolved, modPath, moduleName + ".flux");
        if (fs::exists(resolved)) return std::string(resolved.str());
    }

    // Fallback: original behavior (will produce error message)
    if (!m_options.inputName.empty() && m_options.inputName != "-") {
        resolved = m_options.inputName;
        path::remove_filename(resolved);
        path::append(resolved, moduleName + ".flux");
    } else {
        resolved = moduleName + ".flux";
    }
    return std::string(resolved.str());
}

bool CompilerInstance::importModule(const std::string& moduleName,
                                    CodegenContext& context,
                                    std::map<std::string, FluxType>& returnTypes,
                                    std::string* error,
                                    std::map<std::string, bool>& importedModules) const {
    if (importedModules.find(moduleName) != importedModules.end())
        return true;
    importedModules[moduleName] = true;

    const auto fileName = resolveImportPath(moduleName);
    auto bufferOrErr = llvm::MemoryBuffer::getFile(fileName);
    if (!bufferOrErr) {
        if (error)
            *error = "Could not open imported file: " + fileName;
        return false;
    }

    Parser importedParser(std::string(bufferOrErr.get()->getBuffer()));
    std::string importError;
    bool result = compileParser(importedParser, context, returnTypes, importError, importedModules);
    if (!result && error) {
        *error = importError;
    }
    return result;
}

bool CompilerInstance::compileParser(Parser& parser,
                                     CodegenContext& context,
                                     std::map<std::string, FluxType>& returnTypes,
                                     std::string& error,
                                     std::map<std::string, bool>& importedModules) const {
    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        std::unique_ptr<FunctionAST> functionAst;

        // Handle stray closing brace (end of block without matching open)
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            functionAst = parser.ParseDefinition();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_update)) {
            // Handle update function at top level
            auto updateAst = parser.ParseUpdateFunc();
            if (!updateAst) {
                error = "Failed to parse update function";
                return false;
            }
            returnTypes["update"] = FluxType(TypeKind::Double);
            if (!updateAst->codegen(context)) {
                error = "Code generation failed for update function";
                return false;
            }
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import)) {
            auto importAst = parser.ParseImport();
            if (!importAst) {
                error = "Failed to parse import statement";
                return false;
            }
            // Extract import details from the AST
            auto* importExpr = dynamic_cast<ImportExprAST*>(importAst.get());
            if (!importExpr) {
                error = "Failed to extract import information";
                return false;
            }
            const std::string& moduleName = importExpr->getModuleName();
            if (!importModule(moduleName, context, returnTypes, &error, importedModules)) {
                return false;
            }
            // Register namespace alias marker so module.func lookups work
            const std::string& alias = importExpr->getAlias().empty() ? moduleName : importExpr->getAlias();
            context.NamedValues[alias + ".*"] = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(context.TheContext, 0));
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                returnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(context);
            }
            continue;
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
            continue;
        } else {
            functionAst = parser.ParseTopLevelExpr();
        }

        if (!functionAst) {
            if (parser.CurTok != static_cast<int>(TokenType::tok_eof))
                parser.SkipToSynchronizationPoint();
            continue;
        }

        const std::string functionName = functionAst->getProto()->getName();
        returnTypes[functionName] = functionAst->getProto()->getReturnType();
        if (!functionAst->codegen(context)) {
            error = "Code generation failed for function: " + functionName;
            return false;
        }
    }

    return !parser.hasError();
}

std::vector<TokenInfo> CompilerInstance::tokenize(const std::string& code, std::string* error) const {
    std::vector<TokenInfo> tokens;
    Lexer lexer(code);

    while (true) {
        const int token = lexer.getNextToken();
        tokens.push_back(TokenInfo{
            token,
            tokenSpelling(lexer, token),
            lexer.getCurrentLine(),
            lexer.getCurrentColumn(),
            lexer.getCurrentTokenOffset(),
            lexer.getCurrentTokenLength(),
            lexer.getCurrentTokenText()
        });

        if (token == static_cast<int>(TokenType::tok_eof))
            break;
    }

    if (error)
        error->clear();
    return tokens;
}

std::unique_ptr<CompileArtifacts> CompilerInstance::compileToIR(const std::string& code,
                                                                std::string* error) {
    auto artifacts = std::make_unique<CompileArtifacts>();
    artifacts->codegenContext = createCodegenContext();

    if (m_options.injectStdlib)
        injectStandardLibrary(*artifacts->codegenContext, artifacts->functionReturnTypes);

    Parser parser(code); // Constructor primes the lexer with getNextToken()
    std::map<std::string, bool> importedModules;
    std::string compileError;
    if (!compileParser(parser, *artifacts->codegenContext, artifacts->functionReturnTypes, compileError, importedModules)) {
        if (error) *error = compileError;
        return nullptr;
    }

    if (artifacts->codegenContext->DebugBuilder)
        artifacts->codegenContext->DebugBuilder->finalize();

    if (llvm::verifyModule(*artifacts->codegenContext->TheModule, &llvm::errs())) {
        if (error)
            *error = "Generated LLVM IR is invalid.";
        return nullptr;
    }

    if (error)
        error->clear();
    return artifacts;
}

std::string CompilerInstance::emitLLVMIR(const std::string& code, std::string* error) {
    auto artifacts = compileToIR(code, error);
    if (!artifacts)
        return {};

    std::string ir;
    llvm::raw_string_ostream os(ir);
    artifacts->codegenContext->TheModule->print(os, nullptr);
    os.flush();
    return ir;
}

} // namespace Flux
