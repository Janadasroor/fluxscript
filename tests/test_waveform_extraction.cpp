// Waveform Extraction Test
#include <iostream>
#include <cassert>
#include "flux/runtime/ngspice_bridge.h"

using namespace Flux;

int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          Waveform Extraction - Test                      \n";
    std::cout << "\n";
    std::cout << "\n";

    // Simple RC circuit netlist
    std::string netlist = R"(
* Test RC Circuit
Vin in 0 PULSE(0 5 0 1u 1u 1m 2m)
R1 in out 10k
C1 out 0 1nF
.tran 10u 3m
.END
)";

    // Initialize ngspice
    std::cout << " Initializing ngspice...\n";
    int rc = flux_ngspice_init(netlist.c_str());
    assert(rc == 0 && "ngspice init failed");
    assert(flux_ngspice_is_initialized() && "Should be initialized");

    // Run Transient Analysis
    std::cout << " Running transient analysis...\n";
    rc = flux_ngspice_run_transient(0.0, 0.003, 0.00001);
    assert(rc == 0 && "Transient analysis failed");

    // Extract Vectors
    std::cout << " Extracting vectors...\n";
    
    // Get vector names
    auto names = flux_ngspice_get_vector_names();
    std::cout << "  Available vectors: ";
    for (const auto& n : names) std::cout << n << " ";
    std::cout << "\n";

    // Extract time vector
    auto t_data = flux_ngspice_extract_vector("time");
    std::cout << "  Time points: " << t_data.size() << "\n";
    assert(!t_data.empty() && "Should extract time data");
    assert(t_data.size() > 10 && "Should have enough points");

    // Extract voltage vector
    auto v_data = flux_ngspice_extract_vector("v(out)");
    std::cout << "  Voltage points: " << v_data.size() << "\n";
    assert(!v_data.empty() && "Should extract voltage data");
    assert(t_data.size() == v_data.size() && "Time and Voltage should match");

    // Verify data range
    std::cout << "  Min Voltage: " << v_data.front() << "\n";
    std::cout << "  Max Voltage: " << v_data.back() << "\n";
    assert(v_data.front() >= 0.0 && "Initial voltage should be 0");
    assert(v_data.back() > 0.0 && "Final voltage should be > 0");

    // Cleanup
    flux_ngspice_cleanup();
    std::cout << " Cleanup completed\n";

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          WAVEFORM EXTRACTION TEST PASSED               \n";
    std::cout << "\n";
    std::cout << "\n";

    return 0;
}
