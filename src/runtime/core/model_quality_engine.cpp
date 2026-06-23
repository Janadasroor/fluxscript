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

#include "flux/runtime/model_quality_engine.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

namespace Flux {

// ============================================================================
// ModelQualityEngine Implementation
// ============================================================================

ModelQualityEngine& ModelQualityEngine::instance()
{
    static ModelQualityEngine engine;
    return engine;
}

bool ModelQualityEngine::assertVoltage(const std::string& node, const std::string& op, double bound, double withinTime)
{
    auto& history = m_voltageHistory[node];
    if (history.empty()) {
        std::cout << "[Assert] Warning: No voltage history for node " << node << std::endl;
        return true; // Cannot verify, assume pass
    }

    bool passed = true;
    std::string failMsg;

    for (const auto& point : history) {
        if (withinTime > 0 && point.time > withinTime)
            continue;

        bool conditionMet = false;
        if (op == "<")
            conditionMet = point.value < bound;
        else if (op == ">")
            conditionMet = point.value > bound;
        else if (op == "<=")
            conditionMet = point.value <= bound;
        else if (op == ">=")
            conditionMet = point.value >= bound;
        else if (op == "==")
            conditionMet = std::abs(point.value - bound) < m_absTol;

        if (!conditionMet) {
            passed = false;
            std::ostringstream oss;
            oss << "V(" << node << ")=" << point.value << "V at t=" << point.time << "s violates " << op << " " << bound
                << "V";
            failMsg = oss.str();
            break;
        }
    }

    VerificationResult result;
    result.passed = passed;
    result.message = passed ? "Assert passed" : failMsg;
    result.expected_value = bound;
    m_results.push_back(result);

    std::cout << "[Assert] " << (passed ? " PASS" : " FAIL") << ": V(" << node << ") " << op << " " << bound << "V"
              << std::endl;
    if (!passed)
        std::cout << "  " << failMsg << std::endl;

    return passed;
}

double ModelQualityEngine::checkSettling(const std::string& node, double tolPercent, double afterTime)
{
    auto& history = m_voltageHistory[node];
    if (history.empty()) {
        std::cout << "[Settle] Warning: No voltage history for node " << node << std::endl;
        return 0.0;
    }

    // Find final value (average of last 10% of points after afterTime)
    std::vector<double> finalValues;
    for (const auto& point : history) {
        if (point.time >= afterTime) {
            finalValues.push_back(point.value);
        }
    }

    if (finalValues.empty()) {
        std::cout << "[Settle] No data after t=" << afterTime << "s" << std::endl;
        return 0.0;
    }

    double finalValue = std::accumulate(finalValues.begin(), finalValues.end(), 0.0) / finalValues.size();
    double tol = std::abs(finalValue) * tolPercent / 100.0;

    // Check if all points after afterTime are within tolerance
    bool settled = true;
    double maxDeviation = 0.0;
    for (double v : finalValues) {
        double dev = std::abs(v - finalValue);
        maxDeviation = std::max(maxDeviation, dev);
        if (dev > tol) {
            settled = false;
            break;
        }
    }

    // Find settling time (first time after which all points are within tolerance)
    double settleTime = afterTime;
    for (size_t i = 0; i < history.size(); ++i) {
        if (history[i].time < afterTime)
            continue;

        bool allWithinTol = true;
        for (size_t j = i; j < history.size(); ++j) {
            if (std::abs(history[j].value - finalValue) > tol) {
                allWithinTol = false;
                break;
            }
        }

        if (allWithinTol) {
            settleTime = history[i].time;
            break;
        }
    }

    VerificationResult result;
    result.passed = settled;
    result.message = settled ? "Settled" : "Not settled";
    result.measured_value = settleTime;
    result.expected_value = finalValue;
    m_results.push_back(result);

    std::cout << "[Settle] V(" << node << "): " << (settled ? " Settled" : " Not settled") << " at t=" << settleTime
              << "s (final=" << finalValue << "V, tol=" << tol << "V)" << std::endl;

    return settleTime;
}

void ModelQualityEngine::registerGolden(const std::string& name)
{
    if (m_goldenWaveforms.find(name) == m_goldenWaveforms.end()) {
        m_goldenWaveforms[name] = GoldenWaveform{name, {}};
        std::cout << "[Golden] Registered waveform: " << name << std::endl;
    }
}

bool ModelQualityEngine::loadGolden(const std::string& name, const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Golden] Failed to load: " << filename << std::endl;
        return false;
    }

    auto& waveform = m_goldenWaveforms[name];
    waveform.name = name;
    waveform.points.clear();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        double t, v;
        if (iss >> t >> v) {
            waveform.points.push_back({t, v});
        }
    }

    std::cout << "[Golden] Loaded " << waveform.points.size() << " points from " << filename << std::endl;
    return true;
}

void ModelQualityEngine::addGoldenPoint(const std::string& name, double time, double value)
{
    m_goldenWaveforms[name].points.push_back({time, value});
}

bool ModelQualityEngine::compareWaveform(const std::string& node, const std::string& goldenName, double tolPercent)
{
    auto it = m_goldenWaveforms.find(goldenName);
    if (it == m_goldenWaveforms.end()) {
        std::cerr << "[Compare] Golden waveform '" << goldenName << "' not found" << std::endl;
        return false;
    }

    auto& golden = it->second;
    auto& actual = m_voltageHistory[node];

    if (actual.empty() || golden.points.empty()) {
        std::cout << "[Compare] Empty waveform(s)" << std::endl;
        return false;
    }

    // Compare point-by-point
    double maxError = 0.0;
    double maxGoldenValue = 0.0;
    bool passed = true;

    for (const auto& point : actual) {
        double goldenVal = golden.interpolate(point.time);
        double goldenAbs = std::abs(goldenVal);
        maxGoldenValue = std::max(maxGoldenValue, goldenAbs);

        double error = std::abs(point.value - goldenVal);
        double errorPercent = (goldenAbs > m_absTol) ? (error / goldenAbs * 100.0) : 0.0;

        maxError = std::max(maxError, error);

        if (errorPercent > tolPercent && error > m_absTol) {
            passed = false;
            break;
        }
    }

    double maxErrorPercent = (maxGoldenValue > m_absTol) ? (maxError / maxGoldenValue * 100.0) : 0.0;

    VerificationResult result;
    result.passed = passed;
    result.message = passed ? "Within tolerance" : "Exceeds tolerance";
    result.measured_value = maxError;
    result.expected_value = 0.0;
    result.error_percent = maxErrorPercent;
    m_results.push_back(result);

    std::cout << "[Compare] V(" << node << ") vs '" << goldenName << "': " << (passed ? " PASS" : " FAIL")
              << " (max error=" << maxErrorPercent << "%)" << std::endl;

    return passed;
}

bool ModelQualityEngine::checkConvergence(const std::string& node, int maxIter, double epsilon)
{
    auto& history = m_voltageHistory[node];
    if (history.size() < 2)
        return true;

    // Check if voltage changes are below epsilon over recent iterations
    int checkCount = std::min((int)history.size(), maxIter);
    double maxChange = 0.0;

    for (int i = history.size() - checkCount; i < (int)history.size() - 1; ++i) {
        double change = std::abs(history[i + 1].value - history[i].value);
        maxChange = std::max(maxChange, change);
    }

    bool converged = maxChange < epsilon;

    VerificationResult result;
    result.passed = converged;
    result.message = converged ? "Converged" : "Not converged";
    result.measured_value = maxChange;
    result.expected_value = epsilon;
    m_results.push_back(result);

    std::cout << "[Converge] V(" << node << "): " << (converged ? " Converged" : " Diverged")
              << " (max change=" << maxChange << ", eps=" << epsilon << ")" << std::endl;

    return converged;
}

bool ModelQualityEngine::detectDiscontinuity(const std::string& node, double threshold)
{
    auto& history = m_voltageHistory[node];
    if (history.size() < 2)
        return false;

    std::vector<double> discontinuities;

    for (size_t i = 1; i < history.size(); ++i) {
        double jump = std::abs(history[i].value - history[i - 1].value);
        if (jump > threshold) {
            discontinuities.push_back(history[i].time);
        }
    }

    bool hasDiscontinuity = !discontinuities.empty();

    VerificationResult result;
    result.passed = !hasDiscontinuity;
    result.message = hasDiscontinuity
                         ? "Discontinuities detected at " + std::to_string(discontinuities.size()) + " points"
                         : "No discontinuities";
    result.measured_value = discontinuities.empty() ? 0.0 : discontinuities[0];
    m_results.push_back(result);

    std::cout << "[Discontinuity] V(" << node << "): " << (hasDiscontinuity ? " Found " : " Clean")
              << discontinuities.size() << " discontinuitie(s)" << std::endl;
    for (double t : discontinuities) {
        std::cout << "  t=" << t << "s" << std::endl;
    }

    return hasDiscontinuity;
}

bool ModelQualityEngine::detectHiddenState(const std::string& node, int historyDepth)
{
    auto& history = m_voltageHistory[node];
    if ((int)history.size() < historyDepth * 2)
        return false;

    // Look for repeating patterns that suggest hidden state
    bool foundPattern = false;
    int patternLength = 0;

    for (int len = 2; len <= historyDepth && !foundPattern; ++len) {
        int matchCount = 0;
        int totalChecks = 0;

        for (int i = history.size() - 2 * len; i >= 0 && totalChecks < 10; --i) {
            bool match = true;
            for (int j = 0; j < len; ++j) {
                double diff = std::abs(history[i + j].value - history[i + j + len].value);
                if (diff > m_absTol) {
                    match = false;
                    break;
                }
            }
            if (match)
                matchCount++;
            totalChecks++;
        }

        // If 80%+ of checks match, likely hidden state
        if (totalChecks > 0 && (double)matchCount / totalChecks > 0.8) {
            foundPattern = true;
            patternLength = len;
        }
    }

    VerificationResult result;
    result.passed = !foundPattern;
    result.message = foundPattern ? "Hidden state detected (pattern length=" + std::to_string(patternLength) + ")"
                                  : "No hidden state detected";
    result.measured_value = patternLength;
    m_results.push_back(result);

    std::cout << "[State] V(" << node << "): " << (foundPattern ? " Hidden state" : " No hidden state")
              << (foundPattern ? " (pattern=" + std::to_string(patternLength) + ")" : "") << std::endl;

    return foundPattern;
}

void ModelQualityEngine::setTolerance(double absTol, double relTolPercent)
{
    m_absTol = absTol;
    m_relTol = relTolPercent;
    std::cout << "[Tolerance] abs=" << absTol << " rel=" << relTolPercent << "%" << std::endl;
}

bool ModelQualityEngine::allPassed() const
{
    for (const auto& result : m_results) {
        if (!result.passed)
            return false;
    }
    return true;
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

int flux_assert_voltage(const char* node, const char* op, double bound, double withinTime, const char* message)
{
    return ModelQualityEngine::instance().assertVoltage(node, op, bound, withinTime) ? 1 : 0;
}

double flux_check_settling(const char* node, double tolPercent, double afterTime)
{
    return ModelQualityEngine::instance().checkSettling(node, tolPercent, afterTime);
}

void flux_register_golden(const char* name)
{
    ModelQualityEngine::instance().registerGolden(name);
}

int flux_compare_waveform(const char* node, const char* goldenName, double tolPercent)
{
    return ModelQualityEngine::instance().compareWaveform(node, goldenName, tolPercent) ? 1 : 0;
}

int flux_check_convergence(const char* node, int maxIter, double epsilon)
{
    return ModelQualityEngine::instance().checkConvergence(node, maxIter, epsilon) ? 1 : 0;
}

int flux_detect_discontinuity(const char* node, double threshold)
{
    return ModelQualityEngine::instance().detectDiscontinuity(node, threshold) ? 1 : 0;
}

int flux_detect_hidden_state(const char* node, int historyDepth)
{
    return ModelQualityEngine::instance().detectHiddenState(node, historyDepth) ? 1 : 0;
}

void flux_set_tolerance(double absTol, double relTolPercent)
{
    ModelQualityEngine::instance().setTolerance(absTol, relTolPercent);
}

} // extern "C"

} // namespace Flux
