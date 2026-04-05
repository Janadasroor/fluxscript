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

#ifndef FLUX_RUNTIME_H
#define FLUX_RUNTIME_H

#include <string>

namespace Flux {

class FluxJIT;

void registerRuntimeFunctions(FluxJIT& jit);

} // namespace Flux

extern "C" {
double flux_print_string(const char* str);

double flux_get_voltage(double node);
double flux_get_current(double branch);
double flux_get_parameter(double name);
double flux_set_parameter(double name, double value);

double flux_sim_run();
double flux_sim_stop();
double flux_sim_pause();
double flux_erc_check();

double flux_uramp(double x);
double flux_limit(double x, double low, double high);
double flux_buf(double x);
double flux_inv(double x);

double flux_sin(double x);
double flux_cos(double x);
double flux_tan(double x);
double flux_asin(double x);
double flux_acos(double x);
double flux_atan(double x);
double flux_atan2(double y, double x);
double flux_sqrt(double x);
double flux_exp(double x);
double flux_log(double x);
double flux_log10(double x);
double flux_abs(double x);
double flux_floor(double x);
double flux_ceil(double x);
double flux_round(double x);
double flux_pow(double base, double exp);
double flux_sinh(double x);
double flux_cosh(double x);
double flux_tanh(double x);

double flux_pi();
double flux_e();

// Statistical functions
double flux_mean(double* data, int size);
double flux_std(double* data, int size);
double flux_median(double* data, int size);
double flux_min(double* data, int size);
double flux_max(double* data, int size);

// Signal generation functions
void* flux_sawtooth(double freq, double* t_data, int t_size);
void* flux_square(double freq, double* t_data, int t_size);
void* flux_pulse(double freq, double duty, double* t_data, int t_size);

// Interpolation function
double flux_interp1(double x, double* x_data, double* y_data, int size);

// Numerical integration
double flux_trapz(double* y_data, double* x_data, int size);

// Plotting and Visualization functions
double flux_plot(double* x_data, double* y_data, int size, const char* title, const char* options);
double flux_semilogx(double* x_data, double* y_data, int size, const char* title, const char* options);
double flux_semilogy(double* x_data, double* y_data, int size, const char* title, const char* options);
double flux_subplot(int rows, int cols, int index);
double flux_plot_title(const char* title);
double flux_plot_xlabel(const char* label);
double flux_plot_ylabel(const char* label);
double flux_plot_grid(double enable);
double flux_plot_legend(const char* location);
double flux_plot_save(const char* filename, int width, int height);
double flux_plot_clear();
double flux_plot_theme(const char* theme);

// CSV/HDF5 Export functions
double flux_export_csv(double* data, int rows, int cols, const char* filename, const char* delimiter);
double flux_export_hdf5(double* data, int rows, int cols, const char* filename, const char* dataset_name);

// Image Processing functions
void* flux_array_to_image(double* data, int rows, int cols, const char* colormap);
double flux_image_save(void* image_ptr, const char* filename, int quality);
double flux_image_display(void* image_ptr, const char* title);
void* flux_create_matrix(double* data, int rows, int cols);
void* flux_matrix_mul(void* a_ptr, void* b_ptr);
void* flux_matrix_mul_ms(void* m_ptr, double s);
void* flux_matrix_add(void* a_ptr, void* b_ptr);
void* flux_matrix_sub(void* a_ptr, void* b_ptr);
void* flux_matrix_add_ms(void* m_ptr, double s);
void* flux_matrix_sub_ms(void* m_ptr, double s);
void* flux_matrix_sub_sm(double s, void* m_ptr);
void* flux_matrix_ew_mul(void* a_ptr, void* b_ptr);
void* flux_matrix_ew_div(void* a_ptr, void* b_ptr);
void* flux_matrix_transpose(void* m_ptr);
int flux_matrix_rows(void* m_ptr);
int flux_matrix_cols(void* m_ptr);
double flux_matrix_get(void* m_ptr, int row, int col);
double flux_create_vector_sum(double* data, int size);
double flux_print_matrix(void* m_ptr);

// Security functions
void flux_bounds_check_row(int row, int max_rows, void* matrix);
void flux_bounds_check_col(int col, int max_cols, void* matrix);
void flux_stack_enter();
void flux_stack_leave();
}

#endif // FLUX_RUNTIME_H
