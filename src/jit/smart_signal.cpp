// ============================================================================
// FluxScript SmartSignalItem Implementation
// Extends WaveformData with JIT-compiled behavioral signal generation
// ============================================================================

// Undefine Qt macros to avoid conflicts
#ifdef emit
#undef emit
#endif
#ifdef Q_SIGNALS
#undef Q_SIGNALS
#endif

#include "flux/jit/smart_signal.h"
#include "flux/jit/jit_manager.h"
#ifdef FLUX_HAS_QT
#include <QColor>
#include <QVector>
#include <QString>
#endif
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>
#include <map>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Flux {

// ============================================================================
// SmartSignalItem Implementation
// ============================================================================

SmartSignalItem::SmartSignalItem() : m_name("unnamed"), m_source_type(SignalSourceType::Parametric) {
}

SmartSignalItem::SmartSignalItem(const std::string& name, SignalSourceType source_type)
    : m_name(name), m_source_type(source_type) {
}

SmartSignalItem::~SmartSignalItem() {
    clearJITFunction();
}

void SmartSignalItem::setName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_name = name;
}

void SmartSignalItem::setSourceType(SignalSourceType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_source_type = type;
}

void SmartSignalItem::setWaveformType(ParametricWaveformType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waveform_type = type;
}

void SmartSignalItem::setParameters(const SignalParams& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
}

bool SmartSignalItem::compileJITExpression(std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_source_type != SignalSourceType::JITCompiled && 
        m_source_type != SignalSourceType::Behavioral) {
        if (error) *error = "Signal source type is not JIT-compatible";
        return false;
    }
    
    if (m_params.jit_expression.empty()) {
        if (error) *error = "JIT expression is empty";
        return false;
    }
    
    auto& jit_manager = JITManager::instance();
    
    std::string component_source = 
        "update(t, inputs, outputs, state) {\n"
        "    var result = " + m_params.jit_expression + ";\n"
        "    outputs[0] = result;\n"
        "    return result;\n"
        "}\n";
    
    std::string comp_name = "signal_" + m_name;
    
    if (!jit_manager.registerComponent(comp_name, component_source, 1, 1, error)) {
        return false;
    }
    
    if (!jit_manager.compileComponent(comp_name, error)) {
        return false;
    }
    
    m_jit_compiled = true;
    
    std::cout << "[SmartSignal] JIT-compiled expression for signal: " << m_name << std::endl;
    return true;
}

void SmartSignalItem::clearJITFunction() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_jit_compiled = false;
    m_jit_update_func = nullptr;
    m_jit_state = nullptr;
    
    auto& jit_manager = JITManager::instance();
    std::string comp_name = "signal_" + m_name;
    jit_manager.unregisterComponent(comp_name);
}

SignalGenerationResult SmartSignalItem::generateSignal(double t_start, double t_stop, double sample_rate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto start_time = std::chrono::steady_clock::now();
    SignalGenerationResult result;
    result.success = false;
    result.num_points = 0;
    
    switch (m_source_type) {
        case SignalSourceType::Parametric:
            result = generateParametric(t_start, t_stop, sample_rate);
            break;
        case SignalSourceType::JITCompiled:
            result = generateJITCompiled(t_start, t_stop, sample_rate);
            break;
        case SignalSourceType::Imported:
            result = generateImported();
            break;
        case SignalSourceType::Behavioral:
            result = generateBehavioral(t_start, t_stop, sample_rate);
            break;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.generation_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (result.success) {
        m_time_points = result.time_points;
        m_values = result.values;
        result.num_points = m_values.size();
    }
    
    return result;
}

double SmartSignalItem::evaluateAtTime(double time) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_jit_compiled && m_source_type == SignalSourceType::JITCompiled) {
        auto& jit_manager = JITManager::instance();
        std::string comp_name = "signal_" + m_name;
        
        double inputs[1] = { time };
        double outputs[1] = { 0.0 };
        
        if (jit_manager.evaluateComponent(comp_name, time, 0.0, inputs, outputs)) {
            return outputs[0];
        }
    }
    
    switch (m_waveform_type) {
        case ParametricWaveformType::Sine: return generateSine(time);
        case ParametricWaveformType::Square: return generateSquare(time);
        case ParametricWaveformType::Triangle: return generateTriangle(time);
        case ParametricWaveformType::Sawtooth: return generateSawtooth(time);
        case ParametricWaveformType::Pulse: return generatePulse(time);
        case ParametricWaveformType::Exponential: return generateExponential(time);
        case ParametricWaveformType::PiecewiseLinear: return generatePWL(time);
        case ParametricWaveformType::DC: return generateDC(time);
        default: return 0.0;
    }
}

std::vector<double> SmartSignalItem::evaluateAtTimes(const std::vector<double>& times) {
    std::vector<double> results;
    results.reserve(times.size());
    for (double t : times) {
        results.push_back(evaluateAtTime(t));
    }
    return results;
}

#ifdef FLUX_HAS_QT
void SmartSignalItem::setColor(const QColor& color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_color = color;
}
#endif

void SmartSignalItem::setVisible(bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_visible = visible;
}

void SmartSignalItem::setLabel(const std::string& label) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_label = label;
}

double SmartSignalItem::min() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_values.empty()) return 0.0;
    return *std::min_element(m_values.begin(), m_values.end());
}

double SmartSignalItem::max() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_values.empty()) return 0.0;
    return *std::max_element(m_values.begin(), m_values.end());
}

double SmartSignalItem::average() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : m_values) sum += v;
    return sum / m_values.size();
}

double SmartSignalItem::rms() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_values.empty()) return 0.0;
    double sum_sq = 0.0;
    for (double v : m_values) sum_sq += v * v;
    return std::sqrt(sum_sq / m_values.size());
}

double SmartSignalItem::peakToPeak() const {
    return max() - min();
}

std::string SmartSignalItem::toCSV() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "time," << m_name << "\n";
    for (size_t i = 0; i < m_time_points.size(); i++) {
        oss << m_time_points[i] << "," << m_values[i] << "\n";
    }
    return oss.str();
}

bool SmartSignalItem::exportToCSVFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << toCSV();
    return true;
}

// ============================================================================
// Parametric Signal Generators
// ============================================================================

SignalGenerationResult SmartSignalItem::generateParametric(double t_start, double t_stop, double sample_rate) {
    SignalGenerationResult result;
    result.success = true;
    
    double dt = 1.0 / sample_rate;
    int num_points = static_cast<int>((t_stop - t_start) / dt) + 1;
    
    result.time_points.resize(num_points);
    result.values.resize(num_points);
    
    for (int i = 0; i < num_points; i++) {
        double t = t_start + i * dt;
        result.time_points[i] = t;
        
        switch (m_waveform_type) {
            case ParametricWaveformType::Sine:
                result.values[i] = generateSine(t);
                break;
            case ParametricWaveformType::Square:
                result.values[i] = generateSquare(t);
                break;
            case ParametricWaveformType::Triangle:
                result.values[i] = generateTriangle(t);
                break;
            case ParametricWaveformType::Sawtooth:
                result.values[i] = generateSawtooth(t);
                break;
            case ParametricWaveformType::Pulse:
                result.values[i] = generatePulse(t);
                break;
            case ParametricWaveformType::Exponential:
                result.values[i] = generateExponential(t);
                break;
            case ParametricWaveformType::PiecewiseLinear:
                result.values[i] = generatePWL(t);
                break;
            case ParametricWaveformType::DC:
                result.values[i] = generateDC(t);
                break;
        }
    }
    
    return result;
}

SignalGenerationResult SmartSignalItem::generateJITCompiled(double t_start, double t_stop, double sample_rate) {
    SignalGenerationResult result;
    
    if (!m_jit_compiled) {
        result.success = false;
        result.error_message = "JIT expression not compiled";
        return result;
    }
    
    double dt = 1.0 / sample_rate;
    int num_points = static_cast<int>((t_stop - t_start) / dt) + 1;
    
    auto& jit_manager = JITManager::instance();
    std::string comp_name = "signal_" + m_name;
    
    result.time_points.resize(num_points);
    result.values.resize(num_points);
    
    for (int i = 0; i < num_points; i++) {
        double t = t_start + i * dt;
        result.time_points[i] = t;
        
        double inputs[1] = { t };
        double outputs[1] = { 0.0 };
        
        if (jit_manager.evaluateComponent(comp_name, t, dt, inputs, outputs)) {
            result.values[i] = outputs[0];
        } else {
            result.success = false;
            result.error_message = "JIT evaluation failed at t=" + std::to_string(t);
            return result;
        }
    }
    
    result.success = true;
    return result;
}

SignalGenerationResult SmartSignalItem::generateImported() {
    SignalGenerationResult result;
    result.success = false;
    result.error_message = "Import not implemented";
    return result;
}

SignalGenerationResult SmartSignalItem::generateBehavioral(double t_start, double t_stop, double sample_rate) {
    if (!m_jit_compiled) {
        SignalGenerationResult result;
        result.success = false;
        result.error_message = "Behavioral signal requires JIT compilation";
        return result;
    }
    
    return generateJITCompiled(t_start, t_stop, sample_rate);
}

double SmartSignalItem::generateSine(double t) {
    double angular_freq = 2.0 * M_PI * m_params.frequency;
    return m_params.dc_offset + m_params.amplitude * 
           std::sin(angular_freq * (t - m_params.delay) + m_params.phase);
}

double SmartSignalItem::generateSquare(double t) {
    double period = 1.0 / m_params.frequency;
    double phase_t = fmod(t - m_params.delay, period);
    double val = (phase_t < period / 2.0) ? m_params.amplitude : -m_params.amplitude;
    return m_params.dc_offset + val;
}

double SmartSignalItem::generateTriangle(double t) {
    double period = 1.0 / m_params.frequency;
    double phase_t = fmod(t - m_params.delay, period);
    double normalized = phase_t / period;
    
    double val;
    if (normalized < 0.25) {
        val = 4.0 * normalized;
    } else if (normalized < 0.75) {
        val = 2.0 - 4.0 * normalized;
    } else {
        val = 4.0 * normalized - 4.0;
    }
    
    return m_params.dc_offset + m_params.amplitude * val;
}

double SmartSignalItem::generateSawtooth(double t) {
    double period = 1.0 / m_params.frequency;
    double phase_t = fmod(t - m_params.delay, period);
    double normalized = phase_t / period;
    return m_params.dc_offset + m_params.amplitude * (2.0 * normalized - 1.0);
}

double SmartSignalItem::generatePulse(double t) {
    double adjusted_t = t - m_params.delay;
    if (adjusted_t < 0) return m_params.dc_offset;
    
    double period_t = fmod(adjusted_t, m_params.period);
    double val = 0.0;
    
    if (period_t < m_params.rise_time) {
        val = m_params.amplitude * (period_t / m_params.rise_time);
    } else if (period_t < m_params.pulse_width) {
        val = m_params.amplitude;
    } else if (period_t < m_params.pulse_width + m_params.fall_time) {
        val = m_params.amplitude * (1.0 - (period_t - m_params.pulse_width) / m_params.fall_time);
    }
    
    return m_params.dc_offset + val;
}

double SmartSignalItem::generateExponential(double t) {
    double adjusted_t = t - m_params.delay;
    if (adjusted_t < 0) return m_params.dc_offset;
    
    double val;
    if (adjusted_t < m_params.pulse_width) {
        val = m_params.amplitude * (1.0 - std::exp(-adjusted_t / m_params.tau_rise));
    } else {
        val = m_params.amplitude * std::exp(-(adjusted_t - m_params.pulse_width) / m_params.tau_fall);
    }
    
    return m_params.dc_offset + val;
}

double SmartSignalItem::generatePWL(double t) {
    if (m_params.pwl_points.empty()) return m_params.dc_offset;
    
    if (t <= m_params.pwl_points.front().first) {
        return m_params.dc_offset + m_params.pwl_points.front().second;
    }
    if (t >= m_params.pwl_points.back().first) {
        return m_params.dc_offset + m_params.pwl_points.back().second;
    }
    
    for (size_t i = 0; i < m_params.pwl_points.size() - 1; i++) {
        if (t >= m_params.pwl_points[i].first && t < m_params.pwl_points[i + 1].first) {
            double t1 = m_params.pwl_points[i].first;
            double t2 = m_params.pwl_points[i + 1].first;
            double v1 = m_params.pwl_points[i].second;
            double v2 = m_params.pwl_points[i + 1].second;
            
            double frac = (t - t1) / (t2 - t1);
            return m_params.dc_offset + v1 + frac * (v2 - v1);
        }
    }
    
    return m_params.dc_offset;
}

double SmartSignalItem::generateDC(double /*t*/) {
    return m_params.dc_offset;
}

// ============================================================================
// SignalGroup Implementation
// ============================================================================

void SignalGroup::addSignal(std::shared_ptr<SmartSignalItem> signal) {
    signals.push_back(signal);
}

void SignalGroup::removeSignal(const std::string& sig_name) {
    signals.erase(
        std::remove_if(signals.begin(), signals.end(),
                      [&sig_name](const std::shared_ptr<SmartSignalItem>& s) {
                          return s->name() == sig_name;
                      }),
        signals.end()
    );
}

std::shared_ptr<SmartSignalItem> SignalGroup::getSignal(const std::string& name) const {
    for (const auto& signal : signals) {
        if (signal->name() == name) return signal;
    }
    return nullptr;
}

SignalGenerationResult SignalGroup::generateAll() {
    SignalGenerationResult combined_result;
    combined_result.success = true;
    
    for (const auto& signal : signals) {
        auto result = signal->generateSignal();
        if (!result.success) {
            combined_result.success = false;
            combined_result.error_message += "Failed to generate " + signal->name() + ": " + result.error_message + "; ";
        }
    }
    
    return combined_result;
}

// ============================================================================
// SmartSignalManager Implementation
// ============================================================================

SmartSignalManager& SmartSignalManager::instance() {
    static SmartSignalManager instance;
    return instance;
}

bool SmartSignalManager::addSignal(std::shared_ptr<SmartSignalItem> signal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!signal) return false;
    if (m_signals.count(signal->name())) return false;
    m_signals[signal->name()] = signal;
    return true;
}

bool SmartSignalManager::removeSignal(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signals.erase(name) > 0;
}

std::shared_ptr<SmartSignalItem> SmartSignalManager::getSignal(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_signals.find(name);
    if (it == m_signals.end()) return nullptr;
    return it->second;
}

const std::shared_ptr<SmartSignalItem> SmartSignalManager::getSignal(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_signals.find(name);
    if (it == m_signals.end()) return nullptr;
    return it->second;
}

std::vector<std::string> SmartSignalManager::getSignalNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, signal] : m_signals) {
        names.push_back(name);
    }
    return names;
}

bool SmartSignalManager::hasSignal(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signals.count(name) > 0;
}

bool SmartSignalManager::createGroup(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_groups.count(name)) return false;
    m_groups[name].name = name;
    return true;
}

bool SmartSignalManager::addToGroup(const std::string& group_name, const std::string& signal_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto git = m_groups.find(group_name);
    if (git == m_groups.end()) return false;
    auto sit = m_signals.find(signal_name);
    if (sit == m_signals.end()) return false;
    git->second.addSignal(sit->second);
    return true;
}

SignalGroup* SmartSignalManager::getGroup(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_groups.find(name);
    if (it == m_groups.end()) return nullptr;
    return &it->second;
}

SignalGenerationResult SmartSignalManager::generateAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    SignalGenerationResult result;
    result.success = true;
    
    for (auto& [name, signal] : m_signals) {
        auto sig_result = signal->generateSignal();
        if (!sig_result.success) {
            result.success = false;
            result.error_message += "Failed: " + name + "; ";
        }
    }
    
    return result;
}

SignalGenerationResult SmartSignalManager::generateGroup(const std::string& group_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_groups.find(group_name);
    if (it == m_groups.end()) {
        return SignalGenerationResult{ {}, {}, false, "Group not found", 0, 0 };
    }
    return it->second.generateAll();
}

double SmartSignalManager::evaluateSignal(const std::string& name, double time) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_signals.find(name);
    if (it == m_signals.end()) return 0.0;
    return it->second->evaluateAtTime(time);
}

std::map<std::string, double> SmartSignalManager::evaluateAll(double time) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, double> results;
    for (const auto& [name, signal] : m_signals) {
        results[name] = signal->evaluateAtTime(time);
    }
    return results;
}

bool SmartSignalManager::importSignalsFromCSV(const std::string& filename) {
    std::cout << "[SmartSignalManager] Import from CSV: " << filename << std::endl;
    return false;
}

bool SmartSignalManager::exportSignalsToCSV(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    
    file << "time";
    for (const auto& [name, signal] : m_signals) {
        file << "," << name;
    }
    file << "\n";
    
    if (!m_signals.empty()) {
        const auto& first_signal = m_signals.begin()->second;
        const auto& times = first_signal->timePoints();
        
        for (size_t i = 0; i < times.size(); i++) {
            file << times[i];
            for (const auto& [name, signal] : m_signals) {
                const auto& values = signal->values();
                if (i < values.size()) {
                    file << "," << values[i];
                } else {
                    file << ",";
                }
            }
            file << "\n";
        }
    }
    
    return true;
}

int SmartSignalManager::signalCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signals.size();
}

int SmartSignalManager::groupCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_groups.size();
}

} // namespace Flux
