// ============================================================================
// Tiered JIT Test — verifies promoteFunction and profiling infrastructure
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
#include <cstdint>

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
    opts.optimizationLevel = OptimizationLevel::O2;
    opts.inputName = "tiered_test";
    opts.moduleName = "tiered_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(source, &error);
    if (!artifacts) {
        std::cerr << "  Compile error: " << error << "\n";
        *fn_out = nullptr;
        return nullptr;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O2);
    jit->setAutoPromoteThreshold(5);

    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );

    registerRuntimeFunctions(*jit);

    *fn_out = jit->getPointerToFunction(func_name);
    return jit.release();
}

// ----------------------------------------------------------------
// Test 1: Call counting via getPointerToFunction
// ----------------------------------------------------------------
void test_call_counting() {
    TEST("getPointerToFunction increments call count");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 100.0 do {
                s = s + i
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    // compile_script calls getPointerToFunction once => count = 1
    {
        int cc = jit->getCallCount("main");
        TC(cc == 1, "call count should start at 1 (compile_script lookup), got " + std::to_string(cc));
    }

    for (int i = 0; i < 10; i++)
        jit->getPointerToFunction("main");

    {
        int cc = jit->getCallCount("main");
        TC(cc == 11, "call count should be 11 (1 + 10), got " + std::to_string(cc));
    }

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 2: promoteFunction returns non-null
// ----------------------------------------------------------------
void test_promote_function() {
    TEST("promoteFunction returns non-null pointer");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 100.0 do {
                s = s + i * i
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    auto* promoted = jit->promoteFunction("main", OptimizationLevel::O3);
    TC(promoted != nullptr, "promoteFunction returned null");
    TC(promoted != fn, "promoted pointer should differ from original (rename avoided first-definition-wins)");

    TC(jit->isPromoted("main"), "isPromoted should return true");
    TC(promoted == jit->getPointerToFunction("main"),
       "promoted pointer should match getPointerToFunction");

    // Calling promoted function gives correct result
    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(promoted);
    double r = f();
    // Sum of 0^2 + 1^2 + ... + 99^2 = 99*100*199/6 = 328350
    TC(std::abs(r - 328350.0) < 1.0, "wrong result: " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 3: promoteFunction is idempotent
// ----------------------------------------------------------------
void test_promote_idempotent() {
    TEST("promoteFunction is idempotent");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 50.0 do {
                s = s + i
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    auto* p1 = jit->promoteFunction("main", OptimizationLevel::O3);
    TC(p1 != nullptr, "first promote returned null");

    auto* p2 = jit->promoteFunction("main", OptimizationLevel::O3);
    TC(p2 != nullptr, "second promote returned null");
    TC(p1 == p2, "second promote should return same pointer");

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 4: setAutoPromoteThreshold
// ----------------------------------------------------------------
void test_threshold() {
    TEST("setAutoPromoteThreshold works");

    std::string source = R"(
        def main() { 42.0 }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    TC(jit->getAutoPromoteThreshold() == 5, "default threshold should be 5");

    jit->setAutoPromoteThreshold(10);
    TC(jit->getAutoPromoteThreshold() == 10, "threshold should be 10 after set");

    jit->setAutoPromoteThreshold(0);
    TC(jit->getAutoPromoteThreshold() == 0, "threshold should be 0 after set");

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 5: resetCallCounts
// ----------------------------------------------------------------
void test_reset_counts() {
    TEST("resetCallCounts works");

    std::string source = R"(
        def main() { 1.0 }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    for (int i = 0; i < 10; i++)
        jit->getPointerToFunction("main");

    // compile_script calls getPointerToFunction once
    TC(jit->getCallCount("main") == 11, "call count should be 11 (1 + 10), got " + std::to_string(jit->getCallCount("main")));

    jit->resetCallCounts();
    TC(jit->getCallCount("main") == 0, "call count should be 0 after reset");

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 6: auto-promotion via getPointerToFunction when threshold hit
// ----------------------------------------------------------------
void test_auto_promote() {
    TEST("auto-promotion via getPointerToFunction");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 100.0 do {
                s = s + i * i
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");
    // compile_script calls getPointerToFunction once => count = 1
    TC(!jit->isPromoted("main"), "should not be promoted after first call");

    // Need 2 more calls to reach threshold of 5 (from compile_script)
    for (int i = 0; i < 4; i++)
        jit->getPointerToFunction("main");
    // count = 5 => auto-promote should have fired
    TC(jit->isPromoted("main"), "should be auto-promoted after threshold");

    auto* promoted = jit->getPointerToFunction("main");
    TC(promoted != nullptr, "promoted pointer shouldn't be null");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(promoted);
    double r = f();
    TC(std::abs(r - 328350.0) < 1.0, "wrong result: " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 7: benchmark O0 vs O3 speedup
// ----------------------------------------------------------------
void test_benchmark_speedup() {
    TEST("O0 vs O3 speedup");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 50000000.0 do {
                s = s + i * 0.5
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto o0 = reinterpret_cast<Fn>(fn);

    // Warmup + baseline O0 timing
    volatile double sink;
    int iters = 5;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++)
        sink = o0();
    auto t1 = std::chrono::steady_clock::now();
    auto o0_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / iters;

    // Promote to O3
    auto* promoted = jit->promoteFunction("main", OptimizationLevel::O3);
    TC(promoted != nullptr, "promoteFunction returned null");
    TC(promoted != fn, "promoted pointer should differ from original");
    auto o3 = reinterpret_cast<Fn>(promoted);

    // O3 timing
    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++)
        sink = o3();
    auto t3 = std::chrono::steady_clock::now();
    auto o3_us = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() / iters;

    double speedup = double(o0_us) / double(o3_us);
    std::cout << "O0=" << o0_us << "us O3=" << o3_us << "us speedup=" << speedup << "x\n";
    TC(speedup > 1.0, "O3 should be faster than O0 (got " + std::to_string(speedup) + "x)");

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 8: redirect via JMP at original address (structural verification)
// ----------------------------------------------------------------
void test_redirect_jmp_bytes() {
    TEST("original address contains JMP rel32 to promoted address");

    std::string source = R"(
        def main() {
            var s = 0.0
            var i = 0.0
            while i < 100.0 do {
                s = s + i * 0.5
                i = i + 1.0
            }
            s
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");
    TC(fn != nullptr, "original address is null");

    using Fn = double(*)();

    // Verify O0 is correct before promotion
    auto o0 = reinterpret_cast<Fn>(fn);
    double r0 = o0();
    TC(std::abs(r0 - 2475.0) < 1.0, "O0 result wrong before promotion: " + std::to_string(r0));

    auto* promoted = jit->promoteFunction("main", OptimizationLevel::O3);
    TC(promoted != nullptr, "promoteFunction returned null");
    TC(promoted != fn, "promoted address should differ from original");

    // Structural check: first 5 bytes of original address contain JMP rel32
    auto* code = reinterpret_cast<const uint8_t*>(fn);
    TC(code[0] == 0xE9, "first byte should be JMP rel32 (0xE9)");
    int32_t encodedOffset =
        static_cast<int32_t>(code[1]) |
        (static_cast<int32_t>(code[2]) << 8) |
        (static_cast<int32_t>(code[3]) << 16) |
        (static_cast<int32_t>(code[4]) << 24);
    auto* expectedDest = reinterpret_cast<void*>(
        reinterpret_cast<intptr_t>(fn) + 5 + encodedOffset);
    TC(expectedDest == promoted, "JMP target should match promoted address");

    // Verify promoted result directly
    auto o3 = reinterpret_cast<Fn>(promoted);
    double r3 = o3();
    TC(std::abs(r3 - 2475.0) < 1.0, "promoted result wrong: " + std::to_string(r3));

    // Verify redirected result (calling original address → JMP → O3 code)
    double r_redir = o0();
    TC(std::abs(r_redir - 2475.0) < 1.0, "redirected result wrong: " + std::to_string(r_redir));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 9: JIT-to-JIT calls work after callee promotion (redirect via JMP)
// ----------------------------------------------------------------
void test_jit_to_jit_redirect() {
    TEST("JIT-to-JIT call still works after callee promotion");

    // Function 'compute' does heavy work; 'main' calls 'compute'.
    // After compute is promoted, main still has the old O0 address embedded
    // in its CALL instruction. The JMP redirect ensures it reaches the O3 code.
    std::string source = R"(
        def compute(n) {
            var s = 0.0
            var i = 0.0
            while i < n do {
                s = s + i * i
                i = i + 1.0
            }
            s
        }
        def main() {
            compute(100.0)
        }
    )";

    void* mainFn = nullptr;
    auto* jit = compile_script(source, &mainFn, "main");
    TC(jit != nullptr && mainFn != nullptr, "compile failed");

    // Promote compute (main still has its old O0 address)
    auto* promotedCompute = jit->promoteFunction("compute", OptimizationLevel::O3);
    TC(promotedCompute != nullptr, "promoteFunction returned null");
    TC(jit->isPromoted("compute"), "compute should be promoted");

    // Call main — it internally calls compute at the original O0 address,
    // which now has a JMP redirect to the promoted O3 code.
    using Fn = double(*)();
    auto main = reinterpret_cast<Fn>(mainFn);
    double r = main();
    // Sum of 0^2 + 1^2 + ... + 99^2 = 328350
    TC(std::abs(r - 328350.0) < 1.0, "wrong result via JIT-to-JIT redirect: " + std::to_string(r));

    delete jit;
    TPASS;
}

int main() {
    std::cout << "=== Tiered JIT Tests ===\n";

    test_call_counting();
    test_promote_function();
    test_promote_idempotent();
    test_threshold();
    test_reset_counts();
    test_auto_promote();
    test_benchmark_speedup();
    test_redirect_jmp_bytes();
    test_jit_to_jit_redirect();

    std::cout << "\nResults: " << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
