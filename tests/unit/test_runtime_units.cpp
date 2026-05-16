#include <iostream>
#include <cmath>
#include "flux/runtime/units.h"

using namespace Flux::Units;

static int g_passed = 0, g_failed = 0;

#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)
#define TN(a, b, tol, msg) do { if (std::abs((a) - (b)) > (tol)) { TFAIL(msg); return; } } while(0)

void test_dimensions() {
    TEST("UnitDimensions");
    UnitDimensions d;
    TC(d.isDimensionless(), "default is dimensionless");
    TC(!Dimension::Voltage.isDimensionless(), "voltage is not dimensionless");
    UnitDimensions v = Dimension::Power / Dimension::Current;
    TC(v == Dimension::Voltage, "V = W/A");
    UnitDimensions r = Dimension::Voltage / Dimension::Current;
    TC(r == Dimension::Resistance, "R = V/A");
    UnitDimensions f = Dimension::Mass * Dimension::Acceleration;
    TC(f == Dimension::Force, "F = m*a");
    UnitDimensions e = Dimension::Force * Dimension::Length;
    TC(e == Dimension::Energy, "E = F*d");
    TPASS;
}

void test_convert() {
    TEST("Convert helpers");
    TN(Convert::millivolts(1000.0), 1.0, 1e-15, "mV to V");
    TN(Convert::kiloohms(1.0), 1000.0, 1e-15, "kOhm");
    TN(Convert::microfarads(1.0), 1e-6, 1e-15, "uF");
    TN(Convert::millihenrys(1.0), 1e-3, 1e-15, "mH");
    TN(Convert::milliamperes(1.0), 1e-3, 1e-15, "mA");
    TN(Convert::kilohertz(1.0), 1000.0, 1e-15, "kHz");
    TN(Convert::milliseconds(1.0), 1e-3, 1e-15, "ms");
    TPASS;
}

void test_quantified() {
    TEST("QuantifiedValue");
    ScaledUnit vu(Dimension::Voltage, 1.0, "volt", "V");
    ScaledUnit au(Dimension::Current, 1.0, "ampere", "A");
    QuantifiedValue v1(10.0, vu);
    QuantifiedValue i1(2.0, au);
    QuantifiedValue sum = v1 + v1;
    TN(sum.value(), 20.0, 1e-15, "V + V");
    QuantifiedValue prod = v1 * i1;
    TN(prod.value(), 20.0, 1e-15, "V * A");
    TC(prod.dimensions() == Dimension::Power, "V*A = W");
    QuantifiedValue quot = v1 / i1;
    TN(quot.value(), 5.0, 1e-15, "V / A");
    TC(quot.dimensions() == Dimension::Resistance, "V/A = Ohm");
    TC(v1 == QuantifiedValue(10.0, vu), "equality");
    TC(v1 < QuantifiedValue(20.0, vu), "less than");
    TC(v1 > QuantifiedValue(5.0, vu), "greater than");
    TPASS;
}

void test_registry() {
    TEST("UnitRegistry");
    auto& reg = UnitRegistry::instance();
    const ScaledUnit* ohm = reg.lookupUnit("ohm");
    TC(ohm != nullptr, "ohm registered");
    TC(ohm->dimensions == Dimension::Resistance, "ohm matched Resistance");
    bool compat = reg.areCompatible(
        ScaledUnit(Dimension::Resistance, 1.0, "", ""),
        ScaledUnit(Dimension::Resistance, 1000.0, "", ""));
    TC(compat, "same dimensions compatible");
    ScaledUnit mv(Dimension::Voltage, 0.001, "millivolt", "mV");
    ScaledUnit v(Dimension::Voltage, 1.0, "volt", "V");
    TN(reg.convert(1000.0, mv, v), 1.0, 1e-12, "1000 mV = 1 V");
    TPASS;
}

int main() {
    std::cout << "\n  Runtime: Units Test Suite                              \n\n";
    test_dimensions();
    test_convert();
    test_quantified();
    test_registry();
    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
