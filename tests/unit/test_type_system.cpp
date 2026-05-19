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

// ----------------------------------------------------------------
// Test 4: if with comparison condition
// ----------------------------------------------------------------
void test_if_bool_condition() {
    TEST("if with comparison condition");
    std::string source = R"(
        def main() {
            if 5.0 > 3.0 then 42.0 else 0.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 42.0, "5 > 3 should return 42.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 5: bool AND/OR
// ----------------------------------------------------------------
void test_bool_logic() {
    TEST("bool && and ||");
    std::string source = R"(
        def main() {
            var a = 5.0 > 3.0
            var b = 2.0 > 1.0
            if a && b then 100.0 else 0.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 100.0, "true && true should give 100.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 6: int variable with type annotation
// ----------------------------------------------------------------
void test_int_variable() {
    TEST("let x: int = 5 returns correct value");
    std::string source = R"(
        def main() {
            let x: int = 5
            x
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 5.0, "int 5 should return 5.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 7: bool variable with type annotation
// ----------------------------------------------------------------
void test_bool_variable() {
    TEST("let x: bool = true returns 1.0");
    std::string source = R"(
        def main() {
            let x: bool = true
            if x then 42.0 else 0.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 42.0, "bool true should give 42.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 8: int variable promoted to double in arithmetic
// ----------------------------------------------------------------
void test_int_in_math() {
    TEST("int variable promotes to double in arithmetic");
    std::string source = R"(
        def main() {
            let x: int = 5
            x + 3.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 8.0, "5 + 3 should be 8.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 9: bool variable promotes to double in arithmetic
// ----------------------------------------------------------------
void test_bool_in_math() {
    TEST("bool variable promotes to double in arithmetic");
    std::string source = R"(
        def main() {
            let b: bool = true
            b + 2.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 3.0, "true + 2 should be 3.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 10: !(comparison) produces correct bool
// ----------------------------------------------------------------
void test_not_comparison() {
    TEST("not of comparison works");
    std::string source = R"(
        def main() {
            if !(5.0 > 3.0) then 100.0 else 200.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 200.0, "!(5>3) should be false → 200.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 11: while loop with comparison condition
// ----------------------------------------------------------------
void test_while_bool() {
    TEST("while with comparison condition");
    std::string source = R"(
        def main() {
            var i = 0.0
            while i < 5.0 do {
                i = i + 1.0
            }
            i
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 5.0, "loop should count to 5.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 12: chained boolean logic
// ----------------------------------------------------------------
void test_chained_logic() {
    TEST("chained && with ||");
    std::string source = R"(
        def main() {
            var a = 5.0 > 3.0
            var b = 2.0 < 4.0
            var c = 1.0 > 10.0
            if a && b || c then 1.0 else 0.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 1.0, "true && true || false should be true → 1.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 13: bool to int conversion
// ----------------------------------------------------------------
void test_bool_to_int() {
    TEST("bool converts to int via ZExt");
    std::string source = R"(
        def main() {
            let x: int = true
            let y: int = false
            x + y
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(r == 1.0, "int(true) + int(false) should be 1.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 14: integer literal (bare digits → i32)
// ----------------------------------------------------------------
void test_int_literal() {
    TEST("integer literal returns correct i32 value");
    std::string source = R"(
        def main() {
            42
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    // Declared return type is double, so 42 is SIToFP-promoted to 42.0
    TC(r == 42.0, "42 should be 42.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 15: integer literal in arithmetic with doubles
// ----------------------------------------------------------------
void test_int_literal_math() {
    TEST("int literal promotes to double in mixed arithmetic");
    std::string source = R"(
        def main() {
            5 + 3.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(std::abs(r - 8.0) < 0.001, "5 + 3.0 should be 8.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 16: integer literal with int variable
// ----------------------------------------------------------------
void test_int_literal_var() {
    TEST("int literal assigned to int variable");
    std::string source = R"(
        def main() {
            let x: int = 10
            x + 5
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(std::abs(r - 15.0) < 0.001, "10 + 5 should be 15.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 17: float literal still works (has decimal point)
// ----------------------------------------------------------------
void test_float_literal() {
    TEST("float literal (with decimal point) still works");
    std::string source = R"(
        def main() {
            3.14
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(std::abs(r - 3.14) < 0.001, "3.14 should be 3.14, got " + std::to_string(r));

    delete jit;
    TPASS;
}

// ----------------------------------------------------------------
// Test 18: negative integer literal
// ----------------------------------------------------------------
void test_neg_int_literal() {
    TEST("negative integer literal works");
    std::string source = R"(
        def main() {
            -7 + 3.0
        }
    )";

    void* fn = nullptr;
    auto* jit = compile_script(source, &fn, "main");
    TC(jit != nullptr && fn != nullptr, "compile failed");

    using Fn = double(*)();
    auto f = reinterpret_cast<Fn>(fn);
    double r = f();
    TC(std::abs(r - (-4.0)) < 0.001, "-7 + 3.0 should be -4.0, got " + std::to_string(r));

    delete jit;
    TPASS;
}

int main() {
    std::cout << "=== Type System Tests ===\n";

    test_bool_true();
    test_bool_false();
    test_comparison_result();
    test_if_bool_condition();
    test_bool_logic();
    test_int_variable();
    test_bool_variable();
    test_int_in_math();
    test_bool_in_math();
    test_not_comparison();
    test_while_bool();
    test_chained_logic();
    test_bool_to_int();
    test_int_literal();
    test_int_literal_math();
    test_int_literal_var();
    test_float_literal();
    test_neg_int_literal();

    std::cout << "\nResults: " << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
