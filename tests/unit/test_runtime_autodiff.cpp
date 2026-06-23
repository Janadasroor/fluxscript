#include <iostream>
#include <cmath>
#include "flux/runtime/auto_diff.h"

using namespace Flux::Autodiff;

static int g_passed = 0, g_failed = 0;

#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)
#define TN(a, b, tol, msg) do { if (std::abs((a) - (b)) > (tol)) { TFAIL(msg); return; } } while(0)

void test_dual_construction() {
    TEST("Dual construction and constants");
    Dual a;
    TC(a.value == 0.0 && a.dual == 0.0, "default ctor");
    Dual b(3.0, 1.0);
    TC(b.value == 3.0 && b.dual == 1.0, "value ctor");
    Dual c = Dual::constant(5.0);
    TC(c.value == 5.0 && c.dual == 0.0, "constant");
    Dual d = Dual::variable(2.0);
    TC(d.value == 2.0 && d.dual == 1.0, "variable");
    TPASS;
}

void test_dual_arithmetic() {
    TEST("Dual arithmetic");
    Dual x(2.0, 1.0);
    Dual y(3.0, 0.0);
    Dual sum = x + y;
    TC(sum.value == 5.0 && sum.dual == 1.0, "addition");
    Dual diff = x - y;
    TC(diff.value == -1.0 && diff.dual == 1.0, "subtraction");
    Dual prod = x * y;
    TC(prod.value == 6.0 && prod.dual == 3.0, "product rule");
    Dual quot = x / y;
    TN(quot.value, 2.0/3.0, 1e-15, "quotient value");
    TN(quot.dual, 1.0/3.0, 1e-15, "quotient derivative");
    Dual neg = -x;
    TC(neg.value == -2.0 && neg.dual == -1.0, "negation");
    TPASS;
}

void test_dual_elementary() {
    TEST("Dual elementary functions");
    Dual x(1.0, 1.0);
    Dual s = DualMath::sin(x);
    TN(s.value, std::sin(1.0), 1e-15, "sin value");
    TN(s.dual, std::cos(1.0), 1e-15, "sin derivative = cos(x)");
    Dual c = DualMath::cos(x);
    TN(c.dual, -std::sin(1.0), 1e-15, "cos derivative = -sin(x)");
    Dual e = DualMath::exp(x);
    TN(e.value, std::exp(1.0), 1e-14, "exp value");
    TN(e.dual, std::exp(1.0), 1e-14, "exp derivative = exp(x)");
    Dual l = DualMath::log(Dual(2.0, 1.0));
    TN(l.value, std::log(2.0), 1e-15, "log value");
    TN(l.dual, 0.5, 1e-15, "log derivative = 1/x");
    Dual r = DualMath::sqrt(Dual(4.0, 1.0));
    TN(r.value, 2.0, 1e-15, "sqrt value");
    TN(r.dual, 0.25, 1e-15, "sqrt derivative = 1/(2*sqrt(x))");
    TPASS;
}

void test_chain_rule() {
    TEST("Chain rule: f(x)=sin(x^2) at x=2");
    Dual x(2.0, 1.0);
    Dual u = x * x;
    Dual f = DualMath::sin(u);
    double expected = std::cos(4.0) * 4.0;
    TN(f.dual, expected, 1e-14, "chain rule");
    TPASS;
}

void test_composite_derivative() {
    TEST("Composite: f(x)=exp(-x)*sin(2x) at x=1.5");
    Dual x(1.5, 1.0);
    Dual f = DualMath::exp(-x) * DualMath::sin(x * Dual(2.0, 0.0));
    double ex = std::exp(-1.5);
    double expected = -ex * std::sin(3.0) + ex * 2.0 * std::cos(3.0);
    TN(f.dual, expected, 1e-14, "composite derivative");
    TPASS;
}

int main() {
    std::cout << "\n  Runtime: Autodiff Test Suite                          \n\n";
    test_dual_construction();
    test_dual_arithmetic();
    test_dual_elementary();
    test_chain_rule();
    test_composite_derivative();
    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
