#ifndef FLUX_MODEL_QUALITY_ENGINE_H
#define FLUX_MODEL_QUALITY_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Flux {

// ============================================================================
// Model Quality & Verification Engine
// ============================================================================

struct WaveformPoint {
    double time;
    double value;
};

struct GoldenWaveform {
    std::string name;
    std::vector<WaveformPoint> points;
    
    double interpolate(double t) const {
        if (points.empty()) return 0.0;
        if (t <= points.front().time) return points.front().value;
        if (t >= points.back().time) return points.back().value;
        
        // Linear interpolation
        for (size_t i = 0; i < points.size() - 1; ++i) {
            if (t >= points[i].time && t <= points[i+1].time) {
                double frac = (t - points[i].time) / (points[i+1].time - points[i].time);
                return points[i].value + frac * (points[i+1].value - points[i].value);
            }
        }
        return 0.0;
    }
};

struct VerificationResult {
    bool passed;
    std::string message;
    double measured_value;
    double expected_value;
    double error_percent;
};

class ModelQualityEngine {
public:
    static ModelQualityEngine& instance();
    
    // Assertion system
    bool assertVoltage(const std::string& node, const std::string& op, 
                      double bound, double withinTime);
    
    // Settling time check
    double checkSettling(const std::string& node, double tolPercent, double afterTime);
    
    // Golden waveform
    void registerGolden(const std::string& name);
    bool loadGolden(const std::string& name, const std::string& filename);
    void addGoldenPoint(const std::string& name, double time, double value);
    
    // Waveform comparison
    bool compareWaveform(const std::string& node, const std::string& goldenName, double tolPercent);
    
    // Convergence diagnostics
    bool checkConvergence(const std::string& node, int maxIter, double epsilon);
    bool detectDiscontinuity(const std::string& node, double threshold);
    bool detectHiddenState(const std::string& node, int historyDepth);
    
    // Tolerance
    void setTolerance(double absTol, double relTolPercent);
    
    // Results
    const std::vector<VerificationResult>& getResults() const { return m_results; }
    void clearResults() { m_results.clear(); }
    bool allPassed() const;
    
private:
    ModelQualityEngine() : m_absTol(1e-6), m_relTol(1.0) {}
    
    std::map<std::string, GoldenWaveform> m_goldenWaveforms;
    std::vector<VerificationResult> m_results;
    double m_absTol;
    double m_relTol;
    
    // Simulated voltage history (would be populated by ngspice callbacks)
    std::map<std::string, std::vector<WaveformPoint>> m_voltageHistory;
};

// ============================================================================
// C API for JIT integration
// ============================================================================

extern "C" {
    int flux_assert_voltage(const char* node, const char* op, double bound, 
                           double withinTime, const char* message);
    double flux_check_settling(const char* node, double tolPercent, double afterTime);
    void flux_register_golden(const char* name);
    int flux_compare_waveform(const char* node, const char* goldenName, double tolPercent);
    int flux_check_convergence(const char* node, int maxIter, double epsilon);
    int flux_detect_discontinuity(const char* node, double threshold);
    int flux_detect_hidden_state(const char* node, int historyDepth);
    void flux_set_tolerance(double absTol, double relTolPercent);
}

} // namespace Flux

#endif // FLUX_MODEL_QUALITY_ENGINE_H
