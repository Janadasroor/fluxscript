// ============================================================================
// FluxScript JIT Benchmark Suite
// Compiles actual FluxScript source via the real CompilerInstance → FluxJIT
// pipeline and measures execution performance.
// ============================================================================

#include "flux/jit/flux_jit.h"
#include "flux/compiler/compiler_instance.h"
#include "flux/runtime/flux_runtime.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>

struct BenchmarkResult {
    std::string name;
    std::string category;
    double compile_ms;      // JIT compilation time (cold start)
    double exec_ms;         // Execution time for N iterations
    double exec_per_iter_ns; // Average ns per iteration
    int iterations;
};

static std::vector<BenchmarkResult> g_results;

// Compile FluxScript source to a JIT-compiled function pointer
static void* compile_flux_script(const std::string& source, const std::string& func_name,
                                 double* out_compile_ms, std::string* out_error) {
    auto start = std::chrono::high_resolution_clock::now();

    Flux::CompilerOptions opts;
    opts.injectStdlib = true;
    opts.optimizationLevel = Flux::OptimizationLevel::O2;
    opts.inputName = "benchmark";
    opts.moduleName = "benchmark";

    Flux::CompilerInstance compiler(opts);
    auto artifacts = compiler.compileToIR(source, out_error);

    if (!artifacts || !artifacts->codegenContext || !artifacts->codegenContext->TheModule) {
        *out_compile_ms = -1.0;
        return nullptr;
    }

    auto jit = std::make_unique<Flux::FluxJIT>();
    jit->addModule(
        std::move(artifacts->codegenContext->TheModule),
        std::move(artifacts->codegenContext->OwnedContext)
    );

    // Register runtime functions
    Flux::registerRuntimeFunctions(*jit);

    auto end = std::chrono::high_resolution_clock::now();
    *out_compile_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return jit->getPointerToFunction(func_name);
}

// Execute a compiled function N times and measure
static double exec_benchmark(void* fn_ptr, int iterations, double* out_per_iter_ns) {
    using FnType = double(*)();
    auto fn = reinterpret_cast<FnType>(fn_ptr);

    auto start = std::chrono::high_resolution_clock::now();
    volatile double result = 0;
    for (int i = 0; i < iterations; i++) {
        result = fn();
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    *out_per_iter_ns = (elapsed_ms / iterations) * 1e6;
    return result;
}

// Execute a function with one double argument
static double exec_benchmark_1arg(void* fn_ptr, double arg, int iterations, double* out_per_iter_ns) {
    using FnType = double(*)(double);
    auto fn = reinterpret_cast<FnType>(fn_ptr);

    auto start = std::chrono::high_resolution_clock::now();
    volatile double result = 0;
    for (int i = 0; i < iterations; i++) {
        result = fn(arg);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    *out_per_iter_ns = (elapsed_ms / iterations) * 1e6;
    return result;
}

#define BENCHMARK(name, category, source, func_name, iterations) \
    do { \
        std::string error; \
        double compile_ms = 0; \
        void* fn = compile_flux_script(source, func_name, &compile_ms, &error); \
        if (!fn) { \
            printf("  ❌ %-30s COMPILE ERROR: %s\n", name, error.c_str()); \
        } else { \
            double per_iter_ns = 0; \
            double result = exec_benchmark(fn, iterations, &per_iter_ns); \
            double exec_ms = (per_iter_ns * iterations) / 1e6; \
            (void)result; \
            g_results.push_back({name, category, compile_ms, exec_ms, per_iter_ns, iterations}); \
            printf("  ✅ %-30s compile=%.2fms  exec=%.2fms  %.1fns/iter  (%d iters)\n", \
                   name, compile_ms, exec_ms, per_iter_ns, iterations); \
        } \
    } while(0)

#define BENCHMARK_1ARG(name, category, source, func_name, arg, iterations) \
    do { \
        std::string error; \
        double compile_ms = 0; \
        void* fn = compile_flux_script(source, func_name, &compile_ms, &error); \
        if (!fn) { \
            printf("  ❌ %-30s COMPILE ERROR: %s\n", name, error.c_str()); \
        } else { \
            double per_iter_ns = 0; \
            double result = exec_benchmark_1arg(fn, arg, iterations, &per_iter_ns); \
            double exec_ms = (per_iter_ns * iterations) / 1e6; \
            (void)result; \
            g_results.push_back({name, category, compile_ms, exec_ms, per_iter_ns, iterations}); \
            printf("  ✅ %-30s compile=%.2fms  exec=%.2fms  %.1fns/iter  (%d iters)\n", \
                   name, compile_ms, exec_ms, per_iter_ns, iterations); \
        } \
    } while(0)

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║   FluxScript JIT Benchmark Suite (Real LLVM Pipeline)   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    // === Math Benchmarks ===
    printf("  ─── Math ───\n");
    BENCHMARK("math_sin_10k", "Math",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        r = r + sin(i * 0.001)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    BENCHMARK("math_exp_10k", "Math",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        r = r + exp(i * 0.0001)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    BENCHMARK("math_sqrt_10k", "Math",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        r = r + sqrt(i + 1.0)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    BENCHMARK("math_pow_10k", "Math",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        r = r + pow(i * 0.001, 2.0)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    // === Signal Processing ===
    printf("\n  ─── Signal Processing ───\n");
    BENCHMARK("signal_sine_wave", "Signal",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        r = r + sin(i * 6.28318 / 1000.0)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    BENCHMARK("signal_mixed_funcs", "Signal",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        var t = i * 0.001 in\n"
        "        r = r + sin(t) * cos(t) + exp(-t * 0.1)\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    // === Control Flow ===
    printf("\n  ─── Control Flow ───\n");
    BENCHMARK("control_if_branch", "ControlFlow",
        "def compute() {\n"
        "    var r = 0.0\n"
        "    for i in 0, 10000 do {\n"
        "        if (i % 2) == 0 then {\n"
        "            r = r + sin(i * 0.001)\n"
        "        } else {\n"
        "            r = r + cos(i * 0.001)\n"
        "        }\n"
        "    }\n"
        "    r\n"
        "}\n",
        "compute", 100);

    // === Native C++ baseline (for comparison) ===
    printf("\n  ─── Native C++ Baseline ───\n");

    auto native_start = std::chrono::high_resolution_clock::now();
    volatile double native_result = 0;
    for (int iter = 0; iter < 100; iter++) {
        double r = 0;
        for (int i = 0; i < 10000; i++) {
            r += std::sin(i * 0.001);
        }
        native_result += r;
    }
    auto native_end = std::chrono::high_resolution_clock::now();
    double native_ms = std::chrono::duration<double, std::milli>(native_end - native_start).count();
    double native_per_iter = (native_ms / 100) * 1e6;
    printf("  📊 native_sin_10k (C++):         exec=%.2fms  %.1fns/iter  (100 iters)\n",
           native_ms, native_per_iter);

    // === Summary ===
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║                      Summary                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("  %-30s %10s %10s %12s\n", "Benchmark", "Compile", "Execute", "ns/iter");
    printf("  %-30s %10s %10s %12s\n", "---------", "--------", "-------", "-------");

    for (const auto& r : g_results) {
        printf("  %-30s %8.2fms %8.2fms %10.1fns\n",
               r.name.c_str(), r.compile_ms, r.exec_ms, r.exec_per_iter_ns);
    }

    printf("\n  Native C++ baseline: %.1fns/iter (sin_10k)\n", native_per_iter);

    // Calculate JIT overhead ratio
    if (!g_results.empty()) {
        double avg_compile = 0;
        double avg_exec = 0;
        for (const auto& r : g_results) {
            avg_compile += r.compile_ms;
            avg_exec += r.exec_per_iter_ns;
        }
        avg_compile /= g_results.size();
        avg_exec /= g_results.size();
        printf("\n  Average JIT compile: %.2fms\n", avg_compile);
        printf("  Average JIT exec:    %.1fns/iter\n", avg_exec);
        printf("  JIT overhead vs C++: %.1fx\n", avg_exec / native_per_iter);
    }

    printf("\n");
    return 0;
}
