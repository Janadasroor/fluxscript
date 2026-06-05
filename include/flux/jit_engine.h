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

#ifndef FLUX_JIT_ENGINE_H
#define FLUX_JIT_ENGINE_H

#include <complex>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "flux/compiler/ast.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/compiler/module_loader.h"
#include "flux/jit/flux_jit.h"

#ifdef emit
#undef emit
#endif

namespace Flux {

class FluxJIT;
class Parser;

struct MatrixResult
{
    void* ptr;
    int rows;
    int cols;
};

struct VectorResult
{
    double* data;
    int len;
};

using FluxValue = std::variant<double, std::complex<double>, int, MatrixResult, VectorResult>;

class JITEngine
{
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
    void clearJITCache();

    // Module system integration
    bool importModule(const std::string& moduleName, std::string* error = nullptr);
    bool loadPlugin(const std::string& pluginPath, std::string* error = nullptr);
    bool loadPrecompiledObject(const std::string& path, std::string* error = nullptr);

    // Reflection API
    std::vector<std::string> getLoadedModules() const;
    std::vector<std::string> getModuleExports(const std::string& moduleName) const;
    std::string getFunctionSignature(const std::string& functionName) const;

    // Configuration
    void setDefine(const std::string& name, bool value);
    bool getDefine(const std::string& name) const;
    void registerFunction(const std::string& name, void* funcPtr);
    void setOptimizationLevelForModules(OptimizationLevel level);
    void setInputName(const std::string& inputName) { m_compilerOptions.inputName = inputName; }

    // std::string overloads (for minimal CLI)
    bool executeString(const std::string& code, std::string* error = nullptr);
    bool compileScript(const std::string& code, std::string* error = nullptr);

    CodegenContext& context() { return *m_codegenCtx; }

    // Call a compiled function.
    FluxValue callFunction(const std::string& name, const std::vector<double>& args, std::string* error = nullptr);
    void* getFunctionPointer(const std::string& name);

    // --- Tiered JIT ---
    void* promoteFunction(const std::string& name, OptimizationLevel targetLevel = OptimizationLevel::O3);
    void setTieredJITThreshold(int n);
    int getTieredJITThreshold() const;
    int getFunctionCallCount(const std::string& name) const;
    void resetCallCounts();
    bool isFunctionPromoted(const std::string& name) const;

private:
    JITEngine();
    ~JITEngine();

    void registerEigenFunctions();

    // Function overloading support
    std::string getFunctionSignature(const std::string& name, const std::vector<FluxType>& argTypes);
    void registerOverloadedFunction(const std::string& name, const std::vector<FluxType>& argTypes,
                                    llvm::Function* func);
    llvm::Function* resolveOverloadedFunction(const std::string& name, const std::vector<FluxType>& argTypes);

    std::unique_ptr<FluxJIT> m_jit;
    std::unique_ptr<CodegenContext> m_codegenCtx;
    CompilerOptions m_compilerOptions;
    std::map<std::string, FluxType> m_functionReturnTypes;
    std::map<std::string, llvm::Function*> m_overloadedFunctions; // signature -> function
    std::set<std::string> m_importedModules;
    std::string m_cacheDirectory;
    OptimizationLevel m_optLevel = OptimizationLevel::O2;
    bool m_cacheEnabled = true;
    bool m_lastCompileUsedCache = false;
    bool m_initialized = false;
    ModuleLoader m_moduleLoader;
};

} // namespace Flux

#endif // FLUX_JIT_ENGINE_H
