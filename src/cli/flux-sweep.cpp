// flux-sweep: Parametric Sweep Tool
// Usage: flux-sweep <circuit.flux> --component <Name> --start <Val> --stop <Val> --points <N>

#include <iostream>
#include <string>
#include "flux/sweep/parametric_sweep.h"

using namespace Flux::Sweep;

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║          FluxScript Parametric Sweep                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-sweep <circuit.flux> --component <Name> --start <V> --stop <V> --points <N>\n";
        return 1;
    }

    std::string filename = argv[1];
    std::string component = "R1";
    double start = 1000;
    double stop = 10000;
    int points = 10;

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--component" && i + 1 < argc) component = argv[++i];
        else if (arg == "--start" && i + 1 < argc) start = std::stod(argv[++i]);
        else if (arg == "--stop" && i + 1 < argc) stop = std::stod(argv[++i]);
        else if (arg == "--points" && i + 1 < argc) points = std::stoi(argv[++i]);
    }

    std::cout << "📄 Circuit: " << filename << "\n";

    // Create Engine
    SweepEngine engine;
    engine.setCircuitFile(filename);
    engine.setComponent(component);
    engine.setRange(start, stop, points);
    engine.setMeasure("V(out)");

    // Run Sweep
    SweepReport report = engine.run();

    // Output Results
    std::cout << report.toText();

    return 0;
}
