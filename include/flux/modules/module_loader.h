#ifndef FLUX_MODULE_LOADER_H
#define FLUX_MODULE_LOADER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <functional>

#include "flux/compiler/ast.h"
#include "flux/jit/flux_jit.h"

namespace Flux {

// Module information
struct ModuleInfo {
    std::string name;
    std::string version;
    std::string path;
    std::vector<std::string> exports;  // Exported function names
    std::vector<std::string> dependencies;
    bool isLoaded = false;
    bool isBuiltin = false;
};

// Package manifest (flux-package.json equivalent)
struct PackageManifest {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> dependencies;
    std::string main;  // Main entry point .flux file
    std::vector<std::string> sources;
    std::map<std::string, std::string> config;
};

// Configuration for the module system
struct ModuleConfig {
    std::vector<std::string> searchPaths;
    std::string cacheDirectory;
    bool enableCache = true;
    bool verboseLogging = false;
    
    // Conditional compilation flags
    std::map<std::string, bool> defines;
    
    // Global settings
    int precision = 64;  // Floating point precision
    OptimizationLevel optimization = OptimizationLevel::O2;
};

// Native function registration for plugins
struct PluginFunction {
    std::string name;
    void* funcPtr;
    std::string signature;  // e.g., "double(double, double)"
    std::string description;
};

// Module loader class
class ModuleLoader {
public:
    static ModuleLoader& instance();
    
    // Initialize the module system
    void initialize(const ModuleConfig& config = ModuleConfig());
    void finalize();
    bool isInitialized() const { return m_initialized; }
    
    // Module loading
    bool loadModule(const std::string& moduleName, std::string* error = nullptr);
    bool loadModuleFromFile(const std::string& path, std::string* error = nullptr);
    bool unloadModule(const std::string& moduleName);
    bool isModuleLoaded(const std::string& moduleName) const;
    
    // Import with version support
    bool importModule(const std::string& moduleName, const std::string& version = "", std::string* error = nullptr);
    
    // Namespace resolution
    std::string resolveNamespaceSymbol(const std::string& namespacePath, const std::string& symbol);
    bool registerNamespace(const std::string& ns, const std::string& moduleName);
    
    // Plugin system (.so loading)
    bool loadPlugin(const std::string& pluginPath, std::string* error = nullptr);
    bool registerPluginFunction(const std::string& name, void* funcPtr, const std::string& signature = "");
    void* getPluginFunction(const std::string& name);
    
    // Standard library
    bool loadStandardLibrary(std::string* error = nullptr);
    bool isStandardLibraryLoaded() const { return m_stdlibLoaded; }
    
    // Search path management
    void addSearchPath(const std::string& path);
    std::string findModule(const std::string& moduleName) const;
    
    // Configuration
    const ModuleConfig& getConfig() const { return m_config; }
    void setConfig(const ModuleConfig& config) { m_config = config; }
    
    // Conditional compilation
    void setDefine(const std::string& name, bool value) { m_config.defines[name] = value; }
    bool getDefine(const std::string& name) const;
    
    // Reflection - get module info
    const ModuleInfo* getModuleInfo(const std::string& moduleName) const;
    std::vector<std::string> getLoadedModules() const;
    std::vector<std::string> getModuleExports(const std::string& moduleName) const;
    
    // Get function signature for reflection
    std::string getFunctionSignature(const std::string& functionName) const;
    
private:
    ModuleLoader() = default;
    ~ModuleLoader() = default;
    
    bool compileModule(const std::string& path, const std::string& moduleName, std::string* error);
    bool loadBitcodeModule(const std::string& bcPath, std::string* error = nullptr);
    std::string preprocessSource(const std::string& source, const ModuleConfig& config);
    
    ModuleConfig m_config;
    std::map<std::string, ModuleInfo> m_modules;
    std::map<std::string, std::string> m_namespaces;  // namespace -> module mapping
    std::map<std::string, PluginFunction> m_pluginFunctions;
    std::map<std::string, void*> m_loadedPlugins;  // Handle to loaded .so files
    bool m_initialized = false;
    bool m_stdlibLoaded = false;
};

// Helper functions
namespace ModuleUtils {
    std::string normalizeModuleName(const std::string& name);
    std::string moduleToPath(const std::string& moduleName);
    std::string pathToModule(const std::string& path);
    bool parseVersion(const std::string& versionStr, int& major, int& minor, int& patch);
    std::string versionToString(int major, int minor, int patch);
}

} // namespace Flux

#endif // FLUX_MODULE_LOADER_H
