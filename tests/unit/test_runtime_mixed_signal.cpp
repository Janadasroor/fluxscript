#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "flux/runtime/mixed_signal_runtime.h"

static int g_passed = 0, g_failed = 0;

#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)
#define TN(a, b, tol, msg) do { if (std::abs((a) - (b)) > (tol)) { TFAIL(msg); return; } } while(0)

void test_cross_detect() {
    TEST("Cross detection");
    // First call initializes internal state, returns 0
    double r0 = flux_cross_detect(1.0, 0);
    TC(r0 == 0.0, "first call initializes");
    // Now call with value on other side of zero -> cross detected
    double r1 = flux_cross_detect(-1.0, 0);
    TC(r1 == 1.0, "cross from positive to negative");
    // Staying on same side -> no cross
    double r2 = flux_cross_detect(-2.0, 0);
    TC(r2 == 0.0, "no cross staying negative");
    TPASS;
}

void test_above_detect() {
    TEST("Above detection");
    TC(flux_above_detect(5.0, 3.0) == 1.0, "5 > 3");
    TC(flux_above_detect(1.0, 3.0) == 0.0, "1 < 3");
    TC(flux_above_detect(3.0, 3.0) == 0.0, "3 == 3 not above");
    TPASS;
}

void test_edge_detect() {
    TEST("Edge detection");
    // g_eventState is shared - prev = 3.0 from above_detect
    // Falling edge: 3.0 -> 0.0 (crosses 0.5 downwards)
    TC(flux_edge_detect(0.0, 0) == 1.0, "falling edge");
    // Rising edge: 0.0 -> 5.0 (crosses 0.5 upwards)
    TC(flux_edge_detect(5.0, 0) == 1.0, "rising edge");
    // No edge: staying at 5.0
    TC(flux_edge_detect(5.0, 0) == 0.0, "no edge staying high");
    TPASS;
}

void test_noise() {
    TEST("Noise generation");
    double n1 = flux_white_noise(1.0);
    TC(std::isfinite(n1), "white noise is finite");
    double n2 = flux_white_noise(1.0);
    TC(n1 != n2, "different noise samples");
    double t1 = flux_thermal_noise(1000.0, 300.0);
    TC(std::isfinite(t1), "thermal noise is finite");
    // With R=1000, T=300, BW=1: variance ~1.66e-17, std dev ~4e-9
    // Most samples will be non-zero
    double t2 = flux_thermal_noise(0.0, 300.0);
    TC(t2 == 0.0, "zero resistance = zero noise");
    double tn = flux_flicker_noise(1.0, 100.0);
    TC(std::isfinite(tn), "flicker noise is finite");
    TPASS;
}

void test_table() {
    TEST("Table lookup");
    void* tbl = flux_table_create();
    TC(tbl != nullptr, "table created");
    flux_table_add_entry(tbl, 1.0, 10.0);
    flux_table_add_entry(tbl, 2.0, 20.0);
    TN(flux_table_lookup(tbl, 1.0), 10.0, 1e-12, "exact match 1");
    TN(flux_table_lookup(tbl, 2.0), 20.0, 1e-12, "exact match 2");
    // Beyond all keys returns last entry
    TN(flux_table_lookup(tbl, 99.0), 20.0, 1e-12, "beyond returns last");
    // Below all keys returns first entry
    TN(flux_table_lookup(tbl, -1.0), 10.0, 1e-12, "below returns first");
    // Interpolation between keys
    TN(flux_table_lookup(tbl, 1.5), 15.0, 1e-12, "interpolation");
    TPASS;
}

void test_piecewise() {
    TEST("Piecewise interpolation");
    void* pw = flux_piecewise_create(0.0);
    TC(pw != nullptr, "piecewise created");
    flux_piecewise_add_point(pw, 0.0, 0.0);
    flux_piecewise_add_point(pw, 1.0, 10.0);
    flux_piecewise_add_point(pw, 2.0, 20.0);
    TN(flux_piecewise_eval(pw, 0.0), 0.0, 1e-12, "at start");
    TN(flux_piecewise_eval(pw, 1.0), 10.0, 1e-12, "at mid");
    TN(flux_piecewise_eval(pw, 0.5), 5.0, 1e-12, "interpolated");
    TPASS;
}

void test_fsm() {
    TEST("FSM creation");
    void* fsm = flux_fsm_create(0, 0.0);
    TC(fsm != nullptr, "FSM created");
    TPASS;
}

int main() {
    std::cout << "\n  Runtime: Mixed-Signal Test Suite                       \n\n";
    test_cross_detect();
    test_above_detect();
    test_edge_detect();
    test_noise();
    test_table();
    test_piecewise();
    test_fsm();
    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
