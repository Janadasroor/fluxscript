#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include "flux/compiler/compiler_instance.h"
#include "flux/jit/flux_jit.h"

using namespace Flux;

// Mock for flux_get_voltage (renamed to avoid symbol conflict with library)
double g_voltage = 0.0;
extern "C" double flux_mock_get_voltage(double namePtr) {
    return g_voltage;
}

int main() {
    CompilerInstance compiler;
    FluxJIT jit;

    // Register the mock function
    jit.registerFunction("flux_get_voltage", (void*)flux_mock_get_voltage);

    std::string code = R"(
        def update(t, inputs) {
            let freq = 1000.0;
            let duty = V("In1") / 5.0;
            let ramp = (t * freq) - floor(t * freq);

            if (ramp < duty) {
                return 5.0;
            }
            return 0.0;
        }
    )";

    std::cout << "Compiling script..." << std::endl;
    auto artifacts = compiler.compileToIR(code);
    if (!artifacts || !artifacts->codegenContext) {
        std::cerr << "Compilation failed!" << std::endl;
        return 1;
    }

    auto& context = *artifacts->codegenContext;
    std::cout << "Adding module to JIT..." << std::endl;
    jit.addModule(std::move(context.OwnedModule), std::move(context.OwnedContext));

    void* func_ptr = jit.getPointerToFunction("update");
    if (!func_ptr) {
        std::cerr << "Symbol 'update' not found!" << std::endl;
        return 1;
    }

    // New signature: double update(double t, const double* inputs, int count)
    typedef double (*UpdateFunc)(double, const double*, int);
    auto update = reinterpret_cast<UpdateFunc>(func_ptr);

    double inputs[] = {0.0};
    int count = 1;
    
    // Set V("In1") = 1.0 -> duty = 0.2
    g_voltage = 1.0;
    
    // freq = 1000, t = 1e-4 -> t * freq = 0.1. ramp = 0.1.
    // 0.1 < 0.2 is true. Result should be 5.0
    std::cout << "Testing update(1e-4) [ramp=0.1, duty=0.2]..." << std::endl;
    double res1 = update(1e-4, inputs, count);
    std::cout << "Result: " << res1 << std::endl;
    if (res1 != 5.0) {
        std::cerr << "FAILED: expected 5.0, got " << res1 << std::endl;
    }

    // t = 4e-4 -> t * freq = 0.4. ramp = 0.4.
    // 0.4 < 0.2 is false. Result should be 0.0
    std::cout << "Testing update(4e-4) [ramp=0.4, duty=0.2]..." << std::endl;
    double res2 = update(4e-4, inputs, count);
    std::cout << "Result: " << res2 << std::endl;
    if (res2 != 0.0) {
        std::cerr << "FAILED: expected 0.0, got " << res2 << std::endl;
    }

    if (res1 == 5.0 && res2 == 0.0) {
        std::cout << "All tests PASSED!" << std::endl;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
        return 1;
    }

    return 0;
}
