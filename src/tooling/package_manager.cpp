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

#include "flux/tooling/package_manager.h"
#include "flux/runtime/flux_runtime.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#ifdef FLUX_HAS_CURL
#include <curl/curl.h>
#endif

namespace fs = std::filesystem;

namespace Flux {

PackageManager& PackageManager::instance() {
    static PackageManager manager;
    return manager;
}

PackageManager::PackageManager() : m_initialized(false) {
    const char* home = std::getenv("HOME");
    if (home) {
        m_packagesDir = std::string(home) + "/.fluxscript/packages";
    } else {
        m_packagesDir = ".fluxscript/packages";
    }
}

PackageManager::~PackageManager() {
    finalize();
}

void PackageManager::initialize(const std::string& registryUrl) {
    if (m_initialized) return;
    
    m_registryUrl = registryUrl;
    
    // Create packages directory
    fs::create_directories(m_packagesDir);
    
    // Initialize curl
#ifdef FLUX_HAS_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    
    // Load installed packages
    fs::path installedPath = fs::path(m_packagesDir) / "installed.json";
    if (fs::exists(installedPath)) {
        // Parse installed.json (simplified - would use JSON library in production)
        std::cout << "Loading installed packages..." << std::endl;
    }
    
    // Add default repositories
    m_repositories["official"] = "https://packages.fluxscript.org/official";
    m_repositories["community"] = "https://packages.fluxscript.org/community";
    
    m_initialized = true;
    
    std::cout << "" << std::endl;
    std::cout << "       FLUXSCRIPT PACKAGE MANAGER                       " << std::endl;
    std::cout << "" << std::endl;
    std::cout << " Registry: " << std::left << std::setw(46) << m_registryUrl << "" << std::endl;
    std::cout << " Packages Dir: " << std::left << std::setw(42) << m_packagesDir << "" << std::endl;
    std::cout << " Repositories:                                          " << std::endl;
    for (const auto& [name, url] : m_repositories) {
        std::cout << "    " << std::left << std::setw(48) << name << "" << std::endl;
    }
    std::cout << "" << std::endl;
}

void PackageManager::finalize() {
    curl_global_cleanup();
    m_initialized = false;
}

std::vector<PackageSearchResult> PackageManager::search(const std::string& query) {
    std::vector<PackageSearchResult> results;
    
    std::cout << "\nSearching for \"" << query << "\"..." << std::endl;
    
    // In production, this would query the registry API
    // For now, return mock results
    
    if (query.empty() || query == "*") {
        // Return all available packages
        PackageSearchResult mathLib;
        mathLib.info.name = "math-lib";
        mathLib.info.version = "2.1.0";
        mathLib.info.description = "Comprehensive mathematical functions library";
        mathLib.info.author = "FluxScript Team";
        mathLib.info.license = "MIT";
        mathLib.installed = false;
        results.push_back(mathLib);
        
        PackageSearchResult signalProc;
        signalProc.info.name = "signal-processing";
        signalProc.info.version = "1.5.2";
        signalProc.info.description = "DSP and signal processing algorithms";
        signalProc.info.author = "DSP Labs";
        signalProc.info.license = "Apache-2.0";
        signalProc.installed = false;
        results.push_back(signalProc);
        
        PackageSearchResult controlSys;
        controlSys.info.name = "control-systems";
        controlSys.info.version = "3.0.1";
        controlSys.info.description = "Control theory and system analysis";
        controlSys.info.author = "Control Team";
        controlSys.info.license = "BSD-3";
        controlSys.installed = false;
        results.push_back(controlSys);
    }
    
    // Print results
    std::cout << "\n" << std::endl;
    std::cout << "          SEARCH RESULTS                                " << std::endl;
    std::cout << "" << std::endl;
    
    for (const auto& result : results) {
        std::string line = result.info.name + " v" + result.info.version;
        std::cout << " " << std::left << std::setw(51) << line << "" << std::endl;
        std::cout << "   " << std::left << std::setw(48) << result.info.description << "" << std::endl;
    }
    
    std::cout << "" << std::endl;
    std::cout << "Found " << results.size() << " package(s)" << std::endl;
    
    return results;
}

PackageInfo PackageManager::getPackageInfo(const std::string& name) {
    PackageInfo info;
    
    // In production, fetch from registry
    // For now, return mock data
    if (name == "math-lib") {
        info.name = "math-lib";
        info.version = "2.1.0";
        info.description = "Comprehensive mathematical functions library";
        info.author = "FluxScript Team";
        info.license = "MIT";
        info.dependencies = {"stdlib"};
        info.keywords = {"math", "trig", "calculus"};
        info.repository = "official";
    }
    
    return info;
}

InstallStatus PackageManager::install(const std::string& name,
                                      const std::string& version) {
    std::cout << "\n" << std::endl;
    std::cout << "          INSTALLING PACKAGE                            " << std::endl;
    std::cout << "" << std::endl;
    std::cout << " Package: " << std::left << std::setw(47) << name << "" << std::endl;
    if (!version.empty()) {
        std::cout << " Version: " << std::left << std::setw(47) << version << "" << std::endl;
    }
    std::cout << "" << std::endl;
    
    std::vector<std::string> installed;
    return installInternal(name, version, installed);
}

InstallStatus PackageManager::installInternal(const std::string& name,
                                              const std::string& version,
                                              std::vector<std::string>& installed) {
    // Check if already installed
    if (m_installedPackages.find(name) != m_installedPackages.end()) {
        std::cout << "  Package " << name << " is already installed." << std::endl;
        return InstallStatus::InstallAlreadyInstalled;
    }
    
    // Resolve dependencies first
    std::vector<std::string> dependencies;
    if (!resolveDependencies(name, version, dependencies)) {
        std::cout << "  Error: Failed to resolve dependencies" << std::endl;
        return InstallStatus::InstallDependencyError;
    }
    
    // Install dependencies
    for (const auto& dep : dependencies) {
        std::cout << "  Installing dependency: " << dep << std::endl;
        installInternal(dep, "", installed);
    }
    
    // Download package
    std::string packagePath = fs::path(m_packagesDir) / name;
    std::cout << "  Downloading " << name << "..." << std::endl;
    
    if (!downloadPackage(name, version, packagePath)) {
        std::cout << "  Error: Failed to download package" << std::endl;
        return InstallStatus::InstallDownloadError;
    }
    
    // Verify package
    if (!verifyPackage(packagePath)) {
        std::cout << "  Error: Package verification failed" << std::endl;
        return InstallStatus::InstallInstallError;
    }
    
    // Mark as installed
    PackageInfo info = getPackageInfo(name);
    m_installedPackages[name] = info;
    installed.push_back(name);
    
    std::cout << "   Successfully installed " << name << " v" << info.version << std::endl;
    
    return InstallStatus::InstallSuccess;
}

bool PackageManager::resolveDependencies(const std::string& name,
                                         const std::string& version,
                                         std::vector<std::string>& dependencies) {
    // In production, query registry for dependency tree
    // For now, mock dependency resolution
    
    PackageInfo info = getPackageInfo(name);
    
    for (const auto& dep : info.dependencies) {
        if (m_installedPackages.find(dep) == m_installedPackages.end()) {
            dependencies.push_back(dep);
        }
    }
    
    return true;
}

bool PackageManager::downloadPackage(const std::string& name,
                                     const std::string& version,
                                     const std::string& destPath) {
    // In production, use curl to download from registry
    // For now, create mock package structure
    
    fs::create_directories(destPath);
    
    // Create mock package.flux file
    std::ofstream file(fs::path(destPath) / "package.flux");
    if (file.is_open()) {
        file << "# " << name << " v" << (version.empty() ? "1.0.0" : version) << "\n";
        file << "# Mock package for demonstration\n";
        file.close();
        return true;
    }
    
    return false;
}

bool PackageManager::verifyPackage(const std::string& packagePath) {
    // In production, verify signature and checksum
    // For now, just check if package.flux exists
    return fs::exists(fs::path(packagePath) / "package.flux");
}

bool PackageManager::uninstall(const std::string& name) {
    std::cout << "\nUninstalling " << name << "..." << std::endl;
    
    auto it = m_installedPackages.find(name);
    if (it == m_installedPackages.end()) {
        std::cout << "  Package " << name << " is not installed." << std::endl;
        return false;
    }
    
    // Remove package directory
    fs::path packagePath = fs::path(m_packagesDir) / name;
    if (fs::exists(packagePath)) {
        fs::remove_all(packagePath);
    }
    
    // Remove from installed list
    m_installedPackages.erase(it);
    
    std::cout << "   Successfully uninstalled " << name << std::endl;
    return true;
}

int PackageManager::update(const std::string& name) {
    std::cout << "\n" << std::endl;
    std::cout << "          CHECKING FOR UPDATES                          " << std::endl;
    std::cout << "" << std::endl;
    
    int updated = 0;
    
    if (name.empty()) {
        // Update all packages
        for (const auto& [pkgName, info] : m_installedPackages) {
            std::cout << "  Checking " << pkgName << "..." << std::endl;
            // In production, check registry for newer version
            std::cout << "    " << pkgName << " is up to date." << std::endl;
        }
    } else {
        // Update specific package
        std::cout << "  Checking " << name << "..." << std::endl;
        // In production, check registry for newer version
        std::cout << "    " << name << " is up to date." << std::endl;
    }
    
    std::cout << "" << std::endl;
    std::cout << "Updated " << updated << " package(s)" << std::endl;
    
    return updated;
}

std::vector<PackageInfo> PackageManager::listInstalled() {
    std::vector<PackageInfo> packages;
    
    for (const auto& [name, info] : m_installedPackages) {
        packages.push_back(info);
    }
    
    return packages;
}

std::vector<std::pair<std::string, std::string>> PackageManager::checkForUpdates() {
    std::vector<std::pair<std::string, std::string>> updates;
    
    // In production, query registry for newer versions
    // For now, return empty list
    
    return updates;
}

bool PackageManager::addRepository(const std::string& name, const std::string& url) {
    m_repositories[name] = url;
    std::cout << "Added repository: " << name << " (" << url << ")" << std::endl;
    return true;
}

bool PackageManager::removeRepository(const std::string& name) {
    auto it = m_repositories.find(name);
    if (it != m_repositories.end()) {
        m_repositories.erase(it);
        std::cout << "Removed repository: " << name << std::endl;
        return true;
    }
    return false;
}

std::map<std::string, std::string> PackageManager::listRepositories() {
    return m_repositories;
}

std::string PackageManager::getPackagePath(const std::string& name) {
    return fs::path(m_packagesDir) / name;
}

bool PackageManager::import(const std::string& name) {
    std::string packagePath = getPackagePath(name);
    
    if (!fs::exists(packagePath)) {
        std::cout << "Package " << name << " is not installed." << std::endl;
        std::cout << "Run: flux-pkg install " << name << std::endl;
        return false;
    }
    
    std::cout << "Importing package: " << name << std::endl;
    // In production, add package path to module search paths
    return true;
}

} // namespace Flux
