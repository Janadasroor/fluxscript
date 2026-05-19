// ============================================================================
// Type System Tests — verifies bool, int, float type annotations and literals
// ============================================================================

#include "flux/jit/flux_jit.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/runtime/flux_runtime.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

static int g_passed = 0, g_failed = 0;
#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)

using namespace Flux;

static FluxJIT* compile_script(const std::string& source, void** fn_out,
                                const std::string& func_name = "main") {
    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = "type_test";
    opts.moduleName = "type_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(source, &error);
    if (!artifacts) {
        std::cerr << "  Compile error: " << error << "\n";
        *fn_out = nullptr;
        return nullptr;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );

    registerRuntimeFunctions(*jit);

    *fn_out = jit->getPointerToFunction(func_name);
    return jit.release();
}

// ----------------------------------------------------------------
// Test 1: bool literal true
// ----------------------------------------------------------------
void test_bool_true() {
    TEST("bool literal true returns 1.0");
    std::string source = "def main() { true }";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 1.0, "true should return 1.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 2: bool literal false
// ----------------------------------------------------------------
void test_bool_false() {
    TEST("bool literal false returns 0.0");
    std::string source = "def main() { false }";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 0.0, "false should return 0.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 3: comparison returns bool-compatible value
// ----------------------------------------------------------------
void test_comparison_result() {
    TEST("comparison returns 0.0 or 1.0");
    std::string source = R"(
        def main() {
            var x = 5.0
            var y = 3.0
            x > y
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 1.0, "5 > 3 should be 1.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

int main() {
    std::cout << "=== Type System Tests ===\n";

    test_bool_true();
    test_bool_false();
    test_comparison_result();

    std::cout << "\nResults: " << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
