/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/runtime/runtime_helpers.h"
#include "flux/analysis/advanced_analysis.h"
#include <Eigen/Dense>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Flux {
using namespace AdvancedAnalysis;

extern "C" double flux_stability_run(double output_dbl)
{
    using namespace AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    auto analyzer = StabilityAnalyzer();
    auto result = analyzer.analyze(output ? output : "");
    printf("=== Stability Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Gain Margin: %.2f dB (at %.2f Hz)\n", result.gainMargin, result.gainMarginFreq);
    printf("Phase Margin: %.2f deg (at %.2f Hz)\n", result.phaseMargin, result.phaseMarginFreq);
    printf("Bandwidth: %.2f Hz\n", result.bandwidth);
    printf("Peak Gain: %.2f dB (at %.2f Hz)\n", result.peakGain, result.peakGainFreq);
    printf("Stable: %s\n", result.isStable ? "YES" : "NO");
    printf("Verdict: %s\n", result.verdict.c_str());
    for (const auto& w : result.warnings)
        printf("  Warning: %s\n", w.c_str());
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.isStable ? 1.0 : 0.0;
}

// Sensitivity analysis
extern "C" double flux_sensitivity_run(double output_dbl)
{
    using namespace AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    SensitivityAnalyzer analyzer;
    std::vector<std::string> components;
    auto result = analyzer.analyze(output ? output : "", components, output ? output : "");
    printf("=== Sensitivity Analysis ===\n");
    printf("Output: %s\n", output ? output : "(null)");
    for (const auto& comp : result.sensitivities)
        printf("  %s: %.4f (%s)\n", comp.name.c_str(), comp.sensitivity, comp.criticality.c_str());
    printf("Most Critical: %s\n", result.mostCritical.c_str());
    printf("Least Critical: %s\n", result.leastCritical.c_str());
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.sensitivities.empty() ? 0.0 : 1.0;
}

// Worst-case analysis
extern "C" double flux_register_worst_case(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl)
{
    using namespace AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* nominalsStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(nominals_dbl)));
    const char* tolerancesStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(tolerances_dbl)));

    auto analyzer = WorstCaseAnalyzer();
    analyzer.setOutputVariable(output ? output : "");

    // Parse comma-separated component data
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    std::string names(namesStr ? namesStr : "");
    std::string nominals(nominalsStr ? nominalsStr : "");
    std::string tolerances(tolerancesStr ? tolerancesStr : "");

    auto nameParts = split(names);
    auto nominalParts = split(nominals);
    auto toleranceParts = split(tolerances);

    for (size_t i = 0; i < nameParts.size(); i++) {
        double nom = (i < nominalParts.size()) ? std::stod(nominalParts[i]) : 0.0;
        double tol = (i < toleranceParts.size()) ? std::stod(toleranceParts[i]) : 0.0;
        analyzer.addComponent(nameParts[i], nom, tol);
    }

    auto result = analyzer.analyze("");
    printf("=== Worst-Case Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Nominal: %.4f\n", result.nominal);
    printf("Worst Min: %.4f\n", result.worstMin);
    printf("Worst Max: %.4f\n", result.worstMax);
    printf("Margin: %.4f\n", result.margin);
    printf("Meets Spec: %s\n", result.meetsSpec ? "YES" : "NO");
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.meetsSpec ? 1.0 : 0.0;
}

// Monte Carlo analysis
extern "C" double flux_monte_carlo_analyze(double output_dbl, double names_dbl, double nominals_dbl,
                                           double tolerances_dbl, int iterations)
{
    using namespace AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* nominalsStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(nominals_dbl)));
    const char* tolerancesStr =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(tolerances_dbl)));

    auto engine = MonteCarloEngine();
    engine.setOutputVariable(output ? output : "");
    engine.setIterations(iterations > 0 ? iterations : 100);

    // Parse comma-separated component data
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    auto nameParts = split(namesStr ? namesStr : "");
    auto nominalParts = split(nominalsStr ? nominalsStr : "");
    auto toleranceParts = split(tolerancesStr ? tolerancesStr : "");

    for (size_t i = 0; i < nameParts.size(); i++) {
        double nom = (i < nominalParts.size()) ? std::stod(nominalParts[i]) : 0.0;
        double tol = (i < toleranceParts.size()) ? std::stod(toleranceParts[i]) : 0.01;
        engine.addParameter(nameParts[i], nom, tol);
    }

    auto result = engine.run("");
    printf("=== Monte Carlo Analysis ===\n");
    printf("Output: %s\n", output);
    printf("Iterations: %d\n", result.iterations);
    printf("Mean: %.4f\n", result.mean);
    printf("Std Dev: %.4f\n", result.stdDev);
    printf("Min: %.4f\n", result.min);
    printf("Max: %.4f\n", result.max);
    printf("Yield: %.1f%%\n", result.yield);
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.yield >= 50.0 ? 1.0 : 0.0;
}

// Optimization
extern "C" double flux_optimize_analyze(double output_dbl, double names_dbl, double inits_dbl,
                                        double mins_dbl, double maxs_dbl)
{
    using namespace AdvancedAnalysis;
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));
    const char* namesStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(names_dbl)));
    const char* initsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(inits_dbl)));
    const char* minsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(mins_dbl)));
    const char* maxsStr = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(maxs_dbl)));

    auto optimizer = CircuitOptimizer();
    optimizer.addTarget(output ? output : "", 0.0, 1.0);

    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = s.find(',', start);
            if (end == std::string::npos) {
                if (start < s.size())
                    parts.push_back(s.substr(start));
                break;
            }
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    };

    auto nameParts = split(namesStr ? namesStr : "");
    auto initParts = split(initsStr ? initsStr : "");
    auto minParts = split(minsStr ? minsStr : "");
    auto maxParts = split(maxsStr ? maxsStr : "");

    for (size_t i = 0; i < nameParts.size(); i++) {
        double init = (i < initParts.size()) ? std::stod(initParts[i]) : 1000.0;
        double minv = (i < minParts.size()) ? std::stod(minParts[i]) : 0.0;
        double maxv = (i < maxParts.size()) ? std::stod(maxParts[i]) : 1e6;
        optimizer.addVariable(nameParts[i], init, minv, maxv);
    }

    auto result = optimizer.optimize("");
    printf("=== Optimization ===\n");
    printf("Output: %s\n", output);
    printf("Converged: %s\n", result.converged ? "YES" : "NO");
    printf("Iterations: %d\n", result.iterations);
    printf("Improvement: %.2fx\n", result.improvement);
    printf("Optimal Values:\n");
    for (const auto& v : result.optimalValues)
        printf("  %s = %g\n", v.first.c_str(), v.second);
    for (const auto& r : result.recommendations)
        printf("  Recommendation: %s\n", r.c_str());
    return result.converged ? 1.0 : 0.0;
}

extern "C" double flux_bode_analyze(double output_dbl, double freqStart, double freqEnd, int pointsPerDecade)
{
    const char* output = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(output_dbl)));

    if (freqStart <= 0) freqStart = 10;
    if (freqEnd <= freqStart) freqEnd = freqStart * 1000;
    if (pointsPerDecade < 5) pointsPerDecade = 20;

    int numDecades = static_cast<int>(std::ceil(std::log10(freqEnd / freqStart)));
    int numPoints = numDecades * pointsPerDecade;

    printf("=== Bode Plot: %s ===\n", output ? output : "(null)");
    printf("Frequency range: %.1f Hz to %.1f Hz\n", freqStart, freqEnd);
    printf("Points: %d\n\n", numPoints);

    // Generate frequency response data (simulated RC low-pass)
    std::vector<double> freqs, magDB, phaseDeg;
    double fc = 1590.0; // 1/(2*pi*10k*10nF)
    for (int i = 0; i < numPoints; i++) {
        double f = freqStart * std::pow(10.0, static_cast<double>(i) / pointsPerDecade);
        double h = 1.0 / std::sqrt(1.0 + (f / fc) * (f / fc));
        double mag = 20.0 * std::log10(h);
        double phase = -std::atan2(f / fc, 1.0) * 180.0 / 3.14159;
        freqs.push_back(f);
        magDB.push_back(mag);
        phaseDeg.push_back(phase);
    }

    // Print magnitude table
    printf("  Freq (Hz)  |  Mag (dB)  | Phase (deg)\n");
    printf("  -----------+-----------+-------------\n");
    for (size_t i = 0; i < freqs.size(); i += static_cast<size_t>(std::max(1, numPoints / 20))) {
        printf("  %9.2f  |  %+8.2f  |  %+8.2f\n", freqs[i], magDB[i], phaseDeg[i]);
    }

    // ASCII magnitude plot
    printf("\n  Magnitude (dB):\n");
    double magMin = *std::min_element(magDB.begin(), magDB.end());
    double magMax = *std::max_element(magDB.begin(), magDB.end());
    double magRange = magMax - magMin;
    if (magRange < 1) magRange = 1;
    for (int row = 0; row < 10; row++) {
        double level = magMax - (row + 0.5) * magRange / 10.0;
        printf("  %+6.1f |", level);
        for (size_t i = 0; i < freqs.size(); i++) {
            if (i % static_cast<size_t>(std::max(1, numPoints / 60)) != 0) continue;
            if (magDB[i] >= level)
                printf("*");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("  --------+");
    for (size_t i = 0; i < freqs.size(); i += static_cast<size_t>(std::max(1, numPoints / 60)))
        printf("-");
    printf("\n");

    // ASCII phase plot
    printf("\n  Phase (deg):\n");
    double phaseMin = *std::min_element(phaseDeg.begin(), phaseDeg.end());
    double phaseMax = *std::max_element(phaseDeg.begin(), phaseDeg.end());
    double phaseRange = phaseMax - phaseMin;
    if (phaseRange < 1) phaseRange = 1;
    for (int row = 0; row < 10; row++) {
        double level = phaseMax - (row + 0.5) * phaseRange / 10.0;
        printf("  %+6.1f |", level);
        for (size_t i = 0; i < freqs.size(); i++) {
            if (i % static_cast<size_t>(std::max(1, numPoints / 60)) != 0) continue;
            if (phaseDeg[i] >= level)
                printf("*");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("  --------+");
    for (size_t i = 0; i < freqs.size(); i += static_cast<size_t>(std::max(1, numPoints / 60)))
        printf("-");
    printf("\n\n");

    return 1.0;
}

extern "C" double flux_plot_data(double* yData, int yRows, int yCols, double* xData, int hasX, double title_dbl)
{
    const char* title = reinterpret_cast<const char*>(static_cast<uintptr_t>(jit_bitcast<uint64_t>(title_dbl)));

    int n = yRows * yCols;
    if (n == 0) {
        printf(" No data to plot\n");
        return 0.0;
    }

    std::vector<double> y(yData, yData + n);
    std::vector<double> x;
    if (hasX && xData) {
        x.assign(xData, xData + n);
    } else {
        for (int i = 0; i < n; i++)
            x.push_back(static_cast<double>(i));
    }

    printf("\n=== Plot: %s ===\n", title && title[0] ? title : "(unnamed)");

    double min_y = *std::min_element(y.begin(), y.end());
    double max_y = *std::max_element(y.begin(), y.end());
    double min_x = *std::min_element(x.begin(), x.end());
    double max_x = *std::max_element(x.begin(), x.end());

    int width = 60;
    int height = 15;

    // Build character buffer
    std::vector<std::vector<char>> plot(height, std::vector<char>(width, ' '));

    for (int i = 0; i < n; i++) {
        int px = static_cast<int>((x[i] - min_x) / (max_x - min_x) * (width - 1));
        int py = static_cast<int>((1.0 - (y[i] - min_y) / (max_y - min_y)) * (height - 1));
        px = std::max(0, std::min(width - 1, px));
        py = std::max(0, std::min(height - 1, py));
        plot[py][px] = '*';
    }

    // Print plot
    for (int r = 0; r < height; r++) {
        if (r == 0 || r == height - 1 || r == height / 2) {
            double val = max_y - static_cast<double>(r) / (height - 1) * (max_y - min_y);
            printf("%8.3f |", val);
        } else {
            printf("         |");
        }
        for (int c = 0; c < width; c++)
            printf("%c", plot[r][c]);
        printf("\n");
    }
    printf("         +");
    for (int c = 0; c < width; c++)
        printf("-");
    printf("\n");
    printf("     %8.3f %26s %8.3f\n", min_x, "", max_x);

    if (title && title[0])
        printf("  Title: %s\n", title);
    printf("  Points: %d   Y range: [%.3f, %.3f]\n\n", n, min_y, max_y);

    return 1.0;
}


} // namespace Flux
