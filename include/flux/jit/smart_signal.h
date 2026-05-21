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

// ============================================================================
// FluxScript SmartSignalItem - VioSpice JIT Signal Integration
// Extends WaveformData with JIT-compiled behavioral signal generation
// ============================================================================

#ifndef FLUX_SMART_SIGNAL_H
#define FLUX_SMART_SIGNAL_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Include Qt headers if available, otherwise use stub types
#ifdef Q_SIGNALS
#undef Q_SIGNALS
#endif
#endif

#include "flux/compiler/ast.h"

namespace Flux {

// Signal source types
enum class SignalSourceType
{
    JITCompiled, // JIT-compiled FluxScript expression
    Parametric,  // Parameterized waveform (sine, pulse, etc.)
    Imported,    // Imported from CSV/file
    Behavioral   // Behavioral (driven by JIT update function)
};

// Parametric waveform types (for non-JIT signals)
enum class ParametricWaveformType
{
    DC,
    Sine,
    Square,
    Triangle,
    Sawtooth,
    Pulse,
    Exponential,
    PiecewiseLinear
};

// Signal generation parameters
struct SignalParams
{
    // DC parameters
    double dc_offset = 0.0;

    // AC parameters
    double amplitude = 1.0;
    double frequency = 1e3; // 1 kHz default
    double phase = 0.0;

    // Pulse parameters
    double rise_time = 1e-9;
    double fall_time = 1e-9;
    double pulse_width = 500e-6;
    double period = 1e-3;
    double delay = 0.0;

    // Exponential parameters
    double tau_rise = 1e-6;
    double tau_fall = 1e-6;

    // Piecewise linear points
    std::vector<std::pair<double, double>> pwl_points;

    // JIT expression
    std::string jit_expression;

    // Common
    double t_start = 0.0;
    double t_stop = 1e-3;
    double sample_rate = 1e6; // 1 MHz default
};

// Signal generation result
struct SignalGenerationResult
{
    std::vector<double> time_points;
    std::vector<double> values;
    bool success;
    std::string error_message;
    double generation_time_ms;
    int num_points;
};

// Smart Signal Item - extends basic waveform with JIT capabilities
class SmartSignalItem
{
public:
    SmartSignalItem();
    SmartSignalItem(const std::string& name, SignalSourceType source_type);
    ~SmartSignalItem();

    // Configuration
    void setName(const std::string& name);
    const std::string& name() const { return m_name; }

    void setSourceType(SignalSourceType type);
    SignalSourceType sourceType() const { return m_source_type; }

    void setWaveformType(ParametricWaveformType type);
    ParametricWaveformType waveformType() const { return m_waveform_type; }

    void setParameters(const SignalParams& params);
    const SignalParams& parameters() const { return m_params; }

    // JIT compilation
    bool compileJITExpression(std::string* error = nullptr);
    bool isJITCompiled() const { return m_jit_compiled; }
    void clearJITFunction();

    // Signal generation
    SignalGenerationResult generateSignal(double t_start, double t_stop, double sample_rate);
    SignalGenerationResult generateSignal()
    {
        return generateSignal(m_params.t_start, m_params.t_stop, m_params.sample_rate);
    }

    // Real-time evaluation (for JIT-compiled signals)
    double evaluateAtTime(double time);
    std::vector<double> evaluateAtTimes(const std::vector<double>& times);

    // Waveform data access
    const std::vector<double>& timePoints() const { return m_time_points; }
    const std::vector<double>& values() const { return m_values; }
    size_t numPoints() const { return m_values.size(); }

    // Display properties

    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    void setLabel(const std::string& label);
    const std::string& label() const { return m_label; }

    // Statistics
    double min() const;
    double max() const;
    double average() const;
    double rms() const;
    double peakToPeak() const;

    // Export
    std::string toCSV() const;
    bool exportToCSVFile(const std::string& filename) const;

private:
    SignalGenerationResult generateParametric(double t_start, double t_stop, double sample_rate);
    SignalGenerationResult generateJITCompiled(double t_start, double t_stop, double sample_rate);
    SignalGenerationResult generateImported();
    SignalGenerationResult generateBehavioral(double t_start, double t_stop, double sample_rate);

    // Parametric generators
    double generateSine(double t);
    double generateSquare(double t);
    double generateTriangle(double t);
    double generateSawtooth(double t);
    double generatePulse(double t);
    double generateExponential(double t);
    double generatePWL(double t);
    double generateDC(double t);

    // Member variables
    std::string m_name;
    std::string m_label;
    SignalSourceType m_source_type = SignalSourceType::Parametric;
    ParametricWaveformType m_waveform_type = ParametricWaveformType::Sine;
    SignalParams m_params;

    // JIT state
    bool m_jit_compiled = false;
    void* m_jit_update_func = nullptr; // double (*)(double time, void* state)
    void* m_jit_state = nullptr;

    // Generated data
    std::vector<double> m_time_points;
    std::vector<double> m_values;

    // Display
    bool m_visible = true;

    // Thread safety
    mutable std::mutex m_mutex;
};

// Signal group - manages related signals
struct SignalGroup
{
    std::string name;
    std::vector<std::shared_ptr<SmartSignalItem>> signals;

    void addSignal(std::shared_ptr<SmartSignalItem> signal);
    void removeSignal(const std::string& name);
    std::shared_ptr<SmartSignalItem> getSignal(const std::string& name) const;

    SignalGenerationResult generateAll();
};

// Smart Signal Manager - manages all signal items
class SmartSignalManager
{
public:
    static SmartSignalManager& instance();

    // Signal management
    bool addSignal(std::shared_ptr<SmartSignalItem> signal);
    bool removeSignal(const std::string& name);
    std::shared_ptr<SmartSignalItem> getSignal(const std::string& name);
    const std::shared_ptr<SmartSignalItem> getSignal(const std::string& name) const;
    std::vector<std::string> getSignalNames() const;
    bool hasSignal(const std::string& name) const;

    // Group management
    bool createGroup(const std::string& name);
    bool addToGroup(const std::string& group_name, const std::string& signal_name);
    SignalGroup* getGroup(const std::string& name);

    // Bulk operations
    SignalGenerationResult generateAll();
    SignalGenerationResult generateGroup(const std::string& group_name);

    // Real-time evaluation
    double evaluateSignal(const std::string& name, double time);
    std::map<std::string, double> evaluateAll(double time);

    // Import/Export
    bool importSignalsFromCSV(const std::string& filename);
    bool exportSignalsToCSV(const std::string& filename) const;

    // Statistics
    int signalCount() const;
    int groupCount() const;

private:
    SmartSignalManager() = default;
    ~SmartSignalManager() = default;

    std::map<std::string, std::shared_ptr<SmartSignalItem>> m_signals;
    std::map<std::string, SignalGroup> m_groups;
    mutable std::mutex m_mutex;
};

} // namespace Flux
