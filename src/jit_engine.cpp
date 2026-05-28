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
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/Error.h>

#ifdef emit
#undef emit
#endif

#include "flux/compiler/module_loader.h"
#include "flux/flux_eigen.h"
#include "flux/jit_engine.h"
#include "flux/runtime/flux_runtime.h"
#include "flux/tooling/tooling.h"

#include <filesystem>
#include <iostream>

extern "C" {
void flux_inc_call_depth();
void flux_dec_call_depth();
}

namespace Flux {

JITEngine& JITEngine::instance()
{
    static JITEngine engine;
    return engine;
}

JITEngine::JITEngine() = default;

JITEngine::~JITEngine()
{
    finalize();
}

void JITEngine::setOptimizationLevel(OptimizationLevel level)
{
    if (m_jit)
        m_jit->setOptimizationLevel(level);
}

OptimizationLevel JITEngine::getOptimizationLevel() const
{
    if (m_jit)
        return m_jit->getOptimizationLevel();
    return OptimizationLevel::O2;
}

void JITEngine::initialize()
{
    if (m_initialized)
        return;
    m_jit = std::make_unique<FluxJIT>(OptimizationLevel::O2);
    m_compilerOptions.moduleName = "FluxJITCore";
    m_compilerOptions.injectStdlib = true;
    m_compilerOptions.debugInfo = true;
    if (m_cacheDirectory.empty())
        m_cacheDirectory = Tooling::defaultCacheDirectory();
    m_cacheEnabled = false;
    m_codegenCtx = nullptr;
    m_initialized = true;
    registerEigenFunctions();

    std::cout << "FluxScript C++ LLVM JIT Engine Initialized." << std::endl;
}

void JITEngine::registerFunction(const std::string& name, void* funcPtr)
{
    if (m_jit)
        m_jit->registerFunction(name, funcPtr);
}

void JITEngine::registerEigenFunctions()
{
    if (!m_jit)
        return;

    registerRuntimeFunctions(*m_jit);
}

void JITEngine::finalize()
{
    m_initialized = false;
    m_jit.reset();
    m_codegenCtx.reset();
}

bool JITEngine::isInitialized() const
{
    return m_initialized;
}

bool JITEngine::compileScript(const std::string& code, std::string* error)
{
    if (!m_initialized)
        initialize();
    m_overloadedFunctions.clear();
    m_lastCompileUsedCache = false;

    const std::string importHashes = Tooling::computeImportGraphHash(code);
    const std::string cacheKey = Tooling::computeCacheKey(code, m_compilerOptions, importHashes);
    const std::filesystem::path cacheDir(m_cacheDirectory);
    const std::filesystem::path bcPath = cacheDir / (cacheKey + ".bc");
    const std::filesystem::path metaPath = cacheDir / (cacheKey + ".meta");

    // --- Cache hit: load bitcode from disk, skip parse+codegen ---
    if (m_cacheEnabled && std::filesystem::exists(bcPath)) {
        auto Ctx = std::make_unique<llvm::LLVMContext>();
        auto M = Tooling::loadBitcodeFromFile(bcPath.string(), *Ctx, error);
        if (M) {
            Tooling::loadReturnTypes(metaPath.string(), m_functionReturnTypes, error);
            m_jit->addModule(std::move(M), std::move(Ctx));
            m_lastCompileUsedCache = true;
            return true;
        }
        // If load failed (e.g. corrupted file), fall through and recompile
        if (error)
            error->clear();
        std::filesystem::remove(bcPath);
        std::filesystem::remove(metaPath);
    }

    // --- Cache miss: compile from source ---
    CompilerInstance compiler(m_compilerOptions);
    auto artifacts = compiler.compileToIR(code, error);
    if (!artifacts)
        return false;
    m_functionReturnTypes = artifacts->functionReturnTypes;

    m_codegenCtx = std::move(artifacts->codegenContext);

    // --- Cache: save bitcode + return types ---
    if (m_cacheEnabled) {
        Tooling::saveBitcodeToFile(*m_codegenCtx->TheModule, bcPath.string(), nullptr);
        Tooling::saveReturnTypes(metaPath.string(), m_functionReturnTypes, nullptr);
    }

    m_jit->addModule(std::move(m_codegenCtx->OwnedModule), std::move(m_codegenCtx->OwnedContext));
    m_codegenCtx.reset();
    return true;
}

bool JITEngine::executeString(const std::string& code, std::string* error)
{
    if (!compileScript(code, error))
        return false;
    return true;
}

void* JITEngine::getFunctionPointer(const std::string& name)
{
    if (!m_initialized)
        return nullptr;
    return m_jit->getPointerToFunction(name);
}

FluxValue JITEngine::callFunction(const std::string& name, const std::vector<double>& args, std::string* error)
{
    if (!m_initialized) {
        if (error)
            *error = "JIT not initialized";
        return 0.0;
    }
    void* fnPtr = nullptr;
    std::string resolvedName = name;
    if (name == "__anon_expr") {
        std::string base = m_compilerOptions.moduleName + "_anon_expr";
        for (int i = -1; i < 100; i++) {
            resolvedName = (i == -1) ? base : (base + "_" + std::to_string(i));
            fnPtr = m_jit->getPointerToFunction(resolvedName);
            if (fnPtr)
                break;
        }
        if (!fnPtr)
            resolvedName = name;
    }
    if (!fnPtr)
        fnPtr = m_jit->getPointerToFunction(name);
    if (!fnPtr) {
        if (error)
            *error = "Function not found: " + name;
        return 0.0;
    }

    struct CallDepthGuard {
        CallDepthGuard() {
            flux_inc_call_depth();
        }
        ~CallDepthGuard() {
            flux_dec_call_depth();
        }
    } guard;

    FluxType retType = m_functionReturnTypes[resolvedName];

    if (retType.Kind == TypeKind::Matrix || retType.Kind == TypeKind::ComplexMatrix) {
        struct MatrixRet
        {
            void* ptr;
            int rows;
            int cols;
        };
        auto* r = new MatrixRet{nullptr, 0, 0};

        switch (args.size()) {
        case 0:
            reinterpret_cast<void (*)(MatrixRet*)>(fnPtr)(r);
            break;
        case 1:
            reinterpret_cast<void (*)(MatrixRet*, double)>(fnPtr)(r, args[0]);
            break;
        case 2:
            reinterpret_cast<void (*)(MatrixRet*, double, double)>(fnPtr)(r, args[0], args[1]);
            break;
        case 3:
            reinterpret_cast<void (*)(MatrixRet*, double, double, double)>(fnPtr)(r, args[0], args[1], args[2]);
            break;
        case 4:
            reinterpret_cast<void (*)(MatrixRet*, double, double, double, double)>(fnPtr)(r, args[0], args[1], args[2],
                                                                                          args[3]);
            break;
        case 5:
            reinterpret_cast<void (*)(MatrixRet*, double, double, double, double, double)>(fnPtr)(
                r, args[0], args[1], args[2], args[3], args[4]);
            break;
        default:
            delete r;
            if (error)
                *error = "Unsupported argument count for matrix function";
            return 0.0;
        }

        MatrixResult result{r->ptr, r->rows, r->cols};
        delete r;
        return result;
    } else if (retType.Kind == TypeKind::Vector) {
        struct VectorRet
        {
            double* data;
            int32_t len;
        };
        VectorRet r{nullptr, 0};
#ifdef _WIN32
        switch (args.size()) {
        case 0:
            reinterpret_cast<void (*)(VectorRet*)>(fnPtr)(&r);
            break;
        case 1:
            reinterpret_cast<void (*)(VectorRet*, double)>(fnPtr)(&r, args[0]);
            break;
        case 2:
            reinterpret_cast<void (*)(VectorRet*, double, double)>(fnPtr)(&r, args[0], args[1]);
            break;
        case 3:
            reinterpret_cast<void (*)(VectorRet*, double, double, double)>(fnPtr)(&r, args[0], args[1], args[2]);
            break;
        case 4:
            reinterpret_cast<void (*)(VectorRet*, double, double, double, double)>(fnPtr)(&r, args[0], args[1], args[2], args[3]);
            break;
        case 5:
            reinterpret_cast<void (*)(VectorRet*, double, double, double, double, double)>(fnPtr)(
                &r, args[0], args[1], args[2], args[3], args[4]);
            break;
        default:
            if (error)
                *error = "Unsupported argument count for vector function";
            return VectorResult{nullptr, 0};
        }
#else
        switch (args.size()) {
        case 0:
            r = reinterpret_cast<VectorRet (*)()>(fnPtr)();
            break;
        case 1:
            r = reinterpret_cast<VectorRet (*)(double)>(fnPtr)(args[0]);
            break;
        case 2:
            r = reinterpret_cast<VectorRet (*)(double, double)>(fnPtr)(args[0], args[1]);
            break;
        case 3:
            r = reinterpret_cast<VectorRet (*)(double, double, double)>(fnPtr)(args[0], args[1], args[2]);
            break;
        case 4:
            r = reinterpret_cast<VectorRet (*)(double, double, double, double)>(fnPtr)(args[0], args[1], args[2], args[3]);
            break;
        case 5:
            r = reinterpret_cast<VectorRet (*)(double, double, double, double, double)>(fnPtr)(
                args[0], args[1], args[2], args[3], args[4]);
            break;
        default:
            if (error)
                *error = "Unsupported argument count for vector function";
            return VectorResult{nullptr, 0};
        }
#endif
        return VectorResult{r.data, r.len};
    } else if (retType.Kind == TypeKind::Complex) {
        struct ComplexRet
        {
            double real;
            double imag;
        };
        ComplexRet r;
        switch (args.size()) {
        case 0:
            r = reinterpret_cast<ComplexRet (*)()>(fnPtr)();
            break;
        case 1:
            r = reinterpret_cast<ComplexRet (*)(double)>(fnPtr)(args[0]);
            break;
        case 2:
            r = reinterpret_cast<ComplexRet (*)(double, double)>(fnPtr)(args[0], args[1]);
            break;
        default:
            r = ComplexRet{0, 0};
            break;
        }
        return std::complex<double>(r.real, r.imag);
    } else {
        double result;
        switch (args.size()) {
        case 0:
            result = reinterpret_cast<double (*)()>(fnPtr)();
            break;
        case 1:
            result = reinterpret_cast<double (*)(double)>(fnPtr)(args[0]);
            break;
        case 2:
            result = reinterpret_cast<double (*)(double, double)>(fnPtr)(args[0], args[1]);
            break;
        case 3:
            result = reinterpret_cast<double (*)(double, double, double)>(fnPtr)(args[0], args[1], args[2]);
            break;
        case 4:
            result =
                reinterpret_cast<double (*)(double, double, double, double)>(fnPtr)(args[0], args[1], args[2], args[3]);
            break;
        case 5:
            result = reinterpret_cast<double (*)(double, double, double, double, double)>(fnPtr)(
                args[0], args[1], args[2], args[3], args[4]);
            break;
        default:
            result = 0.0;
            break;
        }
        return result;
    }
    if (error)
        *error = "Unsupported arguments or return type";
    return 0.0;
}

// ============ Module System Integration ============

bool JITEngine::importModule(const std::string& moduleName, std::string* error)
{
    if (!m_initialized) {
        if (error)
            *error = "JIT Engine not initialized";
        return false;
    }

    // Delegate to ModuleLoader
    bool success = m_moduleLoader.loadModule(moduleName, error);
    if (!success)
        return false;

    // Add search path for the module's location so imports resolve correctly
    m_moduleLoader.addSearchPath(std::filesystem::path("modules") / moduleName);

    // If the JIT engine has a running context, compile and add the module IR
    if (m_codegenCtx) {
        auto info = m_moduleLoader.getModuleInfo(moduleName);
        if (!info.sourcePath.empty()) {
            CompilerInstance compiler(m_compilerOptions);
            auto content = llvm::MemoryBuffer::getFile(info.sourcePath.string());
            if (content) {
                auto artifacts = compiler.compileToIR(content->get()->getBuffer().str(), error);
                if (artifacts && m_jit) {
                    m_jit->addModule(std::move(artifacts->codegenContext->OwnedModule),
                                     std::move(artifacts->codegenContext->OwnedContext));
                }
            }
        }
    }

    m_importedModules.insert(moduleName);
    return true;
}

bool JITEngine::loadPlugin(const std::string& pluginPath, std::string* error)
{
    if (!m_initialized) {
        if (error)
            *error = "JIT Engine not initialized";
        return false;
    }

    return m_moduleLoader.loadPlugin(std::filesystem::path(pluginPath), error);
}

std::vector<std::string> JITEngine::getLoadedModules() const
{
    if (!m_initialized)
        return {};
    return m_moduleLoader.getLoadedModules();
}

std::vector<std::string> JITEngine::getModuleExports(const std::string& moduleName) const
{
    if (!m_initialized)
        return {};
    try {
        auto info = m_moduleLoader.getModuleInfo(moduleName);
        return info.exports;
    } catch (...) {
        return {};
    }
}

std::string JITEngine::getFunctionSignature(const std::string& functionName) const
{
    if (!m_initialized)
        return "";
    try {
        auto reflection = m_moduleLoader.getFunctionReflection(functionName);
        std::string sig = reflection.returnType + " " + reflection.name + "(";
        for (size_t i = 0; i < reflection.parameters.size(); i++) {
            if (i > 0)
                sig += ", ";
            sig += reflection.parameters[i].second + " " + reflection.parameters[i].first;
        }
        sig += ")";
        return sig;
    } catch (...) {
        return "";
    }
}

void JITEngine::setDefine(const std::string& name, bool value)
{
    if (!m_initialized)
        return;
    auto config = m_moduleLoader.getCompileConfig();
    config.features[name] = value;
    m_moduleLoader.setCompileConfig(config);
}

bool JITEngine::getDefine(const std::string& name) const
{
    if (!m_initialized)
        return false;
    return m_moduleLoader.checkFeature(name);
}

void JITEngine::setOptimizationLevelForModules(OptimizationLevel level)
{
    if (!m_initialized)
        return;
    auto config = m_moduleLoader.getCompileConfig();
    config.features["opt_level"] = (level >= OptimizationLevel::O2);
    config.constants["FLUX_OPT_LEVEL"] = std::to_string(static_cast<int>(level));
    m_moduleLoader.setCompileConfig(config);
}

void JITEngine::clearJITCache()
{
    if (!m_cacheDirectory.empty()) {
        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator(m_cacheDirectory, ec)) {
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

// --- Tiered JIT ---

void* JITEngine::promoteFunction(const std::string& name, OptimizationLevel targetLevel)
{
    if (!m_jit)
        return nullptr;
    return m_jit->promoteFunction(name, targetLevel);
}

void JITEngine::setTieredJITThreshold(int n)
{
    if (m_jit)
        m_jit->setAutoPromoteThreshold(n);
}

int JITEngine::getTieredJITThreshold() const
{
    if (m_jit)
        return m_jit->getAutoPromoteThreshold();
    return 0;
}

int JITEngine::getFunctionCallCount(const std::string& name) const
{
    if (m_jit)
        return m_jit->getCallCount(name);
    return 0;
}

void JITEngine::resetCallCounts()
{
    if (m_jit)
        m_jit->resetCallCounts();
}

bool JITEngine::isFunctionPromoted(const std::string& name) const
{
    if (m_jit)
        return m_jit->isPromoted(name);
    return false;
}

} // namespace Flux
