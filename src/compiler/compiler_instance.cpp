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
    
    return Lexer::tokenSpelling(token);
}

} // namespace

CompilerInstance::CompilerInstance(CompilerOptions options)
    : m_options(std::move(options)) {}

std::unique_ptr<CodegenContext> CompilerInstance::createCodegenContext() const {
    auto context = std::make_unique<CodegenContext>();
    context->OwnedModule = std::make_unique<llvm::Module>(m_options.moduleName, context->TheContext);
    context->TheModule = context->OwnedModule.get();

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

    // Register extern function types for the codegen so it can emit
    // correct LLVM call instructions instead of assuming double(double,...).
    // Register extern function types for the codegen so it can emit
    // correct LLVM call instructions instead of assuming double(double,...).
    // LLVM return type for matrix-returning functions is void* (ptr).
    // The post-call code in codegen wraps it into {ptr, i32, i32}.
    auto regExtern = [&](const std::string& name, FluxType ret, std::vector<FluxType> argTypes) {
        context.ExternFuncTypes[name] = {ret, std::move(argTypes)};
    };

    auto VoidPtr = [&]() { return FluxType(TypeKind::Double); }; // void* passed as opaque
    auto IntTy = [&]() { return FluxType(TypeKind::Int); };
    auto DblTy = [&]() { return FluxType(TypeKind::Double); };
    auto MatTy = [&]() { return FluxType(TypeKind::Matrix); };

    // Matrix functions returning void* (wrapped to Matrix by codegen)
    regExtern("matrix_zeros",      MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_create",     MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_eye",        MatTy(), {IntTy()});
    regExtern("matrix_ones",       MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_copy",       MatTy(), {MatTy()});
    regExtern("matrix_diag",       MatTy(), {MatTy()});
    regExtern("matrix_hcat",       MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_vcat",       MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_sum",        DblTy(), {MatTy()});
    regExtern("matrix_mean",       DblTy(), {MatTy()});
    regExtern("matrix_slice",      MatTy(), {MatTy(), IntTy(), IntTy(), IntTy(), IntTy()});
    regExtern("matrix_trace",      DblTy(), {MatTy()});
    regExtern("matrix_diag",       MatTy(), {MatTy()});

    // File I/O and string utilities (flux_ prefix avoids libc symbol conflicts)
    regExtern("flux_fopen",   DblTy(), {DblTy(), DblTy()});
    regExtern("flux_fclose",  DblTy(), {DblTy()});
    regExtern("flux_feof",    DblTy(), {DblTy()});
    regExtern("flux_fgets",   DblTy(), {DblTy()});
    regExtern("flux_fprintf", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_strcmp",  DblTy(), {DblTy(), DblTy()});
    regExtern("flux_strlen",        DblTy(), {DblTy()});
    regExtern("flux_string_at",     DblTy(), {DblTy(), DblTy()});
    regExtern("flux_string_slice",  DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_string_find",   DblTy(), {DblTy(), DblTy()});
    regExtern("flux_parse_number",  DblTy(), {DblTy()});

    // FFT
    regExtern("fft",               MatTy(), {MatTy(), DblTy()});
    regExtern("fft_thd",           DblTy(), {MatTy(), DblTy()});
    regExtern("fft_snr",           DblTy(), {MatTy(), DblTy()});

    // SPICE simulation API (strings passed as double bitcast in JIT calling convention)
    regExtern("register_analysis", DblTy(), {DblTy()});
    regExtern("register_measure",  DblTy(), {DblTy(), DblTy()});
    regExtern("register_probe",    DblTy(), {DblTy(), DblTy()});
    regExtern("register_save",     DblTy(), {DblTy()});
    regExtern("register_param",    DblTy(), {DblTy(), DblTy()});
    regExtern("register_ic",       DblTy(), {DblTy(), DblTy()});
    regExtern("matrix_mul",        MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_add",        MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_sub",        MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_transpose",  MatTy(), {MatTy()});
    regExtern("matrix_inv",        MatTy(), {MatTy()});
    regExtern("matrix_solve",      MatTy(), {MatTy(), MatTy()});

    // Matrix functions returning scalar
    regExtern("matrix_det",        DblTy(), {MatTy()});
    regExtern("matrix_get",        DblTy(), {MatTy(), IntTy(), IntTy()});
    regExtern("matrix_rows",       IntTy(), {MatTy()});
    regExtern("matrix_cols",       IntTy(), {MatTy()});

    // Matrix functions returning void
    regExtern("matrix_set",        FluxType(TypeKind::Void), {MatTy(), IntTy(), IntTy(), DblTy()});

    // Decompositions (all return Matrix)
    regExtern("matrix_lu",         MatTy(), {MatTy()});
    regExtern("matrix_qr",         MatTy(), {MatTy()});
    regExtern("matrix_svd",        MatTy(), {MatTy()});
    regExtern("matrix_cholesky",   MatTy(), {MatTy()});
    regExtern("matrix_eigenvalues",MatTy(), {MatTy()});
    regExtern("matrix_eigenvectors",MatTy(), {MatTy()});
    regExtern("matrix_rank",       DblTy(), {MatTy()});
    regExtern("matrix_cond",       DblTy(), {MatTy()});
    regExtern("matrix_norm",       DblTy(), {MatTy(), DblTy()});

    // Math functions: double in, double out
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

    auto injectRet = [&](const std::string& name, int args, TypeKind retKind) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i)
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});
        PrototypeAST proto(name, params, FluxType(retKind));
        proto.codegen(context);
        returnTypes[name] = FluxType(retKind);
    };

    // Matrix/Vector math
    injectRet("det", 1, TypeKind::Double);
    injectRet("inv", 1, TypeKind::Matrix);
    injectRet("eig", 1, TypeKind::Matrix);
    injectRet("dot", 2, TypeKind::Double);
    injectRet("cross", 2, TypeKind::Matrix);
    injectRet("norm", 1, TypeKind::Double);
    injectRet("rows", 1, TypeKind::Double);
    injectRet("cols", 1, TypeKind::Double);

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
    // print and println are handled in codegen.cpp

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

std::unique_ptr<ParsedAST> CompilerInstance::parse(const std::string& code, std::string* error) const {
    auto ast = std::make_unique<ParsedAST>();
    Parser parser(code);

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            if (auto fn = parser.ParseDefinition()) {
                ast->functions.push_back(std::move(fn));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_subckt)) {
            if (auto sub = parser.ParseSubckt()) {
                ast->subckts.push_back(std::move(sub));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_model)) {
            if (auto mod = parser.ParseModel()) {
                ast->models.push_back(std::move(mod));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_analysis)) {
            if (auto an = parser.ParseAnalysis()) {
                ast->analyses.push_back(std::move(an));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_measure)) {
            if (auto meas = parser.ParseMeasure()) {
                ast->measures.push_back(std::move(meas));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import)) {
            parser.ParseImport(); // Just consume for now
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            parser.ParseExtern(); // Just consume
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
        } else {
            // Collect expressions into anonymous function
            std::vector<std::unique_ptr<ExprAST>> Exprs;
            while (parser.CurTok != static_cast<int>(TokenType::tok_eof) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_def) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_subckt) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_model) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_analysis) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_measure) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_extern) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_import) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_rbrace)) {
                
                if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                    parser.getNextToken();
                    continue;
                }
                
                if (auto E = parser.ParseExpression()) {
                    Exprs.push_back(std::move(E));
                } else {
                    break;
                }
            }
            
            if (!Exprs.empty()) {
                auto Block = std::make_unique<BlockExprAST>(std::move(Exprs));
                auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::pair<std::string, FluxType>>(), FluxType(TypeKind::Double));
                ast->functions.push_back(std::make_unique<FunctionAST>(std::move(Proto), std::move(Block)));
            }
        }

        if (parser.hasError()) break;
    }

    if (error && parser.hasError()) *error = "Parsing failed";
    return ast;
}

bool CompilerInstance::compileParser(Parser& parser,
                                     CodegenContext& context,
                                     std::map<std::string, FluxType>& returnTypes,
                                     std::string& error,
                                     std::map<std::string, bool>& importedModules) const {
    // --- Pass 1: Parse all definitions and declare all prototypes ---
    // This ensures all LLVM function declarations exist with correct types
    // before any call sites are generated, avoiding auto-declaration with
    // wrong types (e.g., double instead of matrix struct).
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::unique_ptr<ExprAST> updateFunc;
    bool hasUpdateFunc = false;

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            auto func = parser.ParseDefinition();
            if (func) {
                const std::string& name = func->getProto()->getName();
                returnTypes[name] = func->getProto()->getReturnType();
                func->getProto()->codegen(context);
                functions.push_back(std::move(func));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_update)) {
            auto func = parser.ParseUpdateFunc();
            if (func) {
                hasUpdateFunc = true;
                updateFunc = std::move(func);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import)) {
            auto importAst = parser.ParseImport();
            if (!importAst) {
                error = "Failed to parse import statement";
                return false;
            }
            auto* importExpr = dynamic_cast<ImportExprAST*>(importAst.get());
            if (!importExpr) {
                error = "Failed to extract import information";
                return false;
            }
            const std::string& moduleName = importExpr->getModuleName();
            if (!importModule(moduleName, context, returnTypes, &error, importedModules)) {
                return false;
            }
            const std::string& alias = importExpr->getAlias().empty() ? moduleName : importExpr->getAlias();
            context.NamedValues[alias + ".*"] = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(context.TheContext, 0));
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                returnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_eof)) {
            break;
        } else {
            // Collect consecutive expressions into a single anonymous function
            std::vector<std::unique_ptr<ExprAST>> Exprs;
            while (parser.CurTok != static_cast<int>(TokenType::tok_eof) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_def) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_extern) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_import) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_update) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_rbrace)) {

                if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                    parser.getNextToken();
                    continue;
                }

                if (auto E = parser.ParseExpression()) {
                    Exprs.push_back(std::move(E));
                } else {
                    parser.SkipToSynchronizationPoint();
                }
            }

            if (!Exprs.empty()) {
                auto Block = std::make_unique<BlockExprAST>(std::move(Exprs));
                static unsigned anonCounter = 0;
                std::string anonName = "__anon_expr";
                if (!m_options.moduleName.empty() && m_options.moduleName != "Flux Module") {
                    anonName = m_options.moduleName + "_anon_expr";
                }
                anonName += "_" + std::to_string(anonCounter++);
                auto Proto = std::make_unique<PrototypeAST>(anonName, std::vector<std::pair<std::string, FluxType>>(), FluxType(TypeKind::Double));
                functions.push_back(std::make_unique<FunctionAST>(std::move(Proto), std::move(Block)));
            }
        }
    }

    // --- Pass 2: Codegen all function bodies ---
    // All LLVM declarations now exist, so call sites will find the correct types.
    if (hasUpdateFunc) {
        returnTypes["update"] = FluxType(TypeKind::Double);
        if (!updateFunc->codegen(context)) {
            error = "Code generation failed for update function";
            return false;
        }
    }

    for (auto& func : functions) {
        if (!func->codegen(context)) {
            error = "Code generation failed for function: " + func->getProto()->getName();
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
