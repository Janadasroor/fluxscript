// ============================================================================
// Persistent JIT Cache Test — verifies bitcode disk cache hit/miss/invalidation
// ============================================================================

#include "flux/jit/flux_jit.h"
#include "flux/jit_engine.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/tooling/tooling.h"
#include "flux/runtime/flux_runtime.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>

static int g_passed = 0, g_failed = 0;
#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)

using namespace Flux;

namespace fs = std::filesystem;

static std::string temp_cache_dir() {
    static std::mt19937 rng(std::random_device{}());
    auto base = fs::temp_directory_path();
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::string name = "flux_cache_test_" + std::to_string(rng());
        std::string d = (base / name).string();
        if (fs::create_directory(d)) return d;
    }
    return (base / "flux_cache_test_fallback").string();
}

// ----------------------------------------------------------------
// Test 1: Cache miss on first compile, hit on second
// ----------------------------------------------------------------
void test_cache_hit_after_miss() {
    TEST("First compile is a miss, second is a hit");

    const std::string code = "def main() { 2.0 + 3.0 }";
    std::string cacheDir = temp_cache_dir();
    std::string error;

    auto& engine = JITEngine::instance();
    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);

    // First compile — should be a miss
    TC(engine.compileScript(code, &error), "First compile failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected cache miss on first compile");
    engine.finalize();

    // Second compile — should be a hit
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "Second compile failed: " + error);
    TC(engine.lastCompileUsedCache(), "Expected cache hit on second compile");

    // Verify the function still works
    auto val = engine.callFunction("main", {}, &error);
    TC(error.empty(), "callFunction error: " + error);
    TC(std::holds_alternative<double>(val), "Expected double result");
    TC(std::abs(std::get<double>(val) - 5.0) < 1e-9, "Expected 5.0");

    engine.finalize();
    fs::remove_all(cacheDir);
    TPASS;
}

// ----------------------------------------------------------------
// Test 2: Cache invalidated when source changes
// ----------------------------------------------------------------
void test_cache_invalidated_on_source_change() {
    TEST("Cache invalidated when source code changes");

    std::string cacheDir = temp_cache_dir();
    std::string error;
    auto& engine = JITEngine::instance();

    const std::string code1 = "def main() { 1.0 + 2.0 }";
    const std::string code2 = "def main() { 10.0 + 20.0 }";

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code1, &error), "First compile failed: " + error);

    // Same code — should be a hit
    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code1, &error), "First repeat compile failed: " + error);
    TC(engine.lastCompileUsedCache(), "Expected cache hit for same source");

    // Different code — should be a miss
    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code2, &error), "Second compile failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected cache miss for changed source");

    auto val = engine.callFunction("main", {}, &error);
    TC(error.empty(), "callFunction error: " + error);
    TC(std::holds_alternative<double>(val), "Expected double result");
    TC(std::abs(std::get<double>(val) - 30.0) < 1e-9, "Expected 30.0, got " + std::to_string(std::get<double>(val)));

    engine.finalize();
    fs::remove_all(cacheDir);
    TPASS;
}

// ----------------------------------------------------------------
// Test 3: Cache disabled — always a miss
// ----------------------------------------------------------------
void test_cache_disabled() {
    TEST("Cache disabled always reports miss");

    const std::string code = "def main() { 42.0 }";
    std::string cacheDir = temp_cache_dir();
    std::string error;
    auto& engine = JITEngine::instance();

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(false);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);

    TC(engine.compileScript(code, &error), "First compile failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected miss (disabled)");

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(false);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "Second compile failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected miss (disabled)");

    engine.finalize();
    fs::remove_all(cacheDir);
    TPASS;
}

// ----------------------------------------------------------------
// Test 4: Cache key is stable across runs (same source → same key)
// ----------------------------------------------------------------
void test_cache_key_stable() {
    TEST("Cache key is deterministically stable");

    CompilerOptions opts;
    opts.moduleName = "test";
    opts.inputName = "test.flux";

    const std::string code = "def main() { 1.0 }";

    std::string key1 = Tooling::computeCacheKey(code, opts);
    std::string key2 = Tooling::computeCacheKey(code, opts);

    TC(key1 == key2, "Same source should produce same key: " + key1 + " vs " + key2);

    // Different source should produce different key
    std::string key3 = Tooling::computeCacheKey("def main() { 2.0 }", opts);
    TC(key1 != key3, "Different source should produce different key");

    // Same source with different name should produce different key
    CompilerOptions opts2 = opts;
    opts2.moduleName = "other";
    std::string key4 = Tooling::computeCacheKey(code, opts2);
    TC(key1 != key4, "Different module name should produce different key");

    TPASS;
}

// ----------------------------------------------------------------
// Test 5: clearJITCache removes all cache files
// ----------------------------------------------------------------
void test_clear_cache() {
    TEST("clearJITCache removes all cached files");

    const std::string code = "def main() { 7.0 }";
    std::string cacheDir = temp_cache_dir();
    std::string error;
    auto& engine = JITEngine::instance();

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "Compile failed: " + error);

    // Verify cache file exists
    int count = 0;
    for (auto& entry : fs::directory_iterator(cacheDir))
        ++count;
    TC(count > 0, "Expected cache files, found " + std::to_string(count));

    // Clear cache
    engine.clearJITCache();

    // Verify empty
    count = 0;
    for (auto& entry : fs::directory_iterator(cacheDir))
        ++count;
    TC(count == 0, "Expected no cache files after clear, found " + std::to_string(count));

    // Next compile should be a miss
    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "Recompile after clear failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected miss after cache clear");

    engine.finalize();
    fs::remove_all(cacheDir);
    TPASS;
}

// ----------------------------------------------------------------
// Test 6: Cache with top-level expressions
// ----------------------------------------------------------------
void test_cache_anon_expr() {
    TEST("Cached compilation with anonymous expressions");

    const std::string code = "3.14 * 2.0";
    std::string cacheDir = temp_cache_dir();
    std::string error;
    auto& engine = JITEngine::instance();

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "First compile failed: " + error);
    TC(!engine.lastCompileUsedCache(), "Expected miss on first compile");

    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(true);
    engine.setJITCacheDirectory(cacheDir);
    engine.setOptimizationLevel(OptimizationLevel::O2);
    TC(engine.compileScript(code, &error), "Second compile failed: " + error);
    TC(engine.lastCompileUsedCache(), "Expected hit on second compile");

    engine.finalize();
    fs::remove_all(cacheDir);
    TPASS;
}

// ----------------------------------------------------------------
int main() {
    std::cout << "Persistent JIT Cache Tests:\n";

    test_cache_key_stable();
    test_cache_hit_after_miss();
    test_cache_invalidated_on_source_change();
    test_cache_disabled();
    test_clear_cache();
    test_cache_anon_expr();

    std::cout << "\nResults: " << g_passed << " passed, "
              << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
