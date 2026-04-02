#include "flux/jit/flux_jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
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

    // Print string with newline
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

    auto JIT = llvm::orc::LLJITBuilder().create();
    if (!JIT) {
        logError(JIT.takeError(), "Failed to create LLJIT");
        return;
    }
    m_lljit = std::move(*JIT);
    m_dataLayout = m_lljit->getDataLayout();
    m_targetTriple = llvm::sys::getProcessTriple();

    // Initialize pass builder for optimization
    m_passBuilder = std::make_unique<llvm::PassBuilder>();

    auto ProcessSymbols =
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            m_lljit->getDataLayout().getGlobalPrefix());
    if (!ProcessSymbols) {
        logError(ProcessSymbols.takeError(),
                 "Failed to create current-process symbol generator");
        return;
    }

    m_lljit->getMainJITDylib().addGenerator(std::move(*ProcessSymbols));
    
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

void FluxJIT::prepareModule(llvm::Module& M) {
    if (!m_lljit)
        return;

    M.setDataLayout(m_dataLayout);
    M.setTargetTriple(m_targetTriple);
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
    if (!m_lljit) return;

    auto& mainJD = m_lljit->getMainJITDylib();
    llvm::orc::SymbolMap symMap;
    symMap[m_lljit->mangleAndIntern(Name)] = 
        { llvm::orc::ExecutorAddr::fromPtr(FuncPtr), llvm::JITSymbolFlags::Exported };
    if (auto Err = mainJD.define(llvm::orc::absoluteSymbols(std::move(symMap)))) {
        logError(std::move(Err), "Failed to register JIT symbol: " + Name);
    }
}

} // namespace Flux
