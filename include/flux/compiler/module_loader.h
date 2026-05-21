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

#ifndef FLUX_COMPILER_MODULE_LOADER_H
#define FLUX_COMPILER_MODULE_LOADER_H

#include <filesystem>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

// Version representation for versioned imports
struct Version
{
    int major;
    int minor;
    int patch;

    Version(int maj = 0, int min = 0, int pat = 0) : major(maj), minor(min), patch(pat) {}

    std::string toString() const
    {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    bool operator<(const Version& other) const
    {
        if (major != other.major)
            return major < other.major;
        if (minor != other.minor)
            return minor < other.minor;
        return patch < other.patch;
    }

    bool operator==(const Version& other) const
    {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

// Module metadata
struct ModuleInfo
{
    std::string name;
    Version version;
    std::string description;
    std::vector<std::string> dependencies;
    std::vector<std::string> exports;
    std::filesystem::path sourcePath;
    std::filesystem::path compiledPath;
};

// Import specification
struct ImportSpec
{
    std::string moduleName;
    Version minVersion;
    Version maxVersion;
    std::string alias;
    std::vector<std::string> symbols;
    bool isPlugin = false;
};

// Conditional compilation configuration
struct CompileConfig
{
    std::map<std::string, bool> features;
    std::map<std::string, std::string> constants;
    std::string targetPlatform;

    CompileConfig()
    {
        features["FLUX_HAS_QT"] = false;
        features["FLUX_HAS_MLIR"] = false;
        features["FLUX_HAS_EIGEN"] = true;
        features["FLUX_DEBUG"] = false;

        constants["FLUX_VERSION"] = "1.0.0";
        constants["FLUX_PLATFORM"] = "linux";

        targetPlatform = "linux";
    }

    void setFeature(const std::string& name, bool value) { features[name] = value; }

    bool hasFeature(const std::string& name) const
    {
        auto it = features.find(name);
        return it != features.end() && it->second;
    }
};

// Reflection information for a function
struct FunctionReflection
{
    std::string name;
    std::string moduleName;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::string returnType;
    std::string documentation;
    bool isExternal = false;
    bool isPublic = true;
};

// Module loader class
class ModuleLoader
{
public:
    ModuleLoader();
    ~ModuleLoader();

    bool loadModule(const std::string& moduleName, std::string* error = nullptr);
    bool loadModuleFromFile(const std::filesystem::path& path, std::string* error = nullptr);
    bool loadPlugin(const std::filesystem::path& pluginPath, std::string* error = nullptr);
    bool loadImport(const ImportSpec& spec, std::string* error = nullptr);

    std::filesystem::path findModule(const std::string& moduleName) const;
    std::filesystem::path findModule(const std::string& moduleName, const Version& version) const;

    void addSearchPath(const std::filesystem::path& path);
    void clearSearchPaths();
    std::vector<std::string> getLoadedModules() const;
    bool isModuleLoaded(const std::string& moduleName) const;

    ModuleInfo getModuleInfo(const std::string& moduleName) const;
    std::vector<ModuleInfo> getAllModuleInfo() const;

    void registerNamespace(const std::string& moduleName, const std::string& alias);
    std::string resolveNamespace(const std::string& qualifiedName) const;

    std::vector<FunctionReflection> getModuleFunctions(const std::string& moduleName) const;
    FunctionReflection getFunctionReflection(const std::string& functionName) const;
    std::vector<std::string> listAllSymbols() const;

    void setCompileConfig(const CompileConfig& config);
    CompileConfig getCompileConfig() const;
    bool checkFeature(const std::string& feature) const;

    void loadStandardLibrary(std::string* error = nullptr);

    void setCacheDirectory(const std::filesystem::path& path);
    void clearCache();

private:
    struct ModuleData
    {
        ModuleInfo info;
        std::unique_ptr<llvm::Module> module;
        std::map<std::string, FunctionReflection> functions;
        std::filesystem::path compiledPath;
        bool isLoaded = false;
        bool isPlugin = false;
        void* pluginHandle = nullptr;
    };

    std::map<std::string, ModuleData> m_loadedModules;
    std::vector<std::filesystem::path> m_searchPaths;
    std::map<std::string, std::string> m_namespaceAliases;
    CompileConfig m_compileConfig;
    std::filesystem::path m_cacheDirectory;

    bool parseModuleFile(const std::filesystem::path& path, ModuleInfo& info, std::string* error);
    bool compileModule(const std::filesystem::path& sourcePath, const std::filesystem::path& outputPath,
                       std::string* error);
    bool loadCompiledModule(const std::filesystem::path& path, ModuleData& data, std::string* error);
};

// Global configuration
class GlobalConfig
{
public:
    static GlobalConfig& instance();

    void setOptimizationLevel(int level);
    int getOptimizationLevel() const { return m_optimizationLevel; }

    void setDefaultPrecision(int bits);
    int getDefaultPrecision() const { return m_defaultPrecision; }

    void setJITCacheEnabled(bool enabled);
    bool isJITCacheEnabled() const { return m_jitCacheEnabled; }

    void setJITCacheDirectory(const std::string& path);
    std::string getJITCacheDirectory() const { return m_jitCacheDirectory; }

    void addModuleSearchPath(const std::string& path);
    std::vector<std::string> getModuleSearchPaths() const { return m_moduleSearchPaths; }

    void setDebugEnabled(bool enabled);
    bool isDebugEnabled() const { return m_debugEnabled; }

    void setVerboseOutput(bool verbose);
    bool isVerboseOutput() const { return m_verboseOutput; }

private:
    GlobalConfig();
    ~GlobalConfig();
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;

    int m_optimizationLevel = 2;
    int m_defaultPrecision = 64;
    bool m_jitCacheEnabled = true;
    std::string m_jitCacheDirectory;
    std::vector<std::string> m_moduleSearchPaths;
    bool m_debugEnabled = false;
    bool m_verboseOutput = false;
};

} // namespace Flux

#endif // FLUX_COMPILER_MODULE_LOADER_H
