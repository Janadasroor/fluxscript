// Module System (import) Tests
// Tests: basic import, alias import, selective import, multiple imports,
//        namespace resolution, calling imported functions in JIT

#include "flux/jit/flux_jit.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/runtime/flux_runtime.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) std::cout << "  " << name << "... "
#define PASS() do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define FAIL(msg) do { std::cerr << "FAILED: " << msg << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

using namespace Flux;

// ========== Legacy compilation-only tests ==========

static bool compileAndCheck(const std::string& mainCode, const std::string& moduleDir) {
    CompilerOptions opts;
    opts.inputName = (fs::path(moduleDir) / "test.flux").string();
    opts.moduleName = "test_main";
    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(mainCode, &error);
    if (!artifacts) {
        std::cerr << "  Compile error: " << error << "\n";
        return false;
    }
    return true;
}

void test_basic_import(const std::string& mod_dir) {
    TEST("Basic Import");
    std::string code = R"(
import math_utils
def test() math_utils::square(5.0)
)";
    if (compileAndCheck(code, mod_dir)) { PASS(); }
    else { FAIL("basic import compilation failed"); }
}

void test_alias_import(const std::string& mod_dir) {
    TEST("Alias Import (as)");
    std::string code = R"(
import math_utils as m
def test() m::square(3.0) + m::cube(2.0)
)";
    if (compileAndCheck(code, mod_dir)) { PASS(); }
    else { FAIL("alias import compilation failed"); }
}

void test_selective_import(const std::string& mod_dir) {
    TEST("Selective Import ({symbol})");
    std::string code = R"(
import math_utils {square}
def test() square(11.0)
)";
    if (compileAndCheck(code, mod_dir)) { PASS(); }
    else { FAIL("selective import compilation failed"); }
}

void test_multiple_imports(const std::string& mod_dir) {
    TEST("Multiple Imports");
    std::string code = R"(
import math_utils as m
import strings as s
def test() m::square(3.0) + s::greet(0.0)
)";
    if (compileAndCheck(code, mod_dir)) { PASS(); }
    else { FAIL("multiple imports compilation failed"); }
}

void test_runtime_shadowing(const std::string& mod_dir) {
    TEST("Runtime Symbol Shadowing");
    (void)mod_dir;
    std::string code = R"(
def square(x) x * x
def test() square(6.0)
)";
    if (compileAndCheck(code, mod_dir)) { PASS(); }
    else { FAIL("runtime shadowing compilation failed"); }
}

void test_import_llvm_emit(const std::string& mod_dir) {
    TEST("Import produces valid LLVM IR");
    std::string code = R"(
import math_utils
def test() math_utils::square(5.0)
)";
    CompilerOptions opts;
    opts.inputName = (fs::path(mod_dir) / "test.flux").string();
    opts.moduleName = "test_main";
    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        FAIL("LLVM emit failed: " + error);
        return;
    }

    bool hasSquare = false, hasTest = false;
    for (auto& F : *artifacts->codegenContext->TheModule) {
        if (!F.isDeclaration()) {
            if (F.getName() == "square") hasSquare = true;
            if (F.getName() == "test") hasTest = true;
        }
    }
    TC(hasSquare, "square function not found in module");
    TC(hasTest, "test function not found in module");
    PASS();
}

// ========== New end-to-end JIT tests ==========

// Create a temp dir with module files symlinked from tests/modules/
static std::string setup_module_dir(const std::string& modules_root) {
    char tmpl[] = "/tmp/flux_modtest_XXXXXX";
    char* dir = mkdtemp(tmpl);
    if (!dir) return "";
    std::string d(dir);
    for (auto& entry : fs::directory_iterator(modules_root)) {
        fs::create_symlink(entry.path(), d / entry.path().filename());
    }
    return d;
}

// Symlink a file into the temp module directory.
static bool link_module(const std::string& src, const std::string& dst) {
    if (!fs::exists(src)) return false;
    fs::create_symlink(src, dst);
    return true;
}

// Compile source (which may import modules) and return true on success.
// Does NOT print FAIL on error — use for negative testing.
static bool compile_with_modules_result(const std::string& source,
                                        const std::string& module_dir,
                                        std::string& error_out) {
    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = (fs::path(module_dir) / "test.flux").string();
    opts.moduleName = "test_mod";

    CompilerInstance compiler(opts);
    auto artifacts = compiler.compileToIR(source, &error_out);
    return artifacts != nullptr;
}

// Compile source (which may import modules) and return the FluxJIT.
// Module files are resolved by setting inputName to a file in module_dir,
// so resolveImportPath looks in the same directory.
static FluxJIT* compile_with_modules(const std::string& source,
                                     const std::string& module_dir,
                                     const std::string& func_name = "main") {
    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = (fs::path(module_dir) / "test.flux").string();
    opts.moduleName = "test_mod";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(source, &error);
    if (!artifacts) {
        FAIL("Compile error: " + error);
        return nullptr;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );

    registerRuntimeFunctions(*jit);

    void* fn = jit->getPointerToFunction(func_name);
    if (!fn) {
        FAIL("getPointerToFunction failed for " + func_name);
        return nullptr;
    }

    return jit.release();
}

static double call_double(FluxJIT* jit, const std::string& name, double arg) {
    using Fn = double(*)(double);
    auto* fn = reinterpret_cast<Fn>(jit->getPointerToFunction(name));
    return fn ? fn(arg) : -999.0;
}

void test_basic_import_jit() {
    TEST("Basic import + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils
def main() math_utils::square(5.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 25.0) < 0.001,
       "expected 25.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_alias_import_jit() {
    TEST("Alias import + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils as m
def main() m::cube(3.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 27.0) < 0.001,
       "expected 27.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_multiple_imports_jit() {
    TEST("Multiple imports + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils as m
import greeter as g
def main() m::square(4.0) + g::greet(3.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // square(4.0)=16.0 + greet(3.0)=6.0 = 22.0
    TC(std::abs(result - 22.0) < 0.001,
       "expected 22.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_imported_function_called_directly() {
    TEST("Call imported function directly in JIT");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils
def main() math_utils::half(11.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 5.5) < 0.001,
       "expected 5.5, got " + std::to_string(result));

    double half_result = call_double(jit, "half", 10.0);
    TC(std::abs(half_result - 5.0) < 0.001,
       "direct call to half() expected 5.0, got " + std::to_string(half_result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_import_shadows_runtime() {
    TEST("User function shadows imported function");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    // In the flat LLVM namespace, the user definition replaces the imported one
    // because FunctionAST::codegen reuses the existing function if types match.
    // The user-defined square body replaces the imported body.
    std::string code = R"(
import math_utils
def square(x) x * x  // same name, same type — reuses LLVM function
def main() square(5.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // The user's square(x*x) is in effect because FunctionAST::codegen
    // reused the existing LLVM function (same type) and generated new body.
    TC(std::abs(result - 25.0) < 0.001,
       "expected 25.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_import_chain() {
    TEST("Module that imports another module");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    // Import must be at top level, not inside a function body
    {
        std::ofstream ofs(mod_dir + "/wrapper.flux");
        ofs << "import math_utils\n"
               "def double_square(x) math_utils::square(x) * 2.0\n";
    }

    std::string code = R"(
import wrapper
def main() wrapper::double_square(3.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // double_square(3.0) = square(3.0)*2 = 9.0*2 = 18.0
    TC(std::abs(result - 18.0) < 0.001,
       "expected 18.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_import_inside_function_body() {
    TEST("Import inside function body");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
def main() {
    import math_utils
    math_utils::square(5.0)
}
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 25.0) < 0.001,
       "expected 25.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_import_inside_function_body_with_alias() {
    TEST("Import with alias inside function body");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
def main() {
    import math_utils as m
    m::cube(4.0)
}
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 64.0) < 0.001,
       "expected 64.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_selective_import_jit() {
    TEST("Selective import {square} + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils {square}
def main() square(5.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    TC(std::abs(result - 25.0) < 0.001,
       "expected 25.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_selective_import_excluded_fails() {
    TEST("Selective import: excluded function call fails");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils {square}
def main() cube(5.0)
)";

    std::string error;
    bool ok = compile_with_modules_result(code, mod_dir, error);
    TC(!ok, "expected compilation to fail for excluded function 'cube'");

    fs::remove_all(mod_dir);
    PASS();
}

void test_selective_import_namespaced_excluded_fails() {
    TEST("Selective import: namespaced excluded function call fails");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils {square}
def main() math_utils::cube(5.0)
)";

    std::string error;
    bool ok = compile_with_modules_result(code, mod_dir, error);
    TC(!ok, "expected compilation to fail for namespaced excluded function 'cube'");

    fs::remove_all(mod_dir);
    PASS();
}

void test_selective_import_non_excluded_works() {
    TEST("Selective import: non-module function still auto-declares");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    // cos is not in any module, so it should auto-declare normally
    std::string code = R"(
import math_utils {square}
def main() cos(1.0)
)";

    std::string error;
    bool ok = compile_with_modules_result(code, mod_dir, error);
    TC(ok, "expected non-module function call to compile: " + error);

    fs::remove_all(mod_dir);
    PASS();
}

void test_selective_import_multiple_symbols_jit() {
    TEST("Selective import {square, half} + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string code = R"(
import math_utils {square, half}
def main() square(4.0) + half(10.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // square(4.0)=16.0 + half(10.0)=5.0 = 21.0
    TC(std::abs(result - 21.0) < 0.001,
       "expected 21.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_stdlib_math_import_jit() {
    TEST("Import stdlib/math.flux + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    // Symlink stdlib/math.flux into the temp dir so the ModuleLoader finds it
    std::string math_src = TESTS_SOURCE_DIR "/stdlib/math.flux";
    if (fs::exists(math_src)) {
        fs::create_symlink(math_src, mod_dir + "/math.flux");
    } else {
        fs::remove_all(mod_dir);
        FAIL("stdlib/math.flux not found at " + math_src);
        return;
    }

    std::string code = R"(
import math
def main() max(3.0, 7.0) + min(10.0, 4.0) + clamp(15.0, 0.0, 5.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // max(3,7)=7 + min(10,4)=4 + clamp(15,0,5)=5 = 16.0
    TC(std::abs(result - 16.0) < 0.001,
       "expected 16.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_stdlib_math_functions_jit() {
    TEST("stdlib/math.flux: lerp, rad2deg, sign, factorial");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    std::string math_src = TESTS_SOURCE_DIR "/stdlib/math.flux";
    if (!fs::exists(math_src)) {
        fs::remove_all(mod_dir);
        FAIL("stdlib/math.flux not found");
        return;
    }
    fs::create_symlink(math_src, mod_dir + "/math.flux");

    std::string code = R"(
import math
def main() lerp(0.0, 10.0, 0.5) + sign(-5.0) + factorial(5.0) + rad2deg(3.1415926535)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // lerp(0,10,0.5)=5 + sign(-5)=-1 + factorial(5)=120 + rad2deg(pi~3.14159)≈180 = 304.0
    TC(std::abs(result - 304.0) < 1.0,
       "expected ~304.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_stdlib_trig_import_jit() {
    TEST("Import stdlib/trig.flux + JIT execution");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    if (!link_module(TESTS_SOURCE_DIR "/stdlib/trig.flux", mod_dir + "/trig.flux")) {
        fs::remove_all(mod_dir);
        FAIL("stdlib/trig.flux not found");
        return;
    }

    std::string code = R"(
import trig
def main() hypot(3.0, 4.0) + sec(0.0) + log2(8.0) + cbrt(27.0)
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // hypot(3,4)=5 + sec(0)=1 + log2(8)=3 + cbrt(27)=3 = 12.0
    TC(std::abs(result - 12.0) < 0.01,
       "expected 12.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_auto_import_jit() {
    TEST("Auto-imported stdlib without explicit import");
    // Temporarily chdir to the project root so the ModuleLoader finds
    // CWD/stdlib/math.flux during auto-import.
    auto old_cwd = fs::current_path();
    fs::current_path(TESTS_SOURCE_DIR);
    if (!fs::exists("stdlib/math.flux")) {
        fs::current_path(old_cwd);
        FAIL("stdlib/math.flux not found from project root");
        return;
    }

    std::string code = R"(
def main() max(3.0, 7.0) + min(10.0, 4.0) + clamp(15.0, 0.0, 5.0)
)";

    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = "auto_import_test.flux";
    opts.moduleName = "auto_import_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        fs::current_path(old_cwd);
        FAIL("Compile error (auto-import test): " + error);
        return;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );
    registerRuntimeFunctions(*jit);

    void* fn = jit->getPointerToFunction("main");
    if (!fn) {
        fs::current_path(old_cwd);
        FAIL("getPointerToFunction failed for main");
        return;
    }

    using Fn = double(*)(double);
    double result = reinterpret_cast<Fn>(fn)(0.0);
    // max(3,7)=7 + min(10,4)=4 + clamp(15,0,5)=5 = 16.0
    TC(std::abs(result - 16.0) < 0.001,
       "expected 16.0, got " + std::to_string(result));

    fs::current_path(old_cwd);
    PASS();
}

void test_user_function_returns_matrix() {
    TEST("User function returns matrix");
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    TC(!mod_dir.empty(), "failed to create temp module dir");

    // make_mat() calls matrix_eye(4), inference should set return type to Matrix.
    // Then main() calls make_mat() and uses matrix_rows to get dimension.
    std::string code = R"(
def make_mat() matrix_eye(4)
def main() matrix_rows(make_mat())
)";

    auto jit = compile_with_modules(code, mod_dir);
    if (!jit) { fs::remove_all(mod_dir); return; }

    double result = call_double(jit, "main", 0);
    // make_mat() returns 4x4 identity; matrix_rows(make_mat()) = 4
    TC(std::abs(result - 4.0) < 0.001,
       "expected 4.0, got " + std::to_string(result));

    delete jit;
    fs::remove_all(mod_dir);
    PASS();
}

void test_stdlib_array_auto_import_jit() {
    TEST("Auto-imported array functions (pure-Flux linspace/logspace/arange)");
    auto old_cwd = fs::current_path();
    fs::current_path(TESTS_SOURCE_DIR);

    std::string code = R"(
def main() sum(linspace(0.0, 1.0, 5)) + matrix_rows(logspace(0.0, 2.0, 5)) + sum(arange(0.0, 5.0, 2.0))
)";

    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = "auto_import_array_test.flux";
    opts.moduleName = "auto_import_array_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        fs::current_path(old_cwd);
        FAIL("Compile error (auto-import array test): " + error);
        return;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );
    registerRuntimeFunctions(*jit);

    void* fn = jit->getPointerToFunction("main");
    if (!fn) {
        fs::current_path(old_cwd);
        FAIL("getPointerToFunction failed for main");
        return;
    }

    using Fn = double(*)(double);
    double result = reinterpret_cast<Fn>(fn)(0.0);
    // linspace(0,1,5) sum = 2.5 + logspace(0,2,5) rows = 5 + arange(0,5,2) sum = 6 = 13.5
    TC(std::abs(result - 13.5) < 0.01,
       "expected 13.5, got " + std::to_string(result));

    fs::current_path(old_cwd);
    PASS();
}

void test_stdlib_array_utilities_jit() {
    TEST("Auto-imported array utilities (mean/flatten/reshape/eye/ones/zeros)");
    auto old_cwd = fs::current_path();
    fs::current_path(TESTS_SOURCE_DIR);

    std::string code = R"(
def main() {
    var a = linspace(0.0, 1.0, 5)
    var m = mean(a)
    var f = flatten(eye(3.0))
    var r = reshape(arange(0.0, 6.0, 1.0), 2.0, 3.0)
    var check = matrix_get(r, 1, 2)
    m + matrix_rows(f) + check
}
)";

    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = "auto_import_array_util_test.flux";
    opts.moduleName = "auto_import_array_util_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        fs::current_path(old_cwd);
        FAIL("Compile error (array utilities test): " + error);
        return;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );
    registerRuntimeFunctions(*jit);

    void* fn = jit->getPointerToFunction("main");
    if (!fn) {
        fs::current_path(old_cwd);
        FAIL("getPointerToFunction failed for main");
        return;
    }

    using Fn = double(*)(double);
    double result = reinterpret_cast<Fn>(fn)(0.0);
    // mean(linspace(0,1,5)) = 0.5 + flatten(eye(3)) rows = 9 + r(1,2) = 5.0 = 14.5
    TC(std::abs(result - 14.5) < 0.01,
       "expected 14.5, got " + std::to_string(result));

    fs::current_path(old_cwd);
    PASS();
}

void test_stdlib_stats_jit() {
    TEST("Auto-imported stats module (variance/std/norm/min/max)");
    auto old_cwd = fs::current_path();
    fs::current_path(TESTS_SOURCE_DIR);

    std::string code = R"(
def main() {
    var a = linspace(0.0, 1.0, 5)
    std(a) + min_element(a) + max_element(a)
}
)";

    CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = OptimizationLevel::O0;
    opts.inputName = "auto_import_stats_test.flux";
    opts.moduleName = "auto_import_stats_test";

    CompilerInstance compiler(opts);
    std::string error;
    auto artifacts = compiler.compileToIR(code, &error);
    if (!artifacts) {
        fs::current_path(old_cwd);
        FAIL("Compile error (stats test): " + error);
        return;
    }

    auto jit = std::make_unique<FluxJIT>(OptimizationLevel::O0);
    jit->addModule(
        std::move(artifacts->codegenContext->OwnedModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );
    registerRuntimeFunctions(*jit);

    void* fn = jit->getPointerToFunction("main");
    if (!fn) {
        fs::current_path(old_cwd);
        FAIL("getPointerToFunction failed for main");
        return;
    }

    using Fn = double(*)(double);
    double result = reinterpret_cast<Fn>(fn)(0.0);
    // std(linspace(0,1,5)) ≈ 0.3536 + min=0 + max=1 = 1.3536
    TC(std::abs(result - 1.3536) < 0.01,
       "expected ~1.3536, got " + std::to_string(result));

    fs::current_path(old_cwd);
    PASS();
}

int main() {
    std::string modules_root = TESTS_SOURCE_DIR "/tests/modules";
    std::string mod_dir = setup_module_dir(modules_root);
    if (mod_dir.empty()) {
        std::cerr << "FATAL: could not create temp module dir\n";
        return 1;
    }

    std::cout << "\n========================================\n";
    std::cout << "  FluxScript Module System Tests\n";
    std::cout << "  (compilation + end-to-end JIT)\n";
    std::cout << "========================================\n\n";

    // Compilation-only tests
    std::cout << "--- Compilation Tests ---\n";
    test_basic_import(mod_dir);
    test_alias_import(mod_dir);
    test_selective_import(mod_dir);
    test_multiple_imports(mod_dir);
    test_runtime_shadowing(mod_dir);
    test_import_llvm_emit(mod_dir);

    // End-to-end JIT tests
    std::cout << "\n--- End-to-End JIT Tests ---\n";
    test_basic_import_jit();
    test_alias_import_jit();
    test_multiple_imports_jit();
    test_imported_function_called_directly();
    test_import_shadows_runtime();
    test_import_chain();
    test_import_inside_function_body();
    test_import_inside_function_body_with_alias();

    // Selective import enforcement tests
    std::cout << "\n--- Selective Import Enforcement ---\n";
    test_selective_import_jit();
    test_selective_import_excluded_fails();
    test_selective_import_namespaced_excluded_fails();
    test_selective_import_non_excluded_works();
    test_selective_import_multiple_symbols_jit();

    // Standard library tests
    std::cout << "\n--- Standard Library ---\n";
    test_stdlib_math_import_jit();
    test_stdlib_math_functions_jit();
    test_stdlib_trig_import_jit();
    test_stdlib_array_auto_import_jit();
    test_stdlib_array_utilities_jit();
    test_stdlib_stats_jit();

    if (fs::exists(TESTS_SOURCE_DIR "/stdlib/math.flux")) {
        test_auto_import_jit();
    }

    // Matrix return from user functions
    std::cout << "\n--- Matrix Return from User Functions ---\n";
    test_user_function_returns_matrix();

    fs::remove_all(mod_dir);

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n";
    std::cout << "========================================\n";

    if (g_failed == 0) {
        std::cout << "   ALL MODULE SYSTEM TESTS PASSED\n\n";
        return 0;
    } else {
        std::cout << "   " << g_failed << " TESTS FAILED\n\n";
        return 1;
    }
}
