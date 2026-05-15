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

#include "flux/jit/flux_jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <iostream>
#include <cstdio>

// Complex number helper functions (C linkage for easy JIT binding)
extern "C" {
    // Extract real part from complex {double, double}
    double complex_real(double c_real, double c_imag) {
        return c_real;
    }

    // Extract imaginary part from complex {double, double}
    double complex_imag(double c_real, double c_imag) {
        return c_imag;
    }

    // Create complex from real and imaginary parts (identity function)
    double complex_make_real(double r, double i) {
        return r;
    }

    double complex_make_imag(double r, double i) {
        return i;
    }

    // Print string without newline
    void print_string(const char* str);
    
    // Print double
    void print_double(double d);
    
    // Print matrix
    void print_matrix(double* data, int rows, int cols);

    // String concatenation helpers
    const char* flux_string_concat_double(const char* s, double d);
    const char* flux_string_concat_matrix(const char* s, double* data, int rows, int cols);
    const char* flux_string_concat_string(const char* s1, const char* s2);

    // Matrix math helpers
    double flux_matrix_det(double* data, int rows, int cols);
    void* flux_matrix_inv(double* data, int rows, int cols);
    void* flux_matrix_eig(double* data, int rows, int cols);

    // Print string with newline (legacy, aliasing it)
    void println_string(const char* str) {
        printf("%s\n", str);
    }
}

namespace Flux {

namespace {

void logError(llvm::Error Err, llvm::StringRef Prefix) {
    if (!Err)
        return;

    llvm::errs() << Prefix << "\n";
    llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "  ");
}

} // namespace

FluxJIT::FluxJIT(OptimizationLevel optLevel)
    : m_dataLayout(""),
      m_optLevel(optLevel) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Load the process's dynamic library symbols for external function resolution
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        logError(JTMB.takeError(), "Failed to detect host");
        return;
    }
    JTMB->setCodeModel(llvm::CodeModel::Large);
    JTMB->setRelocationModel(llvm::Reloc::PIC_);
    
    auto JIT = llvm::orc::LLJITBuilder()
        .setJITTargetMachineBuilder(std::move(*JTMB))
        .setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES) {
            auto Layer = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, std::make_unique<llvm::jitlink::InProcessMemoryManager>(16384));
            return Layer;
        })
        .create();
    if (!JIT) {
        logError(JIT.takeError(), "Failed to create LLJIT");
        return;
    }
    m_lljit = std::move(*JIT);
    m_dataLayout = m_lljit->getDataLayout();
    m_targetTriple = llvm::sys::getProcessTriple();

    // Initialize pass builder for optimization
    m_passBuilder = std::make_unique<llvm::PassBuilder>();

    // Create a separate runtime dylib for built-in functions.
    // The main dylib falls back to runtime dylib, so user-defined functions
    // in the main dylib override runtime symbols with the same name.
    llvm::Expected<llvm::orc::JITDylib&> runtimeJDE =
        m_lljit->createJITDylib("flux_runtime");
    if (!runtimeJDE) {
        logError(runtimeJDE.takeError(), "Failed to create runtime JIT dylib");
        return;
    }
    m_runtimeDylib = &runtimeJDE.get();

    auto ProcessSymbols =
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            m_lljit->getDataLayout().getGlobalPrefix());
    if (!ProcessSymbols) {
        logError(ProcessSymbols.takeError(),
                 "Failed to create current-process symbol generator");
        return;
    }
    m_runtimeDylib->addGenerator(std::move(*ProcessSymbols));

    // Main dylib searches runtime dylib for unresolved symbols
    auto& mainJD = m_lljit->getMainJITDylib();
    mainJD.addToLinkOrder(*m_runtimeDylib);

    // Register complex number helper functions with the JIT
    registerComplexHelpers();
}

void FluxJIT::registerComplexHelpers() {
    if (!m_lljit || !m_runtimeDylib) return;

    auto registerSym = [&](const std::string& name, void* ptr) {
        llvm::orc::SymbolMap sym;
        sym[m_lljit->mangleAndIntern(name)] =
            { llvm::orc::ExecutorAddr::fromPtr(ptr), llvm::JITSymbolFlags::Exported };
        (void)m_runtimeDylib->define(llvm::orc::absoluteSymbols(std::move(sym)));
    };

    // Only register helpers defined locally in this file
    // Runtime functions from flux_runtime.cpp are found via ProcessSymbols generator
    registerSym("complex_real", reinterpret_cast<void*>(&complex_real));
    registerSym("complex_imag", reinterpret_cast<void*>(&complex_imag));
    registerSym("println_string", reinterpret_cast<void*>(&println_string));
}

FluxJIT::~FluxJIT() = default;

void FluxJIT::setOptimizationLevel(OptimizationLevel level) {
    m_optLevel = level;
}

void FluxJIT::setSIMDOptions(const SIMDOptions& options) {
    m_simdOptions = options;
}

SIMDOptions FluxJIT::getSIMDOptions() const {
    return m_simdOptions;
}

void FluxJIT::setTCOOptions(const TCOOptions& options) {
    m_tcoOptions = options;
}

TCOOptions FluxJIT::getTCOOptions() const {
    return m_tcoOptions;
}

void FluxJIT::optimizeModule(llvm::Module* M) {
    if (!M || m_optLevel == OptimizationLevel::O0) return;

    // Create the analysis managers
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    // Register the passes
    m_passBuilder->registerModuleAnalyses(MAM);
    m_passBuilder->registerCGSCCAnalyses(CGAM);
    m_passBuilder->registerFunctionAnalyses(FAM);
    m_passBuilder->registerLoopAnalyses(LAM);
    m_passBuilder->crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Create the optimization pipeline based on level
    // O2 and O3 include loop vectorization and SLP vectorization by default
    llvm::ModulePassManager MPM;

    switch (m_optLevel) {
        case OptimizationLevel::O1:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
            break;
        case OptimizationLevel::O2:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
            break;
        case OptimizationLevel::O3:
            MPM = m_passBuilder->buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
            break;
        case OptimizationLevel::O0:
        default:
            return;
    }

    // Run the optimization pipeline
    // Note: LLVM's O1, O2, and O3 pipelines automatically include:
    // - Tail Call Elimination (converts tail-recursive calls to jumps)
    // - Loop Vectorize Pass (for loop-level SIMD)
    // - SLP Vectorizer Pass (for straight-line code SIMD)
    // These passes detect opportunities for:
    //   * TCO: Converts eligible tail calls to jumps, preventing stack overflow
    //   * SIMD: AVX/AVX2/AVX-512 instructions based on target CPU
    MPM.run(*M, MAM);
}

void FluxJIT::prepareModule(llvm::Module& M) {
    if (!m_lljit)
        return;

    M.setDataLayout(m_dataLayout);
    M.setTargetTriple(llvm::Triple(m_targetTriple));
    M.setCodeModel(llvm::CodeModel::Large);
    M.setPICLevel(llvm::PICLevel::BigPIC);
}

void FluxJIT::addModule(std::unique_ptr<llvm::Module> M,
                        std::unique_ptr<llvm::LLVMContext> Ctx) {
    if (!m_lljit || !M || !Ctx)
        return;

    prepareModule(*M);

    if (llvm::verifyModule(*M, &llvm::errs())) {
        llvm::errs() << "Refusing to add invalid LLVM module to JIT\n";
        return;
    }

    // Optimize the module before adding to JIT
    optimizeModule(M.get());

    auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::move(Ctx));
    auto Err = m_lljit->addIRModule(std::move(TSM));
    if (Err) {
        logError(std::move(Err), "Failed to add module to JIT");
    }
}

void FluxJIT::addObjectFile(std::unique_ptr<llvm::MemoryBuffer> ObjectBuffer) {
    if (!m_lljit || !ObjectBuffer)
        return;

    if (auto Err = m_lljit->addObjectFile(std::move(ObjectBuffer))) {
        logError(std::move(Err), "Failed to add object file to JIT");
    }
}

void* FluxJIT::getPointerToFunction(const std::string& Name) {
    if (!m_lljit) return nullptr;

    auto ExprSymbol = m_lljit->lookup(Name);
    if (!ExprSymbol) {
        logError(ExprSymbol.takeError(), "Failed to find symbol: " + Name);
        return nullptr;
    }

    return reinterpret_cast<void*>(ExprSymbol->getValue());
}

void FluxJIT::registerFunction(const std::string& Name, void* FuncPtr) {
    if (!m_lljit || !m_runtimeDylib) return;

    llvm::orc::SymbolMap symMap;
    symMap[m_lljit->mangleAndIntern(Name)] =
        { llvm::orc::ExecutorAddr::fromPtr(FuncPtr), llvm::JITSymbolFlags::Exported };
    if (auto Err = m_runtimeDylib->define(llvm::orc::absoluteSymbols(std::move(symMap)))) {
        llvm::handleAllErrors(std::move(Err), [](const llvm::ErrorInfoBase &EI) {
            std::string msg = EI.message();
            if (msg.find("duplicate definition") == std::string::npos)
                llvm::errs() << "JIT symbol error: " << msg << "\n";
        });
    }
}

} // namespace Flux
