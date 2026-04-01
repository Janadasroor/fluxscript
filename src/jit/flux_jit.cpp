#include "flux/jit/flux_jit.h"
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/DynamicLibrary.h>
#include <iostream>

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
}

FluxJIT::~FluxJIT() = default;

void FluxJIT::setOptimizationLevel(OptimizationLevel level) {
    m_optLevel = level;
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
