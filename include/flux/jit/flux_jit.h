#ifndef FLUX_JIT_H
#define FLUX_JIT_H

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/IR/DataLayout.h>
#include <memory>
#include <string>

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
    void* getPointerToFunction(const std::string& Name);
    void registerFunction(const std::string& Name, void* FuncPtr);

    void setOptimizationLevel(OptimizationLevel level);
    OptimizationLevel getOptimizationLevel() const { return m_optLevel; }

    void setSIMDOptions(const SIMDOptions& options);
    SIMDOptions getSIMDOptions() const;

    void setTCOOptions(const TCOOptions& options);
    TCOOptions getTCOOptions() const;

private:
    void registerNativeFunctions();
    void registerComplexHelpers();
    void optimizeModule(llvm::Module* M);
    void prepareModule(llvm::Module& M);

    std::unique_ptr<llvm::orc::LLJIT> m_lljit;
    std::unique_ptr<llvm::PassBuilder> m_passBuilder;
    llvm::DataLayout m_dataLayout;
    std::string m_targetTriple;
    OptimizationLevel m_optLevel;
    SIMDOptions m_simdOptions;
    TCOOptions m_tcoOptions;
};

} // namespace Flux

#endif // FLUX_JIT_H
