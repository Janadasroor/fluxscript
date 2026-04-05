// Behavioral Source Tests
#include <iostream>
#include <cassert>
#include <sstream>
#include "flux/compiler/netlist_generator.h"

using namespace Flux;

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Behavioral Source Netlist Generation Tests         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    NetlistGenerator gen;

    // Test 1: B-source (Voltage)
    std::cout << "Test 1: B-source (Voltage)... ";
    std::string b_voltage = gen.generateBVoltage("B1", "out", "0", "V(in)*2");
    assert(b_voltage.find("BB1") != std::string::npos);
    assert(b_voltage.find("V={V(in)*2}") != std::string::npos);
    std::cout << "✅\n";

    // Test 2: B-source (Current)
    std::cout << "Test 2: B-source (Current)... ";
    std::string b_current = gen.generateBCurrent("B2", "out", "0", "sin(time)");
    assert(b_current.find("BB2") != std::string::npos);
    assert(b_current.find("I={sin(time)}") != std::string::npos);
    std::cout << "✅\n";

    // Test 3: E-source (VCVS)
    std::cout << "Test 3: E-source (VCVS)... ";
    std::string e_source = gen.generateESource("E1", "out", "0", "in+", "in-", 10.0);
    assert(e_source.find("EE1") != std::string::npos);
    assert(e_source.find("out") != std::string::npos);
    assert(e_source.find("in+") != std::string::npos);
    assert(e_source.find("in-") != std::string::npos);
    assert(e_source.find("10") != std::string::npos);
    std::cout << "✅\n";

    // Test 4: F-source (CCCS)
    std::cout << "Test 4: F-source (CCCS)... ";
    std::string f_source = gen.generateFSource("F1", "out", "0", "Vin", 5.0);
    assert(f_source.find("FF1") != std::string::npos);
    assert(f_source.find("Vin") != std::string::npos);
    assert(f_source.find("5") != std::string::npos);
    std::cout << "✅\n";

    // Test 5: G-source (VCCS)
    std::cout << "Test 5: G-source (VCCS)... ";
    std::string g_source = gen.generateGSource("G1", "out", "0", "ctrl+", "ctrl-", 0.01);
    assert(g_source.find("GG1") != std::string::npos);
    assert(g_source.find("ctrl+") != std::string::npos);
    assert(g_source.find("ctrl-") != std::string::npos);
    assert(g_source.find("0.01") != std::string::npos);
    std::cout << "✅\n";

    // Test 6: H-source (CCVS)
    std::cout << "Test 6: H-source (CCVS)... ";
    std::string h_source = gen.generateHSource("H1", "out", "0", "Vmeas", 100.0);
    assert(h_source.find("HH1") != std::string::npos);
    assert(h_source.find("Vmeas") != std::string::npos);
    assert(h_source.find("100") != std::string::npos);
    std::cout << "✅\n";

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       ALL BEHAVIORAL SOURCE TESTS PASSED ✅              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    return 0;
}
