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

#ifndef FLUX_VISUALIZATION_SMITH_CHART_H
#define FLUX_VISUALIZATION_SMITH_CHART_H

#include <string>
#include <vector>
#include <complex>
#include <map>

namespace Flux {
namespace Visualization {

// ============================================================================
// Smith Chart Data Structures
// ============================================================================

struct SmithChartData {
    std::vector<double> frequency;      // Hz
    std::vector<std::complex<double>> s11;  // Reflection coefficient
    std::vector<std::complex<double>> s21;  // Transmission coefficient
    std::string name;
    
    SmithChartData() {}
};

struct ImpedancePoint {
    double frequency;
    std::complex<double> impedance;     // Z (ohms)
    std::complex<double> reflection;    // Gamma
    double vswr;
    
    ImpedancePoint() : frequency(0), vswr(1) {}
};

struct MatchingNetwork {
    std::string type;  // "L", "Pi", "T", "stub"
    
    // Component values
    double L1, L2;  // Inductors (H)
    double C1, C2;  // Capacitors (F)
    double stubLength;  // Electrical degrees
    
    // Performance
    double bandwidth;
    double insertionLoss;
    double returnLoss;
    
    MatchingNetwork() : L1(0), L2(0), C1(0), C2(0), stubLength(0),
                        bandwidth(0), insertionLoss(0), returnLoss(0) {}
};

// ============================================================================
// Smith Chart Plotter
// ============================================================================

class SmithChart {
public:
    SmithChart(double systemZ0 = 50.0);
    ~SmithChart();
    
    // Configuration
    void setSystemImpedance(double z0) { m_systemZ0 = z0; }
    double systemImpedance() const { return m_systemZ0; }
    
    // Plot data
    void plotS11(const SmithChartData& data);
    void plotImpedance(const std::vector<ImpedancePoint>& points);
    void plotConstantResistance(double r);
    void plotConstantReactance(double x);
    void plotVSWRCircle(double vswr);
    
    // Export
    std::string toSVG() const;
    std::string toNetlist() const;
    void save(const std::string& filename, const std::string& format = "svg");
    
    // Get plot bounds
    double minX() const { return -1.1; }
    double maxX() const { return 1.1; }
    double minY() const { return -1.1; }
    double maxY() const { return 1.1; }
    
private:
    double m_systemZ0;
    std::vector<std::string> m_plotCommands;
    
    // Coordinate conversion
    std::complex<double> impedanceToGamma(std::complex<double> z) const;
    std::complex<double> gammaToImpedance(std::complex<double> gamma) const;
    
    // SVG generation helpers
    std::string drawCircle(double cx, double cy, double r, const std::string& color) const;
    std::string drawArc(double cx, double cy, double r, double startAngle, double endAngle, 
                        const std::string& color) const;
    std::string drawLine(double x1, double y1, double x2, double y2, const std::string& color) const;
};

// ============================================================================
// Impedance Matching Designer
// ============================================================================

class MatchingDesigner {
public:
    MatchingDesigner(double systemZ0 = 50.0);
    ~MatchingDesigner();
    
    // Set load impedance
    void setLoadImpedance(std::complex<double> zLoad, double frequency);
    void setSourceImpedance(std::complex<double> zSource, double frequency);
    
    // Design matching networks
    MatchingNetwork designLNetwork();
    MatchingNetwork designPiNetwork(double targetQ = 1.0);
    MatchingNetwork designTNetwork(double targetQ = 1.0);
    MatchingNetwork designStubMatcher(const std::string& type = "shunt");
    
    // Analyze matching network
    void analyzeNetwork(const MatchingNetwork& network);
    
    // Get results
    std::vector<double> getS11() const { return m_s11; }
    std::vector<double> getFrequency() const { return m_freq; }
    
    // Optimization
    void optimizeForBandwidth(double targetBW);
    void optimizeForInsertionLoss();
    
private:
    double m_systemZ0;
    std::complex<double> m_zLoad;
    std::complex<double> m_zSource;
    double m_frequency;
    
    std::vector<double> m_s11;
    std::vector<double> m_freq;
    
    // Design equations
    double calculateQ() const;
    std::complex<double> normalizeImpedance(std::complex<double> z) const;
};

// ============================================================================
// S-Parameter Utilities
// ============================================================================

class SParameterUtils {
public:
    // Load from file
    static SmithChartData loadTouchstone(const std::string& filename);
    static SmithChartData loadTouchstoneV1(const std::string& filename);
    static SmithChartData loadTouchstoneV2(const std::string& filename);
    
    // Create from circuit
    static SmithChartData simulate(const std::string& netlist, 
                                    double fStart, double fStop, int points);
    
    // Calculations
    static double calculateVSWR(std::complex<double> s11);
    static double calculateReturnLoss(std::complex<double> s11);
    static double calculateInsertionLoss(std::complex<double> s21);
    static double calculateStabilityFactor(std::complex<double> s11, std::complex<double> s12,
                                            std::complex<double> s21, std::complex<double> s22);
    
    // Conversions
    static std::complex<double> s11ToImpedance(std::complex<double> s11, double z0);
    static std::complex<double> impedanceToS11(std::complex<double> z, double z0);
    
    // File I/O
    static void saveTouchstone(const SmithChartData& data, const std::string& filename);
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Smith chart
    void* flux_smith_chart_create(double z0);
    void flux_smith_chart_destroy(void* chart);
    void flux_smith_chart_plot(void* chart, const double* freq, const double* s11Real, 
                                const double* s11Imag, int nPoints);
    void flux_smith_chart_save(void* chart, const char* filename);
    
    // Matching network design
    void* flux_matcher_create(double z0);
    void flux_matcher_destroy(void* matcher);
    void flux_matcher_set_load(void* matcher, double r, double x, double freq);
    void* flux_matcher_design_l_network(void* matcher);
    double flux_matcher_get_l1(void* network);
    double flux_matcher_get_c1(void* network);
    double flux_matcher_get_bandwidth(void* network);
    
    // S-parameter utilities
    void* flux_sparam_load(const char* filename);
    double flux_sparam_vswr(void* data, int index);
    double flux_sparam_return_loss(void* data, int index);
}

} // namespace Visualization
} // namespace Flux

#endif // FLUX_VISUALIZATION_SMITH_CHART_H
