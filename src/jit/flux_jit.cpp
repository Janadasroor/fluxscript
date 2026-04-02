#include "flux/jit/flux_jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/DynamicLibrary.h>
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

    // Print string to stdout (for print_string extern function)
    void print_string(const char* str) {
        printf("%s", str);
    }

    // Print string with newline
    void println_string(const char* str) {
        printf("%s\n", str);
    }
}

namespace Flux {

FluxJIT::FluxJIT(OptimizationLevel optLevel)
    : m_optLevel(optLevel) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Load the process's dynamic library symbols for external function resolution
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    auto JIT = llvm::orc::LLJITBuilder().create();
    if (!JIT) {
        std::cerr << "Failed to create LLJIT" << std::endl;
        return;
    }
    m_lljit = std::move(*JIT);

    // Initialize pass builder for optimization
    m_passBuilder = std::make_unique<llvm::PassBuilder>();
    
    // Register complex number helper functions with the JIT
    registerComplexHelpers();
}

void FluxJIT::registerComplexHelpers() {
    if (!m_lljit) return;

    auto& mainJD = m_lljit->getMainJITDylib();

    // Register complex_real helper
    llvm::orc::SymbolMap realSym;
    realSym[m_lljit->mangleAndIntern("complex_real")] =
        { llvm::orc::ExecutorAddr::fromPtr(&complex_real), llvm::JITSymbolFlags::Exported };
    (void)mainJD.define(llvm::orc::absoluteSymbols(std::move(realSym)));

    // Register complex_imag helper
    llvm::orc::SymbolMap imagSym;
    imagSym[m_lljit->mangleAndIntern("complex_imag")] =
        { llvm::orc::ExecutorAddr::fromPtr(&complex_imag), llvm::JITSymbolFlags::Exported };
    (void)mainJD.define(llvm::orc::absoluteSymbols(std::move(imagSym)));

    // Register print_string function
    llvm::orc::SymbolMap printSym;
    printSym[m_lljit->mangleAndIntern("print_string")] =
        { llvm::orc::ExecutorAddr::fromPtr(&print_string), llvm::JITSymbolFlags::Exported };
    (void)mainJD.define(llvm::orc::absoluteSymbols(std::move(printSym)));

    // Register println_string function
    llvm::orc::SymbolMap printlnSym;
    printlnSym[m_lljit->mangleAndIntern("println_string")] =
        { llvm::orc::ExecutorAddr::fromPtr(&println_string), llvm::JITSymbolFlags::Exported };
    (void)mainJD.define(llvm::orc::absoluteSymbols(std::move(printlnSym)));
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

void FluxJIT::addModule(std::unique_ptr<llvm::Module> M) {
    if (!m_lljit) return;

    // Optimize the module before adding to JIT
    optimizeModule(M.get());

    auto TSM = llvm::orc::ThreadSafeModule(std::move(M), std::make_unique<llvm::LLVMContext>());
    auto Err = m_lljit->addIRModule(std::move(TSM));
    if (Err) {
        std::cerr << "Failed to add module to JIT" << std::endl;
    }
}

void* FluxJIT::getPointerToFunction(const std::string& Name) {
    if (!m_lljit) return nullptr;

    auto ExprSymbol = m_lljit->lookup(Name);
    if (!ExprSymbol) {
        std::cerr << "Failed to find symbol: " << Name << std::endl;
        return nullptr;
    }

    return reinterpret_cast<void*>(ExprSymbol->getValue());
}

void FluxJIT::registerFunction(const std::string& Name, void* FuncPtr) {
    if (!m_lljit) return;

    auto& mainJD = m_lljit->getMainJITDylib();
    llvm::orc::SymbolMap symMap;
    symMap[m_lljit->mangleAndIntern(Name)] = 
        { llvm::orc::ExecutorAddr::fromPtr(FuncPtr), llvm::JITSymbolFlags::Exported };
    (void)mainJD.define(llvm::orc::absoluteSymbols(std::move(symMap)));
}

} // namespace Flux
