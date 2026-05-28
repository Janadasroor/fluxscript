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

// FluxScript CLI - compiler driver and JIT runtime
#include <cmath>
#include <complex>
#include <filesystem>
#include <string>

#include <llvm/Bitcode/BitcodeWriter.h>
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

static const char* VersionStr = "FluxScript v0.1.0 (LLVM JIT)";
static const char* HelpExamples =
    "EXAMPLES:\n"
    "\n"
    "  Run a script and print the result (default JIT mode):\n"
    "    flux myscript.flux\n"
    "\n"
    "  Run a specific function from a script:\n"
    "    flux --entry=my_function myscript.flux\n"
    "\n"
    "  Parse and type-check without executing:\n"
    "    flux --emit=check myscript.flux\n"
    "\n"
    "  Emit LLVM IR and inspect the generated code:\n"
    "    flux --emit=llvm myscript.flux\n"
    "\n"
    "  Emit LLVM bitcode to a file:\n"
    "    flux --emit=bc -o output.bc myscript.flux\n"
    "\n"
    "  Disable JIT cache:\n"
    "    flux --cache=0 myscript.flux\n"
    "\n"
    "  Compile without running (JIT mode, no execution):\n"
    "    flux --no-run myscript.flux\n"
    "\n"
    "  Profile compile and run time (10 iterations):\n"
    "    flux --profile --iterations=10 myscript.flux\n"
    "\n"
    "  Read script from stdin:\n"
    "    echo '42.0' | flux\n"
    "\n"
    "For more information, see https://github.com/Janadasroor/fluxscript\n";

enum class EmitMode
{
    Tokens,
    Check,
    LLVM,
    Bitcode,
    JIT,
    MLIR
};

llvm::cl::OptionCategory FluxCategory("FluxScript options");

llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::desc("<script.flux>"), llvm::cl::init("-"),
                                         llvm::cl::cat(FluxCategory));

llvm::cl::opt<EmitMode> EmitAction(
    "emit", llvm::cl::desc("Compiler output mode (default: jit)"),
    llvm::cl::values(clEnumValN(EmitMode::Tokens, "tokens", "Lex and dump token stream"),
                     clEnumValN(EmitMode::Check, "check", "Parse, type-check, and verify IR without executing"),
                     clEnumValN(EmitMode::LLVM, "llvm", "Emit human-readable LLVM IR to stdout"),
                     clEnumValN(EmitMode::Bitcode, "bc", "Emit LLVM bitcode (.bc) to file (use -o)"),
                     clEnumValN(EmitMode::JIT, "jit", "Compile and execute using the LLVM JIT engine (default)"),
                     clEnumValN(EmitMode::MLIR, "mlir", "Emit MLIR via the optional MLIR integration")),
    llvm::cl::init(EmitMode::JIT), llvm::cl::cat(FluxCategory));

llvm::cl::opt<unsigned> OptLevel(llvm::cl::Prefix, llvm::cl::desc("LLVM optimization level 0-3 (default: 2)"),
                                 llvm::cl::init(2), llvm::cl::cat(FluxCategory));

llvm::cl::opt<std::string> EntryPoint("entry", llvm::cl::desc("Function name to invoke in JIT mode (default: top-level expression)"),
                                      llvm::cl::init("__anon_expr"), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> NoRun("no-run", llvm::cl::desc("JIT compile without executing the entry point"),
                          llvm::cl::init(false), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> ProfileMode("profile", llvm::cl::desc("Benchmark mode: print compile and execution times"),
                                llvm::cl::init(false), llvm::cl::cat(FluxCategory));

llvm::cl::opt<int> Iterations("iterations", llvm::cl::desc("Number of iterations for --profile benchmarking"),
                              llvm::cl::init(1), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> EnableCache("cache", llvm::cl::desc("Enable on-disk JIT compilation cache (default: on, use --cache=0 to disable)"),
                                llvm::cl::init(true), llvm::cl::cat(FluxCategory));

llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("Output file path (used with --emit=bc)"),
                                          llvm::cl::init(""), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> ShowExamples("examples", llvm::cl::desc("Print usage examples and exit"),
                                 llvm::cl::init(false), llvm::cl::cat(FluxCategory));

Flux::OptimizationLevel toOptimizationLevel(unsigned level)
{
    switch (level) {
    case 0:
        return Flux::OptimizationLevel::O0;
    case 1:
        return Flux::OptimizationLevel::O1;
    case 2:
        return Flux::OptimizationLevel::O2;
    case 3:
        return Flux::OptimizationLevel::O3;
    default:
        return Flux::OptimizationLevel::O2;
    }
}

bool readInput(std::string& code, std::string& error)
{
    auto bufferOrErr = llvm::MemoryBuffer::getFileOrSTDIN(InputFilename);
    if (!bufferOrErr) {
        error = bufferOrErr.getError().message();
        return false;
    }

    code = std::string(bufferOrErr.get()->getBuffer());
    return true;
}

void printResult(const Flux::FluxValue& result)
{
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
            llvm::outs() << "Result: " << value.real() << (value.imag() >= 0 ? " + " : " - ") << std::abs(value.imag())
                         << "j\n";
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
            llvm::outs() << "Matrix Result (" << value.rows << "x" << value.cols << "):\n";
        } else {
            llvm::outs() << "Matrix Result (Empty)\n";
        }
        return;
    }

    if (std::holds_alternative<Flux::VectorResult>(result)) {
        const auto value = std::get<Flux::VectorResult>(result);
        if (value.data && value.len > 0) {
            llvm::outs() << "Vector Result (len=" << value.len << "): [";
            for (int i = 0; i < value.len; ++i) {
                if (i > 0) llvm::outs() << ", ";
                llvm::outs() << value.data[i];
            }
            llvm::outs() << "]\n";
        } else {
            llvm::outs() << "Vector Result (Empty)\n";
        }
        return;
    }
}

int runJIT(const std::string& code)
{
    auto& engine = Flux::JITEngine::instance();
    engine.initialize();
    engine.setOptimizationLevel(toOptimizationLevel(OptLevel));
    engine.setJITCacheEnabled(EnableCache);

    if (ProfileMode) {
        std::string error;
        const auto profile = Flux::Tooling::profileScript(engine, code, EntryPoint, Iterations, &error);
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
        actualEntryPoint = "__anon_expr";
    }

    const auto result = engine.callFunction(actualEntryPoint, {}, &error);
    if (!error.empty()) {
        llvm::errs() << "Runtime Error: " << error << "\n";
        return 1;
    }

    printResult(result);
    return 0;
}

int dumpTokens(const std::string& code)
{
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
        llvm::outs() << token.line << ":" << token.column << "  " << token.token << "  " << token.spelling << "\n";
    }
    return 0;
}

int checkOnly(const std::string& code)
{
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

int emitLLVM(const std::string& code)
{
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

int emitMLIR(const std::string& code)
{
    const auto result = Flux::MLIR::emitModule(code);
    if (!result.ok) {
        llvm::errs() << "MLIR Error: " << result.error << "\n";
        return 1;
    }

    llvm::outs() << result.output << "\n";
    return 0;
}

int emitBitcode(const std::string& code)
{
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    options.moduleName = InputFilename == "-" ? std::string("stdin") : std::string(InputFilename);
    options.optimizationLevel = toOptimizationLevel(OptLevel);
    Flux::CompilerInstance compiler(options);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        llvm::errs() << "Compile Error: " << error << "\n";
        return 1;
    }

    if (OutputFilename.empty()) {
        // Write bitcode to stdout
        llvm::WriteBitcodeToFile(*artifacts->codegenContext->TheModule, llvm::outs());
    } else {
        std::error_code EC;
        llvm::raw_fd_ostream os(OutputFilename, EC, llvm::sys::fs::OF_None);
        if (EC) {
            llvm::errs() << "Error opening output file: " << EC.message() << "\n";
            return 1;
        }
        llvm::WriteBitcodeToFile(*artifacts->codegenContext->TheModule, os);
        os.close();
        llvm::outs() << "Wrote " << OutputFilename << "\n";
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    llvm::InitLLVM initLLVM(argc, argv);

    // Override LLVM's default --version handler
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--version" || a == "-v") {
            llvm::outs() << VersionStr << "\n";
            llvm::outs() << "LLVM " << LLVM_VERSION_STRING << "\n";
            return 0;
        }
    }

    llvm::cl::HideUnrelatedOptions(FluxCategory);
    llvm::cl::ParseCommandLineOptions(argc, argv, "FluxScript - LLVM JIT-compiled scripting language\n");

    if (ShowExamples) {
        llvm::outs() << HelpExamples;
        return 0;
    }

    if (OptLevel > 3) {
        llvm::errs() << "Error: Invalid optimization level -O" << OptLevel << " (valid: 0-3)\n";
        return 1;
    }

    std::string code;
    std::string error;
    if (!readInput(code, error)) {
        llvm::errs() << "Error: Could not read input '" << InputFilename << "': " << error << "\n";
        return 1;
    }

    switch (EmitAction) {
    case EmitMode::Tokens:
        return dumpTokens(code);
    case EmitMode::Check:
        return checkOnly(code);
    case EmitMode::LLVM:
        return emitLLVM(code);
    case EmitMode::Bitcode:
        return emitBitcode(code);
    case EmitMode::JIT:
        return runJIT(code);
    case EmitMode::MLIR:
        return emitMLIR(code);
    }

    llvm::errs() << "Unknown emit mode\n";
    return 1;
}
