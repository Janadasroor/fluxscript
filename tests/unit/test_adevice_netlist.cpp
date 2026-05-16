// Test A-device and WAVEFILE netlist generation
#include "flux/compiler/netlist_generator.h"
#include <iostream>
#include <cassert>

using namespace Flux;

int main() {
    NetlistGenerator ng;

    // Test 1: 2-pin BUF (in=pin0, out=pin6)
    std::cout << "=== Test 1: BUF ===" << std::endl;
    std::string buf = ng.generateADevice("buf1", ADeviceType::BUF, {"in"}, {"out"});
    std::cout << buf;
    assert(buf.find("Abuf1 in 0 0 0 0 0 out 0 BUF") != std::string::npos);
    std::cout << "PASS" << std::endl;

    // Test 2: 3-pin AND (in1=pin0, in2=pin1, out=pin6)
    std::cout << "\n=== Test 2: AND ===" << std::endl;
    std::string and_gate = ng.generateADevice("and1", ADeviceType::AND, {"a", "b"}, {"y"});
    std::cout << and_gate;
    assert(and_gate.find("Aand1 a b 0 0 0 0 y 0 AND") != std::string::npos);
    std::cout << "PASS" << std::endl;

    // Test 3: 4-pin DFF (d=pin0, clk=pin1, q=pin6, nq=pin7)
    std::cout << "\n=== Test 3: DFF ===" << std::endl;
    std::string dff = ng.generateADevice("ff1", ADeviceType::DFF, {"d", "clk"}, {"q", "nq"});
    std::cout << dff;
    assert(dff.find("Aff1 d clk 0 0 0 0 q nq DFF") != std::string::npos);
    std::cout << "PASS" << std::endl;

    // Test 4: WAVEFILE voltage source
    std::cout << "\n=== Test 4: WAVEFILE Voltage ===" << std::endl;
    std::string wav = ng.generateWaveFile("wave1", "out", "0", "audio.wav", 0, false);
    std::cout << wav;
    assert(wav.find("Vwave1 out 0 WAVEFILE \"audio.wav\" CHAN 0") != std::string::npos);
    std::cout << "PASS" << std::endl;

    // Test 5: WAVEFILE current source
    std::cout << "\n=== Test 5: WAVEFILE Current ===" << std::endl;
    std::string iwav = ng.generateWaveFile("isrc1", "out", "0", "signal.wav", 1, true);
    std::cout << iwav;
    assert(iwav.find("Iisrc1 out 0 WAVEFILE \"signal.wav\" CHAN 1") != std::string::npos);
    std::cout << "PASS" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
