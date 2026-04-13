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

#ifndef FLUX_ADVANCED_RUNTIME_H
#define FLUX_ADVANCED_RUNTIME_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Flux {

// ============ Plotting Runtime ============

struct PlotConfig {
    std::string title;
    std::vector<std::string> signals;
    std::vector<std::string> colors;
    bool gridEnabled;
    bool autoScale;
    double xMin, xMax, yMin, yMax;
    int width, height;
};

// Plot waveform data
int flux_plot_waveforms(const PlotConfig& config,
                        const std::vector<double>& xData,
                        const std::vector<std::vector<double>>& yData);

// Save plot to file
int flux_plot_save(const std::string& filename, int format); // 0=PNG, 1=SVG, 2=PDF

// Clear all plots
void flux_plot_clear();

// ============ Optimization Runtime ============

struct OptimizationGoal {
    std::string name;
    std::string target;      // e.g., "2.4G", "<1dB", "5%"
    double targetValue;
    double tolerance;
    std::string constraint;  // "min", "max", "exact"
};

struct OptimizationConfig {
    std::string circuit;
    std::vector<OptimizationGoal> goals;
    std::vector<std::string> tuneParams;
    std::string algorithm;   // "genetic", "pso", "bayesian", "gradient"
    int maxIterations;
    bool gpuAccelerated;
    int populationSize;
    double mutationRate;
};

struct OptimizationResult {
    bool success;
    std::map<std::string, double> bestParams;
    std::map<std::string, double> achievedGoals;
    int iterations;
    double fitness;
    std::vector<double> fitnessHistory;
};

// Run optimization
OptimizationResult flux_run_optimization(const OptimizationConfig& config);

// Get optimization progress
double flux_optimization_get_progress();

// Cancel optimization
void flux_optimization_cancel();

// ============ Benchmark Runtime ============

struct BenchmarkConfig {
    std::string circuit;
    std::vector<std::string> comparators;  // "fluxscript", "python", "matlab", "ngspice"
    std::vector<std::string> metrics;      // "time", "memory", "accuracy"
};

struct BenchmarkResult {
    std::string tool;
    double simulationTimeMs;
    double memoryUsageMB;
    double accuracyPercent;
    double startupTimeMs;
};

// Run benchmark comparison
std::vector<BenchmarkResult> flux_run_benchmark(const BenchmarkConfig& config);

// Print benchmark results
void flux_benchmark_print_results(const std::vector<BenchmarkResult>& results);

// Export benchmark to CSV
int flux_benchmark_export_csv(const std::vector<BenchmarkResult>& results,
                              const std::string& filename);

// ============ Interactive Sweep Runtime ============

struct SweepControl {
    std::string name;
    std::string type;      // "slider", "knob", "dropdown", "checkbox"
    double minValue;
    double maxValue;
    double step;
    double currentValue;
    std::vector<std::string> options;  // for dropdown
    bool enabled;
};

struct SweepConfig {
    std::string signal;
    std::vector<SweepControl> controls;
    int updateRateFPS;
    bool autoRun;
};

// Initialize interactive sweep
int flux_sweep_init(const SweepConfig& config);

// Update control value
int flux_sweep_set_control(const std::string& name, double value);

// Get current waveform
std::vector<double> flux_sweep_get_waveform();

// Close sweep
void flux_sweep_close();

// ============ Report Generation Runtime ============

struct ReportSection {
    std::string name;
    std::string content;  // HTML/Markdown content
    std::vector<std::string> images;
    std::vector<std::string> dataFiles;
};

struct ReportConfig {
    std::string filename;
    std::string title;
    std::string author;
    std::vector<std::string> sections;
    bool includePNG;
    bool includeCSV;
    bool includeNetlist;
    bool includeBOM;
    std::string format;    // "html", "pdf", "latex", "markdown"
};

// Generate report
int flux_generate_report(const ReportConfig& config);

// Add section to report
int flux_report_add_section(const std::string& name, const std::string& content);

// Add image to report
int flux_report_add_image(const std::string& section, const std::string& imagePath);

// Add data table to report
int flux_report_add_table(const std::string& section,
                          const std::vector<std::string>& headers,
                          const std::vector<std::vector<std::string>>& data);

// ============ Utility Functions ============

// Get current timestamp
std::string flux_get_timestamp();

// Format number with units
std::string flux_format_value(double value, const std::string& unit);

// Parse value string (e.g., "2.4G", "100mV")
double flux_parse_value(const std::string& str);

// Calculate statistics
double flux_stats_mean(const std::vector<double>& data);
double flux_stats_stddev(const std::vector<double>& data);
double flux_stats_min(const std::vector<double>& data);
double flux_stats_max(const std::vector<double>& data);

} // namespace Flux

#endif // FLUX_ADVANCED_RUNTIME_H
