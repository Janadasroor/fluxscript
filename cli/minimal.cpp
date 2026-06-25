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
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#endif

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
#include "flux/docs_embedded.h"
#include "flux/examples_embedded.h"

namespace {

std::string moduleNameFromInput(const std::string& input)
{
    if (input == "-")
        return "stdin";
    std::filesystem::path p(input);
    auto stem = p.stem();
    return stem.empty() ? input : stem.string();
}

static const char* VersionStr = "FluxScript v0.1.0 (LLVM JIT)";
static const char* HelpText = "FluxScript — LLVM JIT-compiled scripting language\n"
                               "\n"
                               "USAGE:\n"
                               "  flux <script.flux>              Run a script (default JIT mode)\n"
                               "  flux run <script.flux>          Run a script\n"
                               "  flux run --watch <script.flux>  Run and re-run on file changes\n"
                               "  flux check <script.flux>        Parse and type-check without executing\n"
                               "  flux parse <script.flux>        Parse only — report syntax errors\n"
                               "  flux fmt <script.flux>          Format a FluxScript file\n"
                               "  flux new <project>              Create a new FluxScript project\n"
                               "  flux stats [directory]          Show project statistics\n"
                               "  flux bench <script.flux>        Benchmark across opt levels\n"
                               "  flux test [directory]           Run integration tests\n"
                               "  flux repl                       Start interactive REPL\n"
                               "  flux lsp                        Start the LSP server\n"
                               "  flux docs [keyword]             Show/search documentation\n"
                               "  flux examples [keyword]        Show/search code examples\n"
                               "  flux pkg <command>              Package management\n"
                               "\n"
                               "OPTIONS:\n"
                               "  --emit=<mode>    Compiler output mode (tokens, check, parse-only, llvm, bc, jit)\n"
                               "  --opt=<0-3>      LLVM optimization level (default: 2)\n"
                               "  --cache=0        Disable JIT compilation cache\n"
                               "  --quiet, -q      Suppress JIT debug output\n"
                               "  --entry=<name>   Function to invoke in JIT mode\n"
                               "  --profile        Benchmark mode\n"
                               "  --version, -v    Print version\n"
                               "  --help, -h       Print this help\n";
static const char* HelpExamples = "EXAMPLES:\n"
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
    ParseOnly,
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
                     clEnumValN(EmitMode::ParseOnly, "parse-only", "Parse only — report syntax errors without codegen"),
                     clEnumValN(EmitMode::LLVM, "llvm", "Emit human-readable LLVM IR to stdout"),
                     clEnumValN(EmitMode::Bitcode, "bc", "Emit LLVM bitcode (.bc) to file (use -o)"),
                     clEnumValN(EmitMode::JIT, "jit", "Compile and execute using the LLVM JIT engine (default)"),
                     clEnumValN(EmitMode::MLIR, "mlir", "Emit MLIR via the optional MLIR integration")),
    llvm::cl::init(EmitMode::JIT), llvm::cl::cat(FluxCategory));

llvm::cl::opt<unsigned> OptLevel("opt", llvm::cl::Prefix, llvm::cl::desc("LLVM optimization level 0-3 (default: 2)"),
                                 llvm::cl::init(2), llvm::cl::cat(FluxCategory));

llvm::cl::opt<std::string>
    EntryPoint("entry", llvm::cl::desc("Function name to invoke in JIT mode (default: top-level expression)"),
               llvm::cl::init("__anon_expr"), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> NoRun("no-run", llvm::cl::desc("JIT compile without executing the entry point"),
                          llvm::cl::init(false), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> ProfileMode("profile", llvm::cl::desc("Benchmark mode: print compile and execution times"),
                                llvm::cl::init(false), llvm::cl::cat(FluxCategory));

llvm::cl::opt<int> Iterations("iterations", llvm::cl::desc("Number of iterations for --profile benchmarking"),
                              llvm::cl::init(1), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool>
    EnableCache("cache", llvm::cl::desc("Enable on-disk JIT compilation cache (default: on, use --cache=0 to disable)"),
                llvm::cl::init(true), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool>
    Quiet("q", llvm::cl::desc("Quiet mode — suppress JIT debug output"),
          llvm::cl::init(false), llvm::cl::cat(FluxCategory));

llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("Output file path (used with --emit=bc)"),
                                          llvm::cl::init(""), llvm::cl::cat(FluxCategory));

llvm::cl::opt<bool> ShowExamples("examples", llvm::cl::desc("Print usage examples and exit"), llvm::cl::init(false),
                                 llvm::cl::cat(FluxCategory));

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
                if (i > 0)
                    llvm::outs() << ", ";
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
    engine.setInputName(InputFilename);

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

    if (!Quiet) {
        llvm::outs() << "Compiled successfully.\n";
        llvm::outs() << "JIT cache: " << (engine.lastCompileUsedCache() ? "hit" : "miss") << "\n";
    }
    if (NoRun)
        return 0;

    std::string actualEntryPoint = EntryPoint;
    if (actualEntryPoint == "__anon_expr") {
        actualEntryPoint = "__anon_expr";
    }

    // Set a 30-second timeout for JIT execution to prevent infinite loops
    static constexpr unsigned int JIT_TIMEOUT_SECONDS = 30;
#ifndef _WIN32
    static volatile sig_atomic_t timed_out = 0;
    auto prev_handler = std::signal(SIGALRM, [](int) { timed_out = 1; });
    alarm(JIT_TIMEOUT_SECONDS);
#endif

    const auto result = engine.callFunction(actualEntryPoint, {}, &error);

#ifndef _WIN32
    alarm(0); // Cancel alarm
    std::signal(SIGALRM, prev_handler);
#endif

#ifdef _WIN32
    bool timed_out = false;
#endif
    if (timed_out) {
        llvm::errs() << "Runtime Error: Execution timed out after " << JIT_TIMEOUT_SECONDS
                     << " seconds (possible infinite loop)\n";
        return 1;
    }
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
    options.moduleName = moduleNameFromInput(InputFilename);
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

int parseCheckOnly(const std::string& code)
{
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    options.moduleName = moduleNameFromInput(InputFilename);
    options.optimizationLevel = Flux::OptimizationLevel::O0;
    options.parseOnly = true;
    Flux::CompilerInstance compiler(options);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        llvm::errs() << "Parse Error: " << error << "\n";
        return 1;
    }

    llvm::outs() << "OK\n";
    return 0;
}

int emitLLVM(const std::string& code)
{
    Flux::CompilerOptions options;
    options.inputName = InputFilename;
    options.moduleName = moduleNameFromInput(InputFilename);
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
    options.moduleName = moduleNameFromInput(InputFilename);
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

    // --- Subcommand dispatch (before LLVM cl::Parse) ---
    if (argc >= 2) {
        std::string subcmd = argv[1];

        if (subcmd == "--help" || subcmd == "-h") {
            llvm::outs() << HelpText;
            return 0;
        }

        if (subcmd == "run") {
            // Check for --watch flag
            bool watch = false;
            std::vector<const char*> newArgs = {argv[0]};
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--watch") {
                    watch = true;
                } else {
                    newArgs.push_back(argv[i]);
                }
            }
            if (watch && newArgs.size() >= 2) {
                std::string filePath = newArgs[1];
                std::cout << "Watching " << filePath << " for changes... (Ctrl+C to stop)\n";
                auto lastWrite = std::filesystem::last_write_time(filePath);
                while (true) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    auto currentWrite = std::filesystem::last_write_time(filePath);
                    if (currentWrite != lastWrite) {
                        lastWrite = currentWrite;
                        std::cout << "\n--- File changed, re-running ---\n";
                        auto result = std::system(("'" + std::string(argv[0]) + "' --cache=0 '" + filePath + "'").c_str());
                        (void)result;
                    }
                }
                return 0;
            }
            return main(static_cast<int>(newArgs.size()), const_cast<char**>(newArgs.data()));
        }

        if (subcmd == "new" && argc >= 3) {
            std::string projectName = argv[2];
            std::filesystem::path projectDir(projectName);

            if (std::filesystem::exists(projectDir)) {
                std::cerr << "Error: Directory '" << projectName << "' already exists\n";
                return 1;
            }

            std::filesystem::create_directories(projectDir);
            std::filesystem::create_directories(projectDir / "src");
            std::filesystem::create_directories(projectDir / "tests");

            // main.flux
            {
                std::ofstream f(projectDir / "main.flux");
                f << "# " << projectName << "\n"
                  << "#\n"
                  << "# Run with:  flux main.flux\n"
                  << "# Check:    flux check main.flux\n"
                  << "\n"
                  << "def main() -> Double {\n"
                  << "    println(\"Hello from " << projectName << "!\")\n"
                  << "    0.0\n"
                  << "}\n"
                  << "\n"
                  << "main()\n";
            }

            // src/core.flux
            {
                std::ofstream f(projectDir / "src" / "core.flux");
                f << "# Core library for " << projectName << "\n"
                  << "# Import with: import core\n"
                  << "\n"
                  << "def add(x: Double, y: Double) -> Double {\n"
                  << "    x + y\n"
                  << "}\n";
            }

            // tests/test_main.flux
            {
                std::ofstream f(projectDir / "tests" / "test_main.flux");
                f << "import ../src/core\n"
                  << "\n"
                  << "def test_add() {\n"
                  << "    var result = add(2.0, 3.0)\n"
                  << "    assert(result == 5.0, \"add(2,3) should be 5\")\n"
                  << "    println(\"PASS: add\")\n"
                  << "}\n"
                  << "\n"
                  << "def main() -> Double {\n"
                  << "    test_add()\n"
                  << "    println(\"All tests passed!\")\n"
                  << "    0.0\n"
                  << "}\n"
                  << "\n"
                  << "main()\n";
            }

            // .gitignore
            {
                std::ofstream f(projectDir / ".gitignore");
                f << "*.bc\n"
                  << ".flux_cache/\n"
                  << "build/\n";
            }

            // README.md
            {
                std::ofstream f(projectDir / "README.md");
                f << "# " << projectName << "\n"
                  << "\n"
                  << "A FluxScript project.\n"
                  << "\n"
                  << "## Quick Start\n"
                  << "\n"
                  << "```bash\n"
                  << "flux run main.flux        # Run the project\n"
                  << "flux check main.flux      # Type-check\n"
                  << "flux run tests/test_main.flux  # Run tests\n"
                  << "```\n";
            }

            std::cout << "Created project '" << projectName << "'\n\n";
            std::cout << "  " << projectName << "/\n";
            std::cout << "  ├── main.flux          # Entry point\n";
            std::cout << "  ├── src/\n";
            std::cout << "  │   └── core.flux       # Core library\n";
            std::cout << "  ├── tests/\n";
            std::cout << "  │   └── test_main.flux  # Tests\n";
            std::cout << "  ├── .gitignore\n";
            std::cout << "  └── README.md\n\n";
            std::cout << "Next steps:\n";
            std::cout << "  cd " << projectName << "\n";
            std::cout << "  flux run main.flux\n";
            return 0;
        }

        if (subcmd == "check" && argc >= 3) {
            std::string filePath = argv[2];
            auto buf = llvm::MemoryBuffer::getFile(filePath);
            if (!buf) {
                llvm::errs() << "Error: Could not read '" << filePath << "'\n";
                return 1;
            }
            Flux::CompilerOptions opts;
            opts.inputName = filePath;
            opts.moduleName = moduleNameFromInput(filePath);
            opts.optimizationLevel = Flux::OptimizationLevel::O0;
            Flux::CompilerInstance compiler(opts);
            std::string code = (*buf)->getBuffer().str();
            std::string error;
            auto artifacts = compiler.compileToIR(code, &error);
            if (!artifacts) {
                llvm::errs() << "Check Error: " << error << "\n";
                return 1;
            }
            llvm::outs() << "OK\n";
            return 0;
        }

        if (subcmd == "parse" && argc >= 3) {
            std::string filePath = argv[2];
            auto buf = llvm::MemoryBuffer::getFile(filePath);
            if (!buf) {
                llvm::errs() << "Error: Could not read '" << filePath << "'\n";
                return 1;
            }
            Flux::CompilerOptions opts;
            opts.inputName = filePath;
            opts.moduleName = moduleNameFromInput(filePath);
            opts.optimizationLevel = Flux::OptimizationLevel::O0;
            opts.parseOnly = true;
            Flux::CompilerInstance compiler(opts);
            std::string code = (*buf)->getBuffer().str();
            std::string error;
            auto artifacts = compiler.compileToIR(code, &error);
            if (!artifacts) {
                llvm::errs() << "Parse Error: " << error << "\n";
                return 1;
            }
            llvm::outs() << "OK\n";
            return 0;
        }

        if (subcmd == "fmt" && argc >= 3) {
            std::string cmd = "flux-format " + std::string(argv[2]);
            for (int i = 3; i < argc; i++)
                cmd += " " + std::string(argv[i]);
            return std::system(cmd.c_str());
        }

        if (subcmd == "test") {
            std::string cmd = "flux-test";
            for (int i = 2; i < argc; i++)
                cmd += " " + std::string(argv[i]);
            return std::system(cmd.c_str());
        }

        if (subcmd == "repl") {
            return std::system("flux-repl");
        }

        if (subcmd == "lsp") {
            return std::system("flux-lsp");
        }

        if (subcmd == "docs") {
            // If keyword provided, search embedded docs
            if (argc >= 3) {
                std::string keyword = argv[2];
                std::string result = Flux::Docs::searchDocs(keyword);
                llvm::outs() << result;
                return 0;
            }
            // Otherwise, show full embedded docs
            llvm::outs() << Flux::Docs::getDocsText();
            return 0;
        }

        if (subcmd == "examples") {
            if (argc >= 3) {
                std::string keyword = argv[2];
                std::string result = Flux::Examples::searchExamples(keyword);
                llvm::outs() << result;
                return 0;
            }
            llvm::outs() << Flux::Examples::getExamplesText();
            return 0;
        }

        if (subcmd == "bench" && argc >= 3) {
            std::string filePath = argv[2];
            int iterations = 5;
            if (argc >= 5 && std::string(argv[3]) == "--iterations") {
                iterations = std::stoi(argv[4]);
            }

            std::cout << "Benchmarking " << filePath << " (" << iterations << " iterations per opt level)\n";
            std::cout << std::string(60, '=') << "\n";
            printf("%-8s  %12s  %12s  %8s\n", "Opt", "Compile(ms)", "Run(ms)", "Result");
            std::cout << std::string(60, '-') << "\n";

            for (int opt = 0; opt <= 3; opt++) {
                double totalCompile = 0, totalRun = 0;
                double lastResult = 0;

                for (int iter = 0; iter < iterations; iter++) {
                    // Fresh JIT engine for each run
                    auto& engine = Flux::JITEngine::instance();
                    engine.initialize();
                    engine.setOptimizationLevel(static_cast<Flux::OptimizationLevel>(opt));
                    engine.setJITCacheEnabled(false);
                    engine.setInputName(filePath);

                    auto buf = llvm::MemoryBuffer::getFile(filePath);
                    if (!buf) {
                        std::cerr << "Error: Could not read '" << filePath << "'\n";
                        return 1;
                    }
                    std::string code = (*buf)->getBuffer().str();

                    auto compileStart = std::chrono::high_resolution_clock::now();
                    std::string error;
                    if (!engine.compileScript(code, &error)) {
                        std::cerr << "Compile Error (opt=" << opt << "): " << error << "\n";
                        return 1;
                    }
                    auto compileEnd = std::chrono::high_resolution_clock::now();
                    double compileMs = std::chrono::duration<double, std::milli>(compileEnd - compileStart).count();

                    auto runStart = std::chrono::high_resolution_clock::now();
                    auto result = engine.callFunction("__anon_expr", {}, &error);
                    auto runEnd = std::chrono::high_resolution_clock::now();
                    double runMs = std::chrono::duration<double, std::milli>(runEnd - runStart).count();

                    if (!error.empty()) {
                        std::cerr << "Runtime Error (opt=" << opt << "): " << error << "\n";
                        return 1;
                    }

                    totalCompile += compileMs;
                    totalRun += runMs;
                    if (std::holds_alternative<double>(result))
                        lastResult = std::get<double>(result);
                }

                printf("  -O%d    %10.1f ms  %10.1f ms  %8.4g\n",
                       opt, totalCompile / iterations, totalRun / iterations, lastResult);
            }
            std::cout << std::string(60, '=') << "\n";
            return 0;
        }

        if (subcmd == "stats") {
            std::string dir = (argc >= 3) ? argv[2] : ".";
            int totalLines = 0, totalFiles = 0;
            int funcCount = 0, structCount = 0, enumCount = 0, classCount = 0, traitCount = 0;
            int importCount = 0;
            std::map<std::string, int> fileLines;

            for (auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".flux") continue;
                totalFiles++;
                std::ifstream ifs(entry.path());
                std::string line;
                int lines = 0;
                while (std::getline(ifs, line)) {
                    lines++;
                    totalLines++;
                    std::string trimmed = line;
                    // Trim leading whitespace
                    size_t start = trimmed.find_first_not_of(" \t");
                    if (start != std::string::npos) trimmed = trimmed.substr(start);
                    // Skip comments
                    if (trimmed.empty() || trimmed[0] == '#') continue;
                    // Count constructs
                    if (trimmed.substr(0, 4) == "def " || trimmed.substr(0, 11) == "async def ") funcCount++;
                    if (trimmed.substr(0, 7) == "struct ") structCount++;
                    if (trimmed.substr(0, 5) == "enum ") enumCount++;
                    if (trimmed.substr(0, 6) == "class ") classCount++;
                    if (trimmed.substr(0, 6) == "trait ") traitCount++;
                    if (trimmed.substr(0, 7) == "import " || trimmed.substr(0, 5) == "from ") importCount++;
                }
                std::string relPath = std::filesystem::relative(entry.path(), dir).string();
                fileLines[relPath] = lines;
            }

            std::cout << "FluxScript Project Statistics\n";
            std::cout << "=============================\n\n";
            std::cout << "  Files:       " << totalFiles << "\n";
            std::cout << "  Lines:       " << totalLines << "\n";
            std::cout << "  Functions:   " << funcCount << "\n";
            std::cout << "  Structs:     " << structCount << "\n";
            std::cout << "  Enums:       " << enumCount << "\n";
            std::cout << "  Classes:     " << classCount << "\n";
            std::cout << "  Traits:      " << traitCount << "\n";
            std::cout << "  Imports:     " << importCount << "\n\n";

            // Top 10 largest files
            if (!fileLines.empty()) {
                std::vector<std::pair<std::string, int>> sorted(fileLines.begin(), fileLines.end());
                std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
                int show = std::min(10, (int)sorted.size());
                std::cout << "Largest files:\n";
                for (int i = 0; i < show; i++) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "  %-40s %5d lines\n", sorted[i].first.c_str(), sorted[i].second);
                    std::cout << buf;
                }
            }
            return 0;
        }

        if (subcmd == "pkg") {
            std::string cmd = "flux-pkg";
            for (int i = 2; i < argc; i++)
                cmd += " " + std::string(argv[i]);
            return std::system(cmd.c_str());
        }

        // If it looks like a subcommand but wasn't recognized, show help
        if (subcmd[0] != '-' && subcmd.find(".flux") == std::string::npos) {
            llvm::errs() << "Unknown command: " << subcmd << "\n";
            llvm::errs() << "Run 'flux --help' for usage.\n";
            return 1;
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
    case EmitMode::ParseOnly:
        return parseCheckOnly(code);
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
