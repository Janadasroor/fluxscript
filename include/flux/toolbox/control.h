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

#ifndef FLUX_TOOLBOX_CONTROL_H
#define FLUX_TOOLBOX_CONTROL_H

#include <string>
#include <vector>
#include <complex>
#include <map>
#include <memory>

namespace Flux {
namespace Control {

// ============================================================================
// Transfer Function
// ============================================================================

class TransferFunction {
public:
    TransferFunction();
    TransferFunction(const std::vector<double>& num, const std::vector<double>& den);
    ~TransferFunction();
    
    // Create standard transfer functions
    static TransferFunction gain(double K);
    static TransferFunction integrator();
    static TransferFunction differentiator();
    static TransferFunction firstOrder(double K, double tau);
    static TransferFunction secondOrder(double K, double wn, double zeta);
    static TransferFunction pid(double Kp, double Ki, double Kd);
    
    // Operations
    TransferFunction operator*(const TransferFunction& other) const;
    TransferFunction operator+(const TransferFunction& other) const;
    TransferFunction operator/(const TransferFunction& other) const;
    TransferFunction feedback(const TransferFunction& H) const;  // Closed-loop
    
    // Evaluation
    std::complex<double> eval(double omega) const;  // Evaluate at s = jω
    std::complex<double> eval(std::complex<double> s) const;
    
    // Properties
    int order() const;
    std::vector<std::complex<double>> poles() const;
    std::vector<std::complex<double>> zeros() const;
    double dcGain() const;
    
    // Coefficients
    const std::vector<double>& numerator() const { return m_num; }
    const std::vector<double>& denominator() const { return m_den; }
    
    // String representation
    std::string toString() const;
    
private:
    std::vector<double> m_num;  // Numerator coefficients
    std::vector<double> m_den;  // Denominator coefficients
};

// ============================================================================
// Frequency Response
// ============================================================================

struct FrequencyResponse {
    std::vector<double> frequency;      // Hz
    std::vector<double> magnitude;      // dB
    std::vector<double> phase;          // degrees
    std::vector<double> magnitudeLin;   // Linear
    std::vector<double> phaseRad;       // Radians
    
    // Stability margins
    double gainMargin;       // dB
    double phaseMargin;      // degrees
    double unityGainFreq;    // Hz
    double phaseCrossoverFreq; // Hz
    
    FrequencyResponse() : gainMargin(0), phaseMargin(0), 
                          unityGainFreq(0), phaseCrossoverFreq(0) {}
};

class BodeAnalyzer {
public:
    static FrequencyResponse analyze(const TransferFunction& tf,
                                      double fStart, double fStop,
                                      int pointsPerDecade = 10);
    
    // Plotting helpers
    static std::vector<double> getMagnitudePlot(const FrequencyResponse& resp);
    static std::vector<double> getPhasePlot(const FrequencyResponse& resp);
    
    // Stability analysis
    static double getPhaseMargin(const FrequencyResponse& resp);
    static double getGainMargin(const FrequencyResponse& resp);
    static double getBandwidth(const FrequencyResponse& resp);
};

// ============================================================================
// Nyquist Analysis
// ============================================================================

struct NyquistData {
    std::vector<double> frequency;
    std::vector<double> real;
    std::vector<double> imag;
    int encirclements;  // Number of encirclements of -1
    bool stable;
    
    NyquistData() : encirclements(0), stable(true) {}
};

class NyquistAnalyzer {
public:
    static NyquistData analyze(const TransferFunction& tf,
                                double fStart, double fStop,
                                int points = 1000);
    
    static int countEncirclements(const NyquistData& data);
    static bool isStable(const NyquistData& data, int openLoopPolesRHP);
};

// ============================================================================
// PID Controller
// ============================================================================

struct PIDCoefficients {
    double Kp;  // Proportional gain
    double Ki;  // Integral gain
    double Kd;  // Derivative gain
    double N;   // Derivative filter coefficient
    
    PIDCoefficients() : Kp(1), Ki(0), Kd(0), N(10) {}
};

struct PIDTuningResult {
    PIDCoefficients coeffs;
    double riseTime;
    double settlingTime;
    double overshoot;
    double steadyStateError;
    std::string method;
};

class PIDTuner {
public:
    // Tuning methods
    static PIDTuningResult zieglerNichols(const TransferFunction& plant);
    static PIDTuningResult cohenCoon(const TransferFunction& plant);
    static PIDTuningResult internalModelControl(const TransferFunction& plant,
                                                 double closedLoopTimeConstant);
    static PIDTuningResult optimize(const TransferFunction& plant,
                                     const std::string& criterion = "ITAE");
    
    // Step response analysis
    static std::vector<double> stepResponse(const TransferFunction& tf,
                                             double tStart, double tStop, double dt);
    static double getRiseTime(const std::vector<double>& response, double dt);
    static double getSettlingTime(const std::vector<double>& response, double dt,
                                   double tolerance = 0.02);
    static double getOvershoot(const std::vector<double>& response);
};

// ============================================================================
// Op-Amp Stability Analyzer
// ============================================================================

struct OpAmpModel {
    std::string name;
    double A0;            // DC open-loop gain (V/V)
    double fp1;           // First pole (Hz)
    double fp2;           // Second pole (Hz)
    double fp3;           // Third pole (Hz)
    double fz1;           // Zero (Hz)
    double GBW;           // Gain-bandwidth product (Hz)
    double slewRate;      // V/μs
    double inputBias;     // Input bias current (A)
    double inputOffset;   // Input offset voltage (V)
    
    OpAmpModel() : A0(1e5), fp1(10), fp2(1e6), fp3(10e6), fz1(0),
                   GBW(1e6), slewRate(1), inputBias(1e-9), inputOffset(1e-3) {}
    
    TransferFunction openLoopTF() const;
};

struct StabilityResult {
    double phaseMargin;
    double gainMargin;
    double unityGainFreq;
    double bandwidth;
    bool stable;
    std::string recommendation;
    
    // Pole/zero locations
    std::vector<std::complex<double>> closedLoopPoles;
    
    // Step response characteristics
    double overshoot;
    double settlingTime;
    double riseTime;
    
    StabilityResult() : phaseMargin(0), gainMargin(0), unityGainFreq(0),
                        bandwidth(0), stable(true),
                        overshoot(0), settlingTime(0), riseTime(0) {}
};

struct CompensationNetwork {
    std::string type;  // "dominant_pole", "lead_lag", "notch"
    double C;          // Compensation capacitor (F)
    double R;          // Compensation resistor (Ω)
    double fz;         // Zero frequency (Hz)
    double fp;         // Pole frequency (Hz)
};

class OpAmpStabilityAnalyzer {
public:
    OpAmpStabilityAnalyzer();
    ~OpAmpStabilityAnalyzer();
    
    // Load op-amp model
    void loadModel(const std::string& modelName);
    void loadModel(const OpAmpModel& model);
    const OpAmpModel& model() const { return m_model; }
    
    // Circuit configuration
    void setNonInverting(double gain);
    void setInverting(double gain);
    void setVoltageFollower();
    void setCustomFeedback(const TransferFunction& beta);
    
    // Stability analysis
    StabilityResult analyze();
    FrequencyResponse loopGainAnalysis();
    NyquistData nyquistAnalysis();
    
    // Compensation
    CompensationNetwork suggestCompensation(double targetPhaseMargin = 60);
    void applyCompensation(const CompensationNetwork& comp);
    
    // Results
    const StabilityResult& lastResult() const { return m_lastResult; }
    
    // Export
    std::string toSPICE() const;
    void plotBode() const;
    void plotNyquist() const;
    void plotStepResponse() const;
    
private:
    OpAmpModel m_model;
    TransferFunction m_feedback;
    double m_closedLoopGain;
    StabilityResult m_lastResult;
    bool m_compensated;
    CompensationNetwork m_compensation;
    
    TransferFunction loopGain() const;
    TransferFunction closedLoop() const;
};

// ============================================================================
// Component Database (Op-Amp Models)
// ============================================================================

class OpAmpDatabase {
public:
    static OpAmpDatabase& instance();
    
    // Get model by name
    OpAmpModel getModel(const std::string& name) const;
    
    // List available models
    std::vector<std::string> listModels() const;
    
    // Search by specification
    std::vector<std::string> search(double minGBW = 0, double maxGBW = 1e12,
                                     double minSR = 0, double maxSR = 1e6) const;
    
private:
    OpAmpDatabase();
    void initializeDatabase();
    
    std::map<std::string, OpAmpModel> m_models;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Transfer function
    void* flux_tf_create(const double* num, int numOrder, const double* den, int denOrder);
    void flux_tf_destroy(void* tf);
    void* flux_tf_multiply(void* tf1, void* tf2);
    void* flux_tf_feedback(void* tf, void* H);
    
    // Bode analysis
    void flux_bode_analyze(void* tf, double fStart, double fStop, int points);
    double flux_bode_get_phase_margin();
    double flux_bode_get_gain_margin();
    double flux_bode_get_bandwidth();
    
    // Op-amp stability
    void* flux_opamp_create(const char* model);
    void flux_opamp_destroy(void* opamp);
    void flux_opamp_set_gain(void* opamp, double gain);
    void flux_opamp_analyze(void* opamp);
    double flux_opamp_get_phase_margin(void* opamp);
    double flux_opamp_get_overshoot(void* opamp);
    const char* flux_opamp_get_recommendation(void* opamp);
    
    // PID tuning
    void* flux_pid_tune(void* plant, const char* method);
    double flux_pid_get_kp(void* pid);
    double flux_pid_get_ki(void* pid);
    double flux_pid_get_kd(void* pid);
}

} // namespace Control
} // namespace Flux

#endif // FLUX_TOOLBOX_CONTROL_H
