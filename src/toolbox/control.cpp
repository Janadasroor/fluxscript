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

// FluxScript Control Systems Toolbox - Implementation
// Feature #5: Op-Amp Stability Analyzer

#include "flux/toolbox/control.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Flux {
namespace Control {

// ============================================================================
// TransferFunction Implementation
// ============================================================================

TransferFunction::TransferFunction() {}

TransferFunction::TransferFunction(const std::vector<double>& num, const std::vector<double>& den)
    : m_num(num), m_den(den)
{
}

TransferFunction::~TransferFunction() = default;

TransferFunction TransferFunction::gain(double K)
{
    return TransferFunction({K}, {1});
}

TransferFunction TransferFunction::integrator()
{
    return TransferFunction({1}, {1, 0});
}

TransferFunction TransferFunction::differentiator()
{
    return TransferFunction({1, 0}, {1});
}

TransferFunction TransferFunction::firstOrder(double K, double tau)
{
    return TransferFunction({K}, {tau, 1});
}

TransferFunction TransferFunction::secondOrder(double K, double wn, double zeta)
{
    return TransferFunction({K * wn * wn}, {1, 2 * zeta * wn, wn * wn});
}

TransferFunction TransferFunction::pid(double Kp, double Ki, double Kd)
{
    return TransferFunction({Kd, Kp, Ki}, {1, 0});
}

TransferFunction TransferFunction::operator*(const TransferFunction& other) const
{
    // Polynomial multiplication
    std::vector<double> num(m_num.size() + other.m_num.size() - 1, 0);
    std::vector<double> den(m_den.size() + other.m_den.size() - 1, 0);

    for (size_t i = 0; i < m_num.size(); ++i)
        for (size_t j = 0; j < other.m_num.size(); ++j)
            num[i + j] += m_num[i] * other.m_num[j];

    for (size_t i = 0; i < m_den.size(); ++i)
        for (size_t j = 0; j < other.m_den.size(); ++j)
            den[i + j] += m_den[i] * other.m_den[j];

    return TransferFunction(num, den);
}

TransferFunction TransferFunction::operator+(const TransferFunction& other) const
{
    // Need common denominator - simplified implementation
    return TransferFunction(m_num, m_den); // Placeholder
}

TransferFunction TransferFunction::operator/(const TransferFunction& other) const
{
    return TransferFunction(m_num, m_den) * TransferFunction(other.m_den, other.m_num);
}

TransferFunction TransferFunction::feedback(const TransferFunction& H) const
{
    // Closed-loop: G / (1 + GH)
    // Simplified for unity feedback
    std::vector<double> num = m_num;
    std::vector<double> den = m_den;

    // Add feedback
    if (den.size() < num.size())
        den.resize(num.size(), 0);
    den[den.size() - 1] += num[num.size() - 1];

    return TransferFunction(num, den);
}

std::complex<double> TransferFunction::eval(double omega) const
{
    return eval(std::complex<double>(0, omega));
}

std::complex<double> TransferFunction::eval(std::complex<double> s) const
{
    // Evaluate numerator and denominator at s
    std::complex<double> num(0, 0), den(0, 0);
    std::complex<double> sPow(1, 0);

    for (int i = m_num.size() - 1; i >= 0; --i) {
        num += m_num[i] * sPow;
        sPow *= s;
    }

    sPow = std::complex<double>(1, 0);
    for (int i = m_den.size() - 1; i >= 0; --i) {
        den += m_den[i] * sPow;
        sPow *= s;
    }

    return num / den;
}

int TransferFunction::order() const
{
    return m_den.size() - 1;
}

double TransferFunction::dcGain() const
{
    if (m_den.empty())
        return 1;
    return m_num.back() / m_den.back();
}

// ============================================================================
// NyquistAnalyzer Implementation (stub)
// ============================================================================

NyquistData NyquistAnalyzer::analyze(const TransferFunction& tf, double fStart, double fStop, int points)
{
    NyquistData data;

    for (int i = 0; i <= points; ++i) {
        double logF = std::log10(fStart) + (double)i / points * std::log10(fStop / fStart);
        double f = std::pow(10, logF);
        double omega = 2 * M_PI * f;

        std::complex<double> H = tf.eval(omega);

        data.frequency.push_back(f);
        data.real.push_back(H.real());
        data.imag.push_back(H.imag());
    }

    data.encirclements = countEncirclements(data);
    data.stable = isStable(data, 0);

    return data;
}

int NyquistAnalyzer::countEncirclements(const NyquistData& data)
{
    // Simplified encirclement counting
    int encirclements = 0;
    for (size_t i = 1; i < data.real.size(); ++i) {
        double angle1 = std::atan2(data.imag[i - 1], data.real[i - 1] + 1);
        double angle2 = std::atan2(data.imag[i], data.real[i] + 1);
        double dAngle = angle2 - angle1;

        if (dAngle > M_PI)
            dAngle -= 2 * M_PI;
        if (dAngle < -M_PI)
            dAngle += 2 * M_PI;

        encirclements += (dAngle > 0) ? 1 : -1;
    }

    return std::abs(encirclements);
}

bool NyquistAnalyzer::isStable(const NyquistData& data, int openLoopPolesRHP)
{
    // Nyquist stability criterion: Z = N + P
    // For stability, Z (closed-loop RHP poles) must be 0
    return (data.encirclements == openLoopPolesRHP);
}

std::string TransferFunction::toString() const
{
    std::ostringstream oss;
    oss << "Num: [";
    for (size_t i = 0; i < m_num.size(); ++i) {
        oss << m_num[i];
        if (i < m_num.size() - 1)
            oss << ", ";
    }
    oss << "], Den: [";
    for (size_t i = 0; i < m_den.size(); ++i) {
        oss << m_den[i];
        if (i < m_den.size() - 1)
            oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// BodeAnalyzer Implementation
// ============================================================================

FrequencyResponse BodeAnalyzer::analyze(const TransferFunction& tf, double fStart, double fStop, int pointsPerDecade)
{
    FrequencyResponse resp;

    int nDecades = static_cast<int>(std::log10(fStop / fStart));
    int nPoints = nDecades * pointsPerDecade;

    double gainCross = 0;
    double phaseCross = 0;

    for (int i = 0; i <= nPoints; ++i) {
        double logF = std::log10(fStart) + (double)i / pointsPerDecade;
        double f = std::pow(10, logF);
        double omega = 2 * M_PI * f;

        std::complex<double> H = tf.eval(omega);
        double mag = std::abs(H);
        double phase = std::arg(H) * 180 / M_PI;

        resp.frequency.push_back(f);
        resp.magnitude.push_back(20 * std::log10(mag)); // dB
        resp.phase.push_back(phase);
        resp.magnitudeLin.push_back(mag);
        resp.phaseRad.push_back(std::arg(H));

        // Find crossover frequencies
        if (i > 0) {
            if (resp.magnitudeLin[i - 1] > 1 && resp.magnitudeLin[i] <= 1) {
                gainCross = f;
            }
            if (resp.phase[i - 1] > -180 && resp.phase[i] <= -180) {
                phaseCross = f;
            }
        }
    }

    // Calculate margins
    if (gainCross > 0) {
        // Find phase at gain crossover
        for (size_t i = 0; i < resp.frequency.size(); ++i) {
            if (resp.frequency[i] >= gainCross) {
                resp.phaseMargin = 180 + resp.phase[i];
                resp.unityGainFreq = gainCross;
                break;
            }
        }
    }

    if (phaseCross > 0) {
        // Find gain at phase crossover
        for (size_t i = 0; i < resp.frequency.size(); ++i) {
            if (resp.frequency[i] >= phaseCross) {
                resp.gainMargin = -resp.magnitude[i];
                resp.phaseCrossoverFreq = phaseCross;
                break;
            }
        }
    }

    return resp;
}

double BodeAnalyzer::getPhaseMargin(const FrequencyResponse& resp)
{
    return resp.phaseMargin;
}

double BodeAnalyzer::getGainMargin(const FrequencyResponse& resp)
{
    return resp.gainMargin;
}

double BodeAnalyzer::getBandwidth(const FrequencyResponse& resp)
{
    // -3dB bandwidth
    double dcMag = resp.magnitudeLin[0];
    double threshold = dcMag / std::sqrt(2);

    for (size_t i = 0; i < resp.magnitudeLin.size(); ++i) {
        if (resp.magnitudeLin[i] <= threshold) {
            return resp.frequency[i];
        }
    }
    return resp.frequency.back();
}

// ============================================================================
// OpAmpModel Implementation
// ============================================================================

TransferFunction OpAmpModel::openLoopTF() const
{
    // A(s) = A0 / ((1 + s/wp1)(1 + s/wp2)(1 + s/wp3))
    // Simplified to 2 poles

    double wp1 = 2 * M_PI * fp1;
    double wp2 = 2 * M_PI * fp2;

    // Numerator: A0 * wp1 * wp2
    std::vector<double> num = {A0 * wp1 * wp2};

    // Denominator: s^2 + (wp1+wp2)s + wp1*wp2
    std::vector<double> den = {1, wp1 + wp2, wp1 * wp2};

    return TransferFunction(num, den);
}

// ============================================================================
// OpAmpStabilityAnalyzer Implementation
// ============================================================================

OpAmpStabilityAnalyzer::OpAmpStabilityAnalyzer() : m_closedLoopGain(1), m_compensated(false) {}

OpAmpStabilityAnalyzer::~OpAmpStabilityAnalyzer() = default;

void OpAmpStabilityAnalyzer::loadModel(const std::string& modelName)
{
    m_model = OpAmpDatabase::instance().getModel(modelName);
}

void OpAmpStabilityAnalyzer::loadModel(const OpAmpModel& model)
{
    m_model = model;
}

void OpAmpStabilityAnalyzer::setNonInverting(double gain)
{
    m_closedLoopGain = gain;
    // Beta = 1/gain for non-inverting
    double beta = 1.0 / gain;
    m_feedback = TransferFunction::gain(beta);
}

void OpAmpStabilityAnalyzer::setInverting(double gain)
{
    m_closedLoopGain = std::abs(gain);
    double beta = 1.0 / (1 + std::abs(gain));
    m_feedback = TransferFunction::gain(beta);
}

void OpAmpStabilityAnalyzer::setVoltageFollower()
{
    m_closedLoopGain = 1;
    m_feedback = TransferFunction::gain(1);
}

TransferFunction OpAmpStabilityAnalyzer::loopGain() const
{
    // T(s) = A(s) * beta
    TransferFunction A = m_model.openLoopTF();
    return A * m_feedback;
}

TransferFunction OpAmpStabilityAnalyzer::closedLoop() const
{
    // ACL(s) = A(s) / (1 + A(s)*beta)
    TransferFunction A = m_model.openLoopTF();
    TransferFunction T = loopGain();

    std::vector<double> num = A.numerator();
    std::vector<double> den = A.denominator();

    // Add feedback (simplified)
    if (den.size() < num.size())
        den.resize(num.size(), 0);
    den[den.size() - 1] += num[num.size() - 1];

    return TransferFunction(num, den);
}

StabilityResult OpAmpStabilityAnalyzer::analyze()
{
    TransferFunction T = loopGain();

    // Bode analysis
    FrequencyResponse resp = BodeAnalyzer::analyze(T, 1, m_model.GBW * 10, 20);

    m_lastResult.phaseMargin = resp.phaseMargin;
    m_lastResult.gainMargin = resp.gainMargin;
    m_lastResult.unityGainFreq = resp.unityGainFreq;
    m_lastResult.bandwidth = BodeAnalyzer::getBandwidth(resp);

    // Stability criterion
    m_lastResult.stable = (m_lastResult.phaseMargin > 0) && (m_lastResult.phaseMargin < 180);

    // Step response characteristics (simplified)
    double zeta = m_lastResult.phaseMargin / 100.0; // Approximation
    m_lastResult.overshoot = std::exp(-M_PI * zeta / std::sqrt(1 - zeta * zeta)) * 100;
    m_lastResult.settlingTime = 4.0 / (2 * M_PI * m_lastResult.bandwidth * zeta);
    m_lastResult.riseTime = 0.35 / m_lastResult.bandwidth;

    // Recommendation
    if (m_lastResult.phaseMargin < 30) {
        m_lastResult.recommendation = "CRITICAL: Add compensation immediately!";
    } else if (m_lastResult.phaseMargin < 45) {
        m_lastResult.recommendation = "WARNING: Marginal stability. Consider compensation.";
    } else if (m_lastResult.phaseMargin < 60) {
        m_lastResult.recommendation = "Acceptable, but could be improved.";
    } else {
        m_lastResult.recommendation = "Good stability margin.";
    }

    std::cout << "\n=== Op-Amp Stability Analysis ===\n";
    std::cout << "Model: " << m_model.name << "\n";
    std::cout << "Closed-loop Gain: " << m_closedLoopGain << "\n\n";
    std::cout << "Phase Margin: " << m_lastResult.phaseMargin << "\n";
    std::cout << "Gain Margin: " << m_lastResult.gainMargin << " dB\n";
    std::cout << "Unity Gain Freq: " << (m_lastResult.unityGainFreq / 1e6) << " MHz\n";
    std::cout << "Bandwidth: " << (m_lastResult.bandwidth / 1e6) << " MHz\n";
    std::cout << "Stable: " << (m_lastResult.stable ? "Yes" : "No") << "\n";
    std::cout << "Recommendation: " << m_lastResult.recommendation << "\n";

    return m_lastResult;
}

FrequencyResponse OpAmpStabilityAnalyzer::loopGainAnalysis()
{
    TransferFunction T = loopGain();
    return BodeAnalyzer::analyze(T, 1, m_model.GBW * 10, 20);
}

NyquistData OpAmpStabilityAnalyzer::nyquistAnalysis()
{
    return NyquistAnalyzer::analyze(loopGain(), 1, m_model.GBW * 10, 1000);
}

CompensationNetwork OpAmpStabilityAnalyzer::suggestCompensation(double targetPhaseMargin)
{
    CompensationNetwork comp;

    if (m_lastResult.phaseMargin >= targetPhaseMargin) {
        comp.type = "none";
        std::cout << "No compensation needed.\n";
        return comp;
    }

    // Dominant pole compensation
    double additionalPhaseNeeded = targetPhaseMargin - m_lastResult.phaseMargin;

    // Place compensation pole at lower frequency
    comp.type = "dominant_pole";
    comp.fp = m_lastResult.unityGainFreq / std::pow(10, additionalPhaseNeeded / 20);
    comp.C = 1.0 / (2 * M_PI * comp.fp * 10000); // Assume R = 10k

    std::cout << "\n=== Compensation Suggestion ===\n";
    std::cout << "Type: Dominant pole\n";
    std::cout << "Compensation pole: " << (comp.fp / 1e6) << " MHz\n";
    std::cout << "Compensation capacitor: " << (comp.C * 1e12) << " pF\n";

    m_compensated = true;
    m_compensation = comp;

    return comp;
}

void OpAmpStabilityAnalyzer::applyCompensation(const CompensationNetwork& comp)
{
    m_compensated = true;
    m_compensation = comp;
    std::cout << "Compensation applied. Re-analyze for updated results.\n";
}

std::string OpAmpStabilityAnalyzer::toSPICE() const
{
    std::ostringstream oss;
    oss << "* Op-Amp Stability Analysis\n";
    oss << "* Model: " << m_model.name << "\n";
    oss << "* GBW: " << (m_model.GBW / 1e6) << " MHz\n";
    oss << "* SR: " << m_model.slewRate << " V/us\n\n";

    oss << "* Open-loop gain\n";
    oss << "EOL 1 0 POLY(1) 2 0 " << m_model.A0 << "\n";
    oss << "ROL 1 0 1MEG\n";
    oss << "CPL 1 0 " << (1.0 / (2 * M_PI * m_model.fp1 * 1e6)) << "\n\n";

    oss << "* Feedback network\n";
    double R2 = 10000;
    double R1 = R2 * (m_closedLoopGain - 1);
    oss << "R1 OUT FB " << R1 << "\n";
    oss << "R2 FB 0 " << R2 << "\n";

    return oss.str();
}

// ============================================================================
// OpAmpDatabase Implementation
// ============================================================================

OpAmpDatabase& OpAmpDatabase::instance()
{
    static OpAmpDatabase db;
    return db;
}

OpAmpDatabase::OpAmpDatabase()
{
    initializeDatabase();
}

void OpAmpDatabase::initializeDatabase()
{
    // Precision op-amps
    OpAmpModel opa211;
    opa211.name = "OPA211";
    opa211.A0 = 1e6; // 120 dB
    opa211.fp1 = 10;
    opa211.fp2 = 1e6;
    opa211.GBW = 10e6; // 10 MHz
    opa211.slewRate = 27;
    m_models["opa211"] = opa211;

    OpAmpModel opa140;
    opa140.name = "OPA140";
    opa140.A0 = 1e5; // 100 dB
    opa140.fp1 = 5;
    opa140.fp2 = 500e3;
    opa140.GBW = 11e6; // 11 MHz
    opa140.slewRate = 20;
    m_models["opa140"] = opa140;

    // High-speed op-amps
    OpAmpModel ths3091;
    ths3091.name = "THS3091";
    ths3091.A0 = 5e4; // 94 dB
    ths3091.fp1 = 100;
    ths3091.fp2 = 10e6;
    ths3091.GBW = 210e6; // 210 MHz
    ths3091.slewRate = 7300;
    m_models["ths3091"] = ths3091;

    // General purpose
    OpAmpModel lm741;
    lm741.name = "LM741";
    lm741.A0 = 2e5; // 106 dB
    lm741.fp1 = 5;
    lm741.fp2 = 1e6;
    lm741.GBW = 1e6; // 1 MHz
    lm741.slewRate = 0.5;
    m_models["lm741"] = lm741;

    OpAmpModel tl081;
    tl081.name = "TL081";
    tl081.A0 = 2e5;
    tl081.fp1 = 10;
    tl081.fp2 = 3e6;
    tl081.GBW = 3e6; // 3 MHz
    tl081.slewRate = 13;
    m_models["tl081"] = tl081;

    // Rail-to-rail
    OpAmpModel opa333;
    opa333.name = "OPA333";
    opa333.A0 = 1e6;
    opa333.fp1 = 5;
    opa333.fp2 = 500e3;
    opa333.GBW = 350e3; // 350 kHz
    opa333.slewRate = 0.16;
    m_models["opa333"] = opa333;
}

OpAmpModel OpAmpDatabase::getModel(const std::string& name) const
{
    auto it = m_models.find(name);
    if (it != m_models.end()) {
        return it->second;
    }
    std::cerr << "Warning: Op-amp model '" << name << "' not found. Using OPA211.\n";
    return m_models.at("opa211");
}

std::vector<std::string> OpAmpDatabase::listModels() const
{
    std::vector<std::string> names;
    for (const auto& pair : m_models) {
        names.push_back(pair.first);
    }
    return names;
}

// ============================================================================
// C Interface Implementation
// ============================================================================

static OpAmpStabilityAnalyzer* g_currentOpAmp = nullptr;

extern "C" {

void* flux_tf_create(const double* num, int numOrder, const double* den, int denOrder)
{
    std::vector<double> numVec(num, num + numOrder + 1);
    std::vector<double> denVec(den, den + denOrder + 1);
    return new TransferFunction(numVec, denVec);
}

void flux_tf_destroy(void* tf)
{
    delete static_cast<TransferFunction*>(tf);
}

void* flux_opamp_create(const char* model)
{
    auto analyzer = new OpAmpStabilityAnalyzer();
    analyzer->loadModel(model ? model : "opa211");
    g_currentOpAmp = analyzer;
    return analyzer;
}

void flux_opamp_destroy(void* opamp)
{
    delete static_cast<OpAmpStabilityAnalyzer*>(opamp);
}

void flux_opamp_set_gain(void* opamp, double gain)
{
    static_cast<OpAmpStabilityAnalyzer*>(opamp)->setNonInverting(gain);
}

void flux_opamp_analyze(void* opamp)
{
    static_cast<OpAmpStabilityAnalyzer*>(opamp)->analyze();
}

double flux_opamp_get_phase_margin(void* opamp)
{
    return static_cast<OpAmpStabilityAnalyzer*>(opamp)->lastResult().phaseMargin;
}

double flux_opamp_get_overshoot(void* opamp)
{
    return static_cast<OpAmpStabilityAnalyzer*>(opamp)->lastResult().overshoot;
}

const char* flux_opamp_get_recommendation(void* opamp)
{
    static std::string rec;
    rec = static_cast<OpAmpStabilityAnalyzer*>(opamp)->lastResult().recommendation;
    return rec.c_str();
}
}

} // namespace Control
} // namespace Flux
