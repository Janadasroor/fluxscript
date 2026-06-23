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

#include "flux/runtime/runtime_helpers.h"
#include "flux/runtime/matrix_tracker.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace Flux {

MatrixTracker g_matrix_tracker;

extern "C" void flux_free_matrix(void* ptr)
{
    g_matrix_tracker.unregister_matrix(ptr);
}

// Access matrix from data pointer (for advanced_math.cpp)
extern "C" void* flux_lookup_matrix(void* data_ptr)
{
    return g_matrix_tracker.get_matrix(data_ptr);
}

// Register a newly allocated matrix and return its data pointer
extern "C" void* flux_register_new_matrix(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_register_new_complex_matrix(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

// Copy data into a tracked matrix
extern "C" void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols)
{
    auto* M = g_matrix_tracker.get_matrix(data_ptr);
    if (!M || M->rows() != rows || M->cols() != cols)
        return;
    std::memcpy(M->data(), data, rows * cols * sizeof(double));
}

extern "C" void* flux_create_matrix(double* data, int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    if (data) {
        // Use Eigen::Map for fast copy if possible, or just loop
        std::memcpy(mat->data(), data, rows * cols * sizeof(double));
    } else
        mat->setZero();
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) * (*B)));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) * s));
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) + (*B)));
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*A) - (*B)));
}

extern "C" void* flux_matrix_transpose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->transpose()));
}

extern "C" int flux_matrix_rows(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    int r = M ? (int)M->rows() : 0;
    return r;
}

extern "C" int flux_matrix_cols(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? (int)M->cols() : 0;
}

extern "C" double flux_matrix_det(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->determinant() : 0.0;
}

extern "C" void flux_matrix_set(void* m_ptr, int row, int col, double val)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) {
        return;
    }
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) {
        return;
    }
    (*M)(row, col) = val;
}

static constexpr int FLUX_MATRIX_MAX_ELEMENTS = 1000000; // 1M elements max

extern "C" void* flux_matrix_zeros(int rows, int cols)
{
    if (rows <= 0 || cols <= 0 || (long long)rows * cols > FLUX_MATRIX_MAX_ELEMENTS)
        return nullptr;
    auto mat = std::make_unique<Eigen::MatrixXd>(rows, cols);
    mat->setZero();
    void* ptr = g_matrix_tracker.register_matrix(std::move(mat));
    return ptr;
}

extern "C" void* flux_create_range_sum(double start, double step, double end)
{
    if (step == 0.0) {
        auto mat = std::make_unique<Eigen::MatrixXd>(1, 1);
        mat->setZero();
        return g_matrix_tracker.register_matrix(std::move(mat));
    }
    int n = (int)std::floor((end - start) / step) + 1;
    if (n < 1) n = 1;
    auto mat = std::make_unique<Eigen::MatrixXd>(n, 1);
    for (int i = 0; i < n; ++i)
        (*mat)(i, 0) = start + i * step;
    return g_matrix_tracker.register_matrix(std::move(mat));
}

extern "C" void* flux_matrix_ones(int rows, int cols)
{
    if (rows <= 0 || cols <= 0 || (long long)rows * cols > FLUX_MATRIX_MAX_ELEMENTS)
        return nullptr;
    auto* M = new Eigen::MatrixXd(rows, cols);
    M->setOnes();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_eye(int n)
{
    if (n <= 0 || (long long)n * n > FLUX_MATRIX_MAX_ELEMENTS)
        return nullptr;
    auto* M = new Eigen::MatrixXd(n, n);
    M->setIdentity();
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_copy(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return flux_create_matrix(M->data(), M->rows(), M->cols());
}

extern "C" void* flux_matrix_diag(void* v_ptr)
{
    auto* V = g_matrix_tracker.get_matrix(v_ptr);
    if (!V || V->cols() != 1)
        return nullptr;
    int n = V->rows();
    auto* M = new Eigen::MatrixXd(n, n);
    M->setZero();
    for (int i = 0; i < n; i++)
        (*M)(i, i) = (*V)(i, 0);
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(M));
}

extern "C" void* flux_matrix_hcat(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    Eigen::MatrixXd C(A->rows(), A->cols() + B->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" void* flux_matrix_vcat(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    Eigen::MatrixXd C(A->rows() + B->rows(), A->cols());
    C << *A, *B;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(std::move(C)));
}

extern "C" double flux_matrix_trace(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->trace() : 0.0;
}

extern "C" void* flux_matrix_diag_extract(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    int n = std::min(M->rows(), M->cols());
    auto* V = new Eigen::MatrixXd(n, 1);
    for (int i = 0; i < n; i++)
        (*V)(i, 0) = (*M)(i, i);
    return g_matrix_tracker.register_matrix(std::unique_ptr<Eigen::MatrixXd>(V));
}

extern "C" void* flux_matrix_slice(void* m_ptr, int r0, int r1, int c0, int c1)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    if (r0 < 0)
        r0 = 0;
    if (r1 > M->rows())
        r1 = M->rows();
    if (c0 < 0)
        c0 = 0;
    if (c1 > M->cols())
        c1 = M->cols();
    if (r0 >= r1 || c0 >= c1)
        return flux_matrix_zeros(0, 0);
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->block(r0, c0, r1 - r0, c1 - c0)));
}

extern "C" double flux_matrix_sum(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->sum() : 0.0;
}

extern "C" double flux_matrix_mean(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    return M ? M->mean() : 0.0;
}

extern "C" void* flux_matrix_inv(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->inverse()));
}

extern "C" void* flux_matrix_solve(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->partialPivLu().solve(*B)));
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M) {
        return 0.0;
    }
    if (row < 0 || row >= M->rows() || col < 0 || col >= M->cols()) {
        return 0.0;
    }
    double val = (*M)(row, col);
    return val;
}

extern "C" void* flux_create_complex_matrix(std::complex<double>* data, int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    if (data) {
        std::memcpy(mat->data(), data, rows * cols * sizeof(std::complex<double>));
    } else
        mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_mul_decomposed(void* a_ptr, int a_rows, int a_cols, void* b_ptr, int b_rows, int b_cols)
{
    Eigen::Map<Eigen::MatrixXcd> A(static_cast<std::complex<double>*>(a_ptr), a_rows, a_cols);
    Eigen::Map<Eigen::MatrixXcd> B(static_cast<std::complex<double>*>(b_ptr), b_rows, b_cols);
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>(A * B));
}

extern "C" void* flux_promote_matrix_to_complex(void* m_ptr, int rows, int cols)
{
    if (!m_ptr)
        return nullptr;
    Eigen::Map<Eigen::MatrixXd> M(static_cast<double*>(m_ptr), rows, cols);
    auto res = std::make_unique<Eigen::MatrixXcd>(M.cast<std::complex<double>>());
    return g_matrix_tracker.register_complex_matrix(std::move(res));
}

// Complex matrix utility functions (binary operator path)
extern "C" int flux_complex_matrix_rows(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    return M ? (int)M->rows() : 0;
}

extern "C" int flux_complex_matrix_cols(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    return M ? (int)M->cols() : 0;
}

extern "C" void* flux_complex_matrix_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) * (*B)));
}

extern "C" void* flux_complex_matrix_add(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) + (*B)));
}

extern "C" void* flux_complex_matrix_sub(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(std::make_unique<Eigen::MatrixXcd>((*A) - (*B)));
}

extern "C" void* flux_complex_matrix_ew_div(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(A->array() / B->array()));
}

extern "C" void* flux_complex_matrix_ew_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_complex_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_complex_matrix(b_ptr);
    if (!A || !B) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(A->array() * B->array()));
}

extern "C" void* flux_complex_matrix_add_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->array() + s));
}

extern "C" void* flux_complex_matrix_sub_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->array() - s));
}

extern "C" void* flux_complex_matrix_sub_sm(double re, double im, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(s - M->array()));
}

extern "C" void* flux_complex_matrix_mul_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>((*M) * s));
}

extern "C" void* flux_complex_matrix_div_ms(void* m_ptr, double re, double im)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>((*M) / s));
}

extern "C" void* flux_complex_matrix_div_sm(double re, double im, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    std::complex<double> s(re, im);
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(s / M->array()));
}

extern "C" void* flux_complex_matrix_transpose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->transpose()));
}

extern "C" void* flux_complex_matrix_conj(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->conjugate()));
}

extern "C" void* flux_complex_matrix_ctranspose(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->adjoint()));
}

extern "C" void* flux_complex_matrix_inv(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return nullptr;
    return g_matrix_tracker.register_complex_matrix(
        std::make_unique<Eigen::MatrixXcd>(M->inverse()));
}

extern "C" double flux_complex_matrix_det(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return 0.0;
    return std::abs(M->determinant());
}

extern "C" double flux_complex_matrix_trace(void* m_ptr)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M) return 0.0;
    return M->trace().real();
}

extern "C" double flux_complex_matrix_get_real(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M || row < 0 || row >= M->rows() || col < 0 || col >= M->cols())
        return 0.0;
    return (*M)(row, col).real();
}

extern "C" double flux_complex_matrix_get_imag(void* m_ptr, int row, int col)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (!M || row < 0 || row >= M->rows() || col < 0 || col >= M->cols())
        return 0.0;
    return (*M)(row, col).imag();
}

extern "C" void flux_complex_matrix_set(void* m_ptr, int row, int col, double real, double imag)
{
    auto* M = g_matrix_tracker.get_complex_matrix(m_ptr);
    if (M && row >= 0 && row < M->rows() && col >= 0 && col < M->cols())
        (*M)(row, col) = std::complex<double>(real, imag);
}

extern "C" void* flux_complex_matrix_zeros(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setZero();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_ones(int rows, int cols)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(rows, cols);
    mat->setOnes();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_complex_matrix_eye(int n)
{
    auto mat = std::make_unique<Eigen::MatrixXcd>(n, n);
    mat->setIdentity();
    return g_matrix_tracker.register_complex_matrix(std::move(mat));
}

extern "C" void* flux_matrix_div_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>((*M) / s));
}

extern "C" void* flux_matrix_div_sm(double s, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s / M->array()));
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() / B->array()));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr)
{
    auto* A = g_matrix_tracker.get_matrix(a_ptr);
    auto* B = g_matrix_tracker.get_matrix(b_ptr);
    if (!A || !B)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(A->array() * B->array()));
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() + s));
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(M->array() - s));
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr)
{
    auto* M = g_matrix_tracker.get_matrix(m_ptr);
    if (!M)
        return nullptr;
    return g_matrix_tracker.register_matrix(std::make_unique<Eigen::MatrixXd>(s - M->array()));
}

extern "C" double flux_print_matrix(void* m_ptr)
{
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M)
        return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

extern "C" double flux_print_complex_matrix(void* m_ptr)
{
    auto* M = static_cast<Eigen::MatrixXcd*>(m_ptr);
    if (!M)
        return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

} // namespace Flux
