#include "flux/jit_engine.h"
#include <cmath>
#include <iostream>
#include <string>
#include <variant>

static int g_passed = 0, g_failed = 0;
#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)

using namespace Flux;

static void initEngine(JITEngine& engine) {
    engine.finalize();
    engine.initialize();
    engine.setJITCacheEnabled(false);
    engine.setOptimizationLevel(OptimizationLevel::O2);
}

void test_vector_literal() {
    TEST("Vector literal [1, 2, 3] returns double* with 3 elements");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2, 3] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<VectorResult>(val), "expected VectorResult");
    auto vec = std::get<VectorResult>(val);
    TC(vec.len == 3, "expected len 3, got " + std::to_string(vec.len));
    TC(vec.data != nullptr, "expected non-null data");
    TC(std::abs(vec.data[0] - 1.0) < 1e-9, "expected data[0]=1.0, got " + std::to_string(vec.data[0]));
    TC(std::abs(vec.data[1] - 2.0) < 1e-9, "expected data[1]=2.0, got " + std::to_string(vec.data[1]));
    TC(std::abs(vec.data[2] - 3.0) < 1e-9, "expected data[2]=3.0, got " + std::to_string(vec.data[2]));
    TPASS;
}

void test_vector_index_scalar() {
    TEST("Vector index [1, 2, 3][1] returns 2.0");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2, 3][1] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    TC(std::abs(std::get<double>(val) - 2.0) < 1e-9, "expected 2.0, got " + std::to_string(std::get<double>(val)));
    TPASS;
}

void test_vector_index_first() {
    TEST("Vector index [10, 20, 30][0] returns 10.0");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [10, 20, 30][0] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    TC(std::abs(std::get<double>(val) - 10.0) < 1e-9, "expected 10.0, got " + std::to_string(std::get<double>(val)));
    TPASS;
}

void test_vector_index_last() {
    TEST("Vector index [1, 2, 3][2] returns 3.0");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2, 3][2] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    TC(std::abs(std::get<double>(val) - 3.0) < 1e-9, "expected 3.0, got " + std::to_string(std::get<double>(val)));
    TPASS;
}

void test_vector_index_in_expr() {
    TEST("Vector index in arithmetic: [1,2,3][0] + [4,5,6][1]");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2, 3][0] + [4, 5, 6][1] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    TC(std::abs(std::get<double>(val) - 6.0) < 1e-9, "expected 6.0, got " + std::to_string(std::get<double>(val)));
    TPASS;
}

void test_vector_index_compare() {
    TEST("Vector index comparison: [1,2,3][1] == 2.0");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2, 3][1] == 2.0 }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    double result = std::get<double>(val);
    TC(std::abs(result - 1.0) < 1e-9, "expected 1.0 (true), got " + std::to_string(result));
    TPASS;
}

void test_vector_negative_values() {
    TEST("Vector with negatives: [-5, -10, -15][1]");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [-5, -10, -15][1] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    double result = std::get<double>(val);
    TC(std::abs(result - (-10.0)) < 1e-9, "expected -10.0, got " + std::to_string(result));
    TPASS;
}

void test_vector_mixed_types() {
    TEST("Vector with mixed int/double: [1, 2.5, 3][1]");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "def test() { [1, 2.5, 3][1] }";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("test", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<double>(val), "expected double");
    double result = std::get<double>(val);
    TC(std::abs(result - 2.5) < 1e-9, "expected 2.5, got " + std::to_string(result));
    TPASS;
}

void test_vector_return_from_anon() {
    TEST("Anonymous expression [4, 5, 6] returns vector");
    auto& engine = JITEngine::instance();
    initEngine(engine);
    std::string code = "[4, 5, 6]";
    std::string error;
    TC(engine.compileScript(code, &error), "compile: " + error);
    auto val = engine.callFunction("__anon_expr", {}, &error);
    TC(error.empty(), "call: " + error);
    TC(std::holds_alternative<VectorResult>(val), "expected VectorResult");
    auto vec = std::get<VectorResult>(val);
    TC(vec.len == 3, "expected len 3, got " + std::to_string(vec.len));
    TC(vec.data != nullptr, "expected non-null data");
    TC(std::abs(vec.data[0] - 4.0) < 1e-9, "expected data[0]=4.0, got " + std::to_string(vec.data[0]));
    TC(std::abs(vec.data[1] - 5.0) < 1e-9, "expected data[1]=5.0, got " + std::to_string(vec.data[1]));
    TC(std::abs(vec.data[2] - 6.0) < 1e-9, "expected data[2]=6.0, got " + std::to_string(vec.data[2]));
    TPASS;
}

int main() {
    std::cout << "Vector Operations Tests\n";
    std::cout << "========================\n";

    test_vector_literal();
    test_vector_index_scalar();
    test_vector_index_first();
    test_vector_index_last();
    test_vector_index_in_expr();
    test_vector_index_compare();
    test_vector_negative_values();
    test_vector_mixed_types();
    test_vector_return_from_anon();

    auto& engine = JITEngine::instance();
    engine.finalize();

    std::cout << "\nResults: " << g_passed << "/" << (g_passed + g_failed) << " passed\n";
    return g_failed > 0 ? 1 : 0;
}
