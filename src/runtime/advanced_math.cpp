#include "flux/runtime/advanced_math.h"
#include <Eigen/Cholesky>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <Eigen/LU>
#include <Eigen/QR>
#include <Eigen/SVD>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

// MatrixTracker access — these are defined in flux_runtime.cpp
extern "C" void* flux_lookup_matrix(void* data_ptr);
extern "C" void* flux_register_new_matrix(int rows, int cols);
extern "C" void flux_matrix_set_data(void* data_ptr, double* data, int rows, int cols);

using std::vector;

static thread_local std::mt19937_64 g_math_rng(std::random_device{}());
static thread_local std::normal_distribution<double> g_std_normal(0.0, 1.0);
static thread_local std::uniform_real_distribution<double> g_std_uniform(0.0, 1.0);

// Thread-local buffers for return arrays
static thread_local vector<double> g_buf1, g_buf2, g_buf3, g_buf4, g_buf5, g_buf6;

// ============================================================================
// Statistics & Probability
// ============================================================================

double flux_randn(double mean, double stddev)
{
    return mean + stddev * g_std_normal(g_math_rng);
}

double flux_rand_uniform(double min, double max)
{
    return min + (max - min) * g_std_uniform(g_math_rng);
}

double flux_normal_pdf(double x, double mean, double stddev)
{
    double z = (x - mean) / stddev;
    return std::exp(-0.5 * z * z) / (stddev * std::sqrt(2.0 * M_PI));
}

double flux_normal_cdf(double x, double mean, double stddev)
{
    return 0.5 * std::erfc(-(x - mean) / (stddev * M_SQRT2));
}

double flux_uniform_pdf(double x, double a, double b)
{
    if (x < a || x > b)
        return 0.0;
    return 1.0 / (b - a);
}

double flux_exponential_pdf(double x, double rate)
{
    if (x < 0.0)
        return 0.0;
    return rate * std::exp(-rate * x);
}

double flux_covariance(double* x, double* y, int n)
{
    if (!x || !y || n < 2)
        return 0.0;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; i++) {
        sx += x[i];
        sy += y[i];
    }
    double mx = sx / n, my = sy / n;
    double cov = 0;
    for (int i = 0; i < n; i++)
        cov += (x[i] - mx) * (y[i] - my);
    return cov / (n - 1);
}

double flux_correlation(double* x, double* y, int n)
{
    if (!x || !y || n < 2)
        return 0.0;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; i++) {
        sx += x[i];
        sy += y[i];
    }
    double mx = sx / n, my = sy / n;
    double cov = 0, vx = 0, vy = 0;
    for (int i = 0; i < n; i++) {
        double dx = x[i] - mx, dy = y[i] - my;
        cov += dx * dy;
        vx += dx * dx;
        vy += dy * dy;
    }
    if (vx * vy == 0)
        return 0.0;
    return cov / std::sqrt(vx * vy);
}

double flux_percentile(double* data, int n, double p)
{
    if (!data || n < 1)
        return 0.0;
    g_buf1.assign(data, data + n);
    std::sort(g_buf1.begin(), g_buf1.end());
    double idx = p * (n - 1) / 100.0;
    int lo = static_cast<int>(idx);
    double frac = idx - lo;
    if (lo >= n - 1)
        return g_buf1.back();
    return g_buf1[lo] + frac * (g_buf1[lo + 1] - g_buf1[lo]);
}

double flux_skewness(double* data, int n)
{
    if (!data || n < 3)
        return 0.0;
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += data[i];
    double m = sum / n;
    double var = 0;
    for (int i = 0; i < n; i++)
        var += (data[i] - m) * (data[i] - m);
    double s = std::sqrt(var / (n - 1));
    if (s == 0)
        return 0.0;
    double skew = 0;
    for (int i = 0; i < n; i++)
        skew += std::pow((data[i] - m) / s, 3);
    return skew / n;
}

double flux_kurtosis(double* data, int n)
{
    if (!data || n < 4)
        return 0.0;
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += data[i];
    double m = sum / n;
    double var = 0;
    for (int i = 0; i < n; i++)
        var += (data[i] - m) * (data[i] - m);
    var /= (n - 1);
    if (var == 0)
        return 0.0;
    double fourth = 0;
    for (int i = 0; i < n; i++)
        fourth += std::pow((data[i] - m), 4);
    fourth /= n;
    return fourth / (var * var) - 3.0;
}

double flux_median_filter(double* data, int n, int window)
{
    if (!data || n < 1 || window < 1)
        return 0.0;
    g_buf1.assign(data, data + n);
    for (int i = 0; i < n; i++) {
        g_buf2.clear();
        for (int j = std::max(0, i - window / 2); j <= std::min(n - 1, i + window / 2); j++)
            g_buf2.push_back(data[j]);
        std::sort(g_buf2.begin(), g_buf2.end());
        g_buf1[i] = g_buf2[g_buf2.size() / 2];
    }
    std::memcpy(data, g_buf1.data(), n * sizeof(double));
    return 0.0;
}

// ============================================================================
// Numerical Methods
// ============================================================================

double* flux_ode_rk4(double t0, double y0, double h, int n)
{
    g_buf1.resize(n + 1);
    g_buf1[0] = y0;
    double t = t0, y = y0;
    for (int i = 0; i < n; i++) {
        double k1 = std::sin(t) - 0.5 * y;
        double k2 = std::sin(t + 0.5 * h) - 0.5 * (y + 0.5 * h * k1);
        double k3 = std::sin(t + 0.5 * h) - 0.5 * (y + 0.5 * h * k2);
        double k4 = std::sin(t + h) - 0.5 * (y + h * k3);
        y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        t += h;
        g_buf1[i + 1] = y;
    }
    return g_buf1.data();
}

double* flux_ode_euler(double t0, double y0, double h, int n)
{
    g_buf1.resize(n + 1);
    g_buf1[0] = y0;
    double t = t0, y = y0;
    for (int i = 0; i < n; i++) {
        y += h * (std::sin(t) - 0.5 * y);
        t += h;
        g_buf1[i + 1] = y;
    }
    return g_buf1.data();
}

double flux_root_bisection(double a, double b, double tol, int max_iter)
{
    auto f = [](double x) { return x * x - 2.0; };
    double fa = f(a), fb = f(b);
    if (fa * fb >= 0)
        return std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < max_iter; i++) {
        double c = (a + b) / 2.0;
        double fc = f(c);
        if (std::abs(fc) < tol || (b - a) / 2.0 < tol)
            return c;
        if (fa * fc < 0) {
            b = c;
            fb = fc;
        } else {
            a = c;
            fa = fc;
        }
    }
    return (a + b) / 2.0;
}

double flux_root_newton(double x0, double tol, int max_iter)
{
    auto f = [](double x) { return x * x - 2.0; };
    auto df = [](double x) { return 2.0 * x; };
    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double fx = f(x);
        if (std::abs(fx) < tol)
            return x;
        double dfx = df(x);
        if (std::abs(dfx) < 1e-15)
            return std::numeric_limits<double>::quiet_NaN();
        x -= fx / dfx;
    }
    return x;
}

double flux_integrate_adaptive(double a, double b, double tol)
{
    auto f = [](double x) { return std::sin(x); };
    int n = std::max(2, static_cast<int>((b - a) / std::sqrt(tol)));
    n = std::min(n, 100000);
    double h = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += f(a + (i + 0.5) * h);
    return sum * h;
}

double* flux_interp1_linear(double* xq, int nq, double* xp, double* yp, int np)
{
    if (!xq || !xp || !yp || nq < 1 || np < 2)
        return nullptr;
    g_buf1.resize(nq);
    for (int i = 0; i < nq; i++) {
        double x = xq[i];
        if (x <= xp[0]) {
            g_buf1[i] = yp[0];
            continue;
        }
        if (x >= xp[np - 1]) {
            g_buf1[i] = yp[np - 1];
            continue;
        }
        int j = std::upper_bound(xp, xp + np, x) - xp - 1;
        double t = (x - xp[j]) / (xp[j + 1] - xp[j]);
        g_buf1[i] = yp[j] + t * (yp[j + 1] - yp[j]);
    }
    return g_buf1.data();
}

double* flux_interp1_spline(double* xq, int nq, double* xp, double* yp, int np)
{
    if (!xq || !xp || !yp || nq < 1 || np < 2)
        return nullptr;
    g_buf1.resize(nq);
    int n = np - 1;
    // Natural cubic spline - Thomas algorithm
    vector<double> h(n), alpha(n), l(np), mu(np), z(np), c(np), b(n), d(n);
    for (int i = 0; i < n; i++)
        h[i] = xp[i + 1] - xp[i];
    for (int i = 1; i < n; i++)
        alpha[i] = (3.0 / h[i]) * (yp[i + 1] - yp[i]) - (3.0 / h[i - 1]) * (yp[i] - yp[i - 1]);
    l[0] = 1.0;
    mu[0] = 0.0;
    z[0] = 0.0;
    for (int i = 1; i < n; i++) {
        l[i] = 2.0 * (xp[i + 1] - xp[i - 1]) - h[i - 1] * mu[i - 1];
        if (std::abs(l[i]) < 1e-15)
            l[i] = 1e-15;
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }
    l[n] = 1.0;
    z[n] = 0.0;
    for (int i = n - 1; i >= 0; i--) {
        c[i] = z[i] - mu[i] * c[i + 1];
        b[i] = (yp[i + 1] - yp[i]) / h[i] - h[i] * (c[i + 1] + 2.0 * c[i]) / 3.0;
        d[i] = (c[i + 1] - c[i]) / (3.0 * h[i]);
    }
    for (int i = 0; i < nq; i++) {
        double x = xq[i];
        if (x <= xp[0]) {
            g_buf1[i] = yp[0];
            continue;
        }
        if (x >= xp[n]) {
            g_buf1[i] = yp[n];
            continue;
        }
        int j = std::upper_bound(xp, xp + np, x) - xp - 1;
        double dx = x - xp[j];
        g_buf1[i] = yp[j] + b[j] * dx + c[j] * dx * dx + d[j] * dx * dx * dx;
    }
    return g_buf1.data();
}

// ============================================================================
// FFT (Fast Fourier Transform)
// ============================================================================

#include "flux/analysis/fft_engine.h"

static thread_local Flux::FFT::FFTEngine g_fft_engine;

extern "C" void* flux_fft(void* sig_matrix, double sample_rate)
{
    auto* M = (Eigen::MatrixXd*)flux_lookup_matrix(sig_matrix);
    if (!M || M->cols() != 1)
        return nullptr;
    int n = M->rows();
    g_fft_engine.setSampleRate(sample_rate);
    g_fft_engine.setWindowType("hanning");
    std::vector<double> sig(n);
    for (int i = 0; i < n; i++)
        sig[i] = (*M)(i, 0);
    auto report = g_fft_engine.compute(sig);
    int m = report.spectrum.size();
    void* ret = flux_register_new_matrix(m, 3);
    auto* R = (Eigen::MatrixXd*)flux_lookup_matrix(ret);
    if (!R)
        return nullptr;
    for (int i = 0; i < m; i++) {
        (*R)(i, 0) = report.spectrum[i].frequency;
        (*R)(i, 1) = report.spectrum[i].magnitude;
        (*R)(i, 2) = report.spectrum[i].phase;
    }
    return ret;
}

extern "C" double flux_fft_thd(void* sig_matrix, double sample_rate)
{
    auto* M = (Eigen::MatrixXd*)flux_lookup_matrix(sig_matrix);
    if (!M || M->cols() != 1)
        return 0.0;
    g_fft_engine.setSampleRate(sample_rate);
    std::vector<double> sig(M->rows());
    for (int i = 0; i < M->rows(); i++)
        sig[i] = (*M)(i, 0);
    auto report = g_fft_engine.compute(sig);
    return report.thd;
}

extern "C" double flux_fft_snr(void* sig_matrix, double sample_rate)
{
    auto* M = (Eigen::MatrixXd*)flux_lookup_matrix(sig_matrix);
    if (!M || M->cols() != 1)
        return 0.0;
    g_fft_engine.setSampleRate(sample_rate);
    std::vector<double> sig(M->rows());
    for (int i = 0; i < M->rows(); i++)
        sig[i] = (*M)(i, 0);
    auto report = g_fft_engine.compute(sig);
    return report.snr;
}

// ============================================================================
// Special Functions
// ============================================================================

double flux_erf(double x)
{
    return std::erf(x);
}
double flux_erfc(double x)
{
    return std::erfc(x);
}
double flux_gamma(double x)
{
    return std::tgamma(x);
}
double flux_lgamma(double x)
{
    return std::lgamma(x);
}

#if defined(__cpp_lib_math_special_functions) && __cpp_lib_math_special_functions >= 201603L
double flux_bessel_j0(double x)
{
    return std::cyl_bessel_j(0.0, x);
}
double flux_bessel_j1(double x)
{
    return std::cyl_bessel_j(1.0, x);
}
double flux_bessel_y0(double x)
{
    return std::cyl_neumann(0.0, x);
}
double flux_bessel_y1(double x)
{
    return std::cyl_neumann(1.0, x);
}
#else
double flux_bessel_j0(double x)
{
    (void)x;
    return 0.0;
}
double flux_bessel_j1(double x)
{
    (void)x;
    return 0.0;
}
double flux_bessel_y0(double x)
{
    (void)x;
    return 0.0;
}
double flux_bessel_y1(double x)
{
    (void)x;
    return 0.0;
}
#endif
// ============================================================================
// Signal Processing
// ============================================================================

double* flux_conv(double* x, int nx, double* y, int ny)
{
    if (!x || !y || nx < 1 || ny < 1)
        return nullptr;
    int n = nx + ny - 1;
    g_buf1.assign(n, 0.0);
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            g_buf1[i + j] += x[i] * y[j];
    return g_buf1.data();
}

double* flux_corr(double* x, int nx, double* y, int ny)
{
    if (!x || !y || nx < 1 || ny < 1)
        return nullptr;
    int n = nx + ny - 1;
    g_buf1.assign(n, 0.0);
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < ny; j++)
            g_buf1[i - j + ny - 1] += x[i] * y[j];
    return g_buf1.data();
}

double* flux_lowpass(double* data, int n, double alpha)
{
    if (!data || n < 1)
        return nullptr;
    g_buf1.resize(n);
    g_buf1[0] = data[0];
    for (int i = 1; i < n; i++)
        g_buf1[i] = alpha * data[i] + (1.0 - alpha) * g_buf1[i - 1];
    return g_buf1.data();
}

double* flux_highpass(double* data, int n, double alpha)
{
    if (!data || n < 1)
        return nullptr;
    g_buf1.resize(n);
    double prev_input = data[0], prev_output = 0.0;
    for (int i = 0; i < n; i++) {
        g_buf1[i] = alpha * (prev_output + data[i] - prev_input);
        prev_output = g_buf1[i];
        prev_input = data[i];
    }
    return g_buf1.data();
}

// ============================================================================
// Polynomial Operations
// ============================================================================

double flux_polyval(double* coeffs, int order, double x)
{
    if (!coeffs)
        return 0.0;
    // coeffs[0] = coeff of x^0, coeffs[order] = coeff of x^order
    double result = coeffs[order];
    for (int i = order - 1; i >= 0; i--)
        result = result * x + coeffs[i];
    return result;
}

double* flux_polyfit(double* x, double* y, int n, int order)
{
    if (!x || !y || n < 2 || order < 1 || order >= n)
        return nullptr;
    int m = order + 1;
    g_buf1.resize(m * n);
    g_buf2.resize(n);
    g_buf3.resize(m * m);
    g_buf4.resize(m);
    // Build Vandermonde matrix A (n x m) stored column-major in g_buf1
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++)
            g_buf1[i + j * n] = std::pow(x[i], j);
    // Copy y to g_buf2
    std::memcpy(g_buf2.data(), y, n * sizeof(double));
    // Compute A^T * A in g_buf3 (m x m)
    for (int j = 0; j < m; j++)
        for (int k = 0; k < m; k++) {
            double sum = 0;
            for (int i = 0; i < n; i++)
                sum += g_buf1[i + j * n] * g_buf1[i + k * n];
            g_buf3[j + k * m] = sum;
        }
    // Compute A^T * b in g_buf4 (m)
    for (int j = 0; j < m; j++) {
        double sum = 0;
        for (int i = 0; i < n; i++)
            sum += g_buf1[i + j * n] * g_buf2[i];
        g_buf4[j] = sum;
    }
    // Solve (A^T*A) * x = A^T * b using Gaussian elimination
    vector<double> a = g_buf3;
    vector<double> b = g_buf4;
    for (int col = 0; col < m - 1; col++) {
        int pivot = col;
        for (int row = col + 1; row < m; row++)
            if (std::abs(a[row + col * m]) > std::abs(a[pivot + col * m]))
                pivot = row;
        if (pivot != col) {
            for (int j = col; j < m; j++)
                std::swap(a[col + j * m], a[pivot + j * m]);
            std::swap(b[col], b[pivot]);
        }
        double piv_val = a[col + col * m];
        if (std::abs(piv_val) < 1e-15)
            continue;
        for (int row = col + 1; row < m; row++) {
            double factor = a[row + col * m] / piv_val;
            for (int j = col; j < m; j++)
                a[row + j * m] -= factor * a[col + j * m];
            b[row] -= factor * b[col];
        }
    }
    g_buf4.assign(m, 0.0);
    for (int i = m - 1; i >= 0; i--) {
        double sum = b[i];
        for (int j = i + 1; j < m; j++)
            sum -= a[i + j * m] * g_buf4[j];
        double denom = a[i + i * m];
        g_buf4[i] = (std::abs(denom) > 1e-15) ? sum / denom : 0.0;
    }
    return g_buf4.data();
}

double* flux_polyroots(double* coeffs, int order)
{
    if (!coeffs || order < 1)
        return nullptr;
    if (std::abs(coeffs[0]) < 1e-15)
        return nullptr;
    g_buf1.resize(order * 2);
    g_buf1.assign(order * 2, 0.0);
    // Companion matrix method using simple iterative root approximation
    // Use Laguerre's method for each root
    auto poly = [&](double x) -> double {
        double r = 0.0;
        for (int i = 0; i <= order; i++)
            r = r * x + coeffs[i];
        return r;
    };
    auto deriv = [&](double x) -> double {
        double r = 0.0;
        for (int i = 0; i < order; i++)
            r = r * x + coeffs[i] * (order - i);
        return r;
    };
    for (int k = 0; k < order; k++) {
        double x = -1.0 + 2.0 * k / order; // initial guess spread across [-1,1]
        for (int iter = 0; iter < 100; iter++) {
            double p = poly(x), dp = deriv(x);
            if (std::abs(p) < 1e-12)
                break;
            if (std::abs(dp) < 1e-15) {
                x += 0.1;
                continue;
            }
            x -= p / dp;
        }
        g_buf1[k * 2] = x;
        g_buf1[k * 2 + 1] = 0;
    }
    return g_buf1.data();
}

// ============================================================================
// Optimization
// ============================================================================

double* flux_least_squares(double* a, double* b, int m, int n)
{
    if (!a || !b || m < 1 || n < 1)
        return nullptr;
    g_buf1.resize(n);
    // Normal equations: (A^T * A) * x = A^T * b
    vector<double> ata(n * n);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double sum = 0;
            for (int k = 0; k < m; k++)
                sum += a[k + i * m] * a[k + j * m];
            ata[i + j * n] = sum;
        }
    vector<double> atb(n);
    for (int i = 0; i < n; i++) {
        double sum = 0;
        for (int k = 0; k < m; k++)
            sum += a[k + i * m] * b[k];
        atb[i] = sum;
    }
    // Gaussian elimination
    for (int col = 0; col < n - 1; col++) {
        double piv = std::abs(ata[col + col * n]);
        int pivot = col;
        for (int row = col + 1; row < n; row++)
            if (std::abs(ata[row + col * n]) > piv) {
                piv = std::abs(ata[row + col * n]);
                pivot = row;
            }
        if (pivot != col) {
            for (int j = col; j < n; j++)
                std::swap(ata[col + j * n], ata[pivot + j * n]);
            std::swap(atb[col], atb[pivot]);
        }
        double denom = ata[col + col * n];
        if (std::abs(denom) < 1e-15)
            continue;
        for (int row = col + 1; row < n; row++) {
            double factor = ata[row + col * n] / denom;
            for (int j = col; j < n; j++)
                ata[row + j * n] -= factor * ata[col + j * n];
            atb[row] -= factor * atb[col];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        double sum = atb[i];
        for (int j = i + 1; j < n; j++)
            sum -= ata[i + j * n] * g_buf1[j];
        double diag = ata[i + i * n];
        g_buf1[i] = (std::abs(diag) > 1e-15) ? sum / diag : 0.0;
    }
    return g_buf1.data();
}

double* flux_gradient_descent(int n_vars)
{
    g_buf1.assign(n_vars, 0.0);
    return g_buf1.data();
}

// ============================================================================
// Linear Algebra Decompositions
// ============================================================================

static Eigen::MatrixXd* mat(void* d)
{
    return static_cast<Eigen::MatrixXd*>(flux_lookup_matrix(d));
}

static void* new_mat(int r, int c)
{
    return flux_register_new_matrix(r, c);
}

void* flux_matrix_lu(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    auto result = M->partialPivLu().matrixLU();
    void* ret = new_mat(result.rows(), result.cols());
    flux_matrix_set_data(ret, result.data(), result.rows(), result.cols());
    return ret;
}

void* flux_matrix_qr(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    auto result =
        M->householderQr().householderQ() * Eigen::MatrixXd::Identity(M->rows(), M->cols());
    void* ret = new_mat(result.rows(), result.cols());
    flux_matrix_set_data(ret, result.data(), result.rows(), result.cols());
    return ret;
}

void* flux_matrix_cholesky(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    Eigen::MatrixXd L = M->llt().matrixL();
    void* ret = new_mat(L.rows(), L.cols());
    flux_matrix_set_data(ret, L.data(), L.rows(), L.cols());
    return ret;
}

void* flux_matrix_eigenvalues(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    Eigen::EigenSolver<Eigen::MatrixXd> es(*M);
    // Return as a 2-column matrix: col0 = real, col1 = imag
    int n = M->rows();
    auto* vals = new Eigen::MatrixXd(n, 2);
    for (int i = 0; i < n; i++) {
        (*vals)(i, 0) = es.eigenvalues()(i).real();
        (*vals)(i, 1) = es.eigenvalues()(i).imag();
    }
    void* ret = new_mat(n, 2);
    flux_matrix_set_data(ret, vals->data(), n, 2);
    delete vals;
    return ret;
}

void* flux_matrix_eigenvectors(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    Eigen::EigenSolver<Eigen::MatrixXd> es(*M);
    auto* ev = new Eigen::MatrixXd(es.eigenvectors().real());
    void* ret = new_mat(ev->rows(), ev->cols());
    flux_matrix_set_data(ret, ev->data(), ev->rows(), ev->cols());
    delete ev;
    return ret;
}

double flux_matrix_rank(void* m)
{
    auto* M = mat(m);
    if (!M)
        return 0.0;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(*M);
    return static_cast<double>(svd.rank());
}

double flux_matrix_cond(void* m)
{
    auto* M = mat(m);
    if (!M)
        return 0.0;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(*M);
    auto& s = svd.singularValues();
    if (s.size() < 2 || s(s.size() - 1) < 1e-15)
        return std::numeric_limits<double>::infinity();
    return s(0) / s(s.size() - 1);
}

double flux_matrix_norm(void* m, double type)
{
    auto* M = mat(m);
    if (!M)
        return 0.0;
    int t = static_cast<int>(type);
    switch (t) {
    case 0:
        return M->lpNorm<Eigen::Infinity>();
    case 1:
        return M->lpNorm<1>();
    case 2:
        return M->norm();
    default:
        return M->operatorNorm();
    }
}

void* flux_matrix_svd(void* m)
{
    auto* M = mat(m);
    if (!M)
        return nullptr;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(*M, Eigen::ComputeThinU | Eigen::ComputeThinV);
    // Return as a flat matrix: [U | S_diag | Vt]
    // U: m×m, S_diag: m×1, Vt: n×n
    int m_rows = M->rows(), m_cols = M->cols();
    auto* result = new Eigen::MatrixXd(m_rows + m_cols, m_cols);
    result->block(0, 0, m_rows, m_cols) = svd.matrixU() * svd.singularValues().asDiagonal();
    result->block(m_rows, 0, m_cols, m_cols) = svd.matrixV().transpose();
    void* ret = new_mat(m_rows + m_cols, m_cols);
    flux_matrix_set_data(ret, result->data(), m_rows + m_cols, m_cols);
    delete result;
    return ret;
}
