#ifndef FLUX_ADVANCED_MATH_H
#define FLUX_ADVANCED_MATH_H

#include <cstddef>

extern "C" {

// ============================================================================
// Linear Algebra Decompositions (Eigen-based)
// ============================================================================
void* flux_matrix_lu(void* m);
void* flux_matrix_qr(void* m);
void* flux_matrix_svd(void* m);
void* flux_matrix_cholesky(void* m);
void* flux_matrix_eigenvalues(void* m);
void* flux_matrix_eigenvectors(void* m);
double flux_matrix_rank(void* m);
double flux_matrix_cond(void* m);
double flux_matrix_norm(void* m, double type);

// ============================================================================
// Statistics & Probability
// ============================================================================
double flux_randn(double mean, double stddev);
double flux_rand_uniform(double min, double max);
double flux_normal_pdf(double x, double mean, double stddev);
double flux_normal_cdf(double x, double mean, double stddev);
double flux_uniform_pdf(double x, double a, double b);
double flux_exponential_pdf(double x, double rate);
double flux_covariance(double* x, double* y, int n);
double flux_correlation(double* x, double* y, int n);
double flux_percentile(double* data, int n, double p);
double flux_skewness(double* data, int n);
double flux_kurtosis(double* data, int n);
double flux_median_filter(double* data, int n, int window);

// ============================================================================
// Numerical Methods
// ============================================================================
double* flux_ode_rk4(double t0, double y0, double h, int n);
double* flux_ode_euler(double t0, double y0, double h, int n);
double flux_root_bisection(double a, double b, double tol, int max_iter);
double flux_root_newton(double x0, double tol, int max_iter);
double flux_integrate_adaptive(double a, double b, double tol);
double* flux_interp1_linear(double* xq, int nq, double* xp, double* yp, int np);
double* flux_interp1_spline(double* xq, int nq, double* xp, double* yp, int np);

// ============================================================================
// Special Functions
// ============================================================================
double flux_erf(double x);
double flux_erfc(double x);
double flux_gamma(double x);
double flux_lgamma(double x);
double flux_bessel_j0(double x);
double flux_bessel_j1(double x);
double flux_bessel_y0(double x);
double flux_bessel_y1(double x);

// ============================================================================
// FFT (Fast Fourier Transform)
// ============================================================================
void* flux_fft(void* sig_matrix, double sample_rate);
double flux_fft_thd(void* sig_matrix, double sample_rate);
double flux_fft_snr(void* sig_matrix, double sample_rate);

// ============================================================================
// Signal Processing
// ============================================================================
double* flux_conv(double* x, int nx, double* y, int ny);
double* flux_corr(double* x, int nx, double* y, int ny);
double* flux_lowpass(double* data, int n, double alpha);
double* flux_highpass(double* data, int n, double alpha);

// ============================================================================
// Polynomial Operations
// ============================================================================
double flux_polyval(double* coeffs, int order, double x);
double* flux_polyfit(double* x, double* y, int n, int order);
double* flux_polyroots(double* coeffs, int order);

// ============================================================================
// Optimization
// ============================================================================
double* flux_least_squares(double* a, double* b, int m, int n);
double* flux_gradient_descent(int n_vars);

}

#endif
