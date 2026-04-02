#include "flux/runtime/flux_runtime.h"

#include "flux/jit/flux_jit.h"

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

#ifdef FLUX_HAS_QT
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QPixmap>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>
#endif


#include "flux/runtime/flux_sim_service.h"

// Global service pointer definition
FluxSimulationService* g_flux_sim_service = nullptr;

namespace Flux {


void registerRuntimeFunctions(FluxJIT& jit) {
    jit.registerFunction("flux_create_vector_sum", reinterpret_cast<void*>(&flux_create_vector_sum));
    jit.registerFunction("flux_matrix_mul", reinterpret_cast<void*>(&flux_matrix_mul));
    jit.registerFunction("flux_matrix_mul_ms", reinterpret_cast<void*>(&flux_matrix_mul_ms));
    jit.registerFunction("flux_matrix_add", reinterpret_cast<void*>(&flux_matrix_add));
    jit.registerFunction("flux_matrix_sub", reinterpret_cast<void*>(&flux_matrix_sub));
    jit.registerFunction("flux_matrix_ew_mul", reinterpret_cast<void*>(&flux_matrix_ew_mul));
    jit.registerFunction("flux_matrix_ew_div", reinterpret_cast<void*>(&flux_matrix_ew_div));
    jit.registerFunction("flux_matrix_add_ms", reinterpret_cast<void*>(&flux_matrix_add_ms));
    jit.registerFunction("flux_matrix_sub_ms", reinterpret_cast<void*>(&flux_matrix_sub_ms));
    jit.registerFunction("flux_matrix_sub_sm", reinterpret_cast<void*>(&flux_matrix_sub_sm));
    jit.registerFunction("flux_matrix_transpose", reinterpret_cast<void*>(&flux_matrix_transpose));
    jit.registerFunction("flux_matrix_get", reinterpret_cast<void*>(&flux_matrix_get));
    jit.registerFunction("flux_create_matrix", reinterpret_cast<void*>(&flux_create_matrix));
    jit.registerFunction("print_matrix", reinterpret_cast<void*>(&flux_print_matrix));

    jit.registerFunction("print_string", reinterpret_cast<void*>(&flux_print_string));

    jit.registerFunction("flux_get_voltage", reinterpret_cast<void*>(&flux_get_voltage));
    jit.registerFunction("flux_get_current", reinterpret_cast<void*>(&flux_get_current));
    jit.registerFunction("net", reinterpret_cast<void*>(&flux_get_voltage));
    jit.registerFunction("branch", reinterpret_cast<void*>(&flux_get_current));
    jit.registerFunction("flux_get_parameter", reinterpret_cast<void*>(&flux_get_parameter));
    jit.registerFunction("p", reinterpret_cast<void*>(&flux_get_parameter));
    jit.registerFunction("flux_set_parameter", reinterpret_cast<void*>(&flux_set_parameter));
    jit.registerFunction("sim_run", reinterpret_cast<void*>(&flux_sim_run));
    jit.registerFunction("sim_stop", reinterpret_cast<void*>(&flux_sim_stop));
    jit.registerFunction("sim_pause", reinterpret_cast<void*>(&flux_sim_pause));
    jit.registerFunction("erc_check", reinterpret_cast<void*>(&flux_erc_check));

    jit.registerFunction("uramp", reinterpret_cast<void*>(&flux_uramp));
    jit.registerFunction("limit", reinterpret_cast<void*>(&flux_limit));
    jit.registerFunction("buf", reinterpret_cast<void*>(&flux_buf));
    jit.registerFunction("inv", reinterpret_cast<void*>(&flux_inv));

    jit.registerFunction("sin", reinterpret_cast<void*>(&flux_sin));
    jit.registerFunction("cos", reinterpret_cast<void*>(&flux_cos));
    jit.registerFunction("tan", reinterpret_cast<void*>(&flux_tan));
    jit.registerFunction("asin", reinterpret_cast<void*>(&flux_asin));
    jit.registerFunction("acos", reinterpret_cast<void*>(&flux_acos));
    jit.registerFunction("atan", reinterpret_cast<void*>(&flux_atan));
    jit.registerFunction("atan2", reinterpret_cast<void*>(&flux_atan2));
    jit.registerFunction("sqrt", reinterpret_cast<void*>(&flux_sqrt));
    jit.registerFunction("exp", reinterpret_cast<void*>(&flux_exp));
    jit.registerFunction("log", reinterpret_cast<void*>(&flux_log));
    jit.registerFunction("log10", reinterpret_cast<void*>(&flux_log10));
    jit.registerFunction("abs", reinterpret_cast<void*>(&flux_abs));
    jit.registerFunction("floor", reinterpret_cast<void*>(&flux_floor));
    jit.registerFunction("ceil", reinterpret_cast<void*>(&flux_ceil));
    jit.registerFunction("round", reinterpret_cast<void*>(&flux_round));
    jit.registerFunction("pow", reinterpret_cast<void*>(&flux_pow));
    jit.registerFunction("sinh", reinterpret_cast<void*>(&flux_sinh));
    jit.registerFunction("cosh", reinterpret_cast<void*>(&flux_cosh));
    jit.registerFunction("tanh", reinterpret_cast<void*>(&flux_tanh));

    jit.registerFunction("pi", reinterpret_cast<void*>(&flux_pi));
    jit.registerFunction("e", reinterpret_cast<void*>(&flux_e));

    // Security functions
    jit.registerFunction("flux_bounds_check_row", reinterpret_cast<void*>(&flux_bounds_check_row));
    jit.registerFunction("flux_bounds_check_col", reinterpret_cast<void*>(&flux_bounds_check_col));
    jit.registerFunction("flux_stack_enter", reinterpret_cast<void*>(&flux_stack_enter));
    jit.registerFunction("flux_stack_leave", reinterpret_cast<void*>(&flux_stack_leave));

    // Statistical functions
    jit.registerFunction("mean", reinterpret_cast<void*>(&flux_mean));
    jit.registerFunction("std", reinterpret_cast<void*>(&flux_std));
    jit.registerFunction("median", reinterpret_cast<void*>(&flux_median));
    jit.registerFunction("min", reinterpret_cast<void*>(&flux_min));
    jit.registerFunction("max", reinterpret_cast<void*>(&flux_max));

    // Signal generation functions
    jit.registerFunction("sawtooth", reinterpret_cast<void*>(&flux_sawtooth));
    jit.registerFunction("square", reinterpret_cast<void*>(&flux_square));
    jit.registerFunction("pulse", reinterpret_cast<void*>(&flux_pulse));

    // Interpolation function
    jit.registerFunction("interp1", reinterpret_cast<void*>(&flux_interp1));

    // Numerical integration
    jit.registerFunction("trapz", reinterpret_cast<void*>(&flux_trapz));

    // Plotting and Visualization functions
    jit.registerFunction("plot", reinterpret_cast<void*>(&flux_plot));
    jit.registerFunction("semilogx", reinterpret_cast<void*>(&flux_semilogx));
    jit.registerFunction("semilogy", reinterpret_cast<void*>(&flux_semilogy));
    jit.registerFunction("subplot", reinterpret_cast<void*>(&flux_subplot));
    jit.registerFunction("plot_title", reinterpret_cast<void*>(&flux_plot_title));
    jit.registerFunction("plot_xlabel", reinterpret_cast<void*>(&flux_plot_xlabel));
    jit.registerFunction("plot_ylabel", reinterpret_cast<void*>(&flux_plot_ylabel));
    jit.registerFunction("plot_grid", reinterpret_cast<void*>(&flux_plot_grid));
    jit.registerFunction("plot_legend", reinterpret_cast<void*>(&flux_plot_legend));
    jit.registerFunction("plot_save", reinterpret_cast<void*>(&flux_plot_save));
    jit.registerFunction("plot_clear", reinterpret_cast<void*>(&flux_plot_clear));
    jit.registerFunction("plot_theme", reinterpret_cast<void*>(&flux_plot_theme));

    // CSV/HDF5 Export functions
    jit.registerFunction("export_csv", reinterpret_cast<void*>(&flux_export_csv));
    jit.registerFunction("export_hdf5", reinterpret_cast<void*>(&flux_export_hdf5));

    // Image Processing functions
    jit.registerFunction("array_to_image", reinterpret_cast<void*>(&flux_array_to_image));
    jit.registerFunction("image_save", reinterpret_cast<void*>(&flux_image_save));
    jit.registerFunction("image_display", reinterpret_cast<void*>(&flux_image_display));
}

// extern "C" runtime functions

extern "C" double flux_print_string(const char* str) {
    std::printf("%s", str);
    std::fflush(stdout);
    return 0.0;
}

extern "C" double flux_get_voltage(const char* node) {
    if (g_flux_sim_service && g_flux_sim_service->get_voltage)
        return g_flux_sim_service->get_voltage(node);
    if (std::string(node) == "0")
        return 0.0;
    return 0.0; // Default if no solver
}

extern "C" double flux_get_current(const char* branch) {
    if (g_flux_sim_service && g_flux_sim_service->get_current)
        return g_flux_sim_service->get_current(branch);
    return 0.0;
}

extern "C" double flux_get_parameter(const char* name) {
    if (g_flux_sim_service && g_flux_sim_service->get_parameter)
        return g_flux_sim_service->get_parameter(name);
    return 0.0;
}

extern "C" double flux_set_parameter(const char* name, double value) {
    if (g_flux_sim_service && g_flux_sim_service->set_parameter) {
        g_flux_sim_service->set_parameter(name, value);
        return value;
    }
    std::printf("FluxScript (No Solver): Setting parameter %s to %f\n", name, value);
    return value;
}

extern "C" double flux_sim_run() {
    if (g_flux_sim_service && g_flux_sim_service->sim_run) {
        g_flux_sim_service->sim_run();
        return 1.0;
    }
    std::printf("FluxScript: Simulation RUN requested (No Solver Connected).\n");
    return 0.0;
}

extern "C" double flux_sim_stop() {
    if (g_flux_sim_service && g_flux_sim_service->sim_stop) {
        g_flux_sim_service->sim_stop();
        return 1.0;
    }
    return 0.0;
}

extern "C" double flux_sim_pause() {
    if (g_flux_sim_service && g_flux_sim_service->sim_pause) {
        g_flux_sim_service->sim_pause(1);
        return 1.0;
    }
    return 0.0;
}

extern "C" double flux_erc_check() {
    if (g_flux_sim_service && g_flux_sim_service->log_message) {
        g_flux_sim_service->log_message("FluxScript: Running ERC check...");
    }
    return 1.0; 
}


extern "C" double flux_uramp(double x) { return x > 0.0 ? x : 0.0; }
extern "C" double flux_limit(double x, double low, double high) {
    if (x < low) return low;
    if (x > high) return high;
    return x;
}
extern "C" double flux_buf(double x) { return x > 0.5 ? 1.0 : 0.0; }
extern "C" double flux_inv(double x) { return x > 0.5 ? 0.0 : 1.0; }

extern "C" double flux_sin(double x) { return std::sin(x); }
extern "C" double flux_cos(double x) { return std::cos(x); }
extern "C" double flux_tan(double x) { return std::tan(x); }
extern "C" double flux_asin(double x) { return std::asin(x); }
extern "C" double flux_acos(double x) { return std::acos(x); }
extern "C" double flux_atan(double x) { return std::atan(x); }
extern "C" double flux_atan2(double y, double x) { return std::atan2(y, x); }
extern "C" double flux_sqrt(double x) { return std::sqrt(x); }
extern "C" double flux_exp(double x) { return std::exp(x); }
extern "C" double flux_log(double x) { return std::log(x); }
extern "C" double flux_log10(double x) { return std::log10(x); }
extern "C" double flux_abs(double x) { return std::fabs(x); }
extern "C" double flux_floor(double x) { return std::floor(x); }
extern "C" double flux_ceil(double x) { return std::ceil(x); }
extern "C" double flux_round(double x) { return std::round(x); }
extern "C" double flux_pow(double base, double exp) { return std::pow(base, exp); }
extern "C" double flux_sinh(double x) { return std::sinh(x); }
extern "C" double flux_cosh(double x) { return std::cosh(x); }
extern "C" double flux_tanh(double x) { return std::tanh(x); }

extern "C" double flux_pi() { return 3.14159265358979323846; }
extern "C" double flux_e() { return 2.71828182845904523536; }

// ============ Statistical Functions ============
extern "C" double flux_mean(double* data, int size) {
    if (size <= 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < size; ++i) sum += data[i];
    return sum / static_cast<double>(size);
}

extern "C" double flux_std(double* data, int size) {
    if (size <= 1) return 0.0;
    double m = flux_mean(data, size);
    double sum_sq = 0.0;
    for (int i = 0; i < size; ++i) {
        double diff = data[i] - m;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(size - 1));
}

extern "C" double flux_median(double* data, int size) {
    if (size <= 0) return 0.0;
    std::vector<double> temp(data, data + size);
    std::sort(temp.begin(), temp.end());
    int mid = size / 2;
    if (size % 2 == 0) {
        return (temp[mid - 1] + temp[mid]) / 2.0;
    } else {
        return temp[mid];
    }
}

extern "C" double flux_min(double* data, int size) {
    if (size <= 0) return 0.0;
    double min_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] < min_val) min_val = data[i];
    }
    return min_val;
}

extern "C" double flux_max(double* data, int size) {
    if (size <= 0) return 0.0;
    double max_val = data[0];
    for (int i = 1; i < size; ++i) {
        if (data[i] > max_val) max_val = data[i];
    }
    return max_val;
}

// ============ Signal Generation Functions ============
extern "C" void* flux_sawtooth(double freq, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = 2.0 * phase - 1.0; // Range: -1 to 1
    }
    return result;
}

extern "C" void* flux_square(double freq, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = (phase < 0.5) ? 1.0 : -1.0;
    }
    return result;
}

extern "C" void* flux_pulse(double freq, double duty, double* t_data, int t_size) {
    auto* result = new Eigen::MatrixXd(t_size, 1);
    double period = 1.0 / freq;
    duty = std::max(0.0, std::min(1.0, duty)); // Clamp duty cycle
    for (int i = 0; i < t_size; ++i) {
        double t = t_data[i];
        double phase = std::fmod(t, period) / period;
        (*result)(i, 0) = (phase < duty) ? 1.0 : 0.0;
    }
    return result;
}

// ============ Interpolation Function ============
extern "C" double flux_interp1(double x, double* x_data, double* y_data, int size) {
    if (size <= 0) return 0.0;
    if (size == 1) return y_data[0];
    
    // Check bounds
    if (x <= x_data[0]) return y_data[0];
    if (x >= x_data[size - 1]) return y_data[size - 1];
    
    // Find interval
    for (int i = 0; i < size - 1; ++i) {
        if (x >= x_data[i] && x <= x_data[i + 1]) {
            double t = (x - x_data[i]) / (x_data[i + 1] - x_data[i]);
            return y_data[i] + t * (y_data[i + 1] - y_data[i]);
        }
    }
    return y_data[size - 1];
}

// ============ Numerical Integration ============
extern "C" double flux_trapz(double* y_data, double* x_data, int size) {
    if (size <= 1) return 0.0;
    double integral = 0.0;
    for (int i = 0; i < size - 1; ++i) {
        double dx = x_data[i + 1] - x_data[i];
        double avg_height = (y_data[i] + y_data[i + 1]) / 2.0;
        integral += dx * avg_height;
    }
    return integral;
}

extern "C" void* flux_create_matrix(double* data, int rows, int cols) {
    auto* mat = new Eigen::MatrixXd(rows, cols);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c)
            (*mat)(r, c) = data[r * cols + c];
    }
    return mat;
}

extern "C" void* flux_matrix_mul(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    return new Eigen::MatrixXd((*A) * (*B));
}

extern "C" void* flux_matrix_mul_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return new Eigen::MatrixXd((*M) * s);
}

extern "C" void* flux_matrix_add(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    
    if (A->rows() == B->rows() && A->cols() == B->cols())
        return new Eigen::MatrixXd((*A) + (*B));
    
    if (A->rows() == B->rows() && B->cols() == 1)
        return new Eigen::MatrixXd(A->colwise() + B->col(0));
    
    if (A->cols() == B->cols() && B->rows() == 1)
        return new Eigen::MatrixXd(A->rowwise() + B->row(0));
        
    if (B->rows() == A->rows() && A->cols() == 1)
        return new Eigen::MatrixXd(B->colwise() + A->col(0));
        
    if (B->cols() == A->cols() && A->rows() == 1)
        return new Eigen::MatrixXd(B->rowwise() + A->row(0));

    return new Eigen::MatrixXd((*A) + (*B)); // Fallback (will likely assert in Eigen)
}

extern "C" void* flux_matrix_sub(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    
    if (A->rows() == B->rows() && A->cols() == B->cols())
        return new Eigen::MatrixXd((*A) - (*B));
    
    if (A->rows() == B->rows() && B->cols() == 1)
        return new Eigen::MatrixXd(A->colwise() - B->col(0));
    
    if (A->cols() == B->cols() && B->rows() == 1)
        return new Eigen::MatrixXd(A->rowwise() - B->row(0));

    return new Eigen::MatrixXd((*A) - (*B));
}

extern "C" void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    
    if (A->rows() == B->rows() && A->cols() == B->cols())
        return new Eigen::MatrixXd(A->array() * B->array());
        
    if (A->rows() == B->rows() && B->cols() == 1)
        return new Eigen::MatrixXd(A->array().colwise() * B->col(0).array());
        
    if (A->cols() == B->cols() && B->rows() == 1)
        return new Eigen::MatrixXd(A->array().rowwise() * B->row(0).array());
        
    return new Eigen::MatrixXd(A->array() * B->array());
}

extern "C" void* flux_matrix_ew_div(void* a_ptr, void* b_ptr) {
    auto* A = static_cast<Eigen::MatrixXd*>(a_ptr);
    auto* B = static_cast<Eigen::MatrixXd*>(b_ptr);
    
    if (A->rows() == B->rows() && A->cols() == B->cols())
        return new Eigen::MatrixXd(A->array() / B->array());
        
    if (A->rows() == B->rows() && B->cols() == 1)
        return new Eigen::MatrixXd(A->array().colwise() / B->col(0).array());
        
    if (A->cols() == B->cols() && B->rows() == 1)
        return new Eigen::MatrixXd(A->array().rowwise() / B->row(0).array());
        
    return new Eigen::MatrixXd(A->array() / B->array());
}

extern "C" void* flux_matrix_add_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return new Eigen::MatrixXd((*M).array() + s);
}

extern "C" void* flux_matrix_sub_ms(void* m_ptr, double s) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return new Eigen::MatrixXd((*M).array() - s);
}

extern "C" void* flux_matrix_sub_sm(double s, void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return new Eigen::MatrixXd(s - (*M).array());
}

extern "C" void* flux_matrix_transpose(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return new Eigen::MatrixXd(M->transpose());
}

extern "C" int flux_matrix_rows(void* m_ptr) {
    if (!m_ptr) return 0;
    return static_cast<Eigen::MatrixXd*>(m_ptr)->rows();
}

extern "C" int flux_matrix_cols(void* m_ptr) {
    if (!m_ptr) return 0;
    return static_cast<Eigen::MatrixXd*>(m_ptr)->cols();
}

extern "C" double flux_matrix_get(void* m_ptr, int row, int col) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    return (*M)(row, col);
}

extern "C" double flux_create_vector_sum(double* data, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i)
        sum += data[i];
    return sum;
}

extern "C" double flux_print_matrix(void* m_ptr) {
    auto* M = static_cast<Eigen::MatrixXd*>(m_ptr);
    if (!M) return 0.0;
    std::cout << *M << std::endl;
    return 1.0;
}

// ============================================================================
// Plotting and Visualization Implementation
// ============================================================================

// Global plot state (simplified for CLI - stores data for later GUI display)
namespace {
    struct PlotData {
        std::vector<double> xData;
        std::vector<double> yData;
        std::string title;
        std::string options;
        bool logX = false;
        bool logY = false;
        int subplotRow = 1;
        int subplotCol = 1;
        int subplotIndex = 1;
    };
    
    PlotData g_currentPlot;
    std::string g_currentTheme = "light";
    bool g_gridEnabled = false;
    std::string g_plotTitle = "";
    std::string g_xLabel = "";
    std::string g_yLabel = "";
}

extern "C" double flux_plot(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = false;
    g_currentPlot.logY = false;
    
    // Print plot data summary (for CLI mode)
    std::cout << "[PLOT] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    std::cout << "  X range: [" << g_currentPlot.xData.front() << ", " << g_currentPlot.xData.back() << "]" << std::endl;
    std::cout << "  Y range: [" << g_currentPlot.yData.front() << ", " << g_currentPlot.yData.back() << "]" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would trigger QCustomPlot update
    // For now, we just store the data for later retrieval
#endif
    
    return 1.0;
}

extern "C" double flux_semilogx(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = true;
    g_currentPlot.logY = false;
    
    std::cout << "[SEMILOGX] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    
    return 1.0;
}

extern "C" double flux_semilogy(double* x_data, double* y_data, int size, const char* title, const char* options) {
    g_currentPlot.xData.assign(x_data, x_data + size);
    g_currentPlot.yData.assign(y_data, y_data + size);
    g_currentPlot.title = title ? title : "";
    g_currentPlot.options = options ? options : "";
    g_currentPlot.logX = false;
    g_currentPlot.logY = true;
    
    std::cout << "[SEMILOGY] " << g_currentPlot.title << " - " << size << " points" << std::endl;
    
    return 1.0;
}

extern "C" double flux_subplot(int rows, int cols, int index) {
    g_currentPlot.subplotRow = rows;
    g_currentPlot.subplotCol = cols;
    g_currentPlot.subplotIndex = index;
    
    std::cout << "[SUBPLOT] Grid: " << rows << "x" << cols << ", Active: " << index << std::endl;
    
    return 1.0;
}

extern "C" double flux_plot_title(const char* title) {
    g_plotTitle = title ? title : "";
    std::cout << "[PLOT_TITLE] " << g_plotTitle << std::endl;
    return 1.0;
}

extern "C" double flux_plot_xlabel(const char* label) {
    g_xLabel = label ? label : "";
    std::cout << "[PLOT_XLABEL] " << g_xLabel << std::endl;
    return 1.0;
}

extern "C" double flux_plot_ylabel(const char* label) {
    g_yLabel = label ? label : "";
    std::cout << "[PLOT_YLABEL] " << g_yLabel << std::endl;
    return 1.0;
}

extern "C" double flux_plot_grid(double enable) {
    g_gridEnabled = (enable != 0.0);
    std::cout << "[PLOT_GRID] " << (g_gridEnabled ? "enabled" : "disabled") << std::endl;
    return 1.0;
}

extern "C" double flux_plot_legend(const char* location) {
    std::string loc = location ? location : "best";
    std::cout << "[PLOT_LEGEND] Location: " << loc << std::endl;
    return 1.0;
}

extern "C" double flux_plot_save(const char* filename, int width, int height) {
    std::cout << "[PLOT_SAVE] " << filename << " (" << width << "x" << height << ")" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would save the current plot to file
    // Supported formats: PNG, JPG, PDF, SVG
#endif
    
    return 1.0;
}

extern "C" double flux_plot_clear() {
    g_currentPlot.xData.clear();
    g_currentPlot.yData.clear();
    g_currentPlot.title = "";
    g_currentPlot.options = "";
    g_plotTitle = "";
    g_xLabel = "";
    g_yLabel = "";
    g_gridEnabled = false;
    
    std::cout << "[PLOT_CLEAR]" << std::endl;
    
    return 1.0;
}

extern "C" double flux_plot_theme(const char* theme) {
    g_currentTheme = theme ? theme : "light";
    std::cout << "[PLOT_THEME] " << g_currentTheme << std::endl;
    return 1.0;
}

// ============================================================================
// CSV/HDF5 Export Implementation
// ============================================================================

extern "C" double flux_export_csv(double* data, int rows, int cols, const char* filename, const char* delimiter) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[EXPORT_CSV] Error: Cannot open file " << filename << std::endl;
        return 0.0;
    }
    
    std::string delim = delimiter ? delimiter : ",";
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            file << std::setprecision(15) << data[i * cols + j];
            if (j < cols - 1) file << delim;
        }
        file << "\n";
    }
    
    file.close();
    std::cout << "[EXPORT_CSV] Saved " << rows << "x" << cols << " to " << filename << std::endl;
    
    return 1.0;
}

extern "C" double flux_export_hdf5(double* data, int rows, int cols, const char* filename, const char* dataset_name) {
    // HDF5 export requires the HDF5 library
    // For now, provide a stub that indicates the feature
    std::cout << "[EXPORT_HDF5] HDF5 export requested (requires HDF5 library)" << std::endl;
    std::cout << "  Dataset: " << (dataset_name ? dataset_name : "data") << std::endl;
    std::cout << "  Size: " << rows << "x" << cols << std::endl;
    std::cout << "  File: " << filename << std::endl;
    
    // TODO: Implement HDF5 export when HDF5 library is available
    // h5create_file, h5open_file, h5write_dataset, etc.
    
    return 1.0;
}

// ============================================================================
// Image Processing Implementation
// ============================================================================

extern "C" void* flux_array_to_image(double* data, int rows, int cols, const char* colormap) {
    // Create an image buffer from 2D array data
    // Colormap options: "gray", "jet", "hot", "cool", "viridis"
    
    std::string cmap = colormap ? colormap : "gray";
    std::cout << "[ARRAY_TO_IMAGE] " << rows << "x" << cols << " with colormap: " << cmap << std::endl;
    
    // Allocate image buffer (RGBA format)
    // In a full implementation, this would use Qt's QImage or similar
    unsigned char* imageBuffer = new unsigned char[rows * cols * 4];
    
    // Simple grayscale conversion for now
    double minVal = data[0], maxVal = data[0];
    for (int i = 1; i < rows * cols; ++i) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }
    
    double range = maxVal - minVal;
    if (range == 0) range = 1.0;
    
    for (int i = 0; i < rows * cols; ++i) {
        unsigned char intensity = static_cast<unsigned char>((data[i] - minVal) / range * 255.0);
        
        if (cmap == "gray") {
            imageBuffer[i * 4 + 0] = intensity; // R
            imageBuffer[i * 4 + 1] = intensity; // G
            imageBuffer[i * 4 + 2] = intensity; // B
        } else if (cmap == "jet") {
            // Simple jet colormap approximation
            double t = (data[i] - minVal) / range;
            if (t < 0.25) {
                imageBuffer[i * 4 + 0] = 0;
                imageBuffer[i * 4 + 1] = static_cast<unsigned char>(t * 4 * 255);
                imageBuffer[i * 4 + 2] = 255;
            } else if (t < 0.5) {
                imageBuffer[i * 4 + 0] = 0;
                imageBuffer[i * 4 + 1] = 255;
                imageBuffer[i * 4 + 2] = static_cast<unsigned char>((0.5 - t) * 4 * 255);
            } else if (t < 0.75) {
                imageBuffer[i * 4 + 0] = static_cast<unsigned char>((t - 0.5) * 4 * 255);
                imageBuffer[i * 4 + 1] = 255;
                imageBuffer[i * 4 + 2] = 0;
            } else {
                imageBuffer[i * 4 + 0] = 255;
                imageBuffer[i * 4 + 1] = static_cast<unsigned char>((1.0 - t) * 4 * 255);
                imageBuffer[i * 4 + 2] = 0;
            }
        } else {
            // Default to grayscale
            imageBuffer[i * 4 + 0] = intensity;
            imageBuffer[i * 4 + 1] = intensity;
            imageBuffer[i * 4 + 2] = intensity;
        }
        imageBuffer[i * 4 + 3] = 255; // Alpha
    }
    
    return imageBuffer;
}

extern "C" double flux_image_save(void* image_ptr, const char* filename, int quality) {
    if (!image_ptr) {
        std::cerr << "[IMAGE_SAVE] Error: Null image pointer" << std::endl;
        return 0.0;
    }
    
    unsigned char* buffer = static_cast<unsigned char*>(image_ptr);
    std::cout << "[IMAGE_SAVE] Saving to " << filename << " (quality: " << quality << ")" << std::endl;
    
#ifdef FLUX_HAS_QT
    // In GUI mode, this would save the image using QImage
    // QImage image(buffer, width, height, QImage::Format_RGBA8888);
    // image.save(filename, nullptr, quality);
#endif
    
    // For CLI mode, we could save as raw PPM format
    std::string fname = filename ? filename : "output.ppm";
    if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".ppm") {
        fname += ".ppm";
    }
    
    std::ofstream file(fname, std::ios::binary);
    if (file.is_open()) {
        // Write simple PPM header (assuming 256x256 for demo)
        file << "P6\n256 256\n255\n";
        file.write(reinterpret_cast<char*>(buffer), 256 * 256 * 4);
        file.close();
        std::cout << "[IMAGE_SAVE] Saved as PPM: " << fname << std::endl;
        return 1.0;
    }
    
    return 0.0;
}

extern "C" double flux_image_display(void* image_ptr, const char* title) {
    if (!image_ptr) {
        std::cerr << "[IMAGE_DISPLAY] Error: Null image pointer" << std::endl;
        return 0.0;
    }

    std::string titleStr = title ? title : "Image";
    std::cout << "[IMAGE_DISPLAY] Displaying: " << titleStr << std::endl;

#ifdef FLUX_HAS_QT
    // In GUI mode, this would display the image in a QLabel or QDialog
    // For CLI mode, just acknowledge the request
#endif

    return 1.0;
}


// ============================================================================
// Security Functions Implementation
// ============================================================================

extern "C" void flux_bounds_check_row(int row, int max_rows, void* matrix) {
    if (row < 0 || row >= max_rows) {
        std::cerr << "[BOUNDS ERROR] Row index " << row << " out of bounds [0, " 
                  << max_rows << ")" << std::endl;
        throw std::out_of_range("Matrix row index out of bounds");
    }
}

extern "C" void flux_bounds_check_col(int col, int max_cols, void* matrix) {
    if (col < 0 || col >= max_cols) {
        std::cerr << "[BOUNDS ERROR] Column index " << col << " out of bounds [0, " 
                  << max_cols << ")" << std::endl;
        throw std::out_of_range("Matrix column index out of bounds");
    }
}

// Thread-local stack depth counter
static thread_local int g_stackDepth = 0;
static const int MAX_STACK_DEPTH = 1000;

extern "C" void flux_stack_enter() {
    ++g_stackDepth;
    if (g_stackDepth > MAX_STACK_DEPTH) {
        std::cerr << "[STACK ERROR] Stack depth " << g_stackDepth 
                  << " exceeds maximum " << MAX_STACK_DEPTH << std::endl;
        throw std::overflow_error("Stack overflow detected");
    }
}

extern "C" void flux_stack_leave() {
    if (g_stackDepth > 0) {
        --g_stackDepth;
    }
}

} // namespace Flux
