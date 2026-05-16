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
#include "flux/runtime/advanced_math.h"
#include "flux/jit/flux_jit.h"
#include "flux/runtime/symbolic_engine.h"
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

class FluxSimulationService;
extern "C" FluxSimulationService* g_flux_sim_service;
FluxSimulationService* g_flux_sim_service = nullptr;

// Global C-API declarations
extern "C" {
    double flux_register_subckt(double name_ptr, double pins_ptr);
    double flux_register_bsource(double name_ptr, double out_ptr, double ref_ptr);
    void flux_register_goal(double current, double target);
    void flux_hot_swap(const char* name, double model_ptr);
    double flux_optimize();
    double flux_edge_detect(double value, double edge_type);
}

namespace Flux {

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

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
        void* ptr = static_cast<void*>(mat->data());
        matrices[ptr] = std::move(mat);
        return ptr;
    }

    void* register_complex_matrix(std::unique_ptr<Eigen::MatrixXcd> mat) {
        std::lock_guard<std::mutex> lock(mutex);
        void* ptr = static_cast<void*>(mat->data());
        complex_matrices[ptr] = std::move(mat);
        return ptr;
    }

    Eigen::MatrixXd* get_matrix(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = matrices.find(ptr);
        return it != matrices.end() ? it->second.get() : nullptr;
    }

    Eigen::MatrixXcd* get_complex_matrix(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = complex_matrices.find(ptr);
        return it != complex_matrices.end() ? it->second.get() : nullptr;
    }

    void unregister_matrix(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        matrices.erase(ptr);
        complex_matrices.erase(ptr);
    }
};
static MatrixTracker g_matrix_tracker;

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

extern "C" void flux_free_matrix(void* ptr) {
    g_matrix_tracker.unregister_matrix(ptr);
}

// Access matrix from data pointer (for advanced_math.cpp)
extern "C" void* flux_lookup_matrix(void* data_ptr) {
    return g_matrix_tracker.get_matrix(data_ptr);
}

// Register a newly allocated matrix and return its data pointer
extern "C" void* flux_register_new_matrix(int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

// Copy data into a tracked matrix
extern "C" void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols) {
    auto* M = g_matrix_tracker.get_matrix(data_ptr);
    if (!M || M->rows() != rows || M->cols() != cols) return;
    std::memcpy(M->data(), data, rows * cols * sizeof(double));
}

extern "C" void* flux_create_matrix(double* data, int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    if (data) {
        // Use Eigen::Map for fast copy if possible, or just loop
        std::memcpy(mat->data(), data, rows * cols * sizeof(double));
    } else mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) * (*B)));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) * s));
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));
}

extern "C" void* flux_matrix_transpose(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->transpose()));
}

extern "C" int flux_matrix_rows(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? (int)M->rows() : 0;
}

extern "C" int flux_matrix_cols(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? (int)M->cols() : 0;
}

extern "C" double flux_matrix_det(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->determinant() : 0.0;
}

extern "C" void flux_matrix_set(void* m_ptr, int row, int col, double val) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (M && row >= 0 && row < M->rows() && col >= 0 && col < M->cols())
        (*M)(row, col) = val;
}

extern "C" void* flux_matrix_zeros(int rows, int cols) {
    return flux_create_matrix(nullptr, rows, cols);
}

extern "C" void* flux_matrix_ones(int rows, int cols) {
    auto* M = new Eigen::MatrixXd(rows, cols);
    M->setOnes();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_eye(int n) {
    auto* M = new Eigen::MatrixXd(n, n);
    M->setIdentity();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_copy(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return flux_create_matrix(M->data(), M->rows(), M->cols());
}

extern "C" void* flux_matrix_diag(void* v_ptr) {
    auto* V = g_matrix_tracker.get_matrix(v_ptr);
    if (!V || V->cols() != 1) return nullptr;
    int n = V->rows();
    auto* M = new Eigen::MatrixXd(n, n);
    M->setZero();
    for (int i = 0; i < n; i++) (*M)(i, i) = (*V)(i, 0);
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_hcat(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    Eigen::MatrixXd C(A->rows(), A->cols() + B->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" void* flux_matrix_vcat(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    Eigen::MatrixXd C(A->rows() + B->rows(), A->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" void* flux_matrix_slice(void* m_ptr, int r0, int r1, int c0, int c1) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    if (r0 < 0) r0 = 0; if (r1 > M->rows()) r1 = M->rows();
    if (c0 < 0) c0 = 0; if (c1 > M->cols()) c1 = M->cols();
    if (r0 >= r1 || c0 >= c1) return flux_matrix_zeros(0, 0);
    return g_matrix_tracker.register_matrix(
        std::make_unique<Eigen::MatrixXd>(M->block(r0, c0, r1 - r0, c1 - c0)));
}

extern "C" double flux_matrix_sum(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->sum() : 0.0;
}

extern "C" double flux_matrix_mean(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->mean() : 0.0;
}

extern "C" void* flux_matrix_inv(void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->inverse()));
}

extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->partialPivLu().solve(*B)));
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M || row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) return 0.0;
    return (*M)(row, col);
}

extern "C" void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols) {
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    if (data) {
        std::memcpy(mat->data(), data, rows * cols * sizeof(std::complex<double>));
    } else mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_mul(void* a_ptr, int a_rows, int a_cols, void* b_ptr, int b_rows, int b_cols) {
    Eigen::Map<Eigen::MatrixXcd> A(static_cast<std::complex<double>*>(a_ptr), a_rows, a_cols);
    Eigen::Map<Eigen::MatrixXcd> B(static_cast<std::complex<double>*>(b_ptr), b_rows, b_cols);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(A * B));
}

extern "C" void* flux_promote_matrix_to_complex(void* m_ptr, int rows, int cols) {
    if (!m_ptr) return nullptr;
    Eigen::Map<Eigen::MatrixXd> M(static_cast<double*>(m_ptr), rows, cols);
    auto res = std::make_unique<Eigen::MatrixXcd>(M.cast<std::complex<double>>());
    return g_matrix_tracker.register_complex_matrix(std::move(res));
}

extern "C" double flux_pi() { return 3.14159265358979323846; }
extern "C" double flux_sin(double x) { return std::sin(x); }
extern "C" double flux_cos(double x) { return std::cos(x); }
extern "C" double flux_sqrt(double x) { return std::sqrt(x); }
extern "C" double flux_pow(double base, double exp) { return std::pow(base, exp); }

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

extern "C" double flux_sym_decl(const char* name) { return make_sym_ptr(SymbolicEngine::instance().sym(name)); }
extern "C" double flux_sym_number(double val) { return make_sym_ptr(SymbolicExpr::makeNumber(val)); }
extern "C" double flux_sym_add(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().add(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" double flux_sym_sub(double a, double b) {
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().add(sa, SymbolicEngine::instance().mul(neg_one, sb)));
}
extern "C" double flux_sym_mul(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().mul(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" double flux_sym_div(double a, double b) {
    auto sa = get_sym_ptr(a), sb = get_sym_ptr(b);
    auto neg_one = SymbolicExpr::makeNumber(-1.0);
    return make_sym_ptr(SymbolicEngine::instance().mul(sa, SymbolicEngine::instance().power(sb, neg_one)));
}
extern "C" double flux_sym_pow(double a, double b) { return make_sym_ptr(SymbolicEngine::instance().power(get_sym_ptr(a), get_sym_ptr(b))); }
extern "C" double flux_sym_simplify(double e) { return make_sym_ptr(SymbolicEngine::instance().simplify(get_sym_ptr(e))); }
extern "C" double flux_sym_pdiff(double e, const char* v, int o) { return make_sym_ptr(SymbolicEngine::instance().partial_differentiate(get_sym_ptr(e), v, o)); }
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


extern "C" void* flux_matrix_div_ms(void* m_ptr, double s) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) / s));
}

extern "C" void* flux_matrix_div_sm(double s, void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s / M->array()));
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr) {
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() + s));
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() - s));
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr) {
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s - M->array()));
}

void registerRuntimeFunctions(FluxJIT& jit) {
    jit.registerFunction("flux_create_matrix", (void*)&flux_create_matrix);
    jit.registerFunction("flux_matrix_mul", (void*)&flux_matrix_mul);
    jit.registerFunction("flux_matrix_mul_ms", (void*)&flux_matrix_mul_ms);
    jit.registerFunction("flux_matrix_div_ms", (void*)&flux_matrix_div_ms);
    jit.registerFunction("flux_matrix_div_sm", (void*)&flux_matrix_div_sm);
    jit.registerFunction("flux_matrix_ew_div", (void*)&flux_matrix_ew_div);
    jit.registerFunction("flux_matrix_ew_mul", (void*)&flux_matrix_ew_mul);
    jit.registerFunction("flux_matrix_add_ms", (void*)&flux_matrix_add_ms);
    jit.registerFunction("flux_matrix_sub_ms", (void*)&flux_matrix_sub_ms);
    jit.registerFunction("flux_matrix_sub_sm", (void*)&flux_matrix_sub_sm);
    jit.registerFunction("flux_matrix_add", (void*)&flux_matrix_add);
    jit.registerFunction("flux_matrix_sub", (void*)&flux_matrix_sub);
    jit.registerFunction("flux_matrix_transpose", (void*)&flux_matrix_transpose);
    jit.registerFunction("flux_matrix_rows", (void*)&flux_matrix_rows);
    jit.registerFunction("flux_matrix_cols", (void*)&flux_matrix_cols);
    jit.registerFunction("flux_matrix_get", (void*)&flux_matrix_get);
    jit.registerFunction("flux_matrix_det", (void*)&flux_matrix_det);
    jit.registerFunction("flux_matrix_inv", (void*)&flux_matrix_inv);
    jit.registerFunction("flux_matrix_solve", (void*)&flux_matrix_solve);
    jit.registerFunction("print_matrix", (void*)&flux_print_matrix);
    jit.registerFunction("flux_create_complex_matrix", (void*)&flux_create_complex_matrix);
    jit.registerFunction("flux_complex_matrix_mul", (void*)&flux_complex_matrix_mul);
    jit.registerFunction("print_complex_matrix", (void*)&flux_print_complex_matrix);
    jit.registerFunction("flux_promote_matrix_to_complex", (void*)&flux_promote_matrix_to_complex);

    // Clean user-facing matrix API
    jit.registerFunction("matrix_create", (void*)&flux_matrix_zeros);
    jit.registerFunction("matrix_zeros", (void*)&flux_matrix_zeros);
    jit.registerFunction("matrix_ones", (void*)&flux_matrix_ones);
    jit.registerFunction("matrix_eye", (void*)&flux_matrix_eye);
    jit.registerFunction("matrix_copy", (void*)&flux_matrix_copy);
    jit.registerFunction("matrix_diag", (void*)&flux_matrix_diag);
    jit.registerFunction("matrix_hcat", (void*)&flux_matrix_hcat);
    jit.registerFunction("matrix_vcat", (void*)&flux_matrix_vcat);
    jit.registerFunction("matrix_sum", (void*)&flux_matrix_sum);
    jit.registerFunction("matrix_mean", (void*)&flux_matrix_mean);
    jit.registerFunction("matrix_mul", (void*)&flux_matrix_mul);
    jit.registerFunction("matrix_add", (void*)&flux_matrix_add);
    jit.registerFunction("matrix_sub", (void*)&flux_matrix_sub);
    jit.registerFunction("matrix_transpose", (void*)&flux_matrix_transpose);
    jit.registerFunction("matrix_inv", (void*)&flux_matrix_inv);
    jit.registerFunction("matrix_det", (void*)&flux_matrix_det);
    jit.registerFunction("matrix_solve", (void*)&flux_matrix_solve);
    jit.registerFunction("matrix_get", (void*)&flux_matrix_get);
    jit.registerFunction("matrix_set", (void*)&flux_matrix_set);
    jit.registerFunction("flux_matrix_set", (void*)&flux_matrix_set);
    jit.registerFunction("matrix_slice", (void*)&flux_matrix_slice);
    jit.registerFunction("flux_matrix_slice", (void*)&flux_matrix_slice);
    jit.registerFunction("matrix_rows", (void*)&flux_matrix_rows);
    jit.registerFunction("matrix_cols", (void*)&flux_matrix_cols);

    jit.registerFunction("gpu_available", (void*)&flux_gpu_available);
    jit.registerFunction("gpu_enable", (void*)&flux_gpu_set_enabled);
    jit.registerFunction("pi", (void*)&flux_pi);
    jit.registerFunction("sin", (void*)&flux_sin);
    jit.registerFunction("cos", (void*)&flux_cos);
    jit.registerFunction("sqrt", (void*)&flux_sqrt);
    jit.registerFunction("pow", (void*)&flux_pow);
    jit.registerFunction("flux_print_string", (void*)&flux_print_string);
    jit.registerFunction("flux_print_double", (void*)&flux_print_double);
    jit.registerFunction("flux_nn_create", (void*)&AI::flux_nn_create);
    jit.registerFunction("flux_nn_train", (void*)&AI::flux_nn_train);
    jit.registerFunction("flux_nn_predict", (void*)&AI::flux_nn_predict);
    jit.registerFunction("flux_register_subckt", (void*)&flux_register_subckt);
    jit.registerFunction("flux_register_bsource", (void*)&flux_register_bsource);
    jit.registerFunction("flux_register_goal", (void*)&flux_register_goal);
    jit.registerFunction("optimize", (void*)&flux_optimize);
    jit.registerFunction("flux_hot_swap", (void*)&flux_hot_swap);
    jit.registerFunction("edge_detect", (void*)&flux_edge_detect);
    jit.registerFunction("flux_sym_decl", (void*)&flux_sym_decl);
    jit.registerFunction("flux_sym_number", (void*)&flux_sym_number);
    jit.registerFunction("flux_sym_add", (void*)&flux_sym_add);
    jit.registerFunction("flux_sym_sub", (void*)&flux_sym_sub);
    jit.registerFunction("flux_sym_mul", (void*)&flux_sym_mul);
    jit.registerFunction("flux_sym_div", (void*)&flux_sym_div);
    jit.registerFunction("flux_sym_pow", (void*)&flux_sym_pow);
    jit.registerFunction("flux_sym_simplify", (void*)&flux_sym_simplify);
    jit.registerFunction("flux_sym_jacobian", (void*)&flux_sym_jacobian);
    jit.registerFunction("flux_sym_pde_register", (void*)&flux_sym_pde_register);
    jit.registerFunction("flux_sym_pdiff", (void*)&flux_sym_pdiff);
    jit.registerFunction("flux_sym_eq", (void*)&flux_sym_eq);
    jit.registerFunction("flux_sym_ne", (void*)&flux_sym_ne);

    // Advanced math: linear algebra
    jit.registerFunction("matrix_lu",        (void*)&flux_matrix_lu);
    jit.registerFunction("matrix_qr",        (void*)&flux_matrix_qr);
    jit.registerFunction("matrix_svd",       (void*)&flux_matrix_svd);
    jit.registerFunction("matrix_cholesky",  (void*)&flux_matrix_cholesky);
    jit.registerFunction("matrix_eigenvalues",  (void*)&flux_matrix_eigenvalues);
    jit.registerFunction("matrix_eigenvectors", (void*)&flux_matrix_eigenvectors);
    jit.registerFunction("matrix_rank",      (void*)&flux_matrix_rank);
    jit.registerFunction("matrix_cond",      (void*)&flux_matrix_cond);
    jit.registerFunction("matrix_norm",      (void*)&flux_matrix_norm);

    // Advanced math: statistics
    jit.registerFunction("randn",            (void*)&flux_randn);
    jit.registerFunction("rand_uniform",     (void*)&flux_rand_uniform);
    jit.registerFunction("normal_pdf",       (void*)&flux_normal_pdf);
    jit.registerFunction("normal_cdf",       (void*)&flux_normal_cdf);
    jit.registerFunction("uniform_pdf",      (void*)&flux_uniform_pdf);
    jit.registerFunction("exponential_pdf",  (void*)&flux_exponential_pdf);
    jit.registerFunction("covariance",       (void*)&flux_covariance);
    jit.registerFunction("correlation",      (void*)&flux_correlation);
    jit.registerFunction("percentile",       (void*)&flux_percentile);
    jit.registerFunction("skewness",         (void*)&flux_skewness);
    jit.registerFunction("kurtosis",         (void*)&flux_kurtosis);
    jit.registerFunction("median_filter",    (void*)&flux_median_filter);

    // Advanced math: numerical methods
    jit.registerFunction("ode_rk4",          (void*)&flux_ode_rk4);
    jit.registerFunction("ode_euler",        (void*)&flux_ode_euler);
    jit.registerFunction("root_bisection",   (void*)&flux_root_bisection);
    jit.registerFunction("root_newton",      (void*)&flux_root_newton);
    jit.registerFunction("integrate_adaptive", (void*)&flux_integrate_adaptive);
    jit.registerFunction("interp1_linear",   (void*)&flux_interp1_linear);
    jit.registerFunction("interp1_spline",   (void*)&flux_interp1_spline);

    // Advanced math: special functions
    jit.registerFunction("erf",              (void*)&flux_erf);
    jit.registerFunction("erfc",             (void*)&flux_erfc);
    jit.registerFunction("gamma",            (void*)&flux_gamma);
    jit.registerFunction("lgamma",           (void*)&flux_lgamma);
    jit.registerFunction("bessel_j0",        (void*)&flux_bessel_j0);
    jit.registerFunction("bessel_j1",        (void*)&flux_bessel_j1);
    jit.registerFunction("bessel_y0",        (void*)&flux_bessel_y0);
    jit.registerFunction("bessel_y1",        (void*)&flux_bessel_y1);

    // Advanced math: signal processing
    jit.registerFunction("conv",             (void*)&flux_conv);
    jit.registerFunction("corr",             (void*)&flux_corr);
    jit.registerFunction("lowpass",          (void*)&flux_lowpass);
    jit.registerFunction("highpass",         (void*)&flux_highpass);

    // Advanced math: polynomials
    jit.registerFunction("polyval",          (void*)&flux_polyval);
    jit.registerFunction("polyfit",          (void*)&flux_polyfit);
    jit.registerFunction("polyroots",        (void*)&flux_polyroots);

    // Advanced math: optimization
    jit.registerFunction("least_squares",    (void*)&flux_least_squares);
    jit.registerFunction("gradient_descent", (void*)&flux_gradient_descent);
}

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

extern "C" double flux_print_string(const char* str) { if(str) { printf("%s", str); fflush(stdout); } return 0.0; }
extern "C" double flux_print_double(double x) { printf("%g", x); fflush(stdout); return x; }
extern "C" const char* flux_string_concat_double(const char* s, double d) {
    std::string res = std::string(s?s:"") + std::to_string(d);
    return strdup(res.c_str());
}

static std::vector<std::pair<double, double>> g_goals;
extern "C" void flux_register_goal(double current, double target) { g_goals.push_back({current, target}); }
extern "C" double flux_optimize() {
    if (g_goals.empty()) return 0.0;
    double total_error = 0.0;
    for (const auto& goal : g_goals) total_error += std::abs(goal.first - goal.second);
    g_goals.clear();
    return total_error;
}

// End of file

extern "C" double flux_register_subckt(double name_ptr, double pins_ptr) { return 0.0; }

extern "C" void flux_hot_swap(const char* name, double model_ptr) {
    std::string subckt_name = name ? name : "anonymous";
    std::cout << "[FLUX AI] Hot-swapping subcircuit '" << subckt_name << "' with NN Surrogate model at " << std::hex << (uintptr_t)model_ptr << std::dec << "..." << std::endl;
}
