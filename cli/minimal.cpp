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

// Minimal FluxScript CLI without Qt dependencies
#include <cmath>
#include <complex>
#include <string>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#include "flux/compiler/compiler_instance.h"
#include "flux/jit_engine.h"
#include "flux/mlir/flux_mlir.h"
#include "flux/runtime/flux_runtime.h"
#include "flux/tooling/tooling.h"

namespace {

enum class EmitMode {
    Tokens,
    Check,
    LLVM,
    JIT,
    MLIR
};

llvm::cl::OptionCategory FluxCategory("FluxScript options");

llvm::cl::opt<std::string> InputFilename(
    llvm::cl::Positional,
    llvm::cl::desc("<script.flux>"),
    llvm::cl::init("-"),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<EmitMode> EmitAction(
    "emit",
    llvm::cl::desc("Select the compiler output mode"),
    llvm::cl::values(
        clEnumValN(EmitMode::Tokens, "tokens", "Lex and dump tokens"),
        clEnumValN(EmitMode::Check, "check", "Parse and type/codegen-check without running"),
        clEnumValN(EmitMode::LLVM, "llvm", "Emit generated LLVM IR"),
        clEnumValN(EmitMode::JIT, "jit", "Compile and run with the LLVM JIT"),
        clEnumValN(EmitMode::MLIR, "mlir", "Emit MLIR from the optional MLIR integration path")),
    llvm::cl::init(EmitMode::JIT),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<unsigned> OptLevel(
    llvm::cl::Prefix,
    llvm::cl::desc("Optimization level"),
    llvm::cl::init(2),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<std::string> EntryPoint(
    "entry",
    llvm::cl::desc("Function to invoke in JIT mode"),
    llvm::cl::init("__anon_expr"),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> NoRun(
    "no-run",
    llvm::cl::desc("Compile in JIT mode without invoking the entry point"),
    llvm::cl::init(false),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> ProfileMode(
    "profile",
    llvm::cl::desc("Measure JIT compile and run time"),
    llvm::cl::init(false),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<int> Iterations(
    "iterations",
    llvm::cl::desc("Number of entry-point invocations for profiling"),
    llvm::cl::init(1),
    llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> EnableCache(
    "cache",
    llvm::cl::desc("Enable the on-disk JIT cache"),
    llvm::cl::init(true),
    llvm::cl::cat(FluxCategory));

Flux::OptimizationLevel toOptimizationLevel(unsigned level) {
    switch (level) {
        case 0: return Flux::OptimizationLevel::O0;
        case 1: return Flux::OptimizationLevel::O1;
        case 2: return Flux::OptimizationLevel::O2;
        case 3: return Flux::OptimizationLevel::O3;
        default: return Flux::OptimizationLevel::O2;
    }
}

bool readInput(std::string& code, std::string& error) {
    auto bufferOrErr = llvm::MemoryBuffer::getFileOrSTDIN(InputFilename);
    if (!bufferOrErr) {
        error = bufferOrErr.getError().message();
        return false;
    }

    code = std::string(bufferOrErr.get()->getBuffer());
    return true;
}

void printResult(const Flux::FluxValue& result) {
    if (std::holds_alternative<double>(result)) {
        llvm::outs() << "Result: " << std::get<double>(result) << "\n";
        return;
    }

    if (std::holds_alternative<std::complex<double>>(result)) {
        const auto value = std::get<std::complex<double>>(result);
        if (value.imag() == 0.0) {
            llvm::outs() << "Result: " << value.real() << "\n";
        } else if (value.real() == 0.0) {
            llvm::outs() << "Result: " << value.imag() << "j\n";
        } else {
            llvm::outs() << "Result: " << value.real()
                         << (value.imag() >= 0 ? " + " : " - ")
                         << std::abs(value.imag()) << "j\n";
        }
        return;
    }

    if (std::holds_alternative<int>(result)) {
        llvm::outs() << "Result: " << std::get<int>(result) << "\n";
        return;
    }

    if (std::holds_alternative<Flux::MatrixResult>(result)) {
        const auto value = std::get<Flux::MatrixResult>(result);
        if (value.ptr) {
            llvm::outs() << "Matrix Result (" << flux_matrix_rows(value.ptr)
                         << "x" << flux_matrix_cols(value.ptr) << "):\n";
            flux_print_matrix(value.ptr);
        } else {
            llvm::outs() << "Matrix Result (Empty)\n";
        }
    }
}

int runJIT(const std::string& code) {
    auto& engine = Flux::JITEngine::instance();
    engine.initialize();
    engine.setOptimizationLevel(toOptimizationLevel(OptLevel));
    engine.setJITCacheEnabled(EnableCache);

    if (ProfileMode) {
        std::string error;
        const auto profile = Flux::Tooling::profileScript(
            engine, code, EntryPoint, Iterations, &error);
        if (!error.empty()) {
            llvm::errs() << "Profile Error: " << error << "\n";
            return 1;
        }

        llvm::outs() << "Compile: " << profile.compileMs << " ms\n";
        llvm::outs() << "Run(" << profile.iterations << "): " << profile.runMs << " ms\n";
        llvm::outs() << "Cache: " << (engine.lastCompileUsedCache() ? "hit" : "miss") << "\n";
        return 0;
    }

    std::string error;
    if (!engine.compileScript(code, &error)) {
        llvm::errs() << "Compile Error: " << error << "\n";
        return 1;
    }

    llvm::outs() << "Compiled successfully.\n";
    llvm::outs() << "JIT cache: " << (engine.lastCompileUsedCache() ? "hit" : "miss") << "\n";
    if (NoRun)
        return 0;

    std::string actualEntryPoint = EntryPoint;
    if (actualEntryPoint == "__anon_expr") {
        actualEntryPoint = "FluxModule_anon_expr";

    }


    const auto result = engine.callFunction(actualEntryPoint, {}, &error);
    if (!error.empty()) {
        llvm::errs() << "Runtime Error: " << error << "\n";
        return 1;
    }

    printResult(result);
    return 0;
}

int dumpTokens(const std::string& code) {
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    Flux::CompilerInstance compiler(options);
    std::string error;
    const auto tokens = compiler.tokenize(code, &error);
    if (!error.empty()) {
        llvm::errs() << "Tokenization Error: " << error << "\n";
        return 1;
    }

    for (const auto& token : tokens) {
        llvm::outs() << token.line << ":" << token.column
                     << "  " << token.token
                     << "  " << token.spelling << "\n";
    }
    return 0;
}

int checkOnly(const std::string& code) {
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    options.moduleName = InputFilename == "-" ? std::string("stdin") : std::string(InputFilename);
    options.optimizationLevel = toOptimizationLevel(OptLevel);
    Flux::CompilerInstance compiler(options);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        llvm::errs() << "Check Error: " << error << "\n";
        return 1;
    }

    llvm::outs() << "OK\n";
    return 0;
}

int emitLLVM(const std::string& code) {
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    options.moduleName = InputFilename == "-" ? std::string("stdin") : std::string(InputFilename);
    options.optimizationLevel = toOptimizationLevel(OptLevel);
    Flux::CompilerInstance compiler(options);
    std::string error;
    const auto llvmIR = compiler.emitLLVMIR(code, &error);
    if (!error.empty()) {
        llvm::errs() << "LLVM IR Error: " << error << "\n";
        return 1;
    }

    llvm::outs() << llvmIR;
    return 0;
}

int emitMLIR(const std::string& code) {
    const auto result = Flux::MLIR::emitModule(code);
    if (!result.ok) {
        llvm::errs() << "MLIR Error: " << result.error << "\n";
        return 1;
    }

    llvm::outs() << result.output << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    llvm::InitLLVM initLLVM(argc, argv);
    llvm::cl::HideUnrelatedOptions(FluxCategory);
    llvm::cl::ParseCommandLineOptions(argc, argv, "FluxScript LLVM driver\n");

    if (OptLevel > 3) {
        llvm::errs() << "Invalid optimization level: -O" << OptLevel << "\n";
        return 1;
    }

    std::string code;
    std::string error;
    if (!readInput(code, error)) {
        llvm::errs() << "Error: Could not read input '" << InputFilename
                     << "': " << error << "\n";
        return 1;
    }

    switch (EmitAction) {
        case EmitMode::Tokens:
            return dumpTokens(code);
        case EmitMode::Check:
            return checkOnly(code);
        case EmitMode::LLVM:
            return emitLLVM(code);
        case EmitMode::JIT:
            return runJIT(code);
        case EmitMode::MLIR:
            return emitMLIR(code);
    }

    llvm::errs() << "Unknown emit mode\n";
    return 1;
}
