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

#include "flux/package/package_manager.h"
#include "flux/package/toml_parser.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Flux {
namespace PackageManager {

// ============================================================================
// PackageVersion Implementation
// ============================================================================

std::string PackageVersion::toString() const
{
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!prerelease.empty())
        oss << "-" << prerelease;
    return oss.str();
}

PackageVersion PackageVersion::parse(const std::string& str)
{
    PackageVersion version;
    std::istringstream stream(str);
    char dot;

    stream >> version.major;
    stream >> dot; // consume '.'
    stream >> version.minor;
    stream >> dot; // consume '.'
    stream >> version.patch;

    // Check for prerelease
    size_t dashPos = str.find('-');
    if (dashPos != std::string::npos) {
        version.prerelease = str.substr(dashPos + 1);
    }

    return version;
}

bool PackageVersion::operator<(const PackageVersion& other) const
{
    if (major != other.major)
        return major < other.major;
    if (minor != other.minor)
        return minor < other.minor;
    if (patch != other.patch)
        return patch < other.patch;
    // Prerelease versions are less than release versions
    if (!prerelease.empty() && other.prerelease.empty())
        return true;
    if (prerelease.empty() && !other.prerelease.empty())
        return false;
    return prerelease < other.prerelease;
}

bool PackageVersion::operator==(const PackageVersion& other) const
{
    return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
}

bool PackageVersion::operator>=(const PackageVersion& other) const
{
    return !(*this < other);
}

// ============================================================================
// PackageManifest Implementation
// ============================================================================

std::string PackageManifest::toString() const
{
    std::ostringstream oss;
    oss << "[package]\n";
    oss << "name = \"" << name << "\"\n";
    oss << "version = \"" << version.toString() << "\"\n";
    oss << "description = \"" << description << "\"\n";
    oss << "author = \"" << author << "\"\n";
    oss << "license = \"" << license << "\"\n";
    if (!repository.empty())
        oss << "repository = \"" << repository << "\"\n";

    if (!dependencies.empty()) {
        oss << "\n[dependencies]\n";
        for (auto& dep : dependencies) {
            oss << dep.name << " = \"" << dep.versionConstraint << "\"\n";
        }
    }

    return oss.str();
}

PackageManifest PackageManifest::parse(const std::string& toml)
{
    PackageManifest manifest;

    // Simple TOML parsing for package manifests
    std::istringstream stream(toml);
    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        // Trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);
        if (line[0] == '#')
            continue;

        // Section header
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        // Key = value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Trim key and value
        key.erase(key.find_last_not_of(" \t") + 1);
        if (value[0] == '"') {
            size_t endQuote = value.find('"', 1);
            value = value.substr(1, endQuote - 1);
        }

        if (currentSection == "package" || currentSection.empty()) {
            if (key == "name")
                manifest.name = value;
            else if (key == "version")
                manifest.version = PackageVersion::parse(value);
            else if (key == "description")
                manifest.description = value;
            else if (key == "author")
                manifest.author = value;
            else if (key == "license")
                manifest.license = value;
            else if (key == "repository")
                manifest.repository = value;
        } else if (currentSection == "dependencies") {
            PackageDependency dep;
            dep.name = key;
            dep.versionConstraint = value;
            manifest.dependencies.push_back(dep);
        }
    }

    return manifest;
}

// ============================================================================
// PackageRegistry Implementation
// ============================================================================

PackageRegistry& PackageRegistry::instance()
{
    static PackageRegistry registry;
    return registry;
}

PackageRegistry::PackageRegistry()
{
    // Default to official Flux registry
    m_registries.push_back("https://packages.fluxscript.org");
}

std::vector<PackageInfo> PackageRegistry::search(const std::string& query)
{
    std::vector<PackageInfo> results;

    // In production, this queries the registry API
    // For now, return cached results
    for (auto& [name, info] : m_cache) {
        if (name.find(query) != std::string::npos || info.description.find(query) != std::string::npos) {
            results.push_back(info);
        }
    }

    return results;
}

std::vector<PackageInfo> PackageRegistry::searchByKeyword(const std::string& keyword)
{
    std::vector<PackageInfo> results;
    for (auto& [name, info] : m_cache) {
        for (auto& kw : info.keywords) {
            if (kw == keyword) {
                results.push_back(info);
                break;
            }
        }
    }
    return results;
}

PackageInfo PackageRegistry::getPackageInfo(const std::string& name)
{
    auto it = m_cache.find(name);
    if (it != m_cache.end())
        return it->second;

    // In production, fetch from registry API
    PackageInfo info;
    info.name = name;
    return info;
}

std::vector<PackageVersion> PackageRegistry::getVersions(const std::string& name)
{
    auto it = m_cache.find(name);
    if (it != m_cache.end())
        return it->second.versions;
    return {};
}

bool PackageRegistry::publish(const PackageManifest& manifest, const std::string& authToken)
{
    // In production, POST to registry API
    std::cout << "Publishing " << manifest.name << " " << manifest.version.toString() << "...\n";
    return true;
}

void PackageRegistry::addRegistry(const std::string& url)
{
    m_registries.push_back(url);
}

void PackageRegistry::setDefaultRegistry(const std::string& url)
{
    m_registries.clear();
    m_registries.push_back(url);
}

std::vector<std::string> PackageRegistry::getRegistries() const
{
    return m_registries;
}

// ============================================================================
// PackageManager Implementation
// ============================================================================

PackageManager::PackageManager() = default;
PackageManager::~PackageManager() = default;

void PackageManager::initialize(const std::string& projectPath)
{
    m_projectPath = projectPath;
    m_installDir = projectPath + "/packages";
    m_cacheDir = projectPath + "/.pkgcache";

    // Read Fluxfile.toml if it exists
    std::string fluxfilePath = projectPath + "/Fluxfile.toml";
    std::ifstream file(fluxfilePath);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        m_projectManifest = PackageManifest::parse(content);
    }

    // Load installed packages metadata
    loadInstalled();
}

bool PackageManager::install(const std::string& packageName, const std::string& version)
{
    std::cout << "Installing " << packageName;
    if (!version.empty())
        std::cout << " (" << version << ")";
    std::cout << "...\n";

    // Check if already installed
    if (isInstalled(packageName)) {
        std::cout << "  Already installed: " << getInstalledVersion(packageName).toString() << "\n";
        return true;
    }

    // Resolve version constraint
    PackageVersion targetVersion;
    if (version.empty()) {
        // Get latest version from registry
        auto info = PackageRegistry::instance().getPackageInfo(packageName);
        if (info.versions.empty()) {
            std::cerr << "  Error: package not found in registry\n";
            return false;
        }
        targetVersion = info.latestVersion;
    } else {
        targetVersion = PackageVersion::parse(version);
    }

    // Download and install
    std::string packagePath = m_installDir + "/" + packageName + "-" + targetVersion.toString();

    // In production, download from registry
    // For now, create mock package
    std::cout << "  Downloading " << packageName << " " << targetVersion.toString() << "...\n";
    std::cout << "  Installed to " << packagePath << "\n";

    // Add to installed
    InstalledPackage pkg;
    pkg.name = packageName;
    pkg.version = targetVersion;
    pkg.installPath = packagePath;
    pkg.editable = false;
    m_installed.push_back(pkg);

    // Update dependencies
    resolveDependencies();

    // Save state
    saveInstalled();

    return true;
}

bool PackageManager::installFromGit(const std::string& gitUrl, const std::string& branch)
{
    std::cout << "Installing from Git: " << gitUrl << " (" << branch << ")\n";
    // In production, clone repo and build
    return true;
}

bool PackageManager::installLocal(const std::string& path)
{
    std::cout << "Installing local package: " << path << "\n";
    // Copy to packages directory
    return true;
}

bool PackageManager::update(const std::string& packageName)
{
    auto* pkg = findInstalled(packageName);
    if (!pkg) {
        std::cerr << "  Error: " << packageName << " is not installed\n";
        return false;
    }

    // Check for newer version
    auto info = PackageRegistry::instance().getPackageInfo(packageName);
    if (info.latestVersion > pkg->version) {
        std::cout << "  Updating " << packageName << ": " << pkg->version.toString() << " -> "
                  << info.latestVersion.toString() << "\n";
        pkg->version = info.latestVersion;
        saveInstalled();
        return true;
    }

    std::cout << "  " << packageName << " is up to date\n";
    return true;
}

bool PackageManager::updateAll()
{
    bool updated = false;
    for (auto& pkg : m_installed) {
        if (update(pkg.name))
            updated = true;
    }
    return updated;
}

std::vector<std::string> PackageManager::getOutdatedPackages()
{
    std::vector<std::string> outdated;
    for (auto& pkg : m_installed) {
        auto info = PackageRegistry::instance().getPackageInfo(pkg.name);
        if (!info.versions.empty() && info.latestVersion > pkg.version) {
            outdated.push_back(pkg.name);
        }
    }
    return outdated;
}

bool PackageManager::uninstall(const std::string& packageName)
{
    for (auto it = m_installed.begin(); it != m_installed.end(); ++it) {
        if (it->name == packageName) {
            std::cout << "  Uninstalling " << packageName << " " << it->version.toString() << "...\n";
            m_installed.erase(it);
            saveInstalled();
            return true;
        }
    }
    std::cerr << "  Error: " << packageName << " is not installed\n";
    return false;
}

bool PackageManager::clean()
{
    std::cout << "Cleaning unused packages...\n";
    // Remove packages not in dependency tree
    return true;
}

std::vector<InstalledPackage> PackageManager::getInstalledPackages() const
{
    return m_installed;
}

bool PackageManager::isInstalled(const std::string& packageName) const
{
    return findInstalled(packageName) != nullptr;
}

PackageVersion PackageManager::getInstalledVersion(const std::string& packageName) const
{
    auto* pkg = findInstalled(packageName);
    if (pkg)
        return pkg->version;
    return PackageVersion();
}

InstalledPackage* PackageManager::findInstalled(const std::string& name)
{
    for (auto& pkg : m_installed) {
        if (pkg.name == name)
            return &pkg;
    }
    return nullptr;
}

const InstalledPackage* PackageManager::findInstalled(const std::string& name) const
{
    for (auto& pkg : m_installed) {
        if (pkg.name == name)
            return &pkg;
    }
    return nullptr;
}

// ============================================================================
// Dependency Resolution
// ============================================================================

bool PackageManager::resolveDependencies()
{
    std::cout << "Resolving dependencies...\n";
    for (auto& dep : m_projectManifest.dependencies) {
        if (!isInstalled(dep.name)) {
            install(dep.name);
        }
    }
    updateLockFile();
    std::cout << "  Dependency resolution complete\n";
    return true;
}

bool PackageManager::resolveDependency(const PackageDependency& dep,
                                       std::map<std::string, std::vector<PackageDependency>>& depGraph)
{
    auto info = PackageRegistry::instance().getPackageInfo(dep.name);
    if (info.versions.empty())
        return false;

    PackageVersion bestVersion;
    for (auto& v : info.versions) {
        if (satisfiesConstraint(v, dep.versionConstraint)) {
            if (v > bestVersion)
                bestVersion = v;
        }
    }

    if (bestVersion == PackageVersion())
        return false;
    std::cout << "  Resolved " << dep.name << " -> " << bestVersion.toString() << "\n";
    return true;
}

void PackageManager::buildDependencyTree(const std::string& packageName, const std::string& prefix,
                                         std::vector<std::string>& tree)
{
    tree.push_back(prefix + packageName);
}

bool PackageManager::satisfiesConstraint(const PackageVersion& version, const std::string& constraint)
{
    std::istringstream stream(constraint);
    std::string part;

    while (stream >> part) {
        if (part.substr(0, 2) == ">=") {
            auto req = PackageVersion::parse(part.substr(2));
            if (!(version >= req))
                return false;
        } else if (part.substr(0, 1) == ">") {
            auto req = PackageVersion::parse(part.substr(1));
            if (!(version > req))
                return false;
        } else if (part.substr(0, 1) == "<") {
            auto req = PackageVersion::parse(part.substr(1));
            if (!(version < req))
                return false;
        } else if (part.substr(0, 2) == "<=") {
            auto req = PackageVersion::parse(part.substr(2));
            if (version > req && !(version == req))
                return false;
        } else if (part.substr(0, 1) == "^") {
            auto req = PackageVersion::parse(part.substr(1));
            if (version.major != req.major || version < req)
                return false;
        } else if (part.substr(0, 1) == "~") {
            auto req = PackageVersion::parse(part.substr(1));
            if (version.major != req.major || version.minor != req.minor || version < req)
                return false;
        } else {
            auto req = PackageVersion::parse(part);
            if (!(version == req))
                return false;
        }
    }

    return true;
}

// ============================================================================
// Internal Methods (stubs for future implementation)
// ============================================================================

bool PackageManager::downloadPackage(const std::string& name, const PackageVersion& version)
{
    std::cout << "  Downloading " << name << " " << version.toString() << "...\n";
    return true;
}

bool PackageManager::extractPackage(const std::string& archive, const std::string& dest)
{
    // In production, extract tarball/zip
    return true;
}

bool PackageManager::runInstallScript(const std::string& packagePath)
{
    std::string scriptPath = packagePath + "/install.flux";
    std::ifstream file(scriptPath);
    if (!file.is_open())
        return true; // No script is OK

    // Execute install script via JIT
    return true;
}

void PackageManager::updateLockFile()
{
    // Write Fluxfile.lock with resolved dependency versions
    std::string lockPath = m_projectPath + "/Fluxfile.lock";
    std::ofstream file(lockPath);
    if (!file.is_open())
        return;

    file << "# Auto-generated dependency lock file\n";
    file << "# Do not edit manually\n\n";
    for (auto& pkg : m_installed) {
        file << pkg.name << " = \"" << pkg.version.toString() << "\"\n";
    }
}

void PackageManager::loadInstalled()
{
    std::string metadataPath = m_installDir + "/installed.json";
    std::ifstream file(metadataPath);
    if (!file.is_open())
        return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Simple JSON parsing
    size_t namePos = content.find("\"name\"");
    while (namePos != std::string::npos) {
        size_t valueStart = content.find(':', namePos);
        size_t quoteStart = content.find('"', valueStart);
        size_t quoteEnd = content.find('"', quoteStart + 1);

        std::string name = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

        InstalledPackage pkg;
        pkg.name = name;
        pkg.installPath = m_installDir + "/" + name;
        pkg.editable = false;

        m_installed.push_back(pkg);

        namePos = content.find("\"name\"", quoteEnd);
    }
}

void PackageManager::saveInstalled()
{
    // Ensure directory exists
    // In production, write metadata file
}

void PackageManager::clearCache()
{
    std::cout << "Clearing package cache...\n";
}

void PackageManager::setRegistry(const std::string& url)
{
    m_registryUrl = url;
}

void PackageManager::setCacheDir(const std::string& path)
{
    m_cacheDir = path;
}

void PackageManager::setInstallDir(const std::string& path)
{
    m_installDir = path;
}

PackageManager::AuditResult PackageManager::audit()
{
    AuditResult result;
    result.hasIssues = false;
    result.outdatedPackages = getOutdatedPackages();
    return result;
}

// ============================================================================
// Dependency Resolution
// ============================================================================

bool VersionConstraint::satisfies(const PackageVersion& version, const std::string& constraint)
{
    std::istringstream stream(constraint);
    std::string part;

    while (stream >> part) {
        if (part.substr(0, 2) == ">=") {
            auto req = PackageVersion::parse(part.substr(2));
            if (!(version >= req))
                return false;
        } else if (part.substr(0, 1) == ">") {
            auto req = PackageVersion::parse(part.substr(1));
            if (!(version > req))
                return false;
        } else if (part.substr(0, 1) == "<") {
            auto req = PackageVersion::parse(part.substr(1));
            if (!(version < req))
                return false;
        } else if (part.substr(0, 2) == "<=") {
            auto req = PackageVersion::parse(part.substr(2));
            if (version > req && !(version == req))
                return false;
        } else if (part.substr(0, 1) == "^") {
            auto req = PackageVersion::parse(part.substr(1));
            if (version.major != req.major || version < req)
                return false;
        } else if (part.substr(0, 1) == "~") {
            auto req = PackageVersion::parse(part.substr(1));
            if (version.major != req.major || version.minor != req.minor || version < req)
                return false;
        } else {
            auto req = PackageVersion::parse(part);
            if (!(version == req))
                return false;
        }
    }

    return true;
}

bool VersionConstraint::isCompatible(const PackageVersion& v1, const PackageVersion& v2)
{
    return v1.major == v2.major;
}

bool VersionConstraint::matchRange(const PackageVersion& version, const PackageVersion& min, const PackageVersion& max)
{
    return version >= min && version <= max;
}

} // namespace PackageManager
} // namespace Flux
