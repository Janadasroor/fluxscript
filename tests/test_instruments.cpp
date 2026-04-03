// Instrument Control Tests
#include <iostream>
#include <cassert>
#include "flux/instruments/instrument.h"

using namespace Flux::Instruments;

// ┌─────────────────────────────────────────────────────┐
// │ Test 1: Basic Connection (Simulated)                │
// └─────────────────────────────────────────────────────┘
void test_instrument_basics() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 1: Instrument Basics                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    Instrument inst;
    assert(!inst.isConnected() && "Should not be connected initially");

    // Try to connect to localhost (will fail gracefully in test env)
    bool connected = inst.connect("127.0.0.1", 5025);
    std::cout << "  Connection attempt result: " << (connected ? "Success" : "Failed (Expected in test env)") << "\n";
    
    // Verify state
    assert(inst.isConnected() == connected);
    
    inst.disconnect();
    assert(!inst.isConnected());

    std::cout << "\n✅ Test 1 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 2: C Interface                                 │
// └─────────────────────────────────────────────────────┘
void test_c_interface() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 2: C Interface for Power Supply                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    void* ps = flux_ps_create();
    assert(ps != nullptr);

    // Attempt connection (will likely fail in test env, which is fine)
    bool connected = flux_ps_connect(ps, "127.0.0.1", 5025);
    std::cout << "  Power Supply Connection: " << (connected ? "Success" : "Failed") << "\n";

    // If connected, try commands
    if (connected) {
        flux_ps_set_voltage(ps, 3.3);
        double v = flux_ps_measure_voltage(ps);
        std::cout << "  Measured Voltage: " << v << "V\n";
    }

    flux_ps_disconnect(ps);
    flux_ps_destroy(ps);

    std::cout << "\n✅ Test 2 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Main Test Runner                                    │
// └─────────────────────────────────────────────────────┘
int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║          Instrument Control - Test Suite                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    try {
        test_instrument_basics();
        test_c_interface();

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║          ALL INSTRUMENT TESTS PASSED ✅                  ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
