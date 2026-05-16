#include <iostream>
#include <cmath>
#include "flux/runtime/symbolic_engine.h"

using namespace Flux;

static int g_passed = 0, g_failed = 0;

#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)
#define TN(a, b, tol, msg) do { if (std::abs((a) - (b)) > (tol)) { TFAIL(msg); return; } } while(0)

void test_expr_creation() {
    TEST("SymbolicExpr creation");
    auto x = SymbolicExpr::makeSymbol("x");
    TC(x->type == SymbolicExpr::Symbol, "symbol type");
    TC(x->name == "x", "symbol name");
    auto n = SymbolicExpr::makeNumber(3.14);
    TC(n->type == SymbolicExpr::Number, "number type");
    TN(n->value, 3.14, 1e-15, "number value");
    auto a = SymbolicExpr::makeAdd(n, SymbolicExpr::makeNumber(1.0));
    TC(a->type == SymbolicExpr::Add, "add type");
    TC(a->children.size() == 2, "add has 2 children");
    auto m = SymbolicExpr::makeMul(n, SymbolicExpr::makeNumber(2.0));
    TC(m->type == SymbolicExpr::Mul, "mul type");
    auto p = SymbolicExpr::makePower(SymbolicExpr::makeSymbol("x"), SymbolicExpr::makeNumber(2.0));
    TC(p->type == SymbolicExpr::Power, "power type");
    auto fn = SymbolicExpr::makeFunc("sin", SymbolicExpr::makeSymbol("x"));
    TC(fn->type == SymbolicExpr::Function, "function type");
    TC(fn->func_name == "sin", "function name");
    TPASS;
}

void test_to_string() {
    TEST("SymbolicExpr toString");
    auto s = SymbolicExpr::makeAdd(SymbolicExpr::makeSymbol("x"), SymbolicExpr::makeNumber(42.0));
    TC(!s->toString().empty(), "toString produces output");
    TPASS;
}

void test_engine_basics() {
    TEST("SymbolicEngine basics");
    auto& eng = SymbolicEngine::instance();
    auto x = eng.sym("x");
    TC(x->type == SymbolicExpr::Symbol, "sym creates symbol");
    auto y = eng.sym("y");
    auto sum = eng.add(x, y);
    TC(sum->type == SymbolicExpr::Add, "add");
    auto prod = eng.mul(x, y);
    TC(prod->type == SymbolicExpr::Mul, "mul");
    auto pwr = eng.power(x, SymbolicExpr::makeNumber(2.0));
    TC(pwr->type == SymbolicExpr::Power, "power");
    TPASS;
}

void test_simplify() {
    TEST("SymbolicEngine simplify");
    auto& eng = SymbolicEngine::instance();
    auto r = eng.simplify(eng.sym("x"));
    TC(r != nullptr, "simplify returns expression");
    TPASS;
}

void test_differentiate() {
    TEST("SymbolicEngine differentiate");
    auto& eng = SymbolicEngine::instance();
    auto x = eng.sym("x");
    auto x_sq = eng.power(x, SymbolicExpr::makeNumber(2.0));
    auto deriv = eng.differentiate(x_sq, "x");
    TC(deriv != nullptr, "d(x^2)/dx returns expr");
    TC(!deriv->toString().empty(), "derivative has string rep");
    TPASS;
}

void test_expand() {
    TEST("SymbolicEngine expand");
    auto& eng = SymbolicEngine::instance();
    auto e = eng.expand(eng.add(eng.sym("x"), eng.sym("y")));
    TC(e != nullptr, "expand returns expression");
    TPASS;
}

void test_evaluate() {
    TEST("SymbolicEngine evaluate");
    auto& eng = SymbolicEngine::instance();
    auto x = eng.sym("x");
    double result = eng.evaluate(eng.add(x, SymbolicExpr::makeNumber(10.0)), {{"x", 5.0}});
    TN(result, 15.0, 1e-12, "x + 10 = 15 when x = 5");
    TPASS;
}

int main() {
    std::cout << "\n  Runtime: Symbolic Engine Test Suite                    \n\n";
    test_expr_creation();
    test_to_string();
    test_engine_basics();
    test_simplify();
    test_differentiate();
    test_expand();
    test_evaluate();
    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
