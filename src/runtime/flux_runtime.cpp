/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

#include "flux/runtime/flux_runtime.h"
#include "flux/jit/flux_jit.h"
#include "flux/runtime/symbolic_engine.h"
#include "flux/runtime/mixed_signal_runtime.h"
#include "flux/runtime/flux_sim_service.h"
#include "flux/runtime/time_domain_api.h"
#include "flux/ai/surrogate.h"

#include <Eigen/Dense>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <mutex>

// Global service pointer
extern "C" FluxSimulationService* g_flux_sim_service = nullptr;

namespace Flux {

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

// Memory tracking for JIT objects
struct MatrixTracker {
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXd>> matrices;
    std::unordered_map<void*, std::unique_ptr<Eigen::MatrixXcd>> complex_matrices;
    std::mutex mutex;
    bool use_gpu = false;

    MatrixTracker() {
        const char* gpu_env = std::getenv("FLUX_USE_GPU");
        if (gpu_env && std::string(gpu_env) == "1") use_gpu = true;
    }

    void* register_matrix(std::unique_ptr<Eigen::MatrixXd> mat) {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat.get());
        matrices[ptr] = std::move(mat);
        return ptr;
    }

    void* register_complex_matrix(std::unique_ptr<Eigen::MatrixXcd> mat) {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat.get());
        complex_matrices[ptr] = std::move(mat);
        return ptr;
    }
};
static MatrixTracker g_matrix_tracker;

// Symbolic pointer helpers
static std::shared_ptr<SymbolicExpr> get_sym_ptr(double ptr) {
    if (ptr == 0) return nullptr;
    return std::shared_ptr<SymbolicExpr>((SymbolicExpr*)(uintptr_t)ptr, [](SymbolicExpr*){});
}
static double make_sym_ptr(std::shared_ptr<SymbolicExpr> expr) {
    if (!expr) return 0.0;
    auto& engine = SymbolicEngine::instance();
    return (double)(uintptr_t)engine.registerExpr(expr).get();
}

extern "C" double flux_print_string(const char* str);
extern "C" double flux_print_double(double x);

// --- Real Matrix Runtime ---

extern "C" void* flux_create_matrix(double* data, int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    if (data) {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) (*mat)(r, c) = data[r * cols + c];
    } else mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    if (g_matrix_tracker.use_gpu && A->rows() >= 512 && B->cols() >= 512) {
        std::cout << "[FLUX GPU] Offloading matrix multiplication..." << std::endl;
    }
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) * (*B)));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) * s));
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() + s));
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() - s));
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s - M->array()));
}

extern "C" void* flux_matrix_div_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) / s));
}

extern "C" void* flux_matrix_div_sm(double s, void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s / M->array()));
}

extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    if (A->rows() != A->cols()) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->partialPivLu().solve(*B)));
}

extern "C" void* flux_matrix_pow(void* m_ptr, double p) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (M->rows() == M->cols() && std::floor(p) == p && p >= 0) {
        int ip = (int)p;
        if (ip == 0) return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(Eigen::MatrixXd::Identity(M->rows(), M->cols())));
        Eigen::MatrixXd res = *M;
        for (int i = 1; i < ip; i++) res *= *M;
        return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(res));
    }
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array().pow(p)));
}

extern "C" void* flux_matrix_transpose(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->transpose()));
}

extern "C" int flux_matrix_rows(void* m_ptr) { return m_ptr ? (int)static_cast<Eigen::MatrixXd*>(m_ptr)->rows() : 0; }
extern "C" int flux_matrix_cols(void* m_ptr) { return m_ptr ? (int)static_cast<Eigen::MatrixXd*>(m_ptr)->cols() : 0; }

extern "C" double flux_matrix_get(void* m_ptr, int row, int col) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) return 0.0;
    return (*M)(row, col);
}

// --- Complex Matrix Runtime ---

extern "C" void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    if (data) {
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) (*mat)(r, c) = data[r * cols + c];
    } else mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_mul(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXcd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXcd*>(b_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) * (*B)));
}

extern "C" void* flux_complex_matrix_mul_ms(void* m_ptr, std::complex<double> s) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*M) * s));
}

extern "C" void* flux_complex_matrix_add(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXcd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXcd*>(b_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) + (*B)));
}

extern "C" void* flux_complex_matrix_add_ms(void* m_ptr, std::complex<double> s) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(M->array() + s));
}

extern "C" void* flux_complex_matrix_sub(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXcd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXcd*>(b_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) - (*B)));
}

extern "C" void* flux_complex_matrix_sub_ms(void* m_ptr, std::complex<double> s) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(M->array() - s));
}

extern "C" void* flux_complex_matrix_sub_sm(std::complex<double> s, void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(s - M->array()));
}

extern "C" void* flux_complex_matrix_div_ms(void* m_ptr, std::complex<double> s) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*M) / s));
}

extern "C" void* flux_complex_matrix_solve(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXcd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXcd*>(b_ptr);
    if (A->rows() != A->cols()) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(A->partialPivLu().solve(*B)));
}

extern "C" int flux_complex_matrix_rows(void* m_ptr) { return m_ptr ? (int)static_cast<Eigen::MatrixXcd*>(m_ptr)->rows() : 0; }
extern "C" int flux_complex_matrix_cols(void* m_ptr) { return m_ptr ? (int)static_cast<Eigen::MatrixXcd*>(m_ptr)->cols() : 0; }

extern "C" std::complex<double> flux_complex_matrix_get(void* m_ptr, int row, int col) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) return 0.0;
    return (*M)(row, col);
}

extern "C" void* flux_promote_matrix_to_complex(void* m_ptr) {
    if (!m_ptr) return nullptr;
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    auto res = std::make_unique<Eigen::MatrixXcd>(M->rows(), M->cols());
    for(int r=0; r<M->rows(); r++)
        for(int c=0; c<M->cols(); c++) (*res)(r, c) = std::complex<double>((*M)(r, c), 0.0);
    return g_matrix_tracker.register_complex_matrix(std::move(res));
}

// --- Other Runtime ---

extern "C" double flux_pi() { return 3.14159265358979323846; }
extern "C" double flux_e() { return 2.71828182845904523536; }
extern "C" double flux_sin(double x) { return std::sin(x); }
extern "C" double flux_cos(double x) { return std::cos(x); }
extern "C" double flux_sqrt(double x) { return std::sqrt(x); }
extern "C" double flux_pow(double base, double exp) { return std::pow(base, exp); }

extern "C" double flux_create_vector_sum(double* data, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) sum += data[i];
    return sum;
}

extern "C" double flux_gpu_available() { return g_matrix_tracker.use_gpu ? 1.0 : 0.0; }
extern "C" void flux_gpu_set_enabled(double enabled) { g_matrix_tracker.use_gpu = (enabled != 0.0); }

extern "C" double flux_print_matrix(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

extern "C" double flux_print_complex_matrix(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    if (!M) return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

// Symbolic
extern "C" double flux_sym_decl(const char* name) { return make_sym_ptr(SymbolicEngine::instance().sym(name)); }
extern "C" double flux_sym_add(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().add(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" double flux_sym_eq(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().eq(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" double flux_sym_ne(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().ne(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" void* flux_sym_jacobian(void* e_p, int ec, void* v_p, int vc) {
    auto* ptrs = static_cast<double*>(e_p); auto** names = static_cast<const char**>(v_p);
    std::vector<std::shared_ptr<SymbolicExpr>> exprs; for(int i=0; i<ec; i++) exprs.push_back(get_sym_ptr(ptrs[i]));
    std::vector<std::string> vars; for(int i=0; i<vc; i++) vars.push_back(names[i]);
    auto J = SymbolicEngine::instance().jacobian(exprs, vars);
    auto res = std::make_unique<Eigen::MatrixXd>(J.size(), vars.size());
    for(int r=0; r<(int)J.size(); r++) for(int c=0; c<(int)vars.size(); c++) (*res)(r, c) = make_sym_ptr(J[r][c]);
    return g_matrix_tracker.register_matrix(std::move(res));
}
extern "C" double flux_sym_pde_register(double eq, int vc, void* v_p) {
    auto** names = static_cast<const char**>(v_p); std::vector<std::string> vars; for(int i=0; i<vc; i++) vars.push_back(names[i]);
    return make_sym_ptr(SymbolicEngine::instance().pde_register(get_sym_ptr(eq), vars));
}
extern "C" double flux_sym_pdiff(double e, const char* v, int o) { return make_sym_ptr(SymbolicEngine::instance().partial_differentiate(get_sym_ptr(e), v, o)); }

// JIT Registration
void registerRuntimeFunctions(FluxJIT& jit) {
    jit.registerFunction("flux_create_matrix", (void*)&flux_create_matrix);
    jit.registerFunction("flux_matrix_mul", (void*)&flux_matrix_mul);
    jit.registerFunction("flux_matrix_mul_ms", (void*)&flux_matrix_mul_ms);
    jit.registerFunction("flux_matrix_add", (void*)&flux_matrix_add);
    jit.registerFunction("flux_matrix_sub", (void*)&flux_matrix_sub);
    jit.registerFunction("flux_matrix_transpose", (void*)&flux_matrix_transpose);
    jit.registerFunction("flux_matrix_rows", (void*)&flux_matrix_rows);
    jit.registerFunction("flux_matrix_cols", (void*)&flux_matrix_cols);
    jit.registerFunction("flux_matrix_get", (void*)&flux_matrix_get);
    jit.registerFunction("print_matrix", (void*)&flux_print_matrix);
    
    jit.registerFunction("flux_create_complex_matrix", (void*)&flux_create_complex_matrix);
    jit.registerFunction("flux_complex_matrix_mul", (void*)&flux_complex_matrix_mul);
    jit.registerFunction("flux_complex_matrix_mul_ms", (void*)&flux_complex_matrix_mul_ms);
    jit.registerFunction("flux_complex_matrix_add", (void*)&flux_complex_matrix_add);
    jit.registerFunction("flux_complex_matrix_add_ms", (void*)&flux_complex_matrix_add_ms);
    jit.registerFunction("flux_complex_matrix_sub", (void*)&flux_complex_matrix_sub);
    jit.registerFunction("flux_complex_matrix_sub_ms", (void*)&flux_complex_matrix_sub_ms);
    jit.registerFunction("flux_complex_matrix_sub_sm", (void*)&flux_complex_matrix_sub_sm);
    jit.registerFunction("flux_complex_matrix_rows", (void*)&flux_complex_matrix_rows);
    jit.registerFunction("flux_complex_matrix_cols", (void*)&flux_complex_matrix_cols);
    jit.registerFunction("flux_complex_matrix_get", (void*)&flux_complex_matrix_get);
    jit.registerFunction("print_complex_matrix", (void*)&flux_print_complex_matrix);
    jit.registerFunction("flux_promote_matrix_to_complex", (void*)&flux_promote_matrix_to_complex);
    jit.registerFunction("flux_print_string", (void*)&flux_print_string);
    jit.registerFunction("flux_print_double", (void*)&flux_print_double);

    jit.registerFunction("gpu_available", (void*)&flux_gpu_available);
    jit.registerFunction("gpu_enable", (void*)&flux_gpu_set_enabled);
    jit.registerFunction("pi", (void*)&flux_pi);
    jit.registerFunction("e", (void*)&flux_e);
    jit.registerFunction("sin", (void*)&flux_sin);
    jit.registerFunction("cos", (void*)&flux_cos);
    jit.registerFunction("sqrt", (void*)&flux_sqrt);
    jit.registerFunction("pow", (void*)&flux_pow);
    
    // AI / Neural Network
    jit.registerFunction("flux_nn_create", (void*)&AI::flux_nn_create);
    jit.registerFunction("flux_nn_train", (void*)&AI::flux_nn_train);
    jit.registerFunction("flux_nn_predict", (void*)&AI::flux_nn_predict);

    jit.registerFunction("flux_sym_decl", (void*)&flux_sym_decl);
    jit.registerFunction("flux_sym_add", (void*)&flux_sym_add);
    jit.registerFunction("flux_sym_eq", (void*)&flux_sym_eq);
    jit.registerFunction("flux_sym_ne", (void*)&flux_sym_ne);
    jit.registerFunction("flux_sym_jacobian", (void*)&flux_sym_jacobian);
    jit.registerFunction("flux_sym_pde_register", (void*)&flux_sym_pde_register);
    jit.registerFunction("flux_sym_pdiff", (void*)&flux_sym_pdiff);
}

// Parallel
typedef void (*ParallelBodyFunc)(int64_t, void*);
extern "C" void flux_parallel_for(int64_t start, int64_t end, int64_t chunk_size, void* body_func_ptr) {
    if (start >= end || !body_func_ptr) return;
    auto body_func = reinterpret_cast<ParallelBodyFunc>(body_func_ptr);
    std::atomic<int64_t> current(start);
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < std::thread::hardware_concurrency(); t++) {
        threads.emplace_back([&current, end, body_func]() {
            while (true) {
                int64_t i = current.fetch_add(1); if (i >= end) break;
                body_func(i, nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
}

} // namespace Flux

extern "C" double flux_print_string(const char* str) {
    if (str) {
        printf("%s", str);
        fflush(stdout);
    }
    return 0.0;
}

extern "C" double flux_print_double(double x) { printf("%g", x); fflush(stdout); return x; }
extern "C" const char* flux_string_concat_double(const char* s, double d) {
    std::string res = std::string(s?s:"") + std::to_string(d);
    return strdup(res.c_str());
}
