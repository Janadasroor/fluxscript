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

#ifndef FLUX_TOOLING_TOOLING_H
#define FLUX_TOOLING_TOOLING_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBuffer.h>

#include "flux/compiler/compiler_instance.h"

namespace Flux {

class JITEngine;

namespace Tooling {

struct AOTOptions
{
    std::string inputName = "<stdin>";
    std::string moduleName = "flux_module";
    std::string outputPath;
    OptimizationLevel optimizationLevel = OptimizationLevel::O2;
    bool debugInfo = false;
    bool sharedLibrary = false;
    /// Optional progress callback for large projects.
    CompileProgressCallback progressCallback = nullptr;

    /// Number of parallel jobs for function codegen (default 1 = sequential).
    int numJobs = 1;
};

struct ProfileResult
{
    double compileMs = 0.0;
    double runMs = 0.0;
    int iterations = 1;
    bool cacheHit = false;
};

struct TestCase
{
    int line = 0;
    std::string expression;
    double expected = 0.0;
    double tolerance = 1e-9;
};

struct TestSuiteResult
{
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

std::string defaultCacheDirectory();
std::string computeCacheKey(const std::string& code, const CompilerOptions& options,
                            const std::string& importHashes = "");
std::string computeImportGraphHash(const std::string& mainCode);

bool emitObjectBuffer(CompileArtifacts& artifacts, OptimizationLevel optimizationLevel,
                      std::unique_ptr<llvm::MemoryBuffer>& output, std::string* error = nullptr,
                      bool pic = false);
bool emitObjectFile(CompileArtifacts& artifacts, const std::string& outputPath, std::string* error = nullptr);
bool emitArtifact(const std::string& code, const AOTOptions& options, std::string* error = nullptr);

bool saveReturnTypes(const std::string& path, const std::map<std::string, FluxType>& returnTypes,
                     std::string* error = nullptr);
bool loadReturnTypes(const std::string& path, std::map<std::string, FluxType>& returnTypes,
                     std::string* error = nullptr);

std::string formatCode(const std::string& code);
std::string generateDocsMarkdown(const std::string& code, const std::string& moduleName);
std::vector<TestCase> discoverTests(const std::string& code);
TestSuiteResult runTests(const std::string& code, const std::vector<TestCase>& tests,
                         OptimizationLevel optLevel = OptimizationLevel::O2);
ProfileResult profileScript(JITEngine& engine, const std::string& code, const std::string& entryPoint, int iterations,
                            std::string* error = nullptr);

// Persistent JIT cache: bitcode save/load
bool saveBitcodeToFile(llvm::Module& M, const std::string& path, std::string* error = nullptr);
std::unique_ptr<llvm::Module> loadBitcodeFromFile(const std::string& path, llvm::LLVMContext& Ctx,
                                                  std::string* error = nullptr);

} // namespace Tooling
} // namespace Flux

#endif // FLUX_TOOLING_TOOLING_H
