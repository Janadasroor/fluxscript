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

#ifndef FLUX_FFT_ENGINE_H
#define FLUX_FFT_ENGINE_H

#include <vector>
#include <string>
#include <complex>

namespace Flux {
namespace FFT {

// 
//  FFT Result Structure                                 
// 

struct FFTPoint {
    double frequency; // Hz
    double magnitude; // dB
    double phase;     // degrees
};

struct FFTReport {
    std::vector<FFTPoint> spectrum;
    double sampleRate;
    double fundamentalFreq;
    double thd; // Total Harmonic Distortion (%)
    double snr; // Signal to Noise Ratio (dB)
    int numPoints;

    std::string toText() const;
    std::string toMarkdown() const;
    std::string toASCIIPlot(int width = 80, int height = 20) const;
};

// 
//  FFT Engine Class                                     
// 

class FFTEngine {
public:
    FFTEngine();
    ~FFTEngine();

    // Configuration
    void setSampleRate(double rate);
    void setWindowType(const std::string& type); // "hanning", "hamming", "blackman", "none"

    // Execution
    FFTReport compute(const std::vector<double>& signalData);

private:
    double m_sampleRate;
    std::string m_windowType;

    // Internal FFT implementation (Cooley-Tukey)
    void fft(std::vector<std::complex<double>>& x);

    // Windowing functions
    void applyWindow(std::vector<double>& data);
};

} // namespace FFT
} // namespace Flux

#endif // FLUX_FFT_ENGINE_H
