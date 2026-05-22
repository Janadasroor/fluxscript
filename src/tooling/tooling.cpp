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

#include "flux/tooling/tooling.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <set>
#include <sstream>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include "flux/jit_engine.h"

namespace Flux::Tooling {

namespace fs = std::filesystem;

namespace {

std::string shellQuote(const std::string& value)
{
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'')
            quoted += "'\\''";
        else
            quoted += ch;
    }
    quoted += "'";
    return quoted;
}

std::string trim(const std::string& value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string typeName(TypeKind kind)
{
    switch (kind) {
    case TypeKind::Auto:
        return "auto";
    case TypeKind::Double:
        return "double";
    case TypeKind::Float:
        return "float";
    case TypeKind::Int:
        return "int";
    case TypeKind::Void:
        return "void";
    case TypeKind::Complex:
        return "complex";
    case TypeKind::String:
        return "string";
    case TypeKind::Matrix:
        return "matrix";
    case TypeKind::Vector:
        return "vector";
    }
    return "double";
}

bool ensureParentDirectory(const std::string& path, std::string* error)
{
    std::error_code ec;
    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty())
        fs::create_directories(parent, ec);
    if (ec) {
        if (error)
            *error = ec.message();
        return false;
    }
    return true;
}

std::unique_ptr<llvm::TargetMachine> createTargetMachine(const OptimizationLevel level, std::string* error)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    const std::string triple = llvm::sys::getProcessTriple();
    std::string lookupError;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, lookupError);
    if (!target) {
        if (error)
            *error = lookupError;
        return nullptr;
    }

    llvm::TargetOptions options;
    llvm::CodeGenOptLevel codegenLevel = llvm::CodeGenOptLevel::Default;
    switch (level) {
    case OptimizationLevel::O0:
        codegenLevel = llvm::CodeGenOptLevel::None;
        break;
    case OptimizationLevel::O1:
        codegenLevel = llvm::CodeGenOptLevel::Less;
        break;
    case OptimizationLevel::O2:
        codegenLevel = llvm::CodeGenOptLevel::Default;
        break;
    case OptimizationLevel::O3:
        codegenLevel = llvm::CodeGenOptLevel::Aggressive;
        break;
    }

    return std::unique_ptr<llvm::TargetMachine>(
        target->createTargetMachine(llvm::Triple(triple), llvm::sys::getHostCPUName().str(), "", options, std::nullopt,
                                    std::nullopt, codegenLevel));
}

bool writeBufferToFile(llvm::MemoryBuffer& buffer, const std::string& outputPath, std::string* error)
{
    if (!ensureParentDirectory(outputPath, error))
        return false;

    std::error_code ec;
    llvm::raw_fd_ostream stream(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        if (error)
            *error = ec.message();
        return false;
    }

    stream << buffer.getBuffer();
    return true;
}

std::string baseNameWithoutExtension(const std::string& inputName)
{
    const fs::path path(inputName.empty() ? "module" : inputName);
    return path.stem().empty() ? std::string("module") : path.stem().string();
}

} // namespace

std::string defaultCacheDirectory()
{
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg && *xdg)
        return (fs::path(xdg) / "fluxscript" / "jit").string();

    const char* home = std::getenv("HOME");
    if (home && *home)
        return (fs::path(home) / ".cache" / "fluxscript" / "jit").string();

    return ".fluxcache/jit";
}

std::string computeImportGraphHash(const std::string& mainCode)
{
    static const std::regex importRe(R"(^\s*import\s+(\w+))");
    std::set<std::string> visited;
    std::vector<std::string> toVisit;
    toVisit.push_back(mainCode);

    llvm::SHA256 hasher;
    bool hasContent = false;

    while (!toVisit.empty()) {
        std::string source = std::move(toVisit.back());
        toVisit.pop_back();

        std::smatch m;
        std::string::const_iterator it = source.cbegin();
        while (std::regex_search(it, source.cend(), m, importRe)) {
            std::string modName = m[1].str();
            it = m.suffix().first;

            if (visited.find(modName) != visited.end())
                continue;
            visited.insert(modName);

            // Try to find the module file using standard search paths
            std::vector<fs::path> searchPaths = {
                fs::current_path(),
                fs::current_path() / "modules",
                fs::current_path() / "lib",
                fs::current_path() / "stdlib",
            };
            const char* home = std::getenv("HOME");
            if (home) {
                searchPaths.push_back(fs::path(home) / ".flux" / "modules");
                searchPaths.push_back(fs::path(home) / ".flux" / "stdlib");
            }
            searchPaths.push_back("/usr/local/share/flux/modules");
            searchPaths.push_back("/usr/share/flux/modules");

            fs::path modPath;
            for (const auto& dir : searchPaths) {
                auto candidate = dir / (modName + ".flux");
                if (fs::exists(candidate)) {
                    modPath = candidate;
                    break;
                }
            }
            if (modPath.empty())
                continue;

            // Read and hash the module file
            auto buf = llvm::MemoryBuffer::getFile(modPath.string());
            if (!buf)
                continue;
            llvm::StringRef content = buf.get()->getBuffer();
            hasher.update(content);
            hasContent = true;

            // Recursively scan the imported file for its own imports
            toVisit.push_back(content.str());
        }
    }

    if (!hasContent)
        return "";

    auto hash = hasher.final();
    std::ostringstream os;
    for (uint8_t byte : hash)
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return os.str();
}

std::string computeCacheKey(const std::string& code, const CompilerOptions& options,
                            const std::string& importHashes)
{
    const std::string payload = code + "|" + options.inputName + "|" + options.moduleName + "|" +
                                std::to_string(static_cast<int>(options.optimizationLevel)) + "|" +
                                (options.debugInfo ? "dbg" : "nodbg") + "|" + importHashes;
    llvm::SHA256 hasher;
    hasher.update(payload);
    auto hash = hasher.final();
    std::ostringstream os;
    for (uint8_t byte : hash)
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return os.str();
}

bool emitObjectBuffer(CompileArtifacts& artifacts, OptimizationLevel optimizationLevel,
                      std::unique_ptr<llvm::MemoryBuffer>& output, std::string* error)
{
    auto targetMachine = createTargetMachine(optimizationLevel, error);
    if (!targetMachine)
        return false;

    auto& module = *artifacts.codegenContext->TheModule;
    module.setTargetTriple(llvm::Triple(llvm::sys::getProcessTriple()));
    module.setDataLayout(targetMachine->createDataLayout());

    llvm::SmallVector<char, 0> objectBytes;
    llvm::raw_svector_ostream objectStream(objectBytes);
    llvm::legacy::PassManager passManager;
    if (targetMachine->addPassesToEmitFile(passManager, objectStream, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        if (error)
            *error = "Target machine could not emit an object file.";
        return false;
    }

    passManager.run(module);
    output = llvm::MemoryBuffer::getMemBufferCopy(llvm::StringRef(objectBytes.data(), objectBytes.size()),
                                                  module.getName().str() + ".o");
    return true;
}

bool emitObjectFile(CompileArtifacts& artifacts, const std::string& outputPath, std::string* error)
{
    std::unique_ptr<llvm::MemoryBuffer> objectBuffer;
    if (!emitObjectBuffer(artifacts, OptimizationLevel::O2, objectBuffer, error))
        return false;

    return writeBufferToFile(*objectBuffer, outputPath, error);
}

bool emitArtifact(const std::string& code, const AOTOptions& options, std::string* error)
{
    CompilerOptions compilerOptions;
    compilerOptions.inputName = options.inputName;
    compilerOptions.moduleName =
        options.moduleName.empty() ? baseNameWithoutExtension(options.inputName) : options.moduleName;
    compilerOptions.optimizationLevel = options.optimizationLevel;
    compilerOptions.debugInfo = options.debugInfo;

    CompilerInstance compiler(compilerOptions);
    auto artifacts = compiler.compileToIR(code, error);
    if (!artifacts)
        return false;

    const bool emitShared = options.sharedLibrary || fs::path(options.outputPath).extension() == ".so";
    std::unique_ptr<llvm::MemoryBuffer> objectBuffer;
    if (!emitObjectBuffer(*artifacts, options.optimizationLevel, objectBuffer, error))
        return false;

    if (!emitShared)
        return writeBufferToFile(*objectBuffer, options.outputPath, error);

    fs::path objectPath = fs::path(options.outputPath).replace_extension(".o");
    if (!writeBufferToFile(*objectBuffer, objectPath.string(), error))
        return false;

    const std::string command =
        "c++ -shared -o " + shellQuote(options.outputPath) + " " + shellQuote(objectPath.string());
    if (std::system(command.c_str()) != 0) {
        if (error)
            *error = "Failed to link shared library with system C++ driver.";
        return false;
    }

    return true;
}

bool saveReturnTypes(const std::string& path, const std::map<std::string, FluxType>& returnTypes, std::string* error)
{
    if (!ensureParentDirectory(path, error))
        return false;

    std::ofstream out(path);
    if (!out) {
        if (error)
            *error = "Could not open metadata file for writing.";
        return false;
    }

    for (const auto& [name, type] : returnTypes)
        out << name << " " << static_cast<int>(type.Kind) << "\n";
    return true;
}

bool loadReturnTypes(const std::string& path, std::map<std::string, FluxType>& returnTypes, std::string* error)
{
    std::ifstream in(path);
    if (!in) {
        if (error)
            *error = "Could not open metadata file.";
        return false;
    }

    returnTypes.clear();
    std::string name;
    int kind = 0;
    while (in >> name >> kind)
        returnTypes[name] = FluxType(static_cast<TypeKind>(kind));
    return true;
}

std::string formatCode(const std::string& code)
{
    std::istringstream input(code);
    std::ostringstream out;
    std::string line;
    int indent = 0;

    while (std::getline(input, line)) {
        const std::string stripped = trim(line);
        if (stripped.empty()) {
            out << "\n";
            continue;
        }

        if (!stripped.empty() && stripped.front() == '}')
            indent = std::max(0, indent - 1);

        out << std::string(static_cast<size_t>(indent) * 4, ' ') << stripped << "\n";

        if (!stripped.empty() && stripped.back() == '{')
            ++indent;
    }

    return out.str();
}

std::string generateDocsMarkdown(const std::string& code, const std::string& moduleName)
{
    std::istringstream input(code);
    std::ostringstream out;
    out << "# " << moduleName << "\n\n";

    std::vector<std::string> pendingDocs;
    std::string line;
    while (std::getline(input, line)) {
        const std::string stripped = trim(line);
        if (stripped.rfind("///", 0) == 0 || stripped.rfind("##", 0) == 0) {
            pendingDocs.push_back(trim(stripped.substr(stripped[1] == '/' ? 3 : 2)));
            continue;
        }

        if (stripped.rfind("def ", 0) == 0 || stripped.rfind("extern ", 0) == 0) {
            out << "## `" << stripped << "`\n\n";
            if (!pendingDocs.empty()) {
                for (const auto& doc : pendingDocs)
                    out << doc << "\n\n";
                pendingDocs.clear();
            } else {
                out << "No docstring provided.\n\n";
            }
        } else if (!stripped.empty()) {
            pendingDocs.clear();
        }
    }

    return out.str();
}

std::vector<TestCase> discoverTests(const std::string& code)
{
    std::vector<TestCase> tests;
    std::istringstream input(code);
    std::regex pattern(
        R"(^\s*(#|//)\s*TEST:\s*(.*?)\s*=>\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*(?:\+/-\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))?\s*$)");

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::smatch match;
        if (!std::regex_match(line, match, pattern))
            continue;

        TestCase test;
        test.line = lineNumber;
        test.expression = trim(match[2].str());
        test.expected = std::stod(match[3].str());
        if (match[4].matched)
            test.tolerance = std::stod(match[4].str());
        tests.push_back(std::move(test));
    }

    return tests;
}

TestSuiteResult runTests(const std::string& code, const std::vector<TestCase>& tests, OptimizationLevel optLevel)
{
    TestSuiteResult result;
    for (const auto& test : tests) {
        auto& engine = JITEngine::instance();
        engine.finalize();
        engine.initialize();
        engine.setOptimizationLevel(optLevel);

        std::string error;
        if (!engine.compileScript(code + "\n" + test.expression + "\n", &error)) {
            ++result.failed;
            result.failures.push_back("Line " + std::to_string(test.line) + ": compile failed: " + error);
            continue;
        }

        const auto value = engine.callFunction("__anon_expr", {}, &error);
        if (!error.empty() || !std::holds_alternative<double>(value)) {
            ++result.failed;
            result.failures.push_back("Line " + std::to_string(test.line) + ": runtime failed: " + error);
            continue;
        }

        const double actual = std::get<double>(value);
        if (std::fabs(actual - test.expected) <= test.tolerance) {
            ++result.passed;
        } else {
            ++result.failed;
            std::ostringstream message;
            message << "Line " << test.line << ": expected " << test.expected << " +/- " << test.tolerance << ", got "
                    << actual;
            result.failures.push_back(message.str());
        }
    }

    return result;
}

ProfileResult profileScript(JITEngine& engine, const std::string& code, const std::string& entryPoint, int iterations,
                            std::string* error)
{
    ProfileResult result;
    result.iterations = std::max(1, iterations);

    engine.finalize();
    engine.initialize();

    const auto compileStart = std::chrono::steady_clock::now();
    if (!engine.compileScript(code, error))
        return result;
    const auto compileEnd = std::chrono::steady_clock::now();

    const auto runStart = std::chrono::steady_clock::now();
    for (int i = 0; i < result.iterations; ++i) {
        std::string callError;
        (void)engine.callFunction(entryPoint, {}, &callError);
        if (!callError.empty()) {
            if (error)
                *error = callError;
            return result;
        }
    }
    const auto runEnd = std::chrono::steady_clock::now();

    result.compileMs = std::chrono::duration<double, std::milli>(compileEnd - compileStart).count();
    result.runMs = std::chrono::duration<double, std::milli>(runEnd - runStart).count();
    return result;
}

// ---- Persistent JIT Cache: bitcode save/load ----

bool saveBitcodeToFile(llvm::Module& M, const std::string& path, std::string* error)
{
    if (!ensureParentDirectory(path, error))
        return false;

    std::error_code ec;
    llvm::raw_fd_ostream OS(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        if (error)
            *error = ec.message();
        return false;
    }
    llvm::WriteBitcodeToFile(M, OS);
    OS.flush();
    return true;
}

std::unique_ptr<llvm::Module> loadBitcodeFromFile(const std::string& path, llvm::LLVMContext& Ctx, std::string* error)
{
    auto buf = llvm::MemoryBuffer::getFile(path);
    if (!buf) {
        if (error)
            *error = "Could not open bitcode file: " + path;
        return nullptr;
    }
    auto mod = llvm::parseBitcodeFile((*buf)->getMemBufferRef(), Ctx);
    if (!mod) {
        if (error) {
            llvm::raw_string_ostream os(*error);
            llvm::handleAllErrors(mod.takeError(), [&](const llvm::ErrorInfoBase& EI) { os << EI.message(); });
        }
        return nullptr;
    }
    return std::move(*mod);
}

} // namespace Flux::Tooling
