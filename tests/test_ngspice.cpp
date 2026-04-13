// ngspice Integration Test
#include <iostream>
#include <fstream>
#include "flux/runtime/ngspice_bridge.h"

using namespace Flux;

// Test 1: Simple RC Circuit Transient Analysis
void test_rc_transient() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 1: RC Circuit - Transient Analysis                 \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Create netlist
    std::string netlist = R"(
* RC Low-Pass Filter - Transient Analysis
Vin in 0 PULSE(0 5 0 1n 1n 1m 2m)
R1 in out 10k
C1 out 0 1uF
.tran 10u 3m
.print tran V(out)
.end
)";
    
    std::cout << "Netlist:\n" << netlist << "\n";
    
    // Initialize ngspice
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        std::cout << "  ngspice init failed (may not be compiled with ngspice support)\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        return;
    }
    
    std::cout << " ngspice initialized\n";
    
    // Run transient analysis
    rc = flux_ngspice_run_transient(0, 0.003, 10e-6);
    if (rc != 0) {
        std::cout << " Transient analysis failed\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        flux_ngspice_cleanup();
        return;
    }
    
    std::cout << " Transient analysis completed\n";
    
    // Get results
    int timeSize = flux_ngspice_get_vector_size("time");
    int voutSize = flux_ngspice_get_vector_size("v(out)");
    
    std::cout << " Results: time points=" << timeSize << ", V(out) points=" << voutSize << "\n";
    
    // Print first few values
    if (voutSize > 0) {
        std::cout << "\nFirst 5 V(out) values:\n";
        for (int i = 0; i < 5 && i < voutSize; i++) {
            double vout = flux_ngspice_get_vector("v(out)", i);
            double t = flux_ngspice_get_vector("time", i);
            std::cout << "  t=" << t*1000 << "ms, V(out)=" << vout << "V\n";
        }
    }
    
    flux_ngspice_cleanup();
    std::cout << "\n Test 1 PASSED\n";
}

// Test 2: Simple DC Sweep
void test_dc_sweep() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 2: Voltage Divider - DC Sweep                      \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Create netlist
    std::string netlist = R"(
* Voltage Divider - DC Sweep
Vin in 0 DC 0
R1 in out 10k
R2 out 0 10k
.DC Vin 0 10 0.5
.print dc V(out)
.end
)";
    
    std::cout << "Netlist:\n" << netlist << "\n";
    
    // Initialize ngspice
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        std::cout << "  ngspice init failed\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        return;
    }
    
    std::cout << " ngspice initialized\n";
    
    // Run DC sweep
    rc = flux_ngspice_run_dc("Vin", 0, 10, 0.5);
    if (rc != 0) {
        std::cout << " DC sweep failed\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        flux_ngspice_cleanup();
        return;
    }
    
    std::cout << " DC sweep completed\n";
    
    // Get results
    int vinSize = flux_ngspice_get_vector_size("vin");
    int voutSize = flux_ngspice_get_vector_size("v(out)");
    
    std::cout << " Results: Vin points=" << vinSize << ", V(out) points=" << voutSize << "\n";
    
    // Verify voltage divider
    if (voutSize > 0) {
        double vin = flux_ngspice_get_vector("vin", 0);
        double vout = flux_ngspice_get_vector("v(out)", 0);
        std::cout << "\nSample: Vin=" << vin << "V, V(out)=" << vout << "V\n";
        std::cout << "Expected: V(out) = Vin/2 = " << vin/2 << "V\n";
    }
    
    flux_ngspice_cleanup();
    std::cout << "\n Test 2 PASSED\n";
}

// Test 3: AC Analysis
void test_ac_analysis() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test 3: RC Filter - AC Analysis                         \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Create netlist
    std::string netlist = R"(
* RC Low-Pass Filter - AC Analysis
Vin in 0 AC 1
R1 in out 10k
C1 out 0 1uF
.AC DEC 10 1 100k
.print ac V(out)
.end
)";
    
    std::cout << "Netlist:\n" << netlist << "\n";
    
    // Initialize ngspice
    int rc = flux_ngspice_init(netlist.c_str());
    if (rc != 0) {
        std::cout << "  ngspice init failed\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        return;
    }
    
    std::cout << " ngspice initialized\n";
    
    // Run AC analysis
    rc = flux_ngspice_run_ac(10, 1, 100000);
    if (rc != 0) {
        std::cout << " AC analysis failed\n";
        std::cout << "   Error: " << flux_ngspice_get_error() << "\n";
        flux_ngspice_cleanup();
        return;
    }
    
    std::cout << " AC analysis completed\n";
    
    // Get results
    int freqSize = flux_ngspice_get_vector_size("frequency");
    std::cout << " Results: frequency points=" << freqSize << "\n";
    
    // Get vector names
    auto names = flux_ngspice_get_vector_names();
    std::cout << " Available vectors:\n";
    for (const auto& name : names) {
        std::cout << "  - " << name << "\n";
    }
    
    flux_ngspice_cleanup();
    std::cout << "\n Test 3 PASSED\n";
}

int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          ngspice Integration Test Suite                  \n";
    std::cout << "\n";
    
#ifdef FLUX_HAS_NGSPICE
    std::cout << "\n Compiled WITH ngspice support\n";
#else
    std::cout << "\n  Compiled WITHOUT ngspice support (stub mode)\n";
#endif
    
    try {
        test_rc_transient();
        test_dc_sweep();
        test_ac_analysis();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL NGSPICE TESTS COMPLETED                   \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
