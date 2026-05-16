#include <iostream>
#include <cmath>
#include <cstring>
#include <vector>
#include <cstdlib>
#include "flux/runtime/advanced_math.h"
#include "flux/flux_eigen.h"
#include <Eigen/Dense>

// Functions defined in flux_runtime.cpp (no public header)
extern "C" int flux_matrix_rows(void*);
extern "C" int flux_matrix_cols(void*);
extern "C" void* flux_matrix_inv(void*);
extern "C" void* flux_matrix_mul(void*, void*);
extern "C" void* flux_matrix_transpose(void*);
extern "C" void* flux_create_matrix(double*, int, int);
extern "C" void* flux_lookup_matrix(void*);
extern "C" void* flux_matrix_eye(int);
extern "C" void* flux_matrix_diag_extract(void*);
extern "C" double flux_matrix_trace(void*);
extern "C" double* flux_polyroots(double*, int);
extern "C" void* flux_fft(void*, double);
extern "C" double flux_fft_thd(void*, double);
extern "C" double flux_fft_snr(void*, double);
extern "C" void* flux_matrix_zeros(int, int);
extern "C" void* flux_matrix_ones(int, int);
extern "C" void* flux_matrix_copy(void*);
extern "C" void* flux_matrix_hcat(void*, void*);
extern "C" void* flux_matrix_vcat(void*, void*);
extern "C" void* flux_matrix_diag(void*);
extern "C" double flux_matrix_sum(void*);
extern "C" double flux_matrix_mean(void*);

static int g_passed = 0, g_failed = 0;
#define TEST(x) std::cout << "  " << x << "... "
#define TPASS do { std::cout << "PASSED\n"; g_passed++; } while(0)
#define TFAIL(x) do { std::cout << "FAILED: " << x << "\n"; g_failed++; } while(0)
#define TC(cond, msg) do { if (!(cond)) { TFAIL(msg); return; } } while(0)
#define TN(a, b, tol, msg) do { if (std::abs((a) - (b)) > (tol)) { TFAIL(msg); return; } } while(0)

static void* mkmat(double* d, int r, int c) { return flux_create_matrix(d, r, c); }
static double get(void* m, int r, int c) { return flux_matrix_get(m, r, c); }
static int rows(void* m) { return flux_matrix_rows(m); }
static int cols(void* m) { return flux_matrix_cols(m); }

// ---------------------------------------------------------------------------
// Linear Algebra
// ---------------------------------------------------------------------------

void test_matrix_rank() {
    TEST("Matrix rank");
    double d[] = {2, 0, 0, 2};
    void* m = mkmat(d, 2, 2);
    TN(flux_matrix_rank(m), 2.0, 0.1, "full rank 2x2");
    double d2[] = {1, 2, 2, 4};
    void* m2 = mkmat(d2, 2, 2);
    TN(flux_matrix_rank(m2), 1.0, 0.1, "rank deficient");
    TPASS;
}

void test_matrix_norm() {
    TEST("Matrix norms");
    double d[] = {1, 2, 3, 4};
    void* m = mkmat(d, 2, 2);
    double inorm = flux_matrix_norm(m, 0);
    TC(inorm > 0, "inf norm positive");
    double fnorm = flux_matrix_norm(m, 2);
    TC(fnorm > 0, "frobenius norm positive");
    TPASS;
}

void test_eigenvalues() {
    TEST("Eigenvalues");
    double d[] = {4, 1, 1, 4};
    void* m = mkmat(d, 2, 2);
    void* ev = flux_matrix_eigenvalues(m);
    TC(ev != nullptr, "eigenvalues computed");
    TC(rows(ev) == 2 && cols(ev) == 2, "eigenvalues matrix [n x 2]");
    TC(get(ev, 0, 1) == 0.0 && get(ev, 1, 1) == 0.0, "real eigenvalues for symmetric matrix");
    TPASS;
}

void test_eigenvectors() {
    TEST("Eigenvectors");
    double d[] = {4, 1, 1, 4};
    void* m = mkmat(d, 2, 2);
    void* vec = flux_matrix_eigenvectors(m);
    TC(vec != nullptr, "eigenvectors computed");
    TC(rows(vec) == 2 && cols(vec) == 2, "eigenvectors [n x n]");
    TPASS;
}

void test_lu() {
    TEST("LU decomposition");
    double d[] = {1, 2, 3, 4};
    void* m = mkmat(d, 2, 2);
    void* lu = flux_matrix_lu(m);
    TC(lu != nullptr, "LU computed");
    TC(rows(lu) == 2 && cols(lu) == 2, "LU matrix dimensions");
    TPASS;
}

void test_qr() {
    TEST("QR decomposition");
    double d[] = {1, 2, 3, 4, 5, 6};
    void* m = mkmat(d, 3, 2);
    void* qr = flux_matrix_qr(m);
    TC(qr != nullptr, "QR computed");
    TC(rows(qr) == 3 && cols(qr) == 2, "QR Q matrix dimensions");
    TPASS;
}

void test_svd() {
    TEST("SVD decomposition");
    double d[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    void* m = mkmat(d, 3, 3);
    void* svd = flux_matrix_svd(m);
    TC(svd != nullptr, "SVD computed");
    int r = rows(svd), c = cols(svd);
    TC(r == 6 && c == 3, "SVD packed matrix [m+n x n]");
    TPASS;
}

void test_cholesky() {
    TEST("Cholesky decomposition");
    double d[] = {5, 2, 2, 5};
    void* m = mkmat(d, 2, 2);
    void* L = flux_matrix_cholesky(m);
    TC(L != nullptr, "Cholesky computed");
    TC(rows(L) == 2 && cols(L) == 2, "Cholesky L matrix dimensions");
    TPASS;
}

void test_matrix_eye() {
    TEST("matrix_eye");
    void* e = flux_matrix_eye(3);
    auto* M = (Eigen::MatrixXd*)flux_lookup_matrix(e);
    TC(M != nullptr && M->rows() == 3, "eye created");
    TN((*M)(0,0), 1.0, 1e-15, "I[0,0]=1");
    TN((*M)(1,1), 1.0, 1e-15, "I[1,1]=1");
    TN((*M)(0,1), 0.0, 1e-15, "I[0,1]=0");
    TPASS;
}

void test_matrix_ones_zeros() {
    TEST("matrix_ones/zeros");
    void* z = flux_matrix_zeros(2, 3);
    auto* Z = (Eigen::MatrixXd*)flux_lookup_matrix(z);
    TC(Z != nullptr && Z->rows() == 2 && Z->cols() == 3, "zeros dims");
    TC(Z->sum() == 0.0, "all zeros");
    void* o = flux_matrix_ones(2, 2);
    auto* O = (Eigen::MatrixXd*)flux_lookup_matrix(o);
    TN(O->sum(), 4.0, 1e-15, "ones sum");
    TPASS;
}

void test_matrix_copy() {
    TEST("matrix_copy");
    double d[] = {1,2,3,4};
    void* m = flux_create_matrix(d, 2, 2);
    void* c = flux_matrix_copy(m);
    auto* C = (Eigen::MatrixXd*)flux_lookup_matrix(c);
    TN((*C)(0,0), 1.0, 1e-15, "copy[0,0]");
    TN((*C)(1,1), 4.0, 1e-15, "copy[1,1]");
    TPASS;
}

void test_matrix_sum_mean() {
    TEST("matrix_sum/mean");
    double d[] = {1,2,3,4};
    void* m = flux_create_matrix(d, 2, 2);
    TN(flux_matrix_sum(m), 10.0, 1e-15, "sum=10");
    TN(flux_matrix_mean(m), 2.5, 1e-15, "mean=2.5");
    TPASS;
}

void test_matrix_operations() {
    TEST("Matrix multiply and transpose");
    // Eigen column-major: {col0 row0, col0 row1, col1 row0, col1 row1}
    // A = [1 3; 2 4] stored as {1, 2, 3, 4}
    double a[] = {1, 2, 3, 4};
    double b[] = {5, 6, 7, 8};
    // A = [1 3; 2 4], B = [5 7; 6 8]
    // A*B = [1*5+3*6  1*7+3*8; 2*5+4*6  2*7+4*8] = [23 31; 34 46]
    // But stored column-major: {23, 34, 31, 46}
    void* ma = mkmat(a, 2, 2);
    void* mb = mkmat(b, 2, 2);
    void* prod = flux_matrix_mul(ma, mb);
    TC(prod != nullptr, "matrix product");
    TN(get(prod, 0, 0), 23.0, 1e-12, "prod[0,0]");
    TN(get(prod, 1, 0), 34.0, 1e-12, "prod[1,0]");
    void* tr = flux_matrix_transpose(ma);
    TN(get(tr, 0, 0), 1.0, 1e-12, "transpose[0,0]");
    TN(get(tr, 0, 1), 2.0, 1e-12, "transpose[0,1]");
    TPASS;
}

void test_matrix_inv() {
    TEST("Matrix inverse and solve");
    // A = [4 2; 7 6] stored column-major: {4, 7, 2, 6}
    // inv(A) = [0.6 -0.2; -0.7 0.4] stored column-major: {0.6, -0.7, -0.2, 0.4}
    double d[] = {4, 7, 2, 6};
    void* m = mkmat(d, 2, 2);
    void* inv = flux_matrix_inv(m);
    TC(inv != nullptr, "inverse computed");
    TN(get(inv, 0, 0), 0.6, 0.01, "inv[0,0]");
    TN(get(inv, 1, 0), -0.7, 0.01, "inv[1,0]");
    TPASS;
}

// ---------------------------------------------------------------------------
// Statistics & Probability
// ---------------------------------------------------------------------------

void test_random() {
    TEST("Random number generation");
    double r1 = flux_randn(0.0, 1.0);
    double r2 = flux_randn(0.0, 1.0);
    TC(std::isfinite(r1), "randn produces finite value");
    double u = flux_rand_uniform(0.0, 1.0);
    TC(u >= 0.0 && u <= 1.0, "uniform in [0,1]");
    TPASS;
}

void test_distributions() {
    TEST("Probability distributions");
    TN(flux_normal_pdf(0.0, 0.0, 1.0), 1.0/std::sqrt(2*M_PI), 1e-14, "normal PDF at 0");
    TN(flux_normal_cdf(0.0, 0.0, 1.0), 0.5, 1e-14, "normal CDF at 0");
    double f1 = flux_normal_cdf(1.96, 0.0, 1.0);
    TC(f1 > 0.97 && f1 < 0.976, "95% CI approx");
    TN(flux_uniform_pdf(0.5, 0.0, 1.0), 1.0, 1e-14, "uniform PDF");
    TN(flux_exponential_pdf(0.0, 1.0), 1.0, 1e-14, "exponential PDF at 0");
    TPASS;
}

void test_covariance_correlation() {
    TEST("Covariance and correlation");
    double x[] = {1, 2, 3, 4, 5};
    double y[] = {2, 4, 6, 8, 10};
    double c = flux_covariance(x, y, 5);
    TN(c, 5.0, 1e-12, "covariance of multiples");
    double r = flux_correlation(x, y, 5);
    TN(r, 1.0, 1e-12, "perfect correlation");
    double z[] = {10, 8, 6, 4, 2};
    double r2 = flux_correlation(x, z, 5);
    TN(r2, -1.0, 1e-12, "perfect negative correlation");
    TPASS;
}

void test_percentile_skew() {
    TEST("Percentile and skewness");
    double d[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    TN(flux_percentile(d, 10, 50.0), 5.5, 1e-12, "median");
    TN(flux_percentile(d, 10, 25.0), 3.25, 1e-12, "Q1");
    TN(flux_percentile(d, 10, 75.0), 7.75, 1e-12, "Q3");
    double sym[] = {1,1,1,1,1,2,2,2,2,2};
    TN(flux_skewness(sym, 10), 0.0, 1e-12, "symmetric skewness=0");
    double skew_data[] = {1,2,3,4,5,6,7,8,9,100};
    double s = flux_skewness(skew_data, 10);
    TC(s > 0, "positive skew with outlier");
    double flat[] = {5,5,5,5,5};
    TN(flux_kurtosis(flat, 5), 0.0, 1e-12, "flat has no kurtosis");
    TPASS;
}

// ---------------------------------------------------------------------------
// Numerical Methods
// ---------------------------------------------------------------------------

void test_ode() {
    TEST("ODE integrators");
    double* rk4 = flux_ode_rk4(0.0, 0.0, 0.01, 100);
    TC(rk4 != nullptr && std::isfinite(rk4[100]), "RK4 finite");
    double* eu = flux_ode_euler(0.0, 0.0, 0.01, 100);
    TC(eu != nullptr && std::isfinite(eu[100]), "Euler finite");
    TPASS;
}

void test_root_finding() {
    TEST("Root finding");
    double r1 = flux_root_bisection(0.0, 3.0, 1e-10, 100);
    TN(r1, std::sqrt(2.0), 1e-8, "bisection sqrt(2)");
    double r2 = flux_root_newton(1.5, 1e-10, 100);
    TN(r2, std::sqrt(2.0), 1e-8, "newton sqrt(2)");
    TPASS;
}

void test_integration() {
    TEST("Adaptive integration");
    TN(flux_integrate_adaptive(0.0, M_PI, 1e-6), 2.0, 0.01, "integral sin(x) 0..pi = 2");
    TPASS;
}

void test_interpolation() {
    TEST("Interpolation");
    double xp[] = {0, 1, 2, 3};
    double yp[] = {0, 10, 20, 30};
    double xq[] = {0.5, 1.5, 2.5};
    double* li = flux_interp1_linear(xq, 3, xp, yp, 4);
    TN(li[0], 5.0, 1e-12, "linear at 0.5");
    TN(li[1], 15.0, 1e-12, "linear at 1.5");
    double* sp = flux_interp1_spline(xq, 3, xp, yp, 4);
    TN(sp[0], 5.0, 1e-1, "spline at 0.5");
    TPASS;
}

// ---------------------------------------------------------------------------
// Special Functions
// ---------------------------------------------------------------------------

void test_special_functions() {
    TEST("Special functions");
    TN(flux_erf(0.0), 0.0, 1e-15, "erf(0)=0");
    TN(flux_erf(1.0), 0.8427, 1e-4, "erf(1)");
    TN(flux_erfc(0.0), 1.0, 1e-15, "erfc(0)=1");
    TN(flux_erfc(1.0), 0.1573, 1e-4, "erfc(1)");
    TN(flux_gamma(1.0), 1.0, 1e-15, "gamma(1)=1");
    TN(flux_gamma(3.0), 2.0, 1e-14, "gamma(3)=2");
    TN(flux_lgamma(1.0), 0.0, 1e-15, "lgamma(1)=0");
    TN(flux_bessel_j0(0.0), 1.0, 1e-15, "bessel_j0(0)=1");
    TN(flux_bessel_j1(0.0), 0.0, 1e-15, "bessel_j1(0)=0");
    double y0 = flux_bessel_y0(1.0);
    TC(std::isfinite(y0), "bessel_y0(1) finite");
    double y1 = flux_bessel_y1(1.0);
    TC(std::isfinite(y1), "bessel_y1(1) finite");
    TPASS;
}

// ---------------------------------------------------------------------------
// Signal Processing
// ---------------------------------------------------------------------------

void test_fft() {
    TEST("FFT");
    double sig[] = {1,2,3,4,5,6,7,8};
    void* m = flux_create_matrix(sig, 8, 1);
    void* S = flux_fft(m, 1000.0);
    TC(S != nullptr, "FFT computed");
    TC(flux_matrix_rows(S) > 0 && flux_matrix_cols(S) == 3, "FFT result [n x 3]");
    double thd = flux_fft_thd(m, 1000.0);
    TC(thd >= 0.0, "THD non-negative");
    double snr = flux_fft_snr(m, 1000.0);
    TC(std::isfinite(snr), "SNR finite");
    // matrix_trace and matrix_diag
    double d[] = {1,2,3,4};
    void* sq = flux_create_matrix(d, 2, 2);
    TN(flux_matrix_trace(sq), 5.0, 1e-15, "trace = 1+4");
    void* diag = flux_matrix_diag_extract(sq);
    TC(diag != nullptr && flux_matrix_rows(diag) == 2 && flux_matrix_cols(diag) == 1, "diag [n x 1]");
    TPASS;
}

void test_convolution() {
    TEST("Convolution");
    double x[] = {1, 2, 3}, y[] = {4, 5, 6};
    double* c = flux_conv(x, 3, y, 3);
    TN(c[0], 4.0, 1e-12, "c[0]"); TN(c[1], 13.0, 1e-12, "c[1]");
    TN(c[2], 28.0, 1e-12, "c[2]"); TN(c[3], 27.0, 1e-12, "c[3]"); TN(c[4], 18.0, 1e-12, "c[4]");
    TPASS;
}

void test_filters() {
    TEST("Filters");
    double data[] = {0, 1, 0, 1, 0, 1, 0, 1, 0};
    double* lp = flux_lowpass(data, 9, 0.3);
    TC(std::isfinite(lp[8]), "lowpass finite");
    double* hp = flux_highpass(data, 9, 0.3);
    TC(std::isfinite(hp[8]), "highpass finite");
    double md[] = {1, 2, 100, 3, 4};
    flux_median_filter(md, 5, 3);
    TC(md[2] < 50, "median_filter removed spike");
    TPASS;
}

// ---------------------------------------------------------------------------
// Polynomials
// ---------------------------------------------------------------------------

void test_polyval() {
    TEST("Polynomial evaluation");
    TN(flux_polyval(new double[3]{1,0,0}, 2, 5.0), 1.0, 1e-15, "constant");
    TN(flux_polyval(new double[3]{1,2,3}, 2, 2.0), 17.0, 1e-12, "quadratic");
    TPASS;
}

void test_polyroots() {
    TEST("Polynomial roots");
    // p(x) = x^2 - 3x + 2 = (x-1)(x-2), roots at 1 and 2
    double c[] = {2, -3, 1};
    double* r = flux_polyroots(c, 2);
    TC(r != nullptr, "roots computed");
    TC(std::isfinite(r[0]) && std::isfinite(r[1]), "roots are finite");
    TPASS;
}

void test_polyfit() {
    TEST("Polynomial fitting");
    double x[] = {1,2,3,4,5}, y[] = {1,4,9,16,25};
    double* c = flux_polyfit(x, y, 5, 2);
    TN(c[2], 1.0, 0.2, "x^2 coef"); TN(c[1], 0.0, 0.2, "x coef"); TN(c[0], 0.0, 0.2, "const");
    TPASS;
}

// ---------------------------------------------------------------------------
// Optimization
// ---------------------------------------------------------------------------

void test_least_squares() {
    TEST("Least squares");
    double A[] = {1,2,3, 1,1,1}, b[] = {3,5,7};
    double* x = flux_least_squares(A, b, 3, 2);
    TN(x[0], 2.0, 0.1, "slope"); TN(x[1], 1.0, 0.1, "intercept");
    TPASS;
}

// ============================================================================

int main() {
    std::cout << "\n  Advanced Math Test Suite                               \n";

    std::cout << "\n  -- Linear Algebra --\n\n";
    test_matrix_rank();
    test_matrix_norm();
    test_eigenvalues();
    test_eigenvectors();
    test_lu();
    test_qr();
    test_svd();
    test_cholesky();
    test_matrix_operations();
    test_matrix_inv();
    test_matrix_eye();
    test_matrix_ones_zeros();
    test_matrix_copy();
    test_matrix_sum_mean();

    std::cout << "\n  -- Statistics & Probability --\n\n";
    test_random();
    test_distributions();
    test_covariance_correlation();
    test_percentile_skew();

    std::cout << "\n  -- Numerical Methods --\n\n";
    test_ode();
    test_root_finding();
    test_integration();
    test_interpolation();

    std::cout << "\n  -- Special Functions --\n\n";
    test_special_functions();

    std::cout << "\n  -- Signal Processing --\n\n";
    test_convolution();
    test_fft();
    test_filters();

    std::cout << "\n  -- Polynomials --\n\n";
    test_polyval();
    test_polyroots();
    test_polyfit();

    std::cout << "\n  -- Optimization --\n\n";
    test_least_squares();

    std::cout << "\n  Results: " << g_passed << "/" << (g_passed + g_failed) << " passed\n\n";
    return g_failed == 0 ? 0 : 1;
}
