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

#include "flux/compiler/module_loader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>

namespace Flux {

// ============================================================================
// ModuleLoader Implementation
// ============================================================================

ModuleLoader::ModuleLoader() {
    // Add default search paths
    m_searchPaths.push_back(std::filesystem::current_path());
    m_searchPaths.push_back(std::filesystem::current_path() / "modules");
    m_searchPaths.push_back(std::filesystem::current_path() / "lib");
    
    // Add stdlib directory
    m_searchPaths.push_back(std::filesystem::current_path() / "stdlib");

    // User home directory
    const char* home = std::getenv("HOME");
    if (home) {
        m_searchPaths.push_back(std::filesystem::path(home) / ".flux" / "modules");
        m_searchPaths.push_back(std::filesystem::path(home) / ".flux" / "stdlib");
    }

    // System-wide paths
    m_searchPaths.push_back("/usr/local/share/flux/modules");
    m_searchPaths.push_back("/usr/share/flux/modules");
    m_searchPaths.push_back("/usr/local/share/flux/stdlib");
    m_searchPaths.push_back("/usr/share/flux/stdlib");

    // Set default cache directory
    const char* cacheDir = std::getenv("XDG_CACHE_HOME");
    if (cacheDir) {
        m_cacheDirectory = std::filesystem::path(cacheDir) / "flux";
    } else {
        m_cacheDirectory = std::filesystem::current_path() / ".flux_cache";
    }

    // Create cache directory if it doesn't exist
    std::filesystem::create_directories(m_cacheDirectory);
}

ModuleLoader::~ModuleLoader() {
    // Clean up plugin handles
    for (auto& [name, data] : m_loadedModules) {
        if (data.isPlugin && data.pluginHandle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(data.pluginHandle));
#else
            dlclose(data.pluginHandle);
#endif
        }
    }
}

void ModuleLoader::addSearchPath(const std::filesystem::path& path) {
    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), path) == m_searchPaths.end()) {
        m_searchPaths.push_back(path);
    }
}

void ModuleLoader::clearSearchPaths() {
    m_searchPaths.clear();
}

std::filesystem::path ModuleLoader::findModule(const std::string& moduleName) const {
    // Try different file extensions
    std::vector<std::string> extensions = {".flux", ".flux.bc", ".flux.ll"};
    
    for (const auto& searchPath : m_searchPaths) {
        for (const auto& ext : extensions) {
            std::filesystem::path candidate = searchPath / (moduleName + ext);
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
        
        // Also check in subdirectory (moduleName/moduleName.flux)
        for (const auto& ext : extensions) {
            std::filesystem::path candidate = searchPath / moduleName / (moduleName + ext);
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
        }
    }
    
    return std::filesystem::path();
}

std::filesystem::path ModuleLoader::findModule(const std::string& moduleName, const Version& version) const {
    // For versioned modules, look in version-specific directories
    std::string versionedName = moduleName + "-" + version.toString();
    auto path = findModule(versionedName);
    if (!path.empty()) {
        return path;
    }
    
    // Fall back to unversioned
    return findModule(moduleName);
}

bool ModuleLoader::loadModule(const std::string& moduleName, std::string* error) {
    if (isModuleLoaded(moduleName)) {
        return true;  // Already loaded
    }
    
    auto modulePath = findModule(moduleName);
    if (modulePath.empty()) {
        if (error) {
            *error = "Module not found: " + moduleName;
        }
        return false;
    }
    
    return loadModuleFromFile(modulePath, error);
}

bool ModuleLoader::loadModuleFromFile(const std::filesystem::path& path, std::string* error) {
    std::string ext = path.extension().string();
    
    // Check if it's a compiled module
    if (ext == ".bc" || ext == ".ll") {
        ModuleData data;
        if (!loadCompiledModule(path, data, error)) {
            return false;
        }
        
        std::string moduleName = data.info.name;
        if (moduleName.empty()) {
            moduleName = path.stem().string();
            data.info.name = moduleName;
        }
        
        m_loadedModules[moduleName] = std::move(data);
        return true;
    }
    
    // It's a source file - parse metadata
    ModuleInfo info;
    if (!parseModuleFile(path, info, error)) {
        return false;
    }
    
    // Check if already loaded
    if (isModuleLoaded(info.name)) {
        return true;
    }
    
    // Compile the module
    std::filesystem::path compiledPath = m_cacheDirectory / (info.name + ".bc");
    
    // Check if cached version exists and is up-to-date
    bool needsCompile = true;
    if (std::filesystem::exists(compiledPath)) {
        auto sourceTime = std::filesystem::last_write_time(path);
        auto compiledTime = std::filesystem::last_write_time(compiledPath);
        if (compiledTime > sourceTime) {
            needsCompile = false;
        }
    }
    
    if (needsCompile) {
        if (!compileModule(path, compiledPath, error)) {
            return false;
        }
    }
    
    // Load the compiled module
    ModuleData data;
    data.info = info;
    data.compiledPath = compiledPath;
    
    if (!loadCompiledModule(compiledPath, data, error)) {
        return false;
    }
    
    m_loadedModules[info.name] = std::move(data);
    return true;
}

bool ModuleLoader::parseModuleFile(const std::filesystem::path& path, ModuleInfo& info, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) {
            *error = "Cannot open module file: " + path.string();
        }
        return false;
    }
    
    info.sourcePath = path;
    info.name = path.stem().string();
    
    // Parse module metadata from comments
    std::string line;
    std::regex nameRegex(R"(//\s*@name:\s*(\S+))");
    std::regex versionRegex(R"(//\s*@version:\s*(\d+)\.(\d+)\.(\d+))");
    std::regex descRegex(R"(//\s*@description:\s*(.+))");
    std::regex exportRegex(R"(//\s*@export:\s*(\S+))");
    std::regex dependRegex(R"(//\s*@depends:\s*(\S+))");
    
    while (std::getline(file, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, nameRegex)) {
            info.name = match[1];
        } else if (std::regex_search(line, match, versionRegex)) {
            info.version = Version(
                std::stoi(match[1]),
                std::stoi(match[2]),
                std::stoi(match[3])
            );
        } else if (std::regex_search(line, match, descRegex)) {
            info.description = match[1];
        } else if (std::regex_search(line, match, exportRegex)) {
            info.exports.push_back(match[1]);
        } else if (std::regex_search(line, match, dependRegex)) {
            info.dependencies.push_back(match[1]);
        }
    }
    
    return true;
}

bool ModuleLoader::compileModule(const std::filesystem::path& sourcePath, 
                                  const std::filesystem::path& outputPath, 
                                  std::string* error) {
    // Note: Actual compilation would use CompilerInstance
    // This is a stub that would integrate with the existing compiler
    
    std::cout << "[ModuleLoader] Compiling: " << sourcePath.string() 
              << " -> " << outputPath.string() << std::endl;
    
    // For now, just create a placeholder
    // In production, this would:
    // 1. Read source file
    // 2. Parse and compile to LLVM IR
    // 3. Write bitcode to outputPath
    
    return true;
}

bool ModuleLoader::loadCompiledModule(const std::filesystem::path& path, 
                                       ModuleData& data, 
                                       std::string* error) {
    llvm::SMDiagnostic err;
    llvm::LLVMContext context;
    
    auto module = llvm::parseIRFile(path.string(), err, context);
    if (!module) {
        if (error) {
            *error = "Failed to load compiled module: " + path.string();
        }
        return false;
    }
    
    data.module = std::move(module);
    data.isLoaded = true;
    
    // Extract function information for reflection
    for (auto& func : *data.module) {
        if (!func.isDeclaration()) {
            FunctionReflection refl;
            refl.name = func.getName().str();
            refl.moduleName = data.info.name;
            refl.returnType = "double";  // Default return type
            
            for (auto& arg : func.args()) {
                refl.parameters.push_back({arg.getName().str(), "double"});
            }
            
            data.functions[refl.name] = refl;
            data.info.exports.push_back(refl.name);
        }
    }
    
    return true;
}

bool ModuleLoader::loadPlugin(const std::filesystem::path& pluginPath, std::string* error) {
#ifdef _WIN32
    void* handle = LoadLibraryA(pluginPath.string().c_str());
    if (!handle) {
        if (error) {
            *error = "Failed to load plugin: " + pluginPath.string();
        }
        return false;
    }
#else
    void* handle = dlopen(pluginPath.string().c_str(), RTLD_NOW);
    if (!handle) {
        if (error) {
            *error = "Failed to load plugin: " + std::string(dlerror());
        }
        return false;
    }
#endif
    
    ModuleData data;
    data.isPlugin = true;
    data.pluginHandle = handle;
    data.isLoaded = true;
    data.info.name = pluginPath.stem().string();
    data.info.sourcePath = pluginPath;
    
    // Plugin modules should export a registration function
    // This would be called to register the plugin's functions
    
    m_loadedModules[data.info.name] = std::move(data);
    return true;
}

bool ModuleLoader::loadImport(const ImportSpec& spec, std::string* error) {
    if (spec.isPlugin) {
        return loadPlugin(std::filesystem::path(spec.moduleName), error);
    }
    
    // Check version constraints
    auto modulePath = findModule(spec.moduleName);
    if (modulePath.empty()) {
        if (error) {
            *error = "Module not found: " + spec.moduleName;
        }
        return false;
    }
    
    // Parse module info to check version
    ModuleInfo info;
    if (!parseModuleFile(modulePath, info, error)) {
        return false;
    }
    
    // Check version constraints
    if (spec.minVersion.major > 0 && info.version < spec.minVersion) {
        if (error) {
            *error = "Module version too old: " + spec.moduleName + 
                     " (required >= " + spec.minVersion.toString() + 
                     ", found " + info.version.toString() + ")";
        }
        return false;
    }
    
    if (spec.maxVersion.major > 0 && spec.maxVersion < info.version) {
        if (error) {
            *error = "Module version too new: " + spec.moduleName + 
                     " (required <= " + spec.maxVersion.toString() + 
                     ", found " + info.version.toString() + ")";
        }
        return false;
    }
    
    // Load the module
    if (!loadModule(spec.moduleName, error)) {
        return false;
    }
    
    // Register namespace alias if provided
    if (!spec.alias.empty()) {
        registerNamespace(spec.moduleName, spec.alias);
    }
    
    return true;
}

bool ModuleLoader::isModuleLoaded(const std::string& moduleName) const {
    return m_loadedModules.find(moduleName) != m_loadedModules.end() &&
           m_loadedModules.at(moduleName).isLoaded;
}

std::vector<std::string> ModuleLoader::getLoadedModules() const {
    std::vector<std::string> names;
    for (const auto& [name, data] : m_loadedModules) {
        if (data.isLoaded) {
            names.push_back(name);
        }
    }
    return names;
}

ModuleInfo ModuleLoader::getModuleInfo(const std::string& moduleName) const {
    auto it = m_loadedModules.find(moduleName);
    if (it != m_loadedModules.end()) {
        return it->second.info;
    }
    return ModuleInfo();
}

std::vector<ModuleInfo> ModuleLoader::getAllModuleInfo() const {
    std::vector<ModuleInfo> infos;
    for (const auto& [name, data] : m_loadedModules) {
        infos.push_back(data.info);
    }
    return infos;
}

void ModuleLoader::registerNamespace(const std::string& moduleName, const std::string& alias) {
    m_namespaceAliases[alias] = moduleName;
}

std::string ModuleLoader::resolveNamespace(const std::string& qualifiedName) const {
    // Handle math::fft syntax
    size_t pos = qualifiedName.find("::");
    if (pos == std::string::npos) {
        return qualifiedName;  // No namespace
    }
    
    std::string ns = qualifiedName.substr(0, pos);
    std::string func = qualifiedName.substr(pos + 2);
    
    // Check for alias
    auto it = m_namespaceAliases.find(ns);
    if (it != m_namespaceAliases.end()) {
        return it->second + "::" + func;
    }
    
    // Check if namespace matches a loaded module
    if (isModuleLoaded(ns)) {
        return qualifiedName;
    }
    
    return qualifiedName;  // Return as-is if not found
}

std::vector<FunctionReflection> ModuleLoader::getModuleFunctions(const std::string& moduleName) const {
    std::vector<FunctionReflection> functions;
    
    auto it = m_loadedModules.find(moduleName);
    if (it != m_loadedModules.end()) {
        for (const auto& [name, refl] : it->second.functions) {
            functions.push_back(refl);
        }
    }
    
    return functions;
}

FunctionReflection ModuleLoader::getFunctionReflection(const std::string& functionName) const {
    // Check all loaded modules
    for (const auto& [moduleName, data] : m_loadedModules) {
        auto it = data.functions.find(functionName);
        if (it != data.functions.end()) {
            return it->second;
        }
    }
    
    return FunctionReflection();
}

std::vector<std::string> ModuleLoader::listAllSymbols() const {
    std::vector<std::string> symbols;
    
    for (const auto& [moduleName, data] : m_loadedModules) {
        for (const auto& [funcName, refl] : data.functions) {
            symbols.push_back(funcName);
        }
    }
    
    return symbols;
}

void ModuleLoader::setCompileConfig(const CompileConfig& config) {
    m_compileConfig = config;
}

CompileConfig ModuleLoader::getCompileConfig() const {
    return m_compileConfig;
}

bool ModuleLoader::checkFeature(const std::string& feature) const {
    return m_compileConfig.hasFeature(feature);
}

void ModuleLoader::loadStandardLibrary(std::string* error) {
    // Load the standard library module
    // This would load stdlib.bc or compile stdlib.flux
    
    std::cout << "[ModuleLoader] Loading standard library..." << std::endl;
    
    // Try to load from various locations
    std::vector<std::string> stdlibNames = {
        "stdlib",
        "flux_stdlib",
        "flux-stdlib"
    };
    
    for (const auto& name : stdlibNames) {
        if (loadModule(name, error)) {
            std::cout << "[ModuleLoader] Standard library loaded: " << name << std::endl;
            return;
        }
    }
    
    // If not found, that's okay - stdlib is optional
    std::cout << "[ModuleLoader] Standard library not found (optional)" << std::endl;
}

void ModuleLoader::setCacheDirectory(const std::filesystem::path& path) {
    m_cacheDirectory = path;
    std::filesystem::create_directories(m_cacheDirectory);
}

void ModuleLoader::clearCache() {
    if (std::filesystem::exists(m_cacheDirectory)) {
        for (const auto& entry : std::filesystem::directory_iterator(m_cacheDirectory)) {
            if (entry.path().extension() == ".bc") {
                std::filesystem::remove(entry.path());
            }
        }
    }
}

// ============================================================================
// GlobalConfig Implementation
// ============================================================================

GlobalConfig& GlobalConfig::instance() {
    static GlobalConfig config;
    return config;
}

GlobalConfig::GlobalConfig() {
    // Load configuration from environment or config file
    const char* optLevel = std::getenv("FLUX_OPT_LEVEL");
    if (optLevel) {
        m_optimizationLevel = std::stoi(optLevel);
    }
    
    const char* cacheDir = std::getenv("FLUX_CACHE_DIR");
    if (cacheDir) {
        m_jitCacheDirectory = cacheDir;
    }
    
    const char* modulePath = std::getenv("FLUX_MODULE_PATH");
    if (modulePath) {
        std::istringstream iss(modulePath);
        std::string path;
        while (std::getline(iss, path, ':')) {
            m_moduleSearchPaths.push_back(path);
        }
    }
}

GlobalConfig::~GlobalConfig() = default;

void GlobalConfig::setOptimizationLevel(int level) {
    m_optimizationLevel = std::clamp(level, 0, 3);
}

void GlobalConfig::setDefaultPrecision(int bits) {
    m_defaultPrecision = bits;
}

void GlobalConfig::setJITCacheEnabled(bool enabled) {
    m_jitCacheEnabled = enabled;
}

void GlobalConfig::setJITCacheDirectory(const std::string& path) {
    m_jitCacheDirectory = path;
}

void GlobalConfig::addModuleSearchPath(const std::string& path) {
    if (std::find(m_moduleSearchPaths.begin(), m_moduleSearchPaths.end(), path) == m_moduleSearchPaths.end()) {
        m_moduleSearchPaths.push_back(path);
    }
}

void GlobalConfig::setDebugEnabled(bool enabled) {
    m_debugEnabled = enabled;
}

void GlobalConfig::setVerboseOutput(bool verbose) {
    m_verboseOutput = verbose;
}

} // namespace Flux
