#ifndef FLUX_JIT_ENGINE_H
#define FLUX_JIT_ENGINE_H

#include <memory>
#include <vector>
#include <variant>
#include <complex>
#include <string>
#include <map>
#include <set>

#include "flux/compiler/ast.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/jit/flux_jit.h"
#include "flux/modules/module_loader.h"

#ifdef FLUX_HAS_QT
#include <QString>
#endif

#ifdef emit
#undef emit
#endif

namespace Flux {

class FluxJIT;
class Parser;

struct MatrixResult {
    void* ptr;
    int rows;
    int cols;
};

using FluxValue = std::variant<double, std::complex<double>, int, MatrixResult>;

class JITEngine {
public:
    static JITEngine& instance();

    void initialize();
    void finalize();
    bool isInitialized() const;

    // Optimization level control
    void setOptimizationLevel(OptimizationLevel level);
    OptimizationLevel getOptimizationLevel() const;
    void setJITCacheEnabled(bool enabled) { m_cacheEnabled = enabled; }
    bool isJITCacheEnabled() const { return m_cacheEnabled; }
    void setJITCacheDirectory(std::string directory) { m_cacheDirectory = std::move(directory); }
    const std::string& getJITCacheDirectory() const { return m_cacheDirectory; }
    bool lastCompileUsedCache() const { return m_lastCompileUsedCache; }

    // Module system integration
    ModuleLoader& getModuleLoader() { return ModuleLoader::instance(); }
    bool importModule(const std::string& moduleName, std::string* error = nullptr);
    bool loadPlugin(const std::string& pluginPath, std::string* error = nullptr);
    
    // Reflection API
    std::vector<std::string> getLoadedModules() const;
    std::vector<std::string> getModuleExports(const std::string& moduleName) const;
    std::string getFunctionSignature(const std::string& functionName) const;
    
    // Configuration
    void setDefine(const std::string& name, bool value);
    bool getDefine(const std::string& name) const;
    void setOptimizationLevelForModules(OptimizationLevel level);

#ifdef FLUX_HAS_QT
    // QString overloads (for Qt-based GUI)
    bool executeString(const QString& code, QString* error = nullptr);
    bool compileScript(const QString& code, QString* error = nullptr);
#endif

    // std::string overloads (for minimal CLI)
    bool executeString(const std::string& code, std::string* error = nullptr);
    bool compileScript(const std::string& code, std::string* error = nullptr);

    CodegenContext& context() { return *m_codegenCtx; }

    // Call a compiled function.
    FluxValue callFunction(const std::string& name, const std::vector<double>& args, std::string* error = nullptr);
#ifdef FLUX_HAS_QT
    // Qt overload (only available when Qt is enabled)
    FluxValue callFunction(const std::string& name, const std::vector<double>& args, QString* error);
#endif

private:
    JITEngine();
    ~JITEngine();

    void registerEigenFunctions();

    // Function overloading support
    std::string getFunctionSignature(const std::string& name, const std::vector<FluxType>& argTypes);
    void registerOverloadedFunction(const std::string& name, const std::vector<FluxType>& argTypes, llvm::Function* func);
    llvm::Function* resolveOverloadedFunction(const std::string& name, const std::vector<FluxType>& argTypes);

    std::unique_ptr<FluxJIT> m_jit;
    std::unique_ptr<CodegenContext> m_codegenCtx;
    CompilerOptions m_compilerOptions;
    std::map<std::string, FluxType> m_functionReturnTypes;
    std::map<std::string, llvm::Function*> m_overloadedFunctions;  // signature -> function
    std::set<std::string> m_importedModules;
    std::string m_cacheDirectory;
    bool m_cacheEnabled = true;
    bool m_lastCompileUsedCache = false;
    bool m_initialized = false;
};

} // namespace Flux

#endif // FLUX_JIT_ENGINE_H
