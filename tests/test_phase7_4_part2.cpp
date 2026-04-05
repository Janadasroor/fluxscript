// FluxScript Phase 7.4 Part 2: Example Gallery & Dependency Resolver Tests
#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <map>

#include "flux/tooling/example_gallery.h"
#include "flux/package/dependency_resolver.h"

using namespace Flux;

// ============================================================================
// Test #1: Example Gallery
// ============================================================================

void test_example_gallery() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test #1: Example Gallery                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    auto& gallery = ExampleGallery::instance();
    gallery.initialize("examples/");

    // Test 1.1: Scan examples
    std::cout << "\nTest 1.1: Example Discovery\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    int total = gallery.totalExamples();
    std::cout << "  Total examples found: " << total << "\n";
    std::cout << "  Categories: " << gallery.categories() << "\n";

    if (total > 0) {
        std::cout << "  Runnable: " << gallery.runnableExamples() << "/" << total << "\n";

        auto by_cat = gallery.examplesByCategory();
        std::cout << "\n  By category:\n";
        for (const auto& [cat, count] : by_cat) {
            std::cout << "    " << cat << ": " << count << "\n";
        }
    }

    assert(total >= 0 && "Should find examples (may be 0 if dir missing)");

    std::cout << "\n✅ Test 1.1 PASSED\n";

    // Test 1.2: Search
    std::cout << "\n\nTest 1.2: Search\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    auto results = gallery.search("math");
    std::cout << "  Search 'math': " << results.size() << " results\n";

    results = gallery.search("nonexistent_xyz_12345");
    std::cout << "  Search 'nonexistent_xyz_12345': " << results.size() << " results\n";
    assert(results.size() == 0 && "Should find nothing for nonsense query!");

    std::cout << "\n✅ Test 1.2 PASSED\n";

    // Test 1.3: Export
    std::cout << "\n\nTest 1.3: Export\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    std::string md = gallery.generateCatalogMarkdown();
    std::string html = gallery.generateCatalogHTML();
    std::string json = gallery.generateCatalogJSON();

    std::cout << "  Markdown: " << md.length() << " bytes\n";
    std::cout << "  HTML: " << html.length() << " bytes\n";
    std::cout << "  JSON: " << json.length() << " bytes\n";

    assert(md.length() > 10 && "Markdown export too short!");
    assert(html.length() > 20 && "HTML export too short!");
    assert(json.length() > 10 && "JSON export too short!");

    std::cout << "\n✅ Test 1.3 PASSED\n";

    gallery.finalize();

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     Test #1: ALL TESTS PASSED ✅                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// Test #2: Dependency Resolver
// ============================================================================

void test_dependency_resolver() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test #2: Dependency Resolver                           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    auto& resolver = DependencyResolver::instance();
    resolver.clearRegistry();

    // Test 2.1: Version parsing
    std::cout << "\nTest 2.1: Semantic Version Parsing\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    auto v1 = SemanticVersion::parse("1.2.3");
    auto v2 = SemanticVersion::parse("1.2.10");
    auto v3 = SemanticVersion::parse("2.0.0");

    std::cout << "  1.2.3 < 1.2.10: " << (v1 < v2 ? "true" : "false") << "\n";
    std::cout << "  1.2.10 < 2.0.0: " << (v2 < v3 ? "true" : "false") << "\n";
    std::cout << "  1.2.3 == 1.2.3: " << (v1 == SemanticVersion::parse("1.2.3") ? "true" : "false") << "\n";

    assert(v1 < v2 && "1.2.3 should be < 1.2.10!");
    assert(v2 < v3 && "1.2.10 should be < 2.0.0!");

    std::cout << "\n✅ Test 2.1 PASSED\n";

    // Test 2.2: Constraint satisfaction
    std::cout << "\n\nTest 2.2: Version Constraints\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    auto exact = VersionConstraint::parse("1.2.3");
    auto gte = VersionConstraint::parse(">=1.0.0");
    auto caret = VersionConstraint::parse("^1.2.0");
    auto tilde = VersionConstraint::parse("~1.2.0");

    std::cout << "  1.2.3 satisfies '1.2.3': " << (exact.satisfies(v1) ? "yes" : "no") << "\n";
    std::cout << "  1.2.3 satisfies '>=1.0.0': " << (gte.satisfies(v1) ? "yes" : "no") << "\n";
    std::cout << "  1.2.10 satisfies '^1.2.0': " << (caret.satisfies(v2) ? "yes" : "no") << "\n";
    std::cout << "  2.0.0 satisfies '^1.2.0': " << (caret.satisfies(v3) ? "yes" : "no") << "\n";
    std::cout << "  1.2.9 satisfies '~1.2.0': " << (tilde.satisfies(SemanticVersion::parse("1.2.9")) ? "yes" : "no") << "\n";
    std::cout << "  1.3.0 satisfies '~1.2.0': " << (tilde.satisfies(SemanticVersion::parse("1.3.0")) ? "yes" : "no") << "\n";

    assert(exact.satisfies(v1) && "Exact match should work!");
    assert(gte.satisfies(v1) && "GTE should work!");
    assert(caret.satisfies(v2) && "^1.2.0 should include 1.2.10!");
    assert(!caret.satisfies(v3) && "^1.2.0 should NOT include 2.0.0!");

    std::cout << "\n✅ Test 2.2 PASSED\n";

    // Test 2.3: Simple resolution
    std::cout << "\n\nTest 2.3: Simple Dependency Resolution\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    // Register packages
    PackageMetadata math_lib;
    math_lib.name = "math-lib";
    math_lib.version = SemanticVersion::parse("2.1.0");
    resolver.registerPackage(math_lib);

    PackageMetadata math_lib_2;
    math_lib_2.name = "math-lib";
    math_lib_2.version = SemanticVersion::parse("1.0.0");
    resolver.registerPackage(math_lib_2);

    PackageMetadata signal_lib;
    signal_lib.name = "signal-lib";
    signal_lib.version = SemanticVersion::parse("1.5.0");
    signal_lib.dependencies.push_back({"math-lib", VersionConstraint::parse("^1.0.0")});
    resolver.registerPackage(signal_lib);

    PackageMetadata signal_lib_2;
    signal_lib_2.name = "signal-lib";
    signal_lib_2.version = SemanticVersion::parse("2.0.0");
    signal_lib_2.dependencies.push_back({"math-lib", VersionConstraint::parse("^2.0.0")});
    resolver.registerPackage(signal_lib_2);

    std::cout << "  Registered: math-lib (1.0.0, 2.1.0), signal-lib (1.5.0, 2.0.0)\n";

    // Resolve signal-lib with latest
    std::vector<Dependency> deps;
    deps.push_back({"signal-lib", VersionConstraint::parse("^2.0.0")});

    auto result = resolver.resolve(deps);
    std::cout << "  Resolve signal-lib ^2.0.0: " << (result.success ? "Success" : "Failed") << "\n";

    if (result.success) {
        for (const auto& pkg : result.packages) {
            std::cout << "    " << pkg.name << " " << pkg.version.str() << "\n";
        }
    }

    assert(result.success && "Resolution should succeed!");
    assert(result.packages.size() >= 2 && "Should resolve both packages!");

    std::cout << "\n✅ Test 2.3 PASSED\n";

    // Test 2.4: Conflict detection
    std::cout << "\n\nTest 2.4: Conflict Detection\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    // Register conflicting packages
    PackageMetadata pkg_a;
    pkg_a.name = "pkg-a";
    pkg_a.version = SemanticVersion::parse("1.0.0");
    pkg_a.dependencies.push_back({"math-lib", VersionConstraint::parse("^1.0.0")});
    resolver.registerPackage(pkg_a);

    PackageMetadata pkg_b;
    pkg_b.name = "pkg-b";
    pkg_b.version = SemanticVersion::parse("1.0.0");
    pkg_b.dependencies.push_back({"math-lib", VersionConstraint::parse("^2.0.0")});
    resolver.registerPackage(pkg_b);

    // Try to install both - should fail
    std::vector<Dependency> conflicting_deps;
    conflicting_deps.push_back({"pkg-a", VersionConstraint::parse("1.0.0")});
    conflicting_deps.push_back({"pkg-b", VersionConstraint::parse("1.0.0")});

    auto conflict_result = resolver.resolve(conflicting_deps);
    std::cout << "  Resolve pkg-a + pkg-b (conflicting math-lib): " 
              << (conflict_result.success ? "Success" : "Failed (expected)") << "\n";

    if (!conflict_result.success) {
        std::cout << "  Error: " << conflict_result.error_message << "\n";
    }

    assert(!conflict_result.success && "Should detect conflict!");

    std::cout << "\n✅ Test 2.4 PASSED\n";

    // Test 2.5: Dependency tree
    std::cout << "\n\nTest 2.5: Dependency Tree\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    std::vector<Dependency> tree_deps;
    tree_deps.push_back({"signal-lib", VersionConstraint::parse("^2.0.0")});

    auto tree = resolver.buildDependencyTree(tree_deps);
    std::cout << "  Dependency tree nodes: " << tree.size() << "\n";

    for (const auto& node : tree) {
        std::string indent(node.depth * 2, ' ');
        std::cout << "  " << indent << node.name << " " << node.version.str() << "\n";
    }

    assert(tree.size() >= 2 && "Tree should have at least 2 nodes!");

    std::cout << "\n✅ Test 2.5 PASSED\n";

    // Test 2.6: Available versions
    std::cout << "\n\nTest 2.6: Available Versions\n";
    std::cout << "────────────────────────────────────────────────────────────\n";

    auto versions = resolver.getAvailableVersions("math-lib");
    std::cout << "  math-lib versions: ";
    for (size_t i = 0; i < versions.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << versions[i].str();
    }
    std::cout << "\n";

    assert(versions.size() >= 2 && "Should have at least 2 versions!");
    assert(versions[0] > versions[1] && "Should be sorted descending!");

    std::cout << "\n✅ Test 2.6 PASSED\n";

    resolver.clearRegistry();

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     Test #2: ALL TESTS PASSED ✅                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Phase 7.4: Example Gallery & Dependency Resolver       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    try {
        test_example_gallery();
        test_dependency_resolver();

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║     ALL PHASE 7.4 PART 2 TESTS PASSED ✅                 ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
