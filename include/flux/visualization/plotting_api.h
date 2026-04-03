// ============================================================================
// FluxScript Plotting API - VioSpice Waveform Viewer Integration
// Real-time plotting, parameter sweeps, FFT/Bode visualization
// ============================================================================

#ifndef FLUX_PLOTTING_API_H
#define FLUX_PLOTTING_API_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>

namespace Flux {

// Waveform data structure
struct WaveformData {
    std::string name;
    std::string x_label;
    std::string y_label;
    std::string title;
    std::vector<double> x_data;
    std::vector<double> y_data;
    std::string color;
    int line_width = 1;
    bool visible = true;
    std::chrono::steady_clock::time_point last_update;
};

// Plot configuration
struct PlotConfig {
    std::string title;
    std::string x_label;
    std::string y_label;
    bool grid = true;
    bool legend = true;
    double x_min = 0.0;
    double x_max = 0.0;  // 0 = auto
    double y_min = 0.0;
    double y_max = 0.0;  // 0 = auto
    std::string plot_type = "line";  // "line", "scatter", "bar", "histogram"
};

// Sweep parameter
struct SweepParam {
    std::string name;
    double start;
    double stop;
    double step;
    std::vector<double> values;  // Pre-computed values
    std::string unit;
};

// Sweep result
struct SweepResult {
    std::string param_name;
    std::vector<double> param_values;
    std::vector<std::string> measure_names;
    std::map<std::string, std::vector<double>> measure_data;
    std::chrono::duration<double> total_time;
};

// FFT result
struct FFTResult {
    std::string signal_name;
    double sample_rate;
    std::vector<double> frequencies;
    std::vector<double> magnitudes;  // dB
    std::vector<double> phases;      // degrees
    double fundamental_freq;
    double thd;  // Total Harmonic Distortion
    double snr;  // Signal-to-Noise Ratio
};

// Bode plot data
struct BodeData {
    std::string input_name;
    std::string output_name;
    std::vector<double> frequencies;
    std::vector<double> magnitude;  // dB
    std::vector<double> phase;      // degrees
    double bandwidth_3db;
    double phase_margin;
    double gain_margin;
};

// Export format
enum class ExportFormat {
    CSV,
    PNG,
    PDF,
    SVG,
    JSON
};

// Plotting API class
class PlottingAPI {
public:
    static PlottingAPI& instance();

    // Initialize/finalize
    void initialize();
    void finalize();
    bool isInitialized() const { return m_initialized; }

    // Basic plotting
    bool plot(const std::string& trace_name, double x, double y);
    bool plotArray(const std::string& trace_name, 
                  const std::vector<double>& x, const std::vector<double>& y);
    bool clearPlot(const std::string& trace_name);
    bool clearAllPlots();

    // Waveform management
    bool addWaveform(const std::string& name, const WaveformData& data);
    bool updateWaveform(const std::string& name, const WaveformData& data);
    bool removeWaveform(const std::string& name);
    WaveformData* getWaveform(const std::string& name);
    std::vector<std::string> getWaveformNames() const;

    // Plot configuration
    bool configurePlot(const std::string& plot_name, const PlotConfig& config);
    PlotConfig* getPlotConfig(const std::string& plot_name);

    // Real-time updates
    bool enableRealTimeUpdates(bool enable);
    bool isRealTimeEnabled() const;
    void setUpdateIntervalMs(int interval_ms);
    int getUpdateIntervalMs() const;

    // Callbacks for VioSpice integration
    void setWaveformUpdateCallback(std::function<void(const std::string& name)> callback);
    void setPlotReadyCallback(std::function<void(const std::string& plot_name)> callback);

private:
    PlottingAPI() = default;
    ~PlottingAPI() = default;

    std::map<std::string, WaveformData> m_waveforms;
    std::map<std::string, PlotConfig> m_plot_configs;
    std::function<void(const std::string&)> m_update_callback;
    std::function<void(const std::string&)> m_plot_ready_callback;
    
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_real_time_enabled = true;
    int m_update_interval_ms = 100;
};

// ============================================================================
// Waveform Viewer - VioSpice Integration
// ============================================================================

class WaveformViewer {
public:
    static WaveformViewer& instance();

    // Initialize with VioSpice context
    void initialize(void* viospice_context);
    void finalize();

    // Display waveforms
    bool showWaveform(const std::string& waveform_name);
    bool hideWaveform(const std::string& waveform_name);
    bool updateWaveformDisplay(const std::string& waveform_name);

    // Multi-trace display
    bool createMultiTracePlot(const std::string& plot_name,
                             const std::vector<std::string>& waveform_names);
    bool addTraceToPlot(const std::string& plot_name, const std::string& waveform_name);
    bool removeTraceFromPlot(const std::string& plot_name, const std::string& waveform_name);

    // Display options
    bool setGridVisible(bool visible);
    bool setLegendVisible(bool visible);
    bool setAxisLabels(const std::string& x_label, const std::string& y_label);
    bool setTitle(const std::string& title);

    // Zoom and pan
    bool setAxisRange(double x_min, double x_max, double y_min, double y_max);
    bool autoScale();

    // Export
    bool exportPlot(const std::string& plot_name, const std::string& filename, 
                   ExportFormat format);
    bool exportAllPlots(const std::string& directory, ExportFormat format);

private:
    WaveformViewer() = default;
    ~WaveformViewer() = default;

    void* m_viospice_context = nullptr;
    std::vector<std::string> m_active_waveforms;
    std::map<std::string, std::vector<std::string>> m_plots;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

// ============================================================================
// Parameter Sweep Engine
// ============================================================================

class ParameterSweep {
public:
    static ParameterSweep& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // Sweep execution
    bool runSweep(const SweepParam& param, 
                 const std::string& simulation_netlist,
                 const std::vector<std::string>& measure_names,
                 std::function<void(double progress)> progress_callback = nullptr);
    
    bool runMultiParamSweep(const std::vector<SweepParam>& params,
                           const std::string& simulation_netlist,
                           const std::vector<std::string>& measure_names,
                           std::function<void(double progress)> progress_callback = nullptr);

    // Results
    SweepResult getLastResult() const;
    bool exportSweepResult(const SweepResult& result, const std::string& filename,
                          ExportFormat format);

    // Plot sweep results
    bool plotSweepResult(const SweepResult& result, const std::string& measure_name);

private:
    ParameterSweep() = default;
    ~ParameterSweep() = default;

    SweepResult m_last_result;
    mutable std::mutex m_mutex;
};

// ============================================================================
// FFT/Bode Plotter
// ============================================================================

class FFTBodePlotter {
public:
    static FFTBodePlotter& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // FFT analysis
    FFTResult computeFFT(const std::vector<double>& signal, double sample_rate,
                        const std::string& signal_name = "");
    
    // Bode plot
    BodeData computeBode(const std::vector<double>& input_signal,
                        const std::vector<double>& output_signal,
                        double sample_rate,
                        const std::string& input_name = "",
                        const std::string& output_name = "");

    // Display
    bool showFFTResult(const FFTResult& result);
    bool showBodeResult(const BodeData& result);

    // Export
    bool exportFFTResult(const FFTResult& result, const std::string& filename,
                        ExportFormat format);
    bool exportBodeResult(const BodeData& result, const std::string& filename,
                         ExportFormat format);

private:
    FFTBodePlotter() = default;
    ~FFTBodePlotter() = default;

    mutable std::mutex m_mutex;
};

// ============================================================================
// Helper Functions
// ============================================================================

namespace PlotUtils {
    std::string formatNumber(double value, int precision = 6);
    std::string formatFrequency(double freq);
    std::string formatDB(double db);
    std::string formatPhase(double phase);
    
    std::vector<double> generateTimeAxis(double tstart, double tstop, double dt);
    std::vector<double> generateFreqAxis(double fstart, double fstop, int points_per_dec);
}

} // namespace Flux

#endif // FLUX_PLOTTING_API_H
