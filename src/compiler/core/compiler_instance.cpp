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

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <thread>

#include <llvm/ADT/SmallString.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"
#include "flux/compiler/preprocessor.h"

#include <iostream>
#include <sstream>

namespace Flux {

namespace {

std::string tokenSpelling(const Lexer& lexer, int token)
{
    if (token == static_cast<int>(TokenType::tok_identifier))
        return lexer.IdentifierStr;
    if (token == static_cast<int>(TokenType::tok_string))
        return "\"" + lexer.StringVal + "\"";
    if (token == static_cast<int>(TokenType::tok_number) || token == static_cast<int>(TokenType::tok_imaginary)) {
        std::ostringstream os;
        os << lexer.NumVal;
        if (token == static_cast<int>(TokenType::tok_imaginary))
            os << "j";
        return os.str();
    }

    return Lexer::tokenSpelling(token);
}

} // namespace

CompilerInstance::CompilerInstance(CompilerOptions options) : m_options(std::move(options)) {}

std::unique_ptr<CodegenContext> CompilerInstance::createCodegenContext() const
{
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
        context->TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
        context->TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
        context->DebugBuilder = std::make_unique<llvm::DIBuilder>(*context->TheModule);
        context->DebugFile =
            context->DebugBuilder->createFile(fileName.empty() ? llvm::StringRef("<stdin>") : fileName,
                                              directory.empty() ? llvm::StringRef(".") : llvm::StringRef(directory));
        context->DebugCompileUnit = context->DebugBuilder->createCompileUnit(
            llvm::dwarf::DW_LANG_C_plus_plus, context->DebugFile, "FluxScript", false, "", 0);
    }

    return context;
}

void CompilerInstance::injectStandardLibrary(CodegenContext& context,
                                             std::map<std::string, FluxType>& returnTypes) const
{
    auto inject = [&](const std::string& name, int args) {
        std::vector<std::pair<std::string, FluxType>> params;
        for (int i = 0; i < args; ++i)
            params.push_back({"arg" + std::to_string(i), FluxType(TypeKind::Double)});

        PrototypeAST proto(name, params, FluxType(TypeKind::Double));
        proto.codegen(context);
        returnTypes[name] = FluxType(TypeKind::Double);
        context.FuncReturnTypes[name] = FluxType(TypeKind::Double);
    };

    // Register extern function types for the codegen so it can emit
    // correct LLVM call instructions instead of assuming double(double,...).
    // LLVM return type for matrix-returning functions is void* (ptr).
    // The post-call code in codegen wraps it into {ptr, i32, i32}.
    auto regExtern = [&](const std::string& name, FluxType ret, std::vector<FluxType> argTypes) {
        context.ExternFuncTypes[name] = {ret, std::move(argTypes)};
        context.FuncReturnTypes[name] = ret;
    };

    auto VoidPtr = [&]() { return FluxType(TypeKind::Double); }; // void* passed as opaque
    auto IntTy = [&]() { return FluxType(TypeKind::Int); };
    auto DblTy = [&]() { return FluxType(TypeKind::Double); };
    auto MatTy = [&]() { return FluxType(TypeKind::Matrix); };
    auto CMatTy = [&]() { return FluxType(TypeKind::ComplexMatrix); };

    // Matrix functions returning void* (wrapped to Matrix by codegen)
    regExtern("matrix_zeros", MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_create", MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_eye", MatTy(), {IntTy()});
    regExtern("matrix_ones", MatTy(), {IntTy(), IntTy()});
    regExtern("matrix_copy", MatTy(), {MatTy()});
    regExtern("matrix_diag", MatTy(), {MatTy()});
    regExtern("matrix_hcat", MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_vcat", MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_sum", DblTy(), {MatTy()});
    regExtern("matrix_mean", DblTy(), {MatTy()});
    regExtern("matrix_slice", MatTy(), {MatTy(), IntTy(), IntTy(), IntTy(), IntTy()});
    regExtern("matrix_trace", DblTy(), {MatTy()});
    regExtern("matrix_diag", MatTy(), {MatTy()});

    // Complex matrix constructor functions
    regExtern("flux_complex_matrix_zeros", CMatTy(), {IntTy(), IntTy()});
    regExtern("flux_complex_matrix_ones", CMatTy(), {IntTy(), IntTy()});
    regExtern("flux_complex_matrix_eye", CMatTy(), {IntTy()});

    // Complex matrix query functions
    regExtern("flux_complex_matrix_rows", IntTy(), {CMatTy()});
    regExtern("flux_complex_matrix_cols", IntTy(), {CMatTy()});

    // Complex matrix element access
    regExtern("flux_complex_matrix_get_real", DblTy(), {CMatTy(), IntTy(), IntTy()});
    regExtern("flux_complex_matrix_get_imag", DblTy(), {CMatTy(), IntTy(), IntTy()});
    regExtern("flux_complex_matrix_set", FluxType(TypeKind::Void), {CMatTy(), IntTy(), IntTy(), DblTy(), DblTy()});

    // Complex matrix operations
    regExtern("flux_complex_matrix_ctranspose", CMatTy(), {CMatTy()});
    regExtern("flux_complex_matrix_conj", CMatTy(), {CMatTy()});
    regExtern("flux_complex_matrix_transpose", CMatTy(), {CMatTy()});
    regExtern("flux_complex_matrix_inv", CMatTy(), {CMatTy()});
    regExtern("flux_complex_matrix_det", DblTy(), {CMatTy()});
    regExtern("flux_complex_matrix_trace", DblTy(), {CMatTy()});

    // File I/O and string utilities (flux_ prefix avoids libc symbol conflicts)
    regExtern("flux_fopen", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_fclose", DblTy(), {DblTy()});
    regExtern("flux_fflush", DblTy(), {DblTy()});
    regExtern("flux_feof", DblTy(), {DblTy()});
    regExtern("flux_fgets", DblTy(), {DblTy()});
    regExtern("flux_fprintf", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_fetch_url", DblTy(), {DblTy()});
    regExtern("flux_strcmp", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_strlen", DblTy(), {DblTy()});
    regExtern("flux_string_at", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_string_slice", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_string_find", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_parse_number", DblTy(), {DblTy()});
    regExtern("flux_string_concat", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_double_to_string", DblTy(), {DblTy()});
    regExtern("flux_regex_match", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_regex_replace", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_print_string", DblTy(), {DblTy()});
    regExtern("flux_print_sep", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_print_end", DblTy(), {DblTy(), DblTy()});

    // Bridge string utility functions (return String handles as opaque doubles)
    regExtern("flux_str_len", DblTy(), {DblTy()});
    regExtern("flux_str_at", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_str_slice", DblTy(), {DblTy(), DblTy(), DblTy()});
    regExtern("flux_str_from_char", DblTy(), {DblTy()});
    regExtern("flux_str_concat", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_read_file", DblTy(), {DblTy()});
    regExtern("flux_get_env", DblTy(), {DblTy()});
    regExtern("flux_dtoa", DblTy(), {DblTy()});

    // FFT
    regExtern("fft", MatTy(), {MatTy(), DblTy()});
    regExtern("fft_thd", DblTy(), {MatTy(), DblTy()});
    regExtern("fft_snr", DblTy(), {MatTy(), DblTy()});

    // Range expression
    regExtern("flux_create_range_sum", MatTy(), {DblTy(), DblTy(), DblTy()});

    // SPICE simulation API
    regExtern("register_analysis", DblTy(), {DblTy()});
    regExtern("register_measure", DblTy(), {DblTy(), DblTy()});
    regExtern("register_probe", DblTy(), {DblTy(), DblTy()});
    regExtern("register_save", DblTy(), {DblTy()});
    regExtern("register_param", DblTy(), {DblTy(), DblTy()});
    regExtern("register_ic", DblTy(), {DblTy(), DblTy()});

    // flux_register_* names used by SPICE AST codegen (const char* args, double return)
    regExtern("flux_register_analysis", DblTy(), {DblTy()});
    regExtern("flux_register_measure", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_probe", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_save", DblTy(), {DblTy()});
    regExtern("flux_register_param", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_ic", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_model", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_subckt", DblTy(), {DblTy(), DblTy()});
    regExtern("flux_register_bsource", DblTy(), {DblTy(), DblTy(), DblTy(), DblTy()});
    regExtern("matrix_mul", MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_add", MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_sub", MatTy(), {MatTy(), MatTy()});
    regExtern("matrix_transpose", MatTy(), {MatTy()});
    regExtern("matrix_inv", MatTy(), {MatTy()});
    regExtern("matrix_solve", MatTy(), {MatTy(), MatTy()});

    // Matrix functions returning scalar
    regExtern("matrix_det", DblTy(), {MatTy()});
    regExtern("matrix_get", DblTy(), {MatTy(), IntTy(), IntTy()});
    regExtern("matrix_rows", IntTy(), {MatTy()});
    regExtern("matrix_cols", IntTy(), {MatTy()});

    // Matrix functions returning void
    regExtern("matrix_set", FluxType(TypeKind::Void), {MatTy(), IntTy(), IntTy(), DblTy()});

    // Decompositions (all return Matrix)
    regExtern("matrix_lu", MatTy(), {MatTy()});
    regExtern("matrix_qr", MatTy(), {MatTy()});
    regExtern("matrix_svd", MatTy(), {MatTy()});
    regExtern("matrix_cholesky", MatTy(), {MatTy()});
    regExtern("matrix_eigenvalues", MatTy(), {MatTy()});
    regExtern("matrix_eigenvectors", MatTy(), {MatTy()});
    regExtern("matrix_rank", DblTy(), {MatTy()});
    regExtern("matrix_cond", DblTy(), {MatTy()});
    regExtern("matrix_norm", DblTy(), {MatTy(), DblTy()});

    // --- Threading Primitives ---
    // flux_spawn and flux_join use native C pointer types (void*, int64_t),
    // not double — they are declared in SpawnExprAST/JoinExprAST::codegen
    // with the correct LLVM types. Channel functions use opaque double handles.
    regExtern("flux_chan_create", DblTy(), {});
    regExtern("flux_chan_send", FluxType(TypeKind::Void), {DblTy(), DblTy()});
    regExtern("flux_chan_recv", DblTy(), {DblTy()});
    regExtern("flux_chan_close", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_chan_destroy", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_thread_self", DblTy(), {});

    // Mutex
    regExtern("flux_mutex_create", DblTy(), {});
    regExtern("flux_mutex_lock", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_mutex_unlock", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_mutex_destroy", FluxType(TypeKind::Void), {DblTy()});

    // RwLock
    regExtern("flux_rwlock_create", DblTy(), {});
    regExtern("flux_rwlock_read_lock", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_rwlock_write_lock", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_rwlock_unlock", FluxType(TypeKind::Void), {DblTy()});
    regExtern("flux_rwlock_destroy", FluxType(TypeKind::Void), {DblTy()});

    // Pre-declare all registered extern functions with correct LLVM types so that
    // getFunction() in CallExprAST::codegen finds them directly (bypassing the
    // ExternFuncTypes fallback path which can fail during auto-import of stdlib).
    for (const auto& [fnName, fnTypes] : context.ExternFuncTypes) {
        // Skip flux_register_* — they take const char* args in C++ but are registered
        // as double(params) for type inference; their LLVM decl is created by SPICE AST codegen.
        if (fnName.find("flux_register_") == 0)
            continue;
        const auto& retType = fnTypes.first;
        const auto& argTypes = fnTypes.second;
        if (context.TheModule->getFunction(fnName)) {
            continue;
        }
        llvm::Type* RetLTy = (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix)
                                 ? llvm::PointerType::get(context.TheContext, 0)
                                 : ((retType.Kind == TypeKind::Void) ? llvm::Type::getVoidTy(context.TheContext)
                                                                     : retType.getLLVMType(context.TheContext));
        std::vector<llvm::Type*> ArgLTys;
        for (const auto& at : argTypes) {
            if (at.Kind == TypeKind::Matrix || at.Kind == TypeKind::ComplexMatrix)
                ArgLTys.push_back(llvm::PointerType::get(context.TheContext, 0));
            else
                ArgLTys.push_back(at.getLLVMType(context.TheContext));
        }
        llvm::FunctionType* FT = llvm::FunctionType::get(RetLTy, ArgLTys, false);
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, context.TheModule);
    }

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
    regExtern("matrix", MatTy(), {DblTy(), DblTy(), DblTy()});
    returnTypes["matrix"] = FluxType(TypeKind::Matrix);
    injectRet("det", 1, TypeKind::Double);
    regExtern("inv", MatTy(), {MatTy()});
    returnTypes["inv"] = FluxType(TypeKind::Matrix);
    regExtern("eig", MatTy(), {MatTy()});
    returnTypes["eig"] = FluxType(TypeKind::Matrix);
    injectRet("dot", 2, TypeKind::Double);
    regExtern("cross", MatTy(), {MatTy(), MatTy()});
    returnTypes["cross"] = FluxType(TypeKind::Matrix);
    injectRet("norm", 1, TypeKind::Double);
    // rows/cols are defined by stdlib modules (cmatrix, math, etc.)

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

    // Register String methods for obj.method() dispatch
    // Each method wraps the equivalent extern function with double(self, args) signature
    auto registerStringMethod = [&](const std::string& methodName, const std::string& externName, int extraArgs) {
        llvm::Type* DoubleTy = llvm::Type::getDoubleTy(context.TheContext);
        std::vector<llvm::Type*> paramTypes(1 + extraArgs, DoubleTy);
        auto* ft = llvm::FunctionType::get(DoubleTy, paramTypes, false);
        std::string fullName = "String." + methodName;
        auto* func = llvm::Function::Create(ft, llvm::Function::InternalLinkage, fullName, context.TheModule);
        auto* entry = llvm::BasicBlock::Create(context.TheContext, "entry", func);
        context.Builder.SetInsertPoint(entry);
        auto* externFunc = context.TheModule->getFunction(externName);
        if (externFunc) {
            std::vector<llvm::Value*> callArgs;
            for (auto& arg : func->args())
                callArgs.push_back(&arg);
            auto* result = context.Builder.CreateCall(externFunc, callArgs);
            context.Builder.CreateRet(result);
        } else {
            context.Builder.CreateRet(llvm::ConstantFP::get(DoubleTy, 0.0));
        }
        context.TypeMethods["String"][methodName] = func;
        context.FuncReturnTypes[fullName] = FluxType(TypeKind::Double);
    };

    registerStringMethod("len", "flux_strlen", 0);
    registerStringMethod("at", "flux_string_at", 1);
    registerStringMethod("slice", "flux_string_slice", 2);
    registerStringMethod("find", "flux_string_find", 1);
    registerStringMethod("cmp", "flux_strcmp", 1);
    registerStringMethod("concat", "flux_string_concat", 1);
    registerStringMethod("regex", "flux_regex_match", 1);
}

std::string CompilerInstance::resolveImportPath(const std::string& moduleName) const
{
    // Use ModuleLoader's search paths (CWD, modules/, stdlib/, ~/.flux, /usr/share, FLUX_MODULE_PATH)
    auto path = m_moduleLoader.findModule(moduleName);
    if (!path.empty())
        return path.string();

    // Fallback: try basic .flux in CWD (for error message)
    return moduleName + ".flux";
}

bool CompilerInstance::importModule(const std::string& moduleName, CodegenContext& context,
                                    std::map<std::string, FluxType>& returnTypes, std::string* error,
                                    std::map<std::string, bool>& importedModules,
                                    const std::vector<std::string>& symbols) const
{
    if (importedModules.find(moduleName) != importedModules.end())
        return true;
    importedModules[moduleName] = true;

    const auto fileName = resolveImportPath(moduleName);
    // Check if a .flux.bc file exists alongside the source file
    std::string bcFile = fileName;
    auto extPos = bcFile.rfind(".flux");
    if (extPos != std::string::npos) {
        bcFile = bcFile.substr(0, extPos) + ".flux.bc";
    } else {
        bcFile += ".flux.bc";
    }
    if (std::filesystem::exists(bcFile)) {
        return loadAndLinkBitcodeModule(bcFile, context, returnTypes, error);
    }

    auto bufferOrErr = llvm::MemoryBuffer::getFile(fileName);
    if (!bufferOrErr) {
        if (error)
            *error = "Could not open imported file: " + fileName;
        return false;
    }

    Parser importedParser(std::string(bufferOrErr.get()->getBuffer()));
    std::string importError;
    bool result = compileParser(importedParser, context, returnTypes, importError, importedModules, symbols);
    if (!result && error) {
        *error = importError;
    }
    return result;
}

bool CompilerInstance::collectImportFunctions(const std::string& moduleName,
                                              std::vector<std::unique_ptr<FunctionAST>>& outFunctions,
                                              CodegenContext& context, std::map<std::string, FluxType>& returnTypes,
                                              std::string* error, std::map<std::string, bool>& importedModules,
                                              const std::vector<std::string>& symbols,
                                              std::unordered_set<std::string>* knownStructTypeNames,
                                              std::unordered_set<std::string>* knownEnumTypeNames) const
{
    if (importedModules.find(moduleName) != importedModules.end())
        return true;
    importedModules[moduleName] = true;

    const auto fileName = resolveImportPath(moduleName);
    // Check if a .flux.bc file exists alongside the source file
    std::string bcFile = fileName;
    auto extPos = bcFile.rfind(".flux");
    if (extPos != std::string::npos) {
        bcFile = bcFile.substr(0, extPos) + ".flux.bc";
    } else {
        bcFile += ".flux.bc";
    }
    if (std::filesystem::exists(bcFile)) {
        // Load pre-compiled bitcode directly — no FunctionASTs to collect
        return loadAndLinkBitcodeModule(bcFile, context, returnTypes, error);
    }

    auto bufferOrErr = llvm::MemoryBuffer::getFile(fileName);
    if (!bufferOrErr) {
        if (error)
            *error = "Could not open imported file: " + fileName;
        return false;
    }

    Parser parser(std::string(bufferOrErr.get()->getBuffer()));

    // Pre-populate known struct/enum type names from the outer context
    // so that type annotations referring to types from previously imported
    // modules (e.g. struct Vec3 defined in veclib and used in scene) are
    // recognized by the parser even when a nested import is skipped.
    if (knownStructTypeNames) {
        for (const auto& name : *knownStructTypeNames)
            parser.getKnownStructTypeNames().insert(name);
    }
    if (knownEnumTypeNames) {
        for (const auto& name : *knownEnumTypeNames)
            parser.getKnownEnumTypeNames().insert(name);
    }

    std::vector<std::unique_ptr<FunctionAST>> localFunctions;

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_async)) {
            if (auto func = parser.ParseAsyncDef())
                localFunctions.push_back(std::move(func));
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            auto func = parser.ParseDefinition();
            if (func)
                localFunctions.push_back(std::move(func));
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_update)) {
            // Update functions are program-specific, not library code.
            // Skip them in imported modules — they'll be parsed from the
            // main file if present.
            parser.getNextToken();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import) ||
                   parser.CurTok == static_cast<int>(TokenType::tok_from)) {
            auto importAst = (parser.CurTok == static_cast<int>(TokenType::tok_from)) ? parser.ParseFromImport()
                                                                                      : parser.ParseImport();
            if (!importAst) {
                if (error)
                    *error = "Failed to parse import statement";
                return false;
            }
            auto* importExpr = dynamic_cast<ImportExprAST*>(importAst.get());
            if (!importExpr) {
                if (error)
                    *error = "Failed to extract import information";
                return false;
            }
            const std::string& subModuleName = importExpr->getModuleName();
            const std::vector<std::string>& subSymbols = importExpr->getSymbols();
            if (!collectImportFunctions(subModuleName, outFunctions, context, returnTypes, error, importedModules,
                                        subSymbols, knownStructTypeNames, knownEnumTypeNames))
                return false;
            const std::string& alias = importExpr->getAlias().empty() ? subModuleName : importExpr->getAlias();
            context.ModuleNamespaces.insert(alias);
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                returnTypes[proto->getName()] = proto->getReturnType();
                context.FuncReturnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_trait)) {
            if (auto t = parser.ParseTraitDecl()) {
                t->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_struct)) {
            auto structDecl = parser.ParseStructDecl();
            if (structDecl) {
                structDecl->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_class)) {
            std::unique_ptr<StructDeclAST> classStruct;
            std::unique_ptr<ImplDeclAST> classImpl;
            if (parser.ParseClassDecl(&classStruct, &classImpl)) {
                if (classStruct)
                    classStruct->codegen(context);
                if (classImpl) {
                    for (auto& func : localFunctions) {
                        auto* proto = func->getProto();
                        if (!context.TheModule->getFunction(proto->getName())) {
                            context.FuncReturnTypes[proto->getName()] = proto->getReturnType();
                            proto->codegen(context);
                        }
                    }
                    classImpl->codegen(context);
                }
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_enum)) {
            std::vector<std::unique_ptr<StructDeclAST>> anonStructs;
            auto enumDecl = parser.ParseEnumDecl(&anonStructs);
            if (enumDecl) {
                for (auto& s : anonStructs)
                    s->codegen(context);
                enumDecl->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_impl)) {
            auto implDecl = parser.ParseImplDecl();
            if (implDecl) {
                // Ensure all pending top-level function prototypes exist before
                // method body codegen (methods may call top-level functions like
                // keyword_kind, and auto-declaration would guess double return type)
                for (auto& func : localFunctions) {
                    auto* proto = func->getProto();
                    if (!context.TheModule->getFunction(proto->getName())) {
                        context.FuncReturnTypes[proto->getName()] = proto->getReturnType();
                        proto->codegen(context);
                    }
                }
                implDecl->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_eof)) {
            break;
        } else {
            // Skip top-level expressions in imported modules
            // (test code, no anon_expr needed)
            std::vector<std::unique_ptr<ExprAST>> exprs;
            while (parser.CurTok != static_cast<int>(TokenType::tok_eof) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_async) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_def) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_extern) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_struct) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_class) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_enum) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_trait) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_impl) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_import) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_from) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_update) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_rbrace)) {

                if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                    parser.getNextToken();
                    continue;
                }
                if (auto e = parser.ParseExpression()) {
                    // Discard — not needed for library modules
                } else {
                    parser.SkipToSynchronizationPoint();
                }
            }
        }
    }

    // Selective import filtering
    if (!symbols.empty()) {
        std::vector<std::unique_ptr<FunctionAST>> filtered;
        for (auto& func : localFunctions) {
            const std::string& name = func->getProto()->getName();
            if (std::find(symbols.begin(), symbols.end(), name) != symbols.end()) {
                filtered.push_back(std::move(func));
            } else {
                context.ExcludedSymbols.insert(name);
            }
        }
        localFunctions = std::move(filtered);
    }

    // Propagate known struct/enum type names from imported parser
    if (knownStructTypeNames) {
        for (const auto& name : parser.getKnownStructTypeNames())
            knownStructTypeNames->insert(name);
    }
    if (knownEnumTypeNames) {
        for (const auto& name : parser.getKnownEnumTypeNames())
            knownEnumTypeNames->insert(name);
    }

    // Move local functions to the output vector
    for (auto& func : localFunctions)
        outFunctions.push_back(std::move(func));

    return true;
}

std::unique_ptr<ParsedAST> CompilerInstance::parse(const std::string& code, std::string* error) const
{
    auto ast = std::make_unique<ParsedAST>();
    Parser parser(code);

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_async)) {
            if (auto fn = parser.ParseAsyncDef()) {
                ast->functions.push_back(std::move(fn));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
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
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_from)) {
            parser.ParseFromImport(); // Just consume for now
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            parser.ParseExtern(); // Just consume
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_struct)) {
            if (auto s = parser.ParseStructDecl()) {
                ast->structs.push_back(std::move(s));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_class)) {
            std::unique_ptr<StructDeclAST> classStruct;
            std::unique_ptr<ImplDeclAST> classImpl;
            if (parser.ParseClassDecl(&classStruct, &classImpl)) {
                if (classStruct)
                    ast->structs.push_back(std::move(classStruct));
                if (classImpl)
                    ast->impls.push_back(std::move(classImpl));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_enum)) {
            std::vector<std::unique_ptr<StructDeclAST>> anonStructs;
            if (auto e = parser.ParseEnumDecl(&anonStructs)) {
                for (auto& s : anonStructs)
                    ast->structs.push_back(std::move(s));
                ast->enums.push_back(std::move(e));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_impl)) {
            if (auto i = parser.ParseImplDecl()) {
                ast->impls.push_back(std::move(i));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_trait)) {
            if (auto t = parser.ParseTraitDecl()) {
                ast->traits.push_back(std::move(t));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
        } else {
            // Collect expressions into anonymous function
            std::vector<std::unique_ptr<ExprAST>> Exprs;
            while (parser.CurTok != static_cast<int>(TokenType::tok_eof) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_async) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_def) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_subckt) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_model) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_analysis) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_measure) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_extern) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_struct) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_class) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_enum) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_impl) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_trait) &&
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
                FluxType RetType(TypeKind::Double);
                const auto& stmts = Block->getStatements();
                if (!stmts.empty()) {
                    auto* lastExpr = stmts.back().get();
                    if (dynamic_cast<MatrixExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Matrix);
                    else if (dynamic_cast<VectorExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Vector);
                    else if (dynamic_cast<ComplexExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Complex);
                }
                auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                            std::vector<std::pair<std::string, FluxType>>(), RetType);
                ast->functions.push_back(std::make_unique<FunctionAST>(std::move(Proto), std::move(Block)));
            }
        }

        if (parser.hasError())
            break;
    }

    if (error && parser.hasError())
        *error = "Parsing failed";
    return ast;
}

// ---------------------------------------------------------------------------
// Return-type inference for user-defined function bodies.
// Walks the body AST to determine the return type (Matrix, Double, etc.)
// based on called extern functions, last expression in blocks, etc.
// ---------------------------------------------------------------------------
// Forward declarations
static FluxType inferReturnType(const ExprAST* expr,
                                const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
                                const std::map<std::string, FluxType>* userReturnTypes,
                                const std::map<std::string, FluxType>* paramTypes,
                                const std::vector<std::unique_ptr<ExprAST>>* parentStmts);

// Helper: search backward through a statement list for a variable's init
static const ExprAST* findVarInitInStmts(const std::string& varName, const std::vector<std::unique_ptr<ExprAST>>& stmts)
{
    for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
        if (auto* let = dynamic_cast<const LetExprAST*>(it->get())) {
            if (let->getVarName() == varName && let->getInit())
                return let->getInit();
        }
        if (auto* assign = dynamic_cast<const AssignExprAST*>(it->get())) {
            if (auto* lhs = dynamic_cast<const VariableExprAST*>(assign->getLHS())) {
                if (lhs->getName() == varName)
                    return assign->getValueExpr();
            }
        }
    }
    return nullptr;
}

// Find variable init searching local stmts then parent stmts
static const ExprAST* findVarInitInScope(const std::string& varName,
                                         const std::vector<std::unique_ptr<ExprAST>>& localStmts,
                                         const std::vector<std::unique_ptr<ExprAST>>* parentStmts)
{
    const ExprAST* init = findVarInitInStmts(varName, localStmts);
    if (init)
        return init;
    if (parentStmts) {
        init = findVarInitInStmts(varName, *parentStmts);
        if (init)
            return init;
    }
    return nullptr;
}

// Get parameter type, defaulting to Double
static FluxType paramTypeOf(const std::string& name, const std::map<std::string, FluxType>* paramTypes)
{
    if (paramTypes) {
        auto it = paramTypes->find(name);
        if (it != paramTypes->end())
            return it->second;
    }
    return TypeKind::Double;
}

static FluxType inferIfExprReturnType(
    const IfExprAST* ife, const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
    const std::map<std::string, FluxType>* userReturnTypes, const std::map<std::string, FluxType>* paramTypes,
    const std::vector<std::unique_ptr<ExprAST>>* parentStmts)
{
    auto infer = [&](const ExprAST* e) {
        return inferReturnType(e, externTypes, userReturnTypes, paramTypes, parentStmts);
    };
    FluxType thenRet = infer(ife->getThen());
    const ExprAST* elseExpr = ife->getElse();
    if (elseExpr) {
        FluxType elseRet = infer(elseExpr);
        if (thenRet.Kind == TypeKind::Double && elseRet.Kind != TypeKind::Double)
            return elseRet;
        if (elseRet.Kind == TypeKind::Double && thenRet.Kind != TypeKind::Double)
            return thenRet;
    }
    return thenRet;
}

static FluxType inferIfStmtReturnType(
    const IfStmtAST* ifStmt, const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
    const std::map<std::string, FluxType>* userReturnTypes, const std::map<std::string, FluxType>* paramTypes,
    const std::vector<std::unique_ptr<ExprAST>>* parentStmts)
{
    auto infer = [&](const ExprAST* e) {
        return inferReturnType(e, externTypes, userReturnTypes, paramTypes, parentStmts);
    };
    const auto& thenBody = ifStmt->getThenBody();
    if (thenBody.empty())
        return TypeKind::Double;
    const ExprAST* last = thenBody.back().get();
    if (auto* var = dynamic_cast<const VariableExprAST*>(last)) {
        const ExprAST* init = findVarInitInScope(var->getName(), thenBody, parentStmts);
        if (init)
            return infer(init);
        FluxType pt = paramTypeOf(var->getName(), paramTypes);
        if (pt.Kind != TypeKind::Double)
            return pt;
    }
    return infer(last);
}

static FluxType inferReturnType(const ExprAST* expr,
                                const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
                                const std::map<std::string, FluxType>* userReturnTypes,
                                const std::map<std::string, FluxType>* paramTypes,
                                const std::vector<std::unique_ptr<ExprAST>>* parentStmts)
{
    if (!expr)
        return TypeKind::Double;

    // CallExprAST: look up the callee in known extern function types
    if (auto* call = dynamic_cast<const CallExprAST*>(expr)) {
        std::string name = call->getCallee();
        auto sepPos = name.rfind("::");
        if (sepPos != std::string::npos)
            name = name.substr(sepPos + 2);
        auto it = externTypes.find(name);
        if (it != externTypes.end()) {
            return it->second.first;
        }
        if (userReturnTypes) {
            auto uit = userReturnTypes->find(name);
            if (uit != userReturnTypes->end())
                return uit->second;
        }
        return TypeKind::Double;
    }

    auto withUser = [&](const ExprAST* e) {
        return inferReturnType(e, externTypes, userReturnTypes, paramTypes, parentStmts);
    };

    // BlockExprAST: use the type of the last statement
    if (auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
        const auto& stmts = block->getStatements();
        if (stmts.empty())
            return TypeKind::Double;

        // When descending into sub-expressions of this block,
        // pass its stmts as parentStmts for findVarInit.
        auto withBlockStmts = [&](const ExprAST* e) {
            return inferReturnType(e, externTypes, userReturnTypes, paramTypes, &stmts);
        };

        auto inferLast = [&](const ExprAST* last) -> FluxType {
            if (auto* ret = dynamic_cast<const ReturnExprAST*>(last)) {
                const ExprAST* retVal = ret->getVal();
                if (auto* var = dynamic_cast<const VariableExprAST*>(retVal)) {
                    const ExprAST* init = findVarInitInScope(var->getName(), stmts, parentStmts);
                    if (init)
                        return withBlockStmts(init);
                    FluxType pt = paramTypeOf(var->getName(), paramTypes);
                    if (pt.Kind != TypeKind::Double)
                        return pt;
                }
                return withBlockStmts(retVal);
            }
            if (auto* var = dynamic_cast<const VariableExprAST*>(last)) {
                const ExprAST* init = findVarInitInScope(var->getName(), stmts, parentStmts);
                if (init)
                    return withBlockStmts(init);
                FluxType pt = paramTypeOf(var->getName(), paramTypes);
                if (pt.Kind != TypeKind::Double)
                    return pt;
            }
            // For IfExprAST/IfStmtAST as the last statement, we need to descend with
            // the outer block's stmts for findVarInit to work in then/else branches.
            if (auto* ife = dynamic_cast<const IfExprAST*>(last))
                return inferIfExprReturnType(ife, externTypes, userReturnTypes, paramTypes, &stmts);
            if (auto* ifStmt = dynamic_cast<const IfStmtAST*>(last))
                return inferIfStmtReturnType(ifStmt, externTypes, userReturnTypes, paramTypes, &stmts);
            return withBlockStmts(last);
        };
        return inferLast(stmts.back().get());
    }

    // Handle bare VariableExprAST (reached from nested contexts)
    if (auto* var = dynamic_cast<const VariableExprAST*>(expr)) {
        if (parentStmts) {
            const ExprAST* init = findVarInitInStmts(var->getName(), *parentStmts);
            if (init)
                return withUser(init);
        }
        FluxType pt = paramTypeOf(var->getName(), paramTypes);
        if (pt.Kind != TypeKind::Double)
            return pt;
        return TypeKind::Double;
    }

    if (auto* ife = dynamic_cast<const IfExprAST*>(expr))
        return inferIfExprReturnType(ife, externTypes, userReturnTypes, paramTypes, parentStmts);

    if (auto* ifStmt = dynamic_cast<const IfStmtAST*>(expr))
        return inferIfStmtReturnType(ifStmt, externTypes, userReturnTypes, paramTypes, parentStmts);

    if (auto* ret = dynamic_cast<const ReturnExprAST*>(expr))
        return withUser(ret->getVal());

    if (auto* unary = dynamic_cast<const UnaryExprAST*>(expr))
        return withUser(unary->getOperand());

    if (auto* bin = dynamic_cast<const BinaryExprAST*>(expr)) {
        if (bin->getOp() == 'm')
            return FluxType(TypeKind::Matrix);
        return TypeKind::Double;
    }

    if (dynamic_cast<const VectorExprAST*>(expr))
        return FluxType(TypeKind::Vector);
    if (dynamic_cast<const MatrixExprAST*>(expr))
        return FluxType(TypeKind::Matrix);
    if (dynamic_cast<const ComplexExprAST*>(expr))
        return FluxType(TypeKind::Complex);

    if (auto* structCtor = dynamic_cast<const StructConstructExprAST*>(expr)) {
        FluxType retType(TypeKind::UserStruct);
        retType.StructTypeId = -1;
        retType.StructLLVMType = nullptr;
        retType.GenericName = structCtor->getStructName();
        return retType;
    }

    return TypeKind::Double;
}

// Recursive helper: find a call to a struct/enum-returning function and
// store its return type. Stops early once a user struct/enum type is found.
static void findStructReturnCalls(const ExprAST* expr, const std::map<std::string, FluxType>& funcReturnTypes,
                                  FluxType& outRetType)
{
    if (!expr || outRetType.Kind == TypeKind::UserStruct || outRetType.Kind == TypeKind::UserEnum)
        return;

    if (auto* call = dynamic_cast<const CallExprAST*>(expr)) {
        std::string name = call->getCallee();
        auto sepPos = name.rfind("::");
        if (sepPos != std::string::npos)
            name = name.substr(sepPos + 2);
        auto ftIt = funcReturnTypes.find(name);
        if (ftIt != funcReturnTypes.end() &&
            (ftIt->second.Kind == TypeKind::UserStruct || ftIt->second.Kind == TypeKind::UserEnum)) {
            outRetType = ftIt->second;
            return;
        }
        for (const auto& arg : call->getArgs())
            findStructReturnCalls(arg.get(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
        for (const auto& stmt : block->getStatements()) {
            findStructReturnCalls(stmt.get(), funcReturnTypes, outRetType);
            if (outRetType.Kind == TypeKind::UserStruct || outRetType.Kind == TypeKind::UserEnum)
                return;
        }
        return;
    }
    if (auto* let = dynamic_cast<const LetExprAST*>(expr)) {
        if (let->getInit())
            findStructReturnCalls(let->getInit(), funcReturnTypes, outRetType);
        if (outRetType.Kind != TypeKind::UserStruct && outRetType.Kind != TypeKind::UserEnum && let->getBody())
            findStructReturnCalls(let->getBody(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* ife = dynamic_cast<const IfExprAST*>(expr)) {
        findStructReturnCalls(ife->getCond(), funcReturnTypes, outRetType);
        findStructReturnCalls(ife->getThen(), funcReturnTypes, outRetType);
        if (ife->getElse())
            findStructReturnCalls(ife->getElse(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* ifStmt = dynamic_cast<const IfStmtAST*>(expr)) {
        findStructReturnCalls(ifStmt->getCond(), funcReturnTypes, outRetType);
        for (const auto& stmt : ifStmt->getThenBody())
            findStructReturnCalls(stmt.get(), funcReturnTypes, outRetType);
        for (const auto& stmt : ifStmt->getElseBody())
            findStructReturnCalls(stmt.get(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* bin = dynamic_cast<const BinaryExprAST*>(expr)) {
        findStructReturnCalls(bin->getLHS(), funcReturnTypes, outRetType);
        findStructReturnCalls(bin->getRHS(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* unary = dynamic_cast<const UnaryExprAST*>(expr)) {
        findStructReturnCalls(unary->getOperand(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* member = dynamic_cast<const MemberExprAST*>(expr)) {
        findStructReturnCalls(member->getObject(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* index = dynamic_cast<const IndexExprAST*>(expr)) {
        findStructReturnCalls(index->getArray(), funcReturnTypes, outRetType);
        if (index->getRowIndex())
            findStructReturnCalls(index->getRowIndex(), funcReturnTypes, outRetType);
        if (index->getColIndex())
            findStructReturnCalls(index->getColIndex(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* assign = dynamic_cast<const AssignExprAST*>(expr)) {
        findStructReturnCalls(assign->getValueExpr(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* ret = dynamic_cast<const ReturnExprAST*>(expr)) {
        findStructReturnCalls(ret->getVal(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* forExpr = dynamic_cast<const ForExprAST*>(expr)) {
        findStructReturnCalls(forExpr->getStart(), funcReturnTypes, outRetType);
        findStructReturnCalls(forExpr->getEnd(), funcReturnTypes, outRetType);
        findStructReturnCalls(forExpr->getBody(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* whileExpr = dynamic_cast<const WhileExprAST*>(expr)) {
        findStructReturnCalls(whileExpr->getCond(), funcReturnTypes, outRetType);
        findStructReturnCalls(whileExpr->getBody(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* switchExpr = dynamic_cast<const SwitchExprAST*>(expr)) {
        if (switchExpr->getCondition())
            findStructReturnCalls(switchExpr->getCondition(), funcReturnTypes, outRetType);
        for (const auto& caseClause : switchExpr->getCases()) {
            if (caseClause.getValue())
                findStructReturnCalls(caseClause.getValue(), funcReturnTypes, outRetType);
            for (const auto& stmt : caseClause.getBody())
                findStructReturnCalls(stmt.get(), funcReturnTypes, outRetType);
        }
        for (const auto& stmt : switchExpr->getDefaultBody())
            findStructReturnCalls(stmt.get(), funcReturnTypes, outRetType);
        return;
    }
    if (auto* vector = dynamic_cast<const VectorExprAST*>(expr)) {
        for (const auto& elem : vector->getElements())
            findStructReturnCalls(elem.get(), funcReturnTypes, outRetType);
        return;
    }
}

// Recursive helper: visit an expression and record which variables are used
// as matrix arguments to extern calls or user-defined functions.
static void visitExprForParamInference(
    const ExprAST* expr, const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
    const std::map<std::string, std::vector<FluxType>>& userFuncParamTypes, std::map<std::string, FluxType>& paramTypes)
{

    if (!expr)
        return;

    // CallExprAST: check each argument against extern or user function param types
    if (auto* call = dynamic_cast<const CallExprAST*>(expr)) {
        std::string name = call->getCallee();
        auto sepPos = name.rfind("::");
        if (sepPos != std::string::npos)
            name = name.substr(sepPos + 2);
        // Check extern functions first
        auto eit = externTypes.find(name);
        if (eit != externTypes.end()) {
            const auto& argTypes = eit->second.second;
            const auto& args = call->getArgs();
            for (size_t i = 0; i < args.size() && i < argTypes.size(); ++i) {
                if (argTypes[i].Kind == TypeKind::Matrix) {
                    if (auto* var = dynamic_cast<const VariableExprAST*>(args[i].get())) {
                        auto pt = paramTypes.find(var->getName());
                        if (pt != paramTypes.end() && pt->second.Kind == TypeKind::Double) {
                            pt->second = FluxType(TypeKind::Matrix);
                        }
                    }
                }
            }
        } else {
            // Check user-defined functions (already processed earlier in the module)
            auto uit = userFuncParamTypes.find(name);
            if (uit != userFuncParamTypes.end()) {
                const auto& argTypes = uit->second;
                const auto& args = call->getArgs();
                for (size_t i = 0; i < args.size() && i < argTypes.size(); ++i) {
                    if (argTypes[i].Kind == TypeKind::Matrix) {
                        if (auto* var = dynamic_cast<const VariableExprAST*>(args[i].get())) {
                            auto pt = paramTypes.find(var->getName());
                            if (pt != paramTypes.end() && pt->second.Kind == TypeKind::Double) {
                                pt->second = FluxType(TypeKind::Matrix);
                            }
                        }
                    }
                }
            }
        }
        // Recurse into arguments
        for (const auto& arg : call->getArgs())
            visitExprForParamInference(arg.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // BlockExprAST
    if (auto* block = dynamic_cast<const BlockExprAST*>(expr)) {
        for (const auto& stmt : block->getStatements())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // IfExprAST
    if (auto* ife = dynamic_cast<const IfExprAST*>(expr)) {
        visitExprForParamInference(ife->getCond(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(ife->getThen(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(ife->getElse(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // IfStmtAST
    if (auto* ifs = dynamic_cast<const IfStmtAST*>(expr)) {
        visitExprForParamInference(ifs->getCond(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& stmt : ifs->getThenBody())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& stmt : ifs->getElseBody())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // BinaryExprAST
    if (auto* bin = dynamic_cast<const BinaryExprAST*>(expr)) {
        visitExprForParamInference(bin->getLHS(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(bin->getRHS(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // UnaryExprAST
    if (auto* unary = dynamic_cast<const UnaryExprAST*>(expr)) {
        visitExprForParamInference(unary->getOperand(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // LetExprAST
    if (auto* let = dynamic_cast<const LetExprAST*>(expr)) {
        if (let->getInit())
            visitExprForParamInference(let->getInit(), externTypes, userFuncParamTypes, paramTypes);
        if (let->getBody())
            visitExprForParamInference(let->getBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // AssignExprAST
    if (auto* assign = dynamic_cast<const AssignExprAST*>(expr)) {
        visitExprForParamInference(assign->getLHS(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(assign->getValueExpr(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // WhileExprAST
    if (auto* w = dynamic_cast<const WhileExprAST*>(expr)) {
        visitExprForParamInference(w->getCond(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(w->getBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // ForExprAST
    if (auto* f = dynamic_cast<const ForExprAST*>(expr)) {
        visitExprForParamInference(f->getStart(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(f->getEnd(), externTypes, userFuncParamTypes, paramTypes);
        if (f->getStep())
            visitExprForParamInference(f->getStep(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(f->getBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // ForStmtAST (C-style for)
    if (auto* fs = dynamic_cast<const ForStmtAST*>(expr)) {
        if (fs->getInit())
            visitExprForParamInference(fs->getInit(), externTypes, userFuncParamTypes, paramTypes);
        if (fs->getCond())
            visitExprForParamInference(fs->getCond(), externTypes, userFuncParamTypes, paramTypes);
        if (fs->getStep())
            visitExprForParamInference(fs->getStep(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& stmt : fs->getBody())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // WhileStmtAST
    if (auto* ws = dynamic_cast<const WhileStmtAST*>(expr)) {
        visitExprForParamInference(ws->getCond(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& stmt : ws->getBody())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // DoWhileExprAST
    if (auto* dw = dynamic_cast<const DoWhileExprAST*>(expr)) {
        visitExprForParamInference(dw->getBody(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(dw->getCond(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // RepeatUntilExprAST
    if (auto* ru = dynamic_cast<const RepeatUntilExprAST*>(expr)) {
        visitExprForParamInference(ru->getBody(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(ru->getCond(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // ForeachExprAST
    if (auto* fe = dynamic_cast<const ForeachExprAST*>(expr)) {
        visitExprForParamInference(fe->getIterable(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(fe->getBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // ParallelForExprAST
    if (auto* pf = dynamic_cast<const ParallelForExprAST*>(expr)) {
        visitExprForParamInference(pf->getStart(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(pf->getEnd(), externTypes, userFuncParamTypes, paramTypes);
        visitExprForParamInference(pf->getBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // SwitchExprAST
    if (auto* sw = dynamic_cast<const SwitchExprAST*>(expr)) {
        visitExprForParamInference(sw->getCondition(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& c : sw->getCases()) {
            visitExprForParamInference(c.getValue(), externTypes, userFuncParamTypes, paramTypes);
            for (const auto& stmt : c.getBody())
                visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        }
        for (const auto& stmt : sw->getDefaultBody())
            visitExprForParamInference(stmt.get(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // MatchExprAST
    if (auto* ma = dynamic_cast<const MatchExprAST*>(expr)) {
        visitExprForParamInference(ma->getValue(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& arm : ma->getArms())
            visitExprForParamInference(arm.second.get(), externTypes, userFuncParamTypes, paramTypes);
        if (ma->getDefaultArm())
            visitExprForParamInference(ma->getDefaultArm(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }

    // TryCatchExprAST
    if (auto* tc = dynamic_cast<const TryCatchExprAST*>(expr)) {
        visitExprForParamInference(tc->getTryBody(), externTypes, userFuncParamTypes, paramTypes);
        for (const auto& clause : tc->getCatchClauses())
            visitExprForParamInference(clause.second.get(), externTypes, userFuncParamTypes, paramTypes);
        if (tc->getFinallyBody())
            visitExprForParamInference(tc->getFinallyBody(), externTypes, userFuncParamTypes, paramTypes);
        return;
    }
}

static void inferParamTypes(const std::vector<std::unique_ptr<FunctionAST>>& functions,
                            const std::map<std::string, std::pair<FluxType, std::vector<FluxType>>>& externTypes,
                            std::map<std::string, std::vector<FluxType>>& crossModuleParamTypes)
{

    // Map of user function name → parameter types, built incrementally
    // as we process functions in definition order (callees before callers).
    // Start with param types from imported modules so cross-module calls
    // (e.g., circuit_param from acswp.flux) correctly infer Matrix params.
    std::map<std::string, std::vector<FluxType>> userFuncParamTypes = crossModuleParamTypes;

    for (auto& func : functions) {
        auto* proto = func->getProto();
        if (!proto)
            continue;
        // Build map of parameter name -> current type
        std::map<std::string, FluxType> paramTypes;
        for (const auto& arg : proto->getArgs())
            paramTypes[arg.first] = arg.second;

        // Scan body to find parameters used as matrix arguments
        visitExprForParamInference(func->getBody(), externTypes, userFuncParamTypes, paramTypes);

        // Update prototype with inferred types
        std::vector<FluxType> inferredParamTypes;
        for (size_t i = 0; i < proto->getArgs().size(); ++i) {
            const auto& arg = proto->getArgs()[i];
            auto it = paramTypes.find(arg.first);
            if (it != paramTypes.end() && it->second.Kind != arg.second.Kind)
                proto->setArgType(i, it->second);
            inferredParamTypes.push_back(proto->getArgs()[i].second);
        }

        // Record for subsequent functions
        userFuncParamTypes[proto->getName()] = std::move(inferredParamTypes);
    }

    // Store updated types back into cross-module map for subsequent modules.
    crossModuleParamTypes.merge(userFuncParamTypes);
}

bool CompilerInstance::compileParser(Parser& parser, CodegenContext& context,
                                     std::map<std::string, FluxType>& returnTypes, std::string& error,
                                     std::map<std::string, bool>& importedModules,
                                     const std::vector<std::string>& symbols,
                                     std::vector<std::unique_ptr<FunctionAST>>* preParsedFunctions) const
{
    // --- Inject built-in generic enum type names for parser recognition ---
    parser.getKnownEnumTypeNames().insert("Result");
    parser.getKnownEnumTypeNames().insert("Option");

    // --- Pass 1: Parse all definitions (defer prototype declaration) ---
    // Prototype declaration is deferred until after selective-import
    // filtering so that only selected functions get LLVM declarations.
    std::vector<std::unique_ptr<FunctionAST>> functions;
    std::unique_ptr<ExprAST> updateFunc;
    bool hasUpdateFunc = false;
    std::vector<std::unique_ptr<StructDeclAST>> structs;
    std::vector<std::unique_ptr<EnumDeclAST>> enums;
    std::vector<std::unique_ptr<ImplDeclAST>> impls;

    while (parser.CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (parser.CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            parser.getNextToken();
            continue;
        }

        if (parser.CurTok == static_cast<int>(TokenType::tok_async)) {
            if (auto func = parser.ParseAsyncDef()) {
                functions.push_back(std::move(func));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_def)) {
            auto func = parser.ParseDefinition();
            if (func) {
                functions.push_back(std::move(func));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_update)) {
            auto func = parser.ParseUpdateFunc();
            if (func) {
                hasUpdateFunc = true;
                updateFunc = std::move(func);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_import) ||
                   parser.CurTok == static_cast<int>(TokenType::tok_from)) {
            auto importAst = (parser.CurTok == static_cast<int>(TokenType::tok_from)) ? parser.ParseFromImport()
                                                                                      : parser.ParseImport();
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
            const std::vector<std::string>& importSymbols = importExpr->getSymbols();
            if (!collectImportFunctions(moduleName, functions, context, returnTypes, &error, importedModules,
                                        importSymbols, &parser.getKnownStructTypeNames(),
                                        &parser.getKnownEnumTypeNames())) {
                return false;
            }
            const std::string& alias = importExpr->getAlias().empty() ? moduleName : importExpr->getAlias();
            context.ModuleNamespaces.insert(alias);
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_extern)) {
            auto proto = parser.ParseExtern();
            if (proto) {
                returnTypes[proto->getName()] = proto->getReturnType();
                context.FuncReturnTypes[proto->getName()] = proto->getReturnType();
                proto->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_trait)) {
            if (auto t = parser.ParseTraitDecl()) {
                t->codegen(context);
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_struct)) {
            if (auto s = parser.ParseStructDecl()) {
                structs.push_back(std::move(s));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_class)) {
            std::unique_ptr<StructDeclAST> classStruct;
            std::unique_ptr<ImplDeclAST> classImpl;
            if (parser.ParseClassDecl(&classStruct, &classImpl)) {
                if (classStruct)
                    structs.push_back(std::move(classStruct));
                if (classImpl)
                    impls.push_back(std::move(classImpl));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_enum)) {
            std::vector<std::unique_ptr<StructDeclAST>> anonStructs;
            if (auto e = parser.ParseEnumDecl(&anonStructs)) {
                for (auto& s : anonStructs)
                    structs.push_back(std::move(s));
                enums.push_back(std::move(e));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_impl)) {
            if (auto i = parser.ParseImplDecl()) {
                impls.push_back(std::move(i));
            }
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_semicolon)) {
            parser.getNextToken();
        } else if (parser.CurTok == static_cast<int>(TokenType::tok_eof)) {
            break;
        } else {
            // Collect consecutive expressions into a single anonymous function
            std::vector<std::unique_ptr<ExprAST>> Exprs;
            while (parser.CurTok != static_cast<int>(TokenType::tok_eof) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_async) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_def) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_extern) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_struct) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_class) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_enum) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_trait) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_impl) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_import) &&
                   parser.CurTok != static_cast<int>(TokenType::tok_from) &&
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
                FluxType RetType(TypeKind::Double);
                const auto& stmts = Block->getStatements();
                if (!stmts.empty()) {
                    auto* lastExpr = stmts.back().get();
                    if (dynamic_cast<MatrixExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Matrix);
                    else if (dynamic_cast<VectorExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Vector);
                    else if (dynamic_cast<ComplexExprAST*>(lastExpr))
                        RetType = FluxType(TypeKind::Complex);
                }
                static unsigned anonCounter = 0;
                std::string anonName = "__anon_expr";
                if (!m_options.moduleName.empty() && m_options.moduleName != "Flux Module") {
                    anonName = m_options.moduleName + "_anon_expr";
                }
                anonName += "_" + std::to_string(anonCounter++);
                auto Proto =
                    std::make_unique<PrototypeAST>(anonName, std::vector<std::pair<std::string, FluxType>>(), RetType);
                functions.push_back(std::make_unique<FunctionAST>(std::move(Proto), std::move(Block)));
            }
        }
    }

    // --- Inject built-in generic enums (Result, Option) and synthetic payload structs ---
    {
        bool userDeclaredResult = false;
        bool userDeclaredOption = false;
        for (const auto& e : enums) {
            if (e->getName() == "Result")
                userDeclaredResult = true;
            if (e->getName() == "Option")
                userDeclaredOption = true;
        }
        if (!userDeclaredResult) {
            FluxType tGen(TypeKind::Generic);
            tGen.GenericName = "T";
            FluxType eGen(TypeKind::Generic);
            eGen.GenericName = "E";
            // Synthetic structs for braced constructor syntax: Result.Ok { value: v }
            auto resultOkFields = std::make_unique<StructDeclAST>(
                "__enum_Result_Ok_Fields", std::vector<std::pair<std::string, FluxType>>{{"value", tGen}});
            auto resultErrFields = std::make_unique<StructDeclAST>(
                "__enum_Result_Err_Fields", std::vector<std::pair<std::string, FluxType>>{{"value", eGen}});
            parser.getKnownStructTypeNames().insert("__enum_Result_Ok_Fields");
            parser.getKnownStructTypeNames().insert("__enum_Result_Err_Fields");
            structs.push_back(std::move(resultOkFields));
            structs.push_back(std::move(resultErrFields));
            auto resultEnum = std::make_unique<EnumDeclAST>("Result", std::vector<std::string>{"Ok", "Err"},
                                                            std::vector<FluxType>{tGen, eGen});
            resultEnum->setGenericParams({"T", "E"});
            enums.push_back(std::move(resultEnum));
        }
        if (!userDeclaredOption) {
            FluxType tGen(TypeKind::Generic);
            tGen.GenericName = "T";
            // Synthetic struct for braced constructor syntax: Option.Some { value: v }
            auto optionSomeFields = std::make_unique<StructDeclAST>(
                "__enum_Option_Some_Fields", std::vector<std::pair<std::string, FluxType>>{{"value", tGen}});
            parser.getKnownStructTypeNames().insert("__enum_Option_Some_Fields");
            structs.push_back(std::move(optionSomeFields));
            auto optionEnum = std::make_unique<EnumDeclAST>("Option", std::vector<std::string>{"Some", "None"},
                                                            std::vector<FluxType>{tGen, FluxType(TypeKind::Void)});
            optionEnum->setGenericParams({"T"});
            enums.push_back(std::move(optionEnum));
        }
        // Register all generic enums so the generic-enum infrastructure finds them
        for (auto& e : enums) {
            if (e->isGeneric()) {
                context.GenericEnums[e->getName()] = e.get();
            }
        }
    }

    // --- Inject synthetic payload structs for enum variants with bare payloads ---
    // For braced constructor syntax (e.g., MyEnum.Variant { value: expr }),
    // we need __enum_<Enum>_<Variant>_Fields to exist in the struct registry.
    // Variants declared with named fields (e.g., Some { value: T }) are already
    // handled by ParseEnumDecl; bare payload variants (e.g., Some(T)) need injection.
    for (const auto& e : enums) {
        const auto& variants = e->getVariants();
        const auto& payloads = e->getVariantPayloads();
        for (size_t i = 0; i < variants.size(); ++i) {
            if (payloads[i].Kind != TypeKind::Void && payloads[i].Kind != TypeKind::UserStruct) {
                std::string anonName = "__enum_" + e->getName() + "_" + variants[i] + "_Fields";
                if (!parser.getKnownStructTypeNames().count(anonName)) {
                    parser.getKnownStructTypeNames().insert(anonName);
                    structs.push_back(std::make_unique<StructDeclAST>(
                        anonName, std::vector<std::pair<std::string, FluxType>>{{"value", payloads[i]}}));
                }
            }
        }
    }

    // Process struct declarations first so anonymous enum payload
    // struct types are registered before enum codegen references them.
    for (auto& s : structs) {
        s->codegen(context);
    }

    // Pre-populate GenericStructs so enum codegen below can specialize
    // generic struct payloads (e.g., Option.Some(Pair[Double, Double])).
    for (auto& s : structs) {
        if (s->isGeneric()) {
            context.GenericStructs[s->getName()] = s.get();
        }
    }

    // Process enum declarations: register enum types
    for (auto& e : enums) {
        e->codegen(context);
    }

    // Process impl declarations: register type methods
    for (auto& i : impls) {
        i->codegen(context);
    }

    // Merge pre-parsed functions (from auto-imported stdlib modules)
    // before local functions so callees precede callers.
    std::unordered_set<std::string> importedNames;
    if (preParsedFunctions) {
        for (const auto& f : *preParsedFunctions) {
            if (f && f->getProto()) {
                importedNames.insert(f->getProto()->getName());
            }
        }
        functions.insert(functions.begin(), std::make_move_iterator(preParsedFunctions->begin()),
                         std::make_move_iterator(preParsedFunctions->end()));
    }

    // --- Deduplicate functions ---
    // Pre-parsed (stdlib) functions are before user functions in the vector.
    // When a user function has the same name as a pre-parsed function,
    // overwrite the pre-parsed function in-place so that the original topological
    // order of functions (crucial for parameter type inference) is preserved.
    {
        std::vector<std::unique_ptr<FunctionAST>> deduped;
        std::map<std::string, size_t> nameToSlot;
        for (auto& func : functions) {
            if (!func)
                continue;
            std::string name = func->getProto()->getName();
            auto it = nameToSlot.find(name);
            if (it == nameToSlot.end()) {
                nameToSlot[name] = deduped.size();
                deduped.push_back(std::move(func));
            } else {
                std::cerr << "[Flux] Warning: duplicate function '" << name << "' — keeping the last definition\n";
                deduped[it->second] = std::move(func);
            }
        }
        functions = std::move(deduped);
    }

    // --- Selective import filtering ---
    if (!symbols.empty()) {
        std::vector<std::unique_ptr<FunctionAST>> filtered;
        for (auto& func : functions) {
            const std::string& name = func->getProto()->getName();
            if (std::find(symbols.begin(), symbols.end(), name) != symbols.end()) {
                filtered.push_back(std::move(func));
            } else {
                context.ExcludedSymbols.insert(name);
            }
        }
        functions = std::move(filtered);
    }

    // --- Parameter-type inference ---
    // Infer parameter types (Matrix, etc.) by scanning the body for extern
    // or user-function calls that use parameters as matrix arguments.
    // Functions are processed in definition order (callees before callers)
    // so that a function can see its callees' already-inferred param types.
    inferParamTypes(functions, context.ExternFuncTypes, context.CrossModuleParamTypes);

    // --- Separate generic functions ---
    // Generic functions are stored in the codegen context and compiled
    // on demand when a call with concrete type arguments is encountered.
    // Remove them from the main functions vector to skip the standard
    // codegen pipeline (type inference based on generic types is invalid).
    {
        std::vector<std::unique_ptr<FunctionAST>> nonGeneric;
        nonGeneric.reserve(functions.size());
        for (auto& func : functions) {
            if (func->getProto()->isGeneric()) {
                const std::string& name = func->getProto()->getName();
                context.GenericFunctions[name] = func.get();
                returnTypes[name] = func->getProto()->getReturnType();
                context.FuncReturnTypes[name] = func->getProto()->getReturnType();
                // Ownership held by GenericDefs vector
                context.GenericFunctionDefs.push_back(std::move(func));
            } else {
                nonGeneric.push_back(std::move(func));
            }
        }
        functions = std::move(nonGeneric);
    }

    // --- Separate generic structs ---
    {
        std::vector<std::unique_ptr<StructDeclAST>> nonGeneric;
        nonGeneric.reserve(structs.size());
        for (auto& s : structs) {
            if (s->isGeneric()) {
                const std::string& name = s->getName();
                context.GenericStructs[name] = s.get();
                context.GenericStructDefs.push_back(std::move(s));
            } else {
                nonGeneric.push_back(std::move(s));
            }
        }
        structs = std::move(nonGeneric);
    }

    // --- Separate generic enums ---
    {
        std::vector<std::unique_ptr<EnumDeclAST>> nonGeneric;
        nonGeneric.reserve(enums.size());
        for (auto& e : enums) {
            if (e->isGeneric()) {
                const std::string& name = e->getName();
                context.GenericEnums[name] = e.get();
                context.GenericEnumDefs.push_back(std::move(e));
            } else {
                nonGeneric.push_back(std::move(e));
            }
        }
        enums = std::move(nonGeneric);
    }

    // --- Separate generic impls ---
    {
        std::vector<std::unique_ptr<ImplDeclAST>> nonGenericImpls;
        nonGenericImpls.reserve(impls.size());
        for (auto& i : impls) {
            const std::string& name = i->getTypeName();
            if (context.GenericStructs.count(name)) {
                context.GenericImpls[name] = i.get();
                context.GenericImplDefs.push_back(std::move(i));
            } else {
                nonGenericImpls.push_back(std::move(i));
            }
        }
        impls = std::move(nonGenericImpls);
    }

    // --- Return-type inference ---
    // Walk each function body to determine the actual return type
    // (Matrix, Double, etc.) before declaring the LLVM prototype.
    size_t numFunctions = functions.size();
    auto& progCb = m_options.progressCallback;
    if (progCb && !progCb("Inferring return types", 0, numFunctions))
        return false;
    for (size_t fi = 0; fi < numFunctions; ++fi) {
        auto& func = functions[fi];
        const std::string& fname = func->getProto()->getName();

        // Build parameter name -> type map from the prototype
        std::map<std::string, FluxType> paramTypeMap;
        for (auto& arg : func->getProto()->getArgs())
            paramTypeMap[arg.first] = arg.second;
        FluxType inferred =
            inferReturnType(func->getBody(), context.ExternFuncTypes, &context.FuncReturnTypes, &paramTypeMap, nullptr);
        if (inferred.Kind == TypeKind::Matrix || inferred.Kind == TypeKind::ComplexMatrix ||
            inferred.Kind == TypeKind::Vector || inferred.Kind == TypeKind::UserStruct ||
            inferred.Kind == TypeKind::UserEnum) {
            func->getProto()->setReturnType(inferred);
        }
        // Update return type map immediately so subsequent functions
        // (e.g. anonymous expressions calling this function) see the correct type.
        const std::string& name = func->getProto()->getName();
        returnTypes[name] = func->getProto()->getReturnType();
        context.FuncReturnTypes[name] = func->getProto()->getReturnType();
        if (progCb && !progCb("Inferring return types", fi + 1, numFunctions))
            return false;
    }

    // --- Pass 1 (continued): Declare prototypes for remaining functions ---
    if (progCb && !progCb("Declaring prototypes", 0, numFunctions))
        return false;
    for (size_t fi = 0; fi < numFunctions; ++fi) {
        auto& func = functions[fi];
        const std::string& name = func->getProto()->getName();
        llvm::Function* LF = func->getProto()->codegen(context);
        if (LF && importedNames.count(name)) {
            LF->setLinkage(llvm::GlobalValue::InternalLinkage);
        }
        returnTypes[name] = func->getProto()->getReturnType();
        context.FuncReturnTypes[name] = func->getProto()->getReturnType();
        if (progCb && !progCb("Declaring prototypes", fi + 1, numFunctions))
            return false;
    }

    // --- Fix up anonymous function return types for struct/enum returns ---
    // Only consider the LAST expression of the block; intermediate let/if/while
    // statements that call struct/enum-returning functions should NOT override
    // the anon return type.
    for (auto& func : functions) {
        const std::string& name = func->getProto()->getName();
        if (name.find("anon_expr") == std::string::npos)
            continue;
        FluxType protoRet = func->getProto()->getReturnType();
        if (protoRet.Kind == TypeKind::UserStruct || protoRet.Kind == TypeKind::UserEnum)
            continue;
        const ExprAST* lastExpr = nullptr;
        if (auto* block = dynamic_cast<const BlockExprAST*>(func->getBody())) {
            const auto& stmts = block->getStatements();
            if (!stmts.empty())
                lastExpr = stmts.back().get();
        }
        if (!lastExpr)
            continue;
        FluxType fixRet(TypeKind::Void);
        findStructReturnCalls(lastExpr, context.FuncReturnTypes, fixRet);
        if (fixRet.Kind == TypeKind::UserStruct || fixRet.Kind == TypeKind::UserEnum) {
            func->getProto()->setReturnType(fixRet);
            returnTypes[name] = fixRet;
            context.FuncReturnTypes[name] = fixRet;
        }
    }

    // --- Pass 2: Codegen all function bodies ---
    // All LLVM declarations now exist, so call sites will find the correct types.
    // Set up the import callback so ImportExprAST inside function bodies works.
    // Save any existing callback so recursive calls (via importModule) can restore it.
    auto savedImportFn = std::move(context.importModuleFn);
    context.importModuleFn = [&](const std::string& moduleName, const std::string& alias,
                                 const std::vector<std::string>& fnBodySymbols) -> bool {
        auto* SavedInsertBlock = context.Builder.GetInsertBlock();
        if (!importModule(moduleName, context, returnTypes, &error, importedModules, fnBodySymbols)) {
            if (SavedInsertBlock)
                context.Builder.SetInsertPoint(SavedInsertBlock);
            return false;
        }
        if (SavedInsertBlock)
            context.Builder.SetInsertPoint(SavedInsertBlock);
        const std::string& nsAlias = alias.empty() ? moduleName : alias;
        context.ModuleNamespaces.insert(nsAlias);
        return true;
    };

    if (hasUpdateFunc) {
        returnTypes["update"] = FluxType(TypeKind::Double);
        context.FuncReturnTypes["update"] = FluxType(TypeKind::Double);
        if (!updateFunc->codegen(context)) {
            error = "Code generation failed for update function";
            context.importModuleFn = std::move(savedImportFn);
            return false;
        }
    }

    if (progCb && !progCb("Generating code", 0, numFunctions))
        return false;

    if (m_options.numJobs > 1 && numFunctions > 1) {
        // --- Parallel codegen: per-function in isolated threads ---
        int nJobs = std::min(m_options.numJobs, static_cast<int>(numFunctions));

        struct TRes
        {
            std::unique_ptr<CodegenContext> ctx;
            std::string error;
        };
        std::vector<TRes> tres(nJobs);
        std::vector<std::thread> thrds;
        std::atomic<bool> cncl{false};

        for (int j = 0; j < nJobs; ++j) {
            size_t b = j * numFunctions / nJobs;
            size_t e = (j + 1) * numFunctions / nJobs;
            thrds.emplace_back([&, j, b, e]() {
                auto tcg = std::make_unique<CodegenContext>();
                tcg->OwnedModule = std::make_unique<llvm::Module>("t", tcg->TheContext);
                tcg->TheModule = tcg->OwnedModule.get();

                tcg->FuncReturnTypes = context.FuncReturnTypes;
                tcg->ExternFuncTypes = context.ExternFuncTypes;
                tcg->ExcludedSymbols = context.ExcludedSymbols;

                // Declare all function prototypes in this thread's module so
                // cross-thread calls (e.g. median calling flatten) can find
                // the correct declaration instead of creating an implicit one
                // with mismatched types.
                for (auto& func : functions) {
                    llvm::Function* LF = func->getProto()->codegen(*tcg);
                    if (LF && importedNames.count(func->getProto()->getName())) {
                        LF->setLinkage(llvm::GlobalValue::InternalLinkage);
                    }
                }

                for (size_t i = b; i < e && !cncl; ++i) {
                    llvm::Function* LF = functions[i]->codegen(*tcg);
                    if (!LF) {
                        auto fnName = functions[i]->getProto()->getName();
                        // Anonymous expression failures are non-critical
                        // (they also happen harmlessly in the sequential path).
                        if (fnName.find("_anon_expr") != std::string::npos)
                            continue;
                        tres[j].error = "Codegen failed: " + fnName;
                        cncl = true;
                        break;
                    }
                    if (importedNames.count(functions[i]->getProto()->getName())) {
                        LF->setLinkage(llvm::GlobalValue::InternalLinkage);
                    }
                }
                tres[j].ctx = std::move(tcg);
            });
        }
        for (auto& t : thrds)
            t.join();

        if (cncl) {
            for (auto& r : tres)
                if (!r.error.empty()) {
                    error = r.error;
                    break;
                }
            context.importModuleFn = std::move(savedImportFn);
            return false;
        }

        // Merge: serialise each thread module to bitcode, deserialise
        // into main context, then link the thread module into the
        // main module using the same-context linker.
        for (auto& tr : tres) {
            if (!tr.ctx || !tr.ctx->OwnedModule)
                continue;
            auto& threadMod = *tr.ctx->OwnedModule;

            llvm::SmallVector<char, 0> buf;
            llvm::raw_svector_ostream os(buf);
            llvm::WriteBitcodeToFile(threadMod, os);

            auto mb = llvm::MemoryBuffer::getMemBufferCopy(llvm::StringRef(buf.data(), buf.size()));
            auto exp = llvm::parseBitcodeFile(mb->getMemBufferRef(), context.TheContext);
            if (!exp) {
                error = "Parallel codegen: serialisation round-trip failed";
                context.importModuleFn = std::move(savedImportFn);
                return false;
            }
            auto dmod = std::move(*exp);

            if (llvm::Linker::linkModules(*context.TheModule, std::move(dmod))) {
                error = "Parallel codegen: linking thread module failed";
                context.importModuleFn = std::move(savedImportFn);
                return false;
            }
        }

        if (progCb && !progCb("Generating code", numFunctions, numFunctions))
            return false;

    } else {
        // --- Sequential codegen (existing behavior) ---
        for (size_t fi = 0; fi < numFunctions; ++fi) {
            auto& func = functions[fi];
            llvm::Function* LF = func->codegen(context);
            if (!LF) {
                error = "Code generation failed for function: " + func->getProto()->getName();
                context.importModuleFn = std::move(savedImportFn);
                return false;
            }
            if (importedNames.count(func->getProto()->getName())) {
                LF->setLinkage(llvm::GlobalValue::InternalLinkage);
            }
            if (progCb && !progCb("Generating code", fi + 1, numFunctions))
                return false;
        }
    }

    context.importModuleFn = std::move(savedImportFn);
    return !parser.hasError();
}

std::vector<TokenInfo> CompilerInstance::tokenize(const std::string& code, std::string* error) const
{
    std::vector<TokenInfo> tokens;
    Lexer lexer(code);

    while (true) {
        const int token = lexer.getNextToken();
        tokens.push_back(TokenInfo{token, tokenSpelling(lexer, token), lexer.getCurrentLine(), lexer.getCurrentColumn(),
                                   lexer.getCurrentTokenOffset(), lexer.getCurrentTokenLength(),
                                   lexer.getCurrentTokenText()});

        if (token == static_cast<int>(TokenType::tok_eof))
            break;
    }

    if (error)
        error->clear();
    return tokens;
}

bool CompilerInstance::loadAndLinkBitcodeModule(const std::string& bcPath, CodegenContext& context,
                                                std::map<std::string, FluxType>& returnTypes, std::string* error) const
{
    llvm::SMDiagnostic diag;
    auto mod = parseIRFile(bcPath, diag, context.TheContext);
    if (!mod) {
        if (error)
            *error = "Failed to parse bitcode file: " + bcPath + " - " + diag.getMessage().str();
        return false;
    }

    // Register return types and parameter types from the loaded module's function signatures
    for (auto& F : *mod) {
        if (F.isDeclaration())
            continue;
        F.setLinkage(llvm::GlobalValue::InternalLinkage); // Internalize stdlib functions to avoid JIT conflicts
        std::string name = F.getName().str();
        bool hasSret = F.arg_size() > 0 && F.hasParamAttribute(0, llvm::Attribute::StructRet);
        FluxType retTy;
        if (hasSret) {
            llvm::Type* sretTy = F.getParamStructRetType(0);
            if (sretTy && sretTy->isStructTy() &&
                llvm::cast<llvm::StructType>(sretTy)->getName().contains("ComplexMatrix")) {
                retTy = FluxType(TypeKind::ComplexMatrix);
            } else {
                retTy = FluxType(TypeKind::Matrix);
            }
        } else if (F.getReturnType()->isDoubleTy()) {
            retTy = FluxType(TypeKind::Double);
        } else if (F.getReturnType()->isIntegerTy()) {
            retTy = FluxType(TypeKind::Double);
        } else if (F.getReturnType()->isVoidTy()) {
            retTy = FluxType(TypeKind::Double);
        } else {
            // Skip unknown types
            continue;
        }
        context.FuncReturnTypes[name] = retTy;
        returnTypes[name] = retTy;

        // Populate parameter types for cross-module type inference
        std::vector<FluxType> paramTys;
        size_t startIdx = hasSret ? 1 : 0;
        for (size_t i = startIdx; i < F.arg_size(); ++i) {
            llvm::Type* argLTy = F.getFunctionType()->getParamType(i);
            if (argLTy->isStructTy()) {
                if (llvm::cast<llvm::StructType>(argLTy)->getName().contains("ComplexMatrix")) {
                    paramTys.push_back(FluxType(TypeKind::ComplexMatrix));
                } else {
                    paramTys.push_back(FluxType(TypeKind::Matrix));
                }
            } else if (argLTy->isPointerTy()) {
                paramTys.push_back(FluxType(TypeKind::Matrix));
            } else if (argLTy->isDoubleTy()) {
                paramTys.push_back(FluxType(TypeKind::Double));
            } else if (argLTy->isIntegerTy()) {
                paramTys.push_back(FluxType(TypeKind::Int));
            } else {
                paramTys.push_back(FluxType(TypeKind::Double));
            }
        }
        context.CrossModuleParamTypes[name] = std::move(paramTys);
    }

    // Link the loaded module into the main module
    if (llvm::Linker::linkModules(*context.TheModule, std::move(mod))) {
        if (error)
            *error = "Failed to link bitcode module: " + bcPath;
        return false;
    }
    return true;
}

std::unique_ptr<CompileArtifacts> CompilerInstance::compileToIR(const std::string& code, std::string* error)
{
    auto artifacts = std::make_unique<CompileArtifacts>();
    artifacts->compileProgress = m_options.progressCallback;
    artifacts->codegenContext = createCodegenContext();

    // Add the input file's directory to ModuleLoader search paths so that
    // import resolution checks the same directory as the source file.
    if (!m_options.inputName.empty() && m_options.inputName != "-" && m_options.inputName != "<stdin>") {
        std::filesystem::path inputDir = std::filesystem::path(m_options.inputName).parent_path();
        // Walk up parent directories so imports resolve relative to any ancestor.
        // e.g. tests/test.flux can import circuit.flux from the parent directory.
        while (!inputDir.empty() && inputDir != inputDir.parent_path()) {
            m_moduleLoader.addSearchPath(inputDir);
            inputDir = inputDir.parent_path();
        }
    }

    // Add explicit include path for import resolution (used by the bridge
    // when compiling from a source string without a file path).
    if (!m_options.includePath.empty()) {
        m_moduleLoader.addSearchPath(m_options.includePath);
    }

    // Add the stdlib bitcode cache directory to search paths if it exists
    std::filesystem::path stdlibBcDir = std::filesystem::current_path() / "stdlib_bc";
    if (!std::filesystem::exists(stdlibBcDir)) {
        stdlibBcDir = std::filesystem::current_path() / "build" / "stdlib_bc";
    }
    if (std::filesystem::exists(stdlibBcDir)) {
        m_moduleLoader.addSearchPath(stdlibBcDir);
    }

    std::map<std::string, bool> importedModules;
    std::vector<std::unique_ptr<FunctionAST>> preParsedFunctions;
    if (m_options.injectStdlib) {
        injectStandardLibrary(*artifacts->codegenContext, artifacts->functionReturnTypes);
        // Auto-import standard library modules so they're available without
        // explicit import. Functions are collected without immediate codegen
        // and passed to compileParser for combined type inference + codegen.
        // Pre-compiled .flux.bc files are loaded automatically if available.
        std::vector<std::string> stdlibModules = {"math", "trig", "array", "stats", "signal", "string", "cmatrix"};
        for (const auto& mod : stdlibModules) {
            std::string importError;
            collectImportFunctions(mod, preParsedFunctions, *artifacts->codegenContext, artifacts->functionReturnTypes,
                                   &importError, importedModules);
        }
    }

    // Run preprocessor on source code
    Preprocessor pp;
    PreprocessResult ppResult = pp.process(code, m_options.inputName);
    if (!ppResult.success) {
        for (const auto& err : ppResult.errors) {
            std::cerr << "[Preprocessor Error] " << err << "\n";
        }
        if (error)
            *error = "Preprocessor failed";
        return nullptr;
    }
    for (const auto& warn : ppResult.warnings) {
        std::cerr << "[Preprocessor Warning] " << warn << "\n";
    }
    std::string processedCode = ppResult.output;

    Parser parser(processedCode); // Constructor primes the lexer with getNextToken()
    std::string compileError;
    if (!compileParser(parser, *artifacts->codegenContext, artifacts->functionReturnTypes, compileError,
                       importedModules, {}, &preParsedFunctions)) {
        if (error) {
            // Use structured diagnostic if available
            auto diags = parser.getErrors();
            if (!diags.empty()) {
                auto& d = diags.front();
                *error = "Error at line " + std::to_string(d.line) + ", column " + std::to_string(d.column) + ": " +
                         d.message;
            } else {
                *error = compileError;
            }
        }
        return nullptr;
    }

    // In parse-only mode, skip codegen verification — return as soon as parsing succeeds.
    if (m_options.parseOnly) {
        if (error)
            error->clear();
        return artifacts;
    }

    if (artifacts->codegenContext->DebugBuilder)
        artifacts->codegenContext->DebugBuilder->finalize();

    {
        std::string verifyError;
        llvm::raw_string_ostream verifyOS(verifyError);
        if (llvm::verifyModule(*artifacts->codegenContext->TheModule, &verifyOS)) {
            llvm::errs() << "=== INVALID MODULE DUMP ===\n";
            artifacts->codegenContext->TheModule->print(llvm::errs(), nullptr);
            if (error)
                *error = "Generated LLVM IR is invalid: " + verifyOS.str();
            return nullptr;
        }
    }

    if (error)
        error->clear();
    return artifacts;
}

std::string CompilerInstance::emitLLVMIR(const std::string& code, std::string* error)
{
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
