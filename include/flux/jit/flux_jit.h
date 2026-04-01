#ifndef FLUX_JIT_H
#define FLUX_JIT_H

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <memory>
#include <string>

namespace Flux {

enum class OptimizationLevel {
    O0,  // No optimization
    O1,  // Basic optimizations
    O2,  // Moderate optimizations
    O3   // Aggressive optimizations
};

class FluxJIT {
public:
    FluxJIT(OptimizationLevel optLevel = OptimizationLevel::O2);
    ~FluxJIT();

    void addModule(std::unique_ptr<llvm::Module> M);
    void* getPointerToFunction(const std::string& Name);
    void registerFunction(const std::string& Name, void* FuncPtr);

    void setOptimizationLevel(OptimizationLevel level);
    OptimizationLevel getOptimizationLevel() const { return m_optLevel; }

private:
    void registerNativeFunctions();
    void registerComplexHelpers();
    void optimizeModule(llvm::Module* M);

    std::unique_ptr<llvm::orc::LLJIT> m_lljit;
    std::unique_ptr<llvm::PassBuilder> m_passBuilder;
    OptimizationLevel m_optLevel;
};

} // namespace Flux

#endif // FLUX_JIT_H
