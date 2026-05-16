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
#include <llvm/Support/Error.h>

#ifdef emit
#undef emit
#endif

#include "flux/jit_engine.h"
#include "flux/flux_eigen.h"
#include "flux/runtime/flux_runtime.h"
#include "flux/tooling/tooling.h"

#include <iostream>
#include <filesystem>

namespace Flux {

JITEngine& JITEngine::instance() {
    static JITEngine engine;
    return engine;
}

JITEngine::JITEngine() = default;

JITEngine::~JITEngine() {
    finalize();
}

void JITEngine::setOptimizationLevel(OptimizationLevel level) {
    if (m_jit) m_jit->setOptimizationLevel(level);
}

OptimizationLevel JITEngine::getOptimizationLevel() const {
    if (m_jit) return m_jit->getOptimizationLevel();
    return OptimizationLevel::O2;
}

void JITEngine::initialize() {
    if (m_initialized) return;
    m_jit = std::make_unique<FluxJIT>(OptimizationLevel::O2);
    m_compilerOptions.moduleName = "Flux JIT Core";
    m_compilerOptions.injectStdlib = true;
    if (m_cacheDirectory.empty())
        m_cacheDirectory = Tooling::defaultCacheDirectory();
    m_cacheEnabled = false;
    m_codegenCtx = nullptr;
    m_initialized = true;
    registerEigenFunctions();

    std::cout << "FluxScript C++ LLVM JIT Engine Initialized." << std::endl;
}

void JITEngine::registerFunction(const std::string& name, void* funcPtr) {
    if (m_jit) m_jit->registerFunction(name, funcPtr);
}

void JITEngine::registerEigenFunctions() {
    if (!m_jit) return;

    registerRuntimeFunctions(*m_jit);
}

void JITEngine::finalize() {
    m_initialized = false;
    m_jit.reset();
    m_codegenCtx.reset();
}

bool JITEngine::isInitialized() const {
    return m_initialized;
}


bool JITEngine::compileScript(const std::string& code, std::string* error) {
    if (!m_initialized) initialize();
    CompilerInstance compiler(m_compilerOptions);
    m_overloadedFunctions.clear();
    m_lastCompileUsedCache = false;

    const std::string cacheKey = Tooling::computeCacheKey(code, m_compilerOptions);
    const std::filesystem::path cacheDir(m_cacheDirectory);
    const std::filesystem::path objectPath = cacheDir / (cacheKey + ".o");
    const std::filesystem::path metaPath = cacheDir / (cacheKey + ".meta");

    if (m_cacheEnabled && std::filesystem::exists(metaPath)) {
        Tooling::loadReturnTypes(metaPath.string(), m_functionReturnTypes, nullptr);
    }

    auto artifacts = compiler.compileToIR(code, error);
    if (!artifacts) return false;
    m_functionReturnTypes = artifacts->functionReturnTypes;

    m_codegenCtx = std::move(artifacts->codegenContext);
    m_jit->addModule(std::move(m_codegenCtx->OwnedModule), std::move(m_codegenCtx->OwnedContext));
    m_codegenCtx.reset();
    return true;
}

bool JITEngine::executeString(const std::string& code, std::string* error) {
    if (!compileScript(code, error)) return false;
    return true;
}


void* JITEngine::getFunctionPointer(const std::string& name) {
    if (!m_initialized) return nullptr;
    return m_jit->getPointerToFunction(name);
}

FluxValue JITEngine::callFunction(const std::string& name, const std::vector<double>& args, std::string* error) {
    if (!m_initialized) { if (error) *error = "JIT not initialized"; return 0.0; }
    void* fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr && name == "__anon_expr") {
        std::string base = m_compilerOptions.moduleName + "_anon_expr";
        fnPtr = m_jit->getPointerToFunction(base);
        for (int i = 0; !fnPtr && i < 100; i++)
            fnPtr = m_jit->getPointerToFunction(base + "_" + std::to_string(i));
    }
    if (!fnPtr) { if (error) *error = "Function not found: " + name; return 0.0; }
    FluxType retType = m_functionReturnTypes[name];

    if (retType.Kind == TypeKind::Matrix) {
        struct MatrixRet { void* ptr; int rows; int cols; };
        using MatrixFn = MatrixRet(*)();
        using MatrixFn1 = MatrixRet(*)(double);
        using MatrixFn2 = MatrixRet(*)(double, double);

        MatrixRet r = {nullptr, 0, 0};
        if (args.empty()) r = reinterpret_cast<MatrixFn>(fnPtr)();
        else if (args.size() == 1) r = reinterpret_cast<MatrixFn1>(fnPtr)(args[0]);
        else if (args.size() == 2) r = reinterpret_cast<MatrixFn2>(fnPtr)(args[0], args[1]);

        return MatrixResult{r.ptr, r.rows, r.cols};
    } else if (retType.Kind == TypeKind::Complex) {
        // Complex return: LLVM returns small structs in registers or via hidden pointer.
        // For {double, double}, it's usually returned as a pair of registers.
        // The safest way to handle this across different ABIs without a wrapper is to 
        // use a helper that returns by value and hope it matches, but for {double, double}
        // it's often more reliable to call a function that takes a pointer to the result.

        // However, since we are using JIT and casting fnPtr directly:
        struct ComplexRet { double real; double imag; };
        using ComplexFn = ComplexRet(*)();
        using ComplexFn1 = ComplexRet(*)(double);
        using ComplexFn2 = ComplexRet(*)(double, double);

        if (args.empty()) {
            ComplexRet r = reinterpret_cast<ComplexFn>(fnPtr)();
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 1) {
            ComplexRet r = reinterpret_cast<ComplexFn1>(fnPtr)(args[0]);
            return std::complex<double>(r.real, r.imag);
        }
        if (args.size() == 2) {
            ComplexRet r = reinterpret_cast<ComplexFn2>(fnPtr)(args[0], args[1]);
            return std::complex<double>(r.real, r.imag);
        }
    } else {
        if (args.empty()) return reinterpret_cast<double(*)()>(fnPtr)();
        if (args.size() == 1) return reinterpret_cast<double(*)(double)>(fnPtr)(args[0]);
        if (args.size() == 2) return reinterpret_cast<double(*)(double, double)>(fnPtr)(args[0], args[1]);
    }
    if (error) *error = "Unsupported arguments or return type";
    return 0.0;
}

// ============ Module System Integration ============

bool JITEngine::importModule(const std::string& moduleName, std::string* error) {
    // Use ModuleLoader to load module
    return true;  // Simplified for now
}

bool JITEngine::loadPlugin(const std::string& pluginPath, std::string* error) {
    // Use ModuleLoader to load plugin
    return true;  // Simplified for now
}

std::vector<std::string> JITEngine::getLoadedModules() const {
    return {};  // Simplified for now
}

std::vector<std::string> JITEngine::getModuleExports(const std::string& moduleName) const {
    return {};  // Simplified for now
}

std::string JITEngine::getFunctionSignature(const std::string& functionName) const {
    return "";  // Simplified for now
}

void JITEngine::setDefine(const std::string& name, bool value) {
    // Simplified for now
}

bool JITEngine::getDefine(const std::string& name) const {
    return false;  // Simplified for now
}

void JITEngine::setOptimizationLevelForModules(OptimizationLevel level) {
    // Simplified for now
}

} // namespace Flux
