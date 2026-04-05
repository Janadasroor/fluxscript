// ============================================================================
// FluxScript Enhanced Dependency Resolver
// Topological sort, conflict detection, version constraint solving
// ============================================================================

#ifndef FLUX_DEPENDENCY_RESOLVER_H
#define FLUX_DEPENDENCY_RESOLVER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace Flux {

// Version
struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    static SemanticVersion parse(const std::string& str);
    std::string str() const;
    bool operator<(const SemanticVersion& other) const;
    bool operator==(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const { return other < *this; }
    bool operator>=(const SemanticVersion& other) const;
    bool operator!=(const SemanticVersion& other) const { return !(*this == other); }
    bool operator<=(const SemanticVersion& other) const { return !(*this > other); }
};

// Version constraint: ^1.2.0, >=1.0, ~1.2, =1.2.3, etc.
struct VersionConstraint {
    std::string raw;
    enum Type { Exact, GTE, LTE, GT, LT, Caret, Tilde, Range, Any };
    Type type = Any;
    SemanticVersion min_version;
    SemanticVersion max_version;

    static VersionConstraint parse(const std::string& str);
    bool satisfies(const SemanticVersion& version) const;
};

// Dependency
struct Dependency {
    std::string name;
    VersionConstraint constraint;
};

// Package metadata
struct PackageMetadata {
    std::string name;
    SemanticVersion version;
    std::vector<Dependency> dependencies;
};

// Resolution result
struct ResolvedPackage {
    std::string name;
    SemanticVersion version;
};

// Resolution result or error
struct ResolutionResult {
    bool success;
    std::vector<ResolvedPackage> packages;
    std::string error_message;
    std::vector<std::string> conflict_chain;
};

// Dependency graph node
struct DepGraphNode {
    std::string name;
    SemanticVersion version;
    std::vector<std::string> dependencies;
    int depth = 0;
};

// Enhanced Dependency Resolver
class DependencyResolver {
public:
    static DependencyResolver& instance();

    // Register available packages (simulating registry)
    void registerPackage(const PackageMetadata& pkg);
    void registerPackages(const std::vector<PackageMetadata>& pkgs);
    void clearRegistry();

    // Resolve dependencies for a set of root packages
    ResolutionResult resolve(const std::vector<Dependency>& root_deps);

    // Resolve single dependency (find best version)
    SemanticVersion findBestVersion(const std::string& name, const VersionConstraint& constraint);

    // Get all versions of a package
    std::vector<SemanticVersion> getAvailableVersions(const std::string& name) const;

    // Build dependency tree (for visualization)
    std::vector<DepGraphNode> buildDependencyTree(const std::vector<Dependency>& root_deps);

    // Check if constraint is satisfiable
    bool isSatisfiable(const std::string& name, const VersionConstraint& constraint) const;

    // Statistics
    int registrySize() const;

private:
    DependencyResolver() = default;
    ~DependencyResolver() = default;

    // Internal resolution helpers
    bool resolveRecursive(
        const std::string& pkg_name,
        const VersionConstraint& constraint,
        int depth,
        std::map<std::string, SemanticVersion>& resolved,
        std::vector<std::string>& path,
        std::string& error
    );

    SemanticVersion selectBestVersion(
        const std::string& name,
        const VersionConstraint& constraint,
        const std::map<std::string, SemanticVersion>& already_resolved
    );

    bool hasConflict(
        const std::string& name,
        const SemanticVersion& version,
        const std::map<std::string, SemanticVersion>& resolved
    ) const;

    std::vector<DepGraphNode> buildTreeRecursive(
        const std::string& name,
        const SemanticVersion& version,
        int depth,
        std::set<std::string>& visited
    );

    // Registry: name -> version -> metadata
    std::map<std::string, std::map<SemanticVersion, PackageMetadata>> m_registry;
};

} // namespace Flux

#endif // FLUX_DEPENDENCY_RESOLVER_H
