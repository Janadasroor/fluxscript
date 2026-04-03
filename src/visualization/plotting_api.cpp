// ============================================================================
// FluxScript Plotting API Implementation
// Real-time plotting, parameter sweeps, FFT/Bode visualization
// ============================================================================

#include "flux/visualization/plotting_api.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Flux {

// ============================================================================
// PlottingAPI Singleton
// ============================================================================

PlottingAPI& PlottingAPI::instance() {
    static PlottingAPI instance;
    return instance;
}

void PlottingAPI::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return;
    }

    m_waveforms.clear();
    m_plot_configs.clear();
    m_initialized = true;
    m_real_time_enabled = true;
    m_update_interval_ms = 100;

    std::cout << "[PlottingAPI] Initialized successfully" << std::endl;
}

void PlottingAPI::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveforms.clear();
    m_plot_configs.clear();
    m_initialized = false;
    std::cout << "[PlottingAPI] Finalized" << std::endl;
}

// ============================================================================
// Basic Plotting
// ============================================================================

bool PlottingAPI::plot(const std::string& trace_name, double x, double y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        return false;
    }

    auto it = m_waveforms.find(trace_name);
    if (it == m_waveforms.end()) {
        // Create new waveform
        WaveformData data;
        data.name = trace_name;
        data.x_data.push_back(x);
        data.y_data.push_back(y);
        data.last_update = std::chrono::steady_clock::now();
        m_waveforms[trace_name] = data;
    } else {
        // Append to existing
        it->second.x_data.push_back(x);
        it->second.y_data.push_back(y);
        it->second.last_update = std::chrono::steady_clock::now();
    }

    // Callback for real-time updates
    if (m_real_time_enabled && m_update_callback) {
        m_update_callback(trace_name);
    }

    return true;
}

bool PlottingAPI::plotArray(const std::string& trace_name,
                           const std::vector<double>& x, const std::vector<double>& y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        return false;
    }

    if (x.size() != y.size()) {
        std::cerr << "[PlottingAPI] X and Y arrays must have same size" << std::endl;
        return false;
    }

    WaveformData data;
    data.name = trace_name;
    data.x_data = x;
    data.y_data = y;
    data.last_update = std::chrono::steady_clock::now();
    m_waveforms[trace_name] = data;

    if (m_real_time_enabled && m_update_callback) {
        m_update_callback(trace_name);
    }

    return true;
}

bool PlottingAPI::clearPlot(const std::string& trace_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveforms.erase(trace_name);
    return true;
}

bool PlottingAPI::clearAllPlots() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveforms.clear();
    return true;
}

// ============================================================================
// Waveform Management
// ============================================================================

bool PlottingAPI::addWaveform(const std::string& name, const WaveformData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveforms[name] = data;
    return true;
}

bool PlottingAPI::updateWaveform(const std::string& name, const WaveformData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_waveforms.find(name);
    if (it == m_waveforms.end()) {
        return false;
    }
    it->second = data;
    it->second.last_update = std::chrono::steady_clock::now();
    return true;
}

bool PlottingAPI::removeWaveform(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveforms.erase(name);
    return true;
}

WaveformData* PlottingAPI::getWaveform(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_waveforms.find(name);
    if (it == m_waveforms.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> PlottingAPI::getWaveformNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, data] : m_waveforms) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Plot Configuration
// ============================================================================

bool PlottingAPI::configurePlot(const std::string& plot_name, const PlotConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plot_configs[plot_name] = config;
    return true;
}

PlotConfig* PlottingAPI::getPlotConfig(const std::string& plot_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_plot_configs.find(plot_name);
    if (it == m_plot_configs.end()) {
        return nullptr;
    }
    return &it->second;
}

// ============================================================================
// Real-Time Updates
// ============================================================================

bool PlottingAPI::enableRealTimeUpdates(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_real_time_enabled = enable;
    return true;
}

bool PlottingAPI::isRealTimeEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_real_time_enabled;
}

void PlottingAPI::setUpdateIntervalMs(int interval_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_update_interval_ms = interval_ms;
}

int PlottingAPI::getUpdateIntervalMs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_update_interval_ms;
}

void PlottingAPI::setWaveformUpdateCallback(std::function<void(const std::string& name)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_update_callback = callback;
}

void PlottingAPI::setPlotReadyCallback(std::function<void(const std::string& plot_name)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plot_ready_callback = callback;
}

// ============================================================================
// WaveformViewer Singleton
// ============================================================================

WaveformViewer& WaveformViewer::instance() {
    static WaveformViewer instance;
    return instance;
}

void WaveformViewer::initialize(void* viospice_context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_viospice_context = viospice_context;
    m_initialized = true;
    std::cout << "[WaveformViewer] Initialized with VioSpice context" << std::endl;
}

void WaveformViewer::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active_waveforms.clear();
    m_plots.clear();
    m_initialized = false;
    std::cout << "[WaveformViewer] Finalized" << std::endl;
}

bool WaveformViewer::showWaveform(const std::string& waveform_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        return false;
    }

    if (std::find(m_active_waveforms.begin(), m_active_waveforms.end(), waveform_name) == 
        m_active_waveforms.end()) {
        m_active_waveforms.push_back(waveform_name);
    }

    std::cout << "[WaveformViewer] Showing: " << waveform_name << std::endl;
    return true;
}

bool WaveformViewer::hideWaveform(const std::string& waveform_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active_waveforms.erase(
        std::remove(m_active_waveforms.begin(), m_active_waveforms.end(), waveform_name),
        m_active_waveforms.end()
    );
    return true;
}

bool WaveformViewer::updateWaveformDisplay(const std::string& waveform_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Update display with new data from PlottingAPI
    auto& plotting = PlottingAPI::instance();
    auto* waveform = plotting.getWaveform(waveform_name);
    if (!waveform) {
        return false;
    }

    // Send to VioSpice waveform viewer
    std::cout << "[WaveformViewer] Updated: " << waveform_name 
              << " (" << waveform->x_data.size() << " points)" << std::endl;
    return true;
}

bool WaveformViewer::createMultiTracePlot(const std::string& plot_name,
                                         const std::vector<std::string>& waveform_names) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plots[plot_name] = waveform_names;
    std::cout << "[WaveformViewer] Created multi-trace plot: " << plot_name 
              << " with " << waveform_names.size() << " traces" << std::endl;
    return true;
}

bool WaveformViewer::addTraceToPlot(const std::string& plot_name, const std::string& waveform_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_plots.find(plot_name);
    if (it == m_plots.end()) {
        return false;
    }
    it->second.push_back(waveform_name);
    return true;
}

bool WaveformViewer::removeTraceFromPlot(const std::string& plot_name, 
                                        const std::string& waveform_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_plots.find(plot_name);
    if (it == m_plots.end()) {
        return false;
    }
    it->second.erase(
        std::remove(it->second.begin(), it->second.end(), waveform_name),
        it->second.end()
    );
    return true;
}

bool WaveformViewer::setGridVisible(bool visible) {
    std::cout << "[WaveformViewer] Grid " << (visible ? "enabled" : "disabled") << std::endl;
    return true;
}

bool WaveformViewer::setLegendVisible(bool visible) {
    std::cout << "[WaveformViewer] Legend " << (visible ? "enabled" : "disabled") << std::endl;
    return true;
}

bool WaveformViewer::setAxisLabels(const std::string& x_label, const std::string& y_label) {
    std::cout << "[WaveformViewer] Axis labels: X='" << x_label << "', Y='" << y_label << "'" << std::endl;
    return true;
}

bool WaveformViewer::setTitle(const std::string& title) {
    std::cout << "[WaveformViewer] Title: " << title << std::endl;
    return true;
}

bool WaveformViewer::setAxisRange(double x_min, double x_max, double y_min, double y_max) {
    std::cout << "[WaveformViewer] Axis range: X[" << x_min << ", " << x_max 
              << "], Y[" << y_min << ", " << y_max << "]" << std::endl;
    return true;
}

bool WaveformViewer::autoScale() {
    std::cout << "[WaveformViewer] Auto-scale enabled" << std::endl;
    return true;
}

bool WaveformViewer::exportPlot(const std::string& plot_name, const std::string& filename,
                               ExportFormat format) {
    std::cout << "[WaveformViewer] Exporting plot '" << plot_name << "' to " << filename << std::endl;
    return true;
}

bool WaveformViewer::exportAllPlots(const std::string& directory, ExportFormat format) {
    std::cout << "[WaveformViewer] Exporting all plots to " << directory << std::endl;
    return true;
}

// ============================================================================
// ParameterSweep Singleton
// ============================================================================

ParameterSweep& ParameterSweep::instance() {
    static ParameterSweep instance;
    return instance;
}

void ParameterSweep::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[ParameterSweep] Initialized" << std::endl;
}

void ParameterSweep::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[ParameterSweep] Finalized" << std::endl;
}

bool ParameterSweep::runSweep(const SweepParam& param,
                             const std::string& simulation_netlist,
                             const std::vector<std::string>& measure_names,
                             std::function<void(double progress)> progress_callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto start_time = std::chrono::steady_clock::now();

    m_last_result.param_name = param.name;
    m_last_result.measure_names = measure_names;

    // Generate parameter values if not provided
    std::vector<double> values;
    if (param.values.empty()) {
        for (double v = param.start; v <= param.stop; v += param.step) {
            values.push_back(v);
        }
    } else {
        values = param.values;
    }

    m_last_result.param_values = values;

    std::cout << "[ParameterSweep] Running sweep: " << param.name 
              << " from " << param.start << " to " << param.stop 
              << " (" << values.size() << " points)" << std::endl;

    // Simulate sweep (would integrate with actual simulator)
    for (size_t i = 0; i < values.size(); i++) {
        // Run simulation with parameter value
        // Collect measurements
        for (const auto& measure : measure_names) {
            m_last_result.measure_data[measure].push_back(0.0);  // Placeholder
        }

        // Progress callback
        if (progress_callback) {
            double progress = (double)(i + 1) / values.size() * 100.0;
            progress_callback(progress);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    m_last_result.total_time = end_time - start_time;

    std::cout << "[ParameterSweep] Completed in " 
              << m_last_result.total_time.count() << " seconds" << std::endl;

    return true;
}

bool ParameterSweep::runMultiParamSweep(const std::vector<SweepParam>& params,
                                       const std::string& simulation_netlist,
                                       const std::vector<std::string>& measure_names,
                                       std::function<void(double progress)> progress_callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[ParameterSweep] Multi-param sweep with " << params.size() << " parameters" << std::endl;
    // Implementation for multi-dimensional sweeps
    return true;
}

SweepResult ParameterSweep::getLastResult() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_result;
}

bool ParameterSweep::exportSweepResult(const SweepResult& result, const std::string& filename,
                                      ExportFormat format) {
    std::cout << "[ParameterSweep] Exporting result to " << filename << std::endl;
    return true;
}

bool ParameterSweep::plotSweepResult(const SweepResult& result, const std::string& measure_name) {
    std::cout << "[ParameterSweep] Plotting: " << measure_name << " vs " << result.param_name << std::endl;
    return true;
}

// ============================================================================
// FFTBodePlotter Singleton
// ============================================================================

FFTBodePlotter& FFTBodePlotter::instance() {
    static FFTBodePlotter instance;
    return instance;
}

void FFTBodePlotter::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[FFTBodePlotter] Initialized" << std::endl;
}

void FFTBodePlotter::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << "[FFTBodePlotter] Finalized" << std::endl;
}

FFTResult FFTBodePlotter::computeFFT(const std::vector<double>& signal, double sample_rate,
                                    const std::string& signal_name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FFTResult result;
    result.signal_name = signal_name;
    result.sample_rate = sample_rate;

    int N = signal.size();
    int num_freqs = N / 2;

    // Compute FFT (simplified - would use actual FFT implementation)
    result.frequencies.resize(num_freqs);
    result.magnitudes.resize(num_freqs);
    result.phases.resize(num_freqs);

    for (int i = 0; i < num_freqs; i++) {
        result.frequencies[i] = i * sample_rate / N;
        result.magnitudes[i] = 0.0;  // Placeholder
        result.phases[i] = 0.0;      // Placeholder
    }

    // Find fundamental frequency
    result.fundamental_freq = result.frequencies[0];
    result.thd = 0.0;
    result.snr = 0.0;

    std::cout << "[FFTBodePlotter] Computed FFT for " << signal_name 
              << " (" << N << " points, " << sample_rate << " Hz)" << std::endl;

    return result;
}

BodeData FFTBodePlotter::computeBode(const std::vector<double>& input_signal,
                                    const std::vector<double>& output_signal,
                                    double sample_rate,
                                    const std::string& input_name,
                                    const std::string& output_name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    BodeData result;
    result.input_name = input_name;
    result.output_name = output_name;

    int N = input_signal.size();
    int num_freqs = N / 2;

    result.frequencies.resize(num_freqs);
    result.magnitude.resize(num_freqs);
    result.phase.resize(num_freqs);

    for (int i = 0; i < num_freqs; i++) {
        result.frequencies[i] = i * sample_rate / N;
        result.magnitude[i] = 0.0;  // Placeholder
        result.phase[i] = 0.0;      // Placeholder
    }

    result.bandwidth_3db = 0.0;
    result.phase_margin = 0.0;
    result.gain_margin = 0.0;

    std::cout << "[FFTBodePlotter] Computed Bode plot: " << input_name << " -> " << output_name << std::endl;

    return result;
}

bool FFTBodePlotter::showFFTResult(const FFTResult& result) {
    std::cout << "[FFTBodePlotter] Displaying FFT: " << result.signal_name << std::endl;
    return true;
}

bool FFTBodePlotter::showBodeResult(const BodeData& result) {
    std::cout << "[FFTBodePlotter] Displaying Bode: " << result.input_name << " -> " << result.output_name << std::endl;
    return true;
}

bool FFTBodePlotter::exportFFTResult(const FFTResult& result, const std::string& filename,
                                    ExportFormat format) {
    std::cout << "[FFTBodePlotter] Exporting FFT to " << filename << std::endl;
    return true;
}

bool FFTBodePlotter::exportBodeResult(const BodeData& result, const std::string& filename,
                                     ExportFormat format) {
    std::cout << "[FFTBodePlotter] Exporting Bode to " << filename << std::endl;
    return true;
}

// ============================================================================
// Plot Utilities
// ============================================================================

namespace PlotUtils {

std::string formatNumber(double value, int precision) {
    std::ostringstream oss;
    oss.precision(precision);
    oss << value;
    return oss.str();
}

std::string formatFrequency(double freq) {
    if (freq >= 1e9) {
        return formatNumber(freq / 1e9, 3) + " GHz";
    } else if (freq >= 1e6) {
        return formatNumber(freq / 1e6, 3) + " MHz";
    } else if (freq >= 1e3) {
        return formatNumber(freq / 1e3, 3) + " kHz";
    } else {
        return formatNumber(freq, 3) + " Hz";
    }
}

std::string formatDB(double db) {
    return formatNumber(db, 2) + " dB";
}

std::string formatPhase(double phase) {
    return formatNumber(phase, 2) + "°";
}

std::vector<double> generateTimeAxis(double tstart, double tstop, double dt) {
    std::vector<double> times;
    for (double t = tstart; t <= tstop; t += dt) {
        times.push_back(t);
    }
    return times;
}

std::vector<double> generateFreqAxis(double fstart, double fstop, int points_per_dec) {
    std::vector<double> freqs;
    double decades = std::log10(fstop / fstart);
    int total_points = (int)(decades * points_per_dec);
    
    for (int i = 0; i < total_points; i++) {
        double freq = fstart * std::pow(10.0, (double)i / points_per_dec);
        freqs.push_back(freq);
    }
    
    return freqs;
}

} // namespace PlotUtils

} // namespace Flux
