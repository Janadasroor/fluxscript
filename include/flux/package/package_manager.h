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

#ifndef FLUX_PACKAGE_MANAGER_H
#define FLUX_PACKAGE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace Flux {
namespace PackageManager {

// ============================================================================
// Package Data Structures
// ============================================================================

struct PackageVersion {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    
    PackageVersion() : major(0), minor(0), patch(0) {}
    PackageVersion(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}
    
    std::string toString() const;
    static PackageVersion parse(const std::string& str);
    
    bool operator<(const PackageVersion& other) const;
    bool operator==(const PackageVersion& other) const;
    bool operator>(const PackageVersion& other) const { return other < *this; }
    bool operator>=(const PackageVersion& other) const;
    bool operator<=(const PackageVersion& other) const { return !(*this > other); }
};

struct PackageDependency {
    std::string name;
    std::string versionConstraint;  // e.g., ">=1.0.0 <2.0.0"
    bool optional;
    
    PackageDependency() : optional(false) {}
};

struct PackageManifest {
    std::string name;
    PackageVersion version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> keywords;
    std::vector<PackageDependency> dependencies;
    std::vector<std::string> sourceFiles;
    std::string repository;
    std::string homepage;
    
    std::string toString() const;
    static PackageManifest parse(const std::string& toml);
};

struct InstalledPackage {
    std::string name;
    PackageVersion version;
    std::string installPath;
    bool editable;
    std::vector<std::string> files;
};

// ============================================================================
// Package Registry
// ============================================================================

struct PackageInfo {
    std::string name;
    std::string description;
    std::vector<PackageVersion> versions;
    PackageVersion latestVersion;
    std::string repository;
    int downloads;
    double rating;
    std::vector<std::string> keywords;
    std::string author;
    std::string license;
    bool verified;  // Officially verified package
};

class PackageRegistry {
public:
    static PackageRegistry& instance();
    
    // Search packages
    std::vector<PackageInfo> search(const std::string& query);
    std::vector<PackageInfo> searchByKeyword(const std::string& keyword);
    
    // Get package info
    PackageInfo getPackageInfo(const std::string& name);
    std::vector<PackageVersion> getVersions(const std::string& name);
    
    // Publish (for package authors)
    bool publish(const PackageManifest& manifest, const std::string& authToken);
    
    // Registry configuration
    void addRegistry(const std::string& url);
    void setDefaultRegistry(const std::string& url);
    std::vector<std::string> getRegistries() const;
    
private:
    PackageRegistry();
    std::vector<std::string> m_registries;
    std::map<std::string, PackageInfo> m_cache;
};

// ============================================================================
// Package Manager Core
// ============================================================================

class PackageManager {
public:
    PackageManager();
    ~PackageManager();
    
    // Initialize
    void initialize(const std::string& projectPath);
    
    // Install packages
    bool install(const std::string& packageName, const std::string& version = "");
    bool installFromGit(const std::string& gitUrl, const std::string& branch = "main");
    bool installLocal(const std::string& path);
    
    // Update packages
    bool update(const std::string& packageName);
    bool updateAll();
    std::vector<std::string> getOutdatedPackages();
    
    // Remove packages
    bool uninstall(const std::string& packageName);
    bool clean();  // Remove unused packages
    
    // Query installed packages
    std::vector<InstalledPackage> getInstalledPackages() const;
    bool isInstalled(const std::string& packageName) const;
    PackageVersion getInstalledVersion(const std::string& packageName) const;
    
    // Dependency resolution
    bool resolveDependencies();
    std::vector<std::string> getDependencyTree(const std::string& packageName);
    
    // Audit
    struct AuditResult {
        bool hasIssues;
        std::vector<std::string> securityIssues;
        std::vector<std::string> licenseIssues;
        std::vector<std::string> outdatedPackages;
    };
    
    AuditResult audit();
    
    // Cache management
    void clearCache();
    size_t getCacheSize() const;
    
    // Configuration
    void setRegistry(const std::string& url);
    void setCacheDir(const std::string& path);
    void setInstallDir(const std::string& path);
    
private:
    std::string m_projectPath;
    std::string m_cacheDir;
    std::string m_installDir;
    std::string m_registryUrl;
    
    std::vector<InstalledPackage> m_installed;
    PackageManifest m_projectManifest;
    
    // Internal methods
    bool downloadPackage(const std::string& name, const PackageVersion& version);
    bool extractPackage(const std::string& archive, const std::string& dest);
    bool runInstallScript(const std::string& packagePath);
    void updateLockFile();
    void loadInstalled();
    void saveInstalled();
    bool satisfiesConstraint(const PackageVersion& version, const std::string& constraint);
    bool resolveDependency(const PackageDependency& dep,
                           std::map<std::string, std::vector<PackageDependency>>& depGraph);
    void buildDependencyTree(const std::string& packageName, const std::string& prefix,
                             std::vector<std::string>& tree);
    InstalledPackage* findInstalled(const std::string& name);
    const InstalledPackage* findInstalled(const std::string& name) const;
};

// ============================================================================
// Version Constraint Parser
// ============================================================================

class VersionConstraint {
public:
    static bool satisfies(const PackageVersion& version, const std::string& constraint);
    static bool isCompatible(const PackageVersion& v1, const PackageVersion& v2);
    
private:
    static bool matchRange(const PackageVersion& version, 
                           const PackageVersion& min, const PackageVersion& max);
};

// ============================================================================
// CLI Interface
// ============================================================================

class PackageCLI {
public:
    static int run(int argc, char** argv);
    
private:
    static void printHelp();
    static void printVersion();
    static void printList(const std::vector<InstalledPackage>& packages);
    static void printSearchResults(const std::vector<PackageInfo>& results);
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Package manager initialization
    void* flux_pkg_create(const char* projectPath);
    void flux_pkg_destroy(void* pkg);
    
    // Install/uninstall
    int flux_pkg_install(void* pkg, const char* packageName, const char* version);
    int flux_pkg_uninstall(void* pkg, const char* packageName);
    int flux_pkg_update(void* pkg, const char* packageName);
    
    // Query
    int flux_pkg_is_installed(void* pkg, const char* packageName);
    const char* flux_pkg_get_version(void* pkg, const char* packageName);
    
    // Search
    const char* flux_pkg_search(void* pkg, const char* query);
    
    // Audit
    const char* flux_pkg_audit(void* pkg);
}

} // namespace PackageManager
} // namespace Flux

#endif // FLUX_PACKAGE_MANAGER_H
