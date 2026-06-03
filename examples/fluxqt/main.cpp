// fluxqt standalone runner
// Links the FluxScript JIT engine with the Qt widget bridge.
// Usage: fluxqt <file.flux>

#include "bridge.h"
#include <flux/jit_engine.h>
#include <QApplication>
#include <QString>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <llvm/Support/InitLLVM.h>

// Forward-declared in bridge.cpp — called from onBridgeEvent
static Flux::JITEngine* g_jit = nullptr;

extern "C" double flux_print_string(double);

void callFluxFunction(const char* name) {
    if (!g_jit) return;
    std::vector<double> args;
    g_jit->callFunction(name, args);
}

int main(int argc, char* argv[]) {
    llvm::InitLLVM initLLVM(argc, argv);
    QApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: fluxqt <file.flux>\n";
        return 1;
    }

    // Read the .flux file
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: could not open " << argv[1] << "\n";
        return 1;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    // Initialize JIT engine
    auto& jit = Flux::JITEngine::instance();
    g_jit = &jit;
    jit.initialize();

    // Register our Qt bridge functions
    registerFluxQtSymbols(jit);

    // Register bridge-only globals (flux_get_var / flux_set_var)
    // In a minimal example we use property-based handle sharing instead.
    // The modern_dashboard.flux uses flux_get_var/flux_set_var — register those too.
    // For simplicity, we embed a minimal global map:
    static std::map<double, double> g_vars;
    jit.registerFunction("flux_set_var", (void*)(+[](double name, double val) -> double {
        g_vars[name] = val;
        return 0.0;
    }));
    jit.registerFunction("flux_get_var", (void*)(+[](double name) -> double {
        auto it = g_vars.find(name);
        return it != g_vars.end() ? it->second : 0.0;
    }));
    jit.registerFunction("viora_flux_print", (void*)(+[](double str) -> double {
        auto* s = reinterpret_cast<const char*>(static_cast<void*>(reinterpret_cast<int64_t*>(&str)));
        flux_print_string(str);
        return 0.0;
    }));

    // Compile
    std::string error;
    if (!jit.compileScript(source, &error)) {
        std::cerr << "FluxScript error: " << error << "\n";
        return 1;
    }

    // Execute top-level expression (__anon_expr) to run script logic
    std::string anonErr;
    auto result = jit.callFunction("__anon_expr", {}, &anonErr);
    if (!anonErr.empty()) {
        std::cerr << "[anon_expr] " << anonErr << "\n";
        anonErr.clear();
    }

    // Enter Qt event loop — keeps the UI alive
    return app.exec();
}
