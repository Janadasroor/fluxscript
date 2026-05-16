#include <iostream>
#include <cmath>
#include <numeric>
#include "flux/runtime/parallel_runtime.h"

using namespace Flux;

static int g_passed = 0, g_failed = 0;

#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)

void test_singleton() {
    TEST("Singleton and thread count");
    auto& rt = ParallelRuntime::instance();
    TC(rt.getNumThreads() > 0, "positive thread count");
    TC(rt.getLastExecutionTime() == 0.0, "initial time is zero");
    TPASS;
}

void test_set_threads() {
    TEST("SetNumThreads");
    auto& rt = ParallelRuntime::instance();
    int orig = rt.getNumThreads();
    rt.setNumThreads(2);
    TC(rt.getNumThreads() == 2, "set to 2");
    rt.setNumThreads(0);
    TC(rt.getNumThreads() > 0, "auto-detect");
    rt.setNumThreads(orig);
    TPASS;
}

void test_parallel_for() {
    TEST("parallelFor");
    auto& rt = ParallelRuntime::instance();
    std::vector<int> results(100, 0);
    rt.parallelFor(0, 100, [&](int i) { results[i] = i * i; });
    bool ok = true;
    for (int i = 0; i < 100; i++) { if (results[i] != i * i) { ok = false; break; } }
    TC(ok, "all elements computed");
    TC(rt.getLastExecutionTime() > 0.0, "execution time recorded");
    TPASS;
}

void test_reduce_sum() {
    TEST("parallelReduce sum");
    auto& rt = ParallelRuntime::instance();
    double sum = rt.parallelReduce(1, 1001, [](int i) { return (double)i; },
                                   [](double a, double b) { return a + b; }, 0.0);
    double expected = 1001.0 * 1000.0 / 2.0;
    TC(std::abs(sum - expected) < 1.0, "sum 1..1000");
    TPASS;
}

void test_reduce_max() {
    TEST("parallelReduce max");
    auto& rt = ParallelRuntime::instance();
    double mx = rt.parallelReduce(0, 1000, [](int i) { return (double)(i * 2); },
                                  [](double a, double b) { return std::max(a, b); }, 0.0);
    TC(std::abs(mx - 1998.0) < 0.5, "max of 0..1998");
    TPASS;
}

int main() {
    std::cout << "\n  Runtime: Parallel Test Suite                            \n\n";
    test_singleton();
    test_set_threads();
    test_parallel_for();
    test_reduce_sum();
    test_reduce_max();
    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
