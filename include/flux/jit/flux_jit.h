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

#ifndef FLUX_JIT_H
#define FLUX_JIT_H

// Undefine Qt's emit macro before including LLVM headers
// Qt defines 'emit' as a macro which conflicts with LLVM-18's Orc JIT layer
#ifdef emit
#undef emit
#endif

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR >= 22
#include <llvm/Plugins/PassPlugin.h>
#else
#include <llvm/Passes/PassPlugin.h>
#endif
#include <llvm/IR/DataLayout.h>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Flux {

enum class OptimizationLevel {
    O0,  // No optimization
    O1,  // Basic optimizations
    O2,  // Moderate optimizations
    O3   // Aggressive optimizations
};

struct SIMDOptions {
    bool enableLoopVectorization = true;   // Enable loop vectorizer
    bool enableSLPVectorization = true;    // Enable SLP (Superword-Level Parallelism) vectorizer
    bool enableInterleavedAccess = true;   // Enable interleaved memory access optimization
    int vectorizationFactor = 0;           // 0 = auto, otherwise specify factor (2, 4, 8, etc.)
};

struct TCOOptions {
    bool enableTailCallElimination = true;  // Enable tail call optimization
    bool enableTailCallMerging = true;      // Enable tail call merging for mutual recursion
    int maxRecursionDepth = 0;              // 0 = unlimited (with TCO), otherwise limit depth
};

class FluxJIT {
public:
    FluxJIT(OptimizationLevel optLevel = OptimizationLevel::O2);
    ~FluxJIT();

    void addModule(std::unique_ptr<llvm::Module> M, std::unique_ptr<llvm::LLVMContext> Ctx);
    void addObjectFile(std::unique_ptr<llvm::MemoryBuffer> ObjectBuffer);
    void* getPointerToFunction(const std::string& Name);
    void registerFunction(const std::string& Name, void* FuncPtr);

    void setOptimizationLevel(OptimizationLevel level);
    OptimizationLevel getOptimizationLevel() const { return m_optLevel; }

    void setSIMDOptions(const SIMDOptions& options);
    SIMDOptions getSIMDOptions() const;

    void setTCOOptions(const TCOOptions& options);
    TCOOptions getTCOOptions() const;

    const llvm::DataLayout& getDataLayout() const { return m_dataLayout; }
    const std::string& getTargetTriple() const { return m_targetTriple; }

private:
    void registerNativeFunctions();
    void registerComplexHelpers();
    void optimizeModule(llvm::Module* M);
    void prepareModule(llvm::Module& M);

    std::unique_ptr<llvm::orc::LLJIT> m_lljit;
    std::unique_ptr<llvm::PassBuilder> m_passBuilder;
    llvm::orc::JITDylib* m_runtimeDylib = nullptr;
    llvm::DataLayout m_dataLayout;
    std::string m_targetTriple;
    OptimizationLevel m_optLevel;
    SIMDOptions m_simdOptions;
    TCOOptions m_tcoOptions;

    // Eagerly compiled function pointer cache (avoids ORC lazy materialization issues)
    std::unordered_map<std::string, void*> m_functionPtrs;
    std::mutex m_fnMapMutex;
};

} // namespace Flux

#endif // FLUX_JIT_H
