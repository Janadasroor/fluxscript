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

// FFT Engine Implementation
#include "flux/analysis/fft_engine.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#define _USE_MATH_DEFINES

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Flux {
namespace FFT {

FFTEngine::FFTEngine() : m_sampleRate(1000.0), m_windowType("hanning") {}

FFTEngine::~FFTEngine() = default;

void FFTEngine::setSampleRate(double rate)
{
    m_sampleRate = rate;
}

void FFTEngine::setWindowType(const std::string& type)
{
    m_windowType = type;
}

//
//  Windowing Functions
//

void FFTEngine::applyWindow(std::vector<double>& data)
{
    int N = data.size();
    if (m_windowType == "none")
        return;

    for (int i = 0; i < N; ++i) {
        double w = 1.0;
        if (m_windowType == "hanning") {
            w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (N - 1)));
        } else if (m_windowType == "hamming") {
            w = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (N - 1));
        } else if (m_windowType == "blackman") {
            w = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (N - 1)) + 0.08 * std::cos(4.0 * M_PI * i / (N - 1));
        }
        data[i] *= w;
    }
}

//
//  FFT Algorithm (Cooley-Tukey)
//

void FFTEngine::fft(std::vector<std::complex<double>>& x)
{
    int N = x.size();
    if (N <= 1)
        return;

    // Divide
    std::vector<std::complex<double>> even(N / 2), odd(N / 2);
    for (int i = 0; i < N / 2; ++i) {
        even[i] = x[2 * i];
        odd[i] = x[2 * i + 1];
    }

    // Conquer
    fft(even);
    fft(odd);

    // Combine
    for (int k = 0; k < N / 2; ++k) {
        std::complex<double> t = std::polar(1.0, -2.0 * M_PI * k / N) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
}

//
//  Compute FFT
//

FFTReport FFTEngine::compute(const std::vector<double>& signalData)
{
    FFTReport report;
    report.sampleRate = m_sampleRate;
    report.numPoints = signalData.size();

    // Ensure power of 2 (pad with zeros if needed)
    size_t N = 1;
    while (N < signalData.size())
        N *= 2;
    N *= 2; // Double it to increase resolution via zero-padding

    std::vector<double> paddedData(N, 0.0);
    std::copy(signalData.begin(), signalData.end(), paddedData.begin());

    // Apply window
    applyWindow(paddedData);

    // Convert to complex
    std::vector<std::complex<double>> x(N);
    for (size_t i = 0; i < N; ++i) {
        x[i] = std::complex<double>(paddedData[i], 0.0);
    }

    // Run FFT
    fft(x);

    // Compute magnitude spectrum (only first half)
    double maxMag = 0.0;
    double maxFreq = 0.0;
    double fundamentalMag = 0.0;

    for (size_t i = 0; i < N / 2; ++i) {
        double freq = (double)i * m_sampleRate / N;
        double mag = std::abs(x[i]);

        // Convert to dB
        double db = 20 * std::log10(mag / N + 1e-12);

        if (mag > maxMag) {
            maxMag = mag;
            maxFreq = freq;
        }

        FFTPoint point;
        point.frequency = freq;
        point.magnitude = db;
        point.phase = std::arg(x[i]) * 180.0 / M_PI;

        report.spectrum.push_back(point);
    }

    report.fundamentalFreq = maxFreq;
    fundamentalMag = maxMag;

    // Calculate THD (Total Harmonic Distortion)
    double harmonicPower = 0.0;
    double noisePower = 0.0;
    for (size_t i = 2; i < N / 2; ++i) {
        double freq = (double)i * m_sampleRate / N;
        double mag = std::abs(x[i]);

        // Simple THD approximation: harmonics of fundamental
        bool isHarmonic = false;
        for (int h = 2; h <= 10; ++h) {
            if (std::abs(freq - h * maxFreq) < (m_sampleRate / N) * 2) {
                isHarmonic = true;
                harmonicPower += mag * mag;
                break;
            }
        }
        if (!isHarmonic) {
            noisePower += mag * mag;
        }
    }

    if (fundamentalMag > 0) {
        report.thd = 100.0 * std::sqrt(harmonicPower) / fundamentalMag;
        report.snr = 10 * std::log10(fundamentalMag * fundamentalMag / (noisePower + 1e-12));
    } else {
        report.thd = 0.0;
        report.snr = -120.0;
    }

    return report;
}

//
//  Report Output Methods
//

std::string FFTReport::toText() const
{
    std::ostringstream oss;
    oss << "\n";
    oss << "              FREQUENCY DOMAIN ANALYSIS               \n";
    oss << "\n\n";
    oss << "Sample Rate:       " << sampleRate << " Hz\n";
    oss << "Data Points:       " << numPoints << "\n";
    oss << "Fundamental Freq:  " << fundamentalFreq << " Hz\n";
    oss << "THD:               " << std::fixed << std::setprecision(2) << thd << "%\n";
    oss << "SNR:               " << snr << " dB\n\n";

    oss << "Spectrum:\n";
    oss << "\n";
    oss << "  Freq (Hz)   |  Mag (dB)  |  Phase (deg)\n";
    oss << "  ------------+------------+-------------\n";

    for (const auto& pt : spectrum) {
        // Only print peaks or fundamental
        if (pt.magnitude > -40.0 || std::abs(pt.frequency - fundamentalFreq) < 1.0) {
            oss << "  " << std::setw(8) << pt.frequency << "    |  " << std::setw(6) << pt.magnitude << "    |  "
                << std::setw(6) << pt.phase << "\n";
        }
    }
    oss << "\n";

    return oss.str();
}

std::string FFTReport::toMarkdown() const
{
    std::ostringstream oss;
    oss << "# FFT Analysis Report\n\n";
    oss << "**Fundamental Frequency:** " << fundamentalFreq << " Hz\n";
    oss << "**THD:** " << thd << "%\n";
    oss << "**SNR:** " << snr << " dB\n\n";
    oss << "| Frequency (Hz) | Magnitude (dB) |\n";
    oss << "|----------------|----------------|\n";

    for (const auto& pt : spectrum) {
        if (pt.magnitude > -40.0) {
            oss << "| " << pt.frequency << " | " << pt.magnitude << " |\n";
        }
    }
    return oss.str();
}

std::string FFTReport::toASCIIPlot(int width, int height) const
{
    if (spectrum.empty())
        return "No data to plot";

    std::vector<std::vector<char>> plot(height, std::vector<char>(width, ' '));

    double minDb = -100.0;
    double maxDb = 0.0;

    // Map frequencies to columns
    // We show up to Nyquist (N/2 points) mapped to width
    // For efficiency, we might subsample or use decibels

    int numFreqPoints = spectrum.size();

    for (const auto& pt : spectrum) {
        // Map frequency to X
        int x = (int)((pt.frequency / (sampleRate / 2.0)) * (width - 1));
        x = std::max(0, std::min(width - 1, x));

        // Map dB to Y (0 is top, height-1 is bottom)
        int y = (int)((1.0 - (pt.magnitude - minDb) / (maxDb - minDb)) * (height - 1));
        y = std::max(0, std::min(height - 1, y));

        if (pt.magnitude > -120.0) { // Only plot non-noise floor
            plot[y][x] = '|';
        }
    }

    std::ostringstream oss;
    oss << "  Magnitude (dB)\n";
    for (int r = 0; r < height; ++r) {
        double val = maxDb - (double)r / (height - 1) * (maxDb - minDb);
        oss << std::setw(6) << val << " |";
        for (int c = 0; c < width; ++c) {
            oss << plot[r][c];
        }
        oss << "\n";
    }
    oss << "       +";
    for (int c = 0; c < width; ++c)
        oss << "-";
    oss << "\n";
    oss << "       0 Hz" << std::setw(width - 10) << "" << sampleRate / 2 << " Hz\n";
    oss << "                         Frequency\n";

    return oss.str();
}

} // namespace FFT
} // namespace Flux
