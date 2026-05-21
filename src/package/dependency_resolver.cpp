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

// ============================================================================
// FluxScript Enhanced Dependency Resolver Implementation
// ============================================================================

#include "flux/package/dependency_resolver.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>

namespace Flux {

// ============================================================================
// SemanticVersion
// ============================================================================

SemanticVersion SemanticVersion::parse(const std::string& str)
{
    SemanticVersion v;
    int n = sscanf(str.c_str(), "%d.%d.%d", &v.major, &v.minor, &v.patch);
    if (n < 1)
        return v;

    // Check for prerelease
    size_t dash = str.find('-');
    if (dash != std::string::npos) {
        v.prerelease = str.substr(dash + 1);
    }

    return v;
}

std::string SemanticVersion::str() const
{
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    if (!prerelease.empty())
        oss << "-" << prerelease;
    return oss.str();
}

bool SemanticVersion::operator<(const SemanticVersion& other) const
{
    if (major != other.major)
        return major < other.major;
    if (minor != other.minor)
        return minor < other.minor;
    if (patch != other.patch)
        return patch < other.patch;
    // Prerelease < release
    if (prerelease.empty() && !other.prerelease.empty())
        return false;
    if (!prerelease.empty() && other.prerelease.empty())
        return true;
    return prerelease < other.prerelease;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const
{
    return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const
{
    return !(*this < other);
}

// ============================================================================
// VersionConstraint
// ============================================================================

VersionConstraint VersionConstraint::parse(const std::string& str)
{
    VersionConstraint vc;
    vc.raw = str;

    if (str.empty() || str == "*") {
        vc.type = Any;
        return vc;
    }

    // Range: >=1.0.0,<2.0.0
    size_t comma = str.find(',');
    if (comma != std::string::npos) {
        vc.type = Range;
        std::string left = str.substr(0, comma);
        std::string right = str.substr(comma + 1);
        VersionConstraint min_c = VersionConstraint::parse(left);
        VersionConstraint max_c = VersionConstraint::parse(right);
        vc.min_version = min_c.min_version;
        vc.max_version = max_c.min_version;
        return vc;
    }

    // Caret: ^1.2.3
    if (str[0] == '^') {
        vc.type = Caret;
        vc.min_version = SemanticVersion::parse(str.substr(1));
        // ^1.2.3 means >=1.2.3, <2.0.0
        vc.max_version = {vc.min_version.major + 1, 0, 0};
        return vc;
    }

    // Tilde: ~1.2.3
    if (str[0] == '~') {
        vc.type = Tilde;
        vc.min_version = SemanticVersion::parse(str.substr(1));
        // ~1.2.3 means >=1.2.3, <1.3.0
        vc.max_version = {vc.min_version.major, vc.min_version.minor + 1, 0};
        return vc;
    }

    // Operators
    if (str.substr(0, 2) == ">=") {
        vc.type = GTE;
        vc.min_version = SemanticVersion::parse(str.substr(2));
    } else if (str.substr(0, 2) == "<=") {
        vc.type = LTE;
        vc.min_version = SemanticVersion::parse(str.substr(2));
    } else if (str[0] == '>') {
        vc.type = GT;
        vc.min_version = SemanticVersion::parse(str.substr(1));
    } else if (str[0] == '<') {
        vc.type = LT;
        vc.min_version = SemanticVersion::parse(str.substr(1));
    } else if (str[0] == '=') {
        vc.type = Exact;
        vc.min_version = SemanticVersion::parse(str.substr(1));
    } else {
        vc.type = Exact;
        vc.min_version = SemanticVersion::parse(str);
    }

    return vc;
}

bool VersionConstraint::satisfies(const SemanticVersion& version) const
{
    switch (type) {
    case Any:
        return true;
    case Exact:
        return version == min_version;
    case GTE:
        return version >= min_version;
    case LTE:
        return version < min_version || version == min_version;
    case GT:
        return min_version < version;
    case LT:
        return version < min_version;
    case Caret:
        return version >= min_version && version < max_version;
    case Tilde:
        return version >= min_version && version < max_version;
    case Range:
        return version >= min_version && version < max_version;
    default:
        return true;
    }
}

// ============================================================================
// DependencyResolver Singleton
// ============================================================================

DependencyResolver& DependencyResolver::instance()
{
    static DependencyResolver instance;
    return instance;
}

void DependencyResolver::registerPackage(const PackageMetadata& pkg)
{
    m_registry[pkg.name][pkg.version] = pkg;
}

void DependencyResolver::registerPackages(const std::vector<PackageMetadata>& pkgs)
{
    for (const auto& pkg : pkgs) {
        registerPackage(pkg);
    }
}

void DependencyResolver::clearRegistry()
{
    m_registry.clear();
}

ResolutionResult DependencyResolver::resolve(const std::vector<Dependency>& root_deps)
{
    ResolutionResult result;
    result.success = true;

    std::map<std::string, SemanticVersion> resolved;

    for (const auto& dep : root_deps) {
        std::vector<std::string> path;
        std::string error;
        if (!resolveRecursive(dep.name, dep.constraint, 0, resolved, path, error)) {
            result.success = false;
            result.error_message = error;
            result.conflict_chain = path;
            return result;
        }
    }

    // Convert to output format
    for (const auto& [name, version] : resolved) {
        ResolvedPackage rp;
        rp.name = name;
        rp.version = version;
        result.packages.push_back(rp);
    }

    return result;
}

bool DependencyResolver::resolveRecursive(const std::string& pkg_name, const VersionConstraint& constraint, int depth,
                                          std::map<std::string, SemanticVersion>& resolved,
                                          std::vector<std::string>& path, std::string& error)
{
    if (depth > 100) {
        error = "Dependency resolution exceeded maximum depth (cycle detected?)";
        return false;
    }

    path.push_back(pkg_name + " " + constraint.raw);

    // Check if already resolved
    auto it = resolved.find(pkg_name);
    if (it != resolved.end()) {
        // Check if existing version satisfies constraint
        if (constraint.satisfies(it->second)) {
            path.pop_back();
            return true;
        } else {
            error = "Version conflict for " + pkg_name + ": " + "need " + constraint.raw + ", but " + it->second.str() +
                    " already resolved";
            return false;
        }
    }

    // Find best version
    SemanticVersion best = selectBestVersion(pkg_name, constraint, resolved);
    if (best.major == 0 && best.minor == 0 && best.patch == 0 && best.prerelease.empty()) {
        auto versions = getAvailableVersions(pkg_name);
        if (versions.empty()) {
            error = "Package not found: " + pkg_name;
        } else {
            error = "No version of " + pkg_name + " satisfies constraint: " + constraint.raw;
        }
        return false;
    }

    resolved[pkg_name] = best;

    // Get dependencies of selected version
    auto pkg_it = m_registry.find(pkg_name);
    if (pkg_it == m_registry.end()) {
        error = "Package not in registry: " + pkg_name;
        return false;
    }

    auto ver_it = pkg_it->second.find(best);
    if (ver_it == pkg_it->second.end()) {
        error = "Version " + best.str() + " not found for " + pkg_name;
        return false;
    }

    // Resolve transitive dependencies
    for (const auto& dep : ver_it->second.dependencies) {
        if (!resolveRecursive(dep.name, dep.constraint, depth + 1, resolved, path, error)) {
            // Backtrack
            resolved.erase(pkg_name);
            return false;
        }
    }

    path.pop_back();
    return true;
}

SemanticVersion DependencyResolver::selectBestVersion(const std::string& name, const VersionConstraint& constraint,
                                                      const std::map<std::string, SemanticVersion>& already_resolved)
{
    auto pkg_it = m_registry.find(name);
    if (pkg_it == m_registry.end()) {
        return {};
    }

    SemanticVersion best;
    bool found = false;

    // Iterate versions in reverse (highest first)
    for (auto it = pkg_it->second.rbegin(); it != pkg_it->second.rend(); ++it) {
        const auto& version = it->first;
        if (constraint.satisfies(version)) {
            // Check if this version conflicts with already-resolved packages
            bool conflict = false;
            for (const auto& dep : it->second.dependencies) {
                auto resolved_it = already_resolved.find(dep.name);
                if (resolved_it != already_resolved.end()) {
                    if (!dep.constraint.satisfies(resolved_it->second)) {
                        conflict = true;
                        break;
                    }
                }
            }

            if (!conflict) {
                best = version;
                found = true;
                break;
            }
        }
    }

    return found ? best : SemanticVersion{};
}

bool DependencyResolver::hasConflict(const std::string& name, const SemanticVersion& version,
                                     const std::map<std::string, SemanticVersion>& resolved) const
{
    auto pkg_it = m_registry.find(name);
    if (pkg_it == m_registry.end())
        return true;

    auto ver_it = pkg_it->second.find(version);
    if (ver_it == pkg_it->second.end())
        return true;

    for (const auto& dep : ver_it->second.dependencies) {
        auto resolved_it = resolved.find(dep.name);
        if (resolved_it != resolved.end()) {
            if (!dep.constraint.satisfies(resolved_it->second)) {
                return true;
            }
        }
    }

    return false;
}

SemanticVersion DependencyResolver::findBestVersion(const std::string& name, const VersionConstraint& constraint)
{
    std::map<std::string, SemanticVersion> empty;
    return selectBestVersion(name, constraint, empty);
}

std::vector<SemanticVersion> DependencyResolver::getAvailableVersions(const std::string& name) const
{
    std::vector<SemanticVersion> versions;
    auto it = m_registry.find(name);
    if (it == m_registry.end())
        return versions;

    for (const auto& [ver, meta] : it->second) {
        versions.push_back(ver);
    }

    std::sort(versions.rbegin(), versions.rend());
    return versions;
}

std::vector<DepGraphNode> DependencyResolver::buildDependencyTree(const std::vector<Dependency>& root_deps)
{
    std::vector<DepGraphNode> tree;
    std::set<std::string> visited;

    for (const auto& dep : root_deps) {
        auto version = findBestVersion(dep.name, dep.constraint);
        if (version.major == 0 && version.minor == 0 && version.patch == 0)
            continue;

        auto nodes = buildTreeRecursive(dep.name, version, 0, visited);
        tree.insert(tree.end(), nodes.begin(), nodes.end());
    }

    return tree;
}

std::vector<DepGraphNode> DependencyResolver::buildTreeRecursive(const std::string& name,
                                                                 const SemanticVersion& version, int depth,
                                                                 std::set<std::string>& visited)
{
    std::vector<DepGraphNode> nodes;
    std::string key = name + "@" + version.str();

    if (visited.count(key))
        return nodes;
    visited.insert(key);

    DepGraphNode node;
    node.name = name;
    node.version = version;
    node.depth = depth;

    auto pkg_it = m_registry.find(name);
    if (pkg_it != m_registry.end()) {
        auto ver_it = pkg_it->second.find(version);
        if (ver_it != pkg_it->second.end()) {
            for (const auto& dep : ver_it->second.dependencies) {
                auto child_version = findBestVersion(dep.name, dep.constraint);
                if (child_version.major != 0 || child_version.minor != 0 || child_version.patch != 0) {
                    auto children = buildTreeRecursive(dep.name, child_version, depth + 1, visited);
                    node.dependencies.push_back(dep.name);
                    nodes.insert(nodes.end(), children.begin(), children.end());
                }
            }
        }
    }

    nodes.insert(nodes.begin(), node);
    return nodes;
}

bool DependencyResolver::isSatisfiable(const std::string& name, const VersionConstraint& constraint) const
{
    auto it = m_registry.find(name);
    if (it == m_registry.end())
        return false;

    for (const auto& [ver, meta] : it->second) {
        if (constraint.satisfies(ver))
            return true;
    }
    return false;
}

int DependencyResolver::registrySize() const
{
    return m_registry.size();
}

} // namespace Flux
