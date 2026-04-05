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

namespace Flux {

// Package metadata
struct PackageInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> dependencies;
    std::vector<std::string> keywords;
    std::string repository;  // URL to package repository
};

// Search result
struct PackageSearchResult {
    PackageInfo info;
    bool installed;
    std::string installedVersion;
    std::string latestVersion;
};

// Installation status
enum InstallStatus {
    InstallSuccess,
    InstallAlreadyInstalled,
    InstallDependencyError,
    InstallDownloadError,
    InstallInstallError,
    InstallNotFound
};

// Package manager class
class PackageManager {
public:
    static PackageManager& instance();
    
    // Initialize package manager
    void initialize(const std::string& registryUrl = "https://packages.fluxscript.org");
    void finalize();
    
    // Search packages
    std::vector<PackageSearchResult> search(const std::string& query);
    
    // Get package info
    PackageInfo getPackageInfo(const std::string& name);
    
    // Install package
    InstallStatus install(const std::string& name, const std::string& version = "");
    
    // Uninstall package
    bool uninstall(const std::string& name);
    
    // Update packages
    int update(const std::string& name = "");  // Empty = update all
    
    // List installed packages
    std::vector<PackageInfo> listInstalled();
    
    // Check for updates
    std::vector<std::pair<std::string, std::string>> checkForUpdates();
    
    // Add repository
    bool addRepository(const std::string& name, const std::string& url);
    
    // Remove repository
    bool removeRepository(const std::string& name);
    
    // List repositories
    std::map<std::string, std::string> listRepositories();
    
    // Get installation directory for a package
    std::string getPackagePath(const std::string& name);
    
    // Import package in FluxScript code
    bool import(const std::string& name);

private:
    PackageManager();
    ~PackageManager();
    
    InstallStatus installInternal(const std::string& name, 
                                  const std::string& version,
                                  std::vector<std::string>& installed);
    bool resolveDependencies(const std::string& name, 
                            const std::string& version,
                            std::vector<std::string>& dependencies);
    bool downloadPackage(const std::string& name, 
                        const std::string& version,
                        const std::string& destPath);
    bool verifyPackage(const std::string& packagePath);
    
    std::string m_registryUrl;
    std::map<std::string, std::string> m_repositories;
    std::map<std::string, PackageInfo> m_installedPackages;
    std::map<std::string, PackageInfo> m_availablePackages;
    std::string m_packagesDir;
    bool m_initialized;
};

} // namespace Flux

#endif // FLUX_PACKAGE_MANAGER_H
