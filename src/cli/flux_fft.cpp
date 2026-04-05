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

// flux-fft: Perform Fast Fourier Transform analysis on signal data
// Usage: flux-fft <data.csv> [--rate 1000] [--plot ascii]

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "flux/analysis/fft_engine.h"

using namespace Flux::FFT;

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║          FluxScript FFT Analyzer                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-fft <data.csv> [--rate 1000] [--plot ascii] [--window hanning]\n";
        std::cerr << "\nExample data.csv:\n";
        std::cerr << "  Time,Value\n";
        std::cerr << "  0.000,0.123\n";
        std::cerr << "  0.001,0.456\n";
        return 1;
    }

    std::string filename = argv[1];
    double sampleRate = 1000.0;
    std::string plotMode = "";
    std::string windowType = "hanning";

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rate" && i + 1 < argc) {
            sampleRate = std::stod(argv[++i]);
        } else if (arg == "--plot" && i + 1 < argc) {
            plotMode = argv[++i];
        } else if (arg == "--window" && i + 1 < argc) {
            windowType = argv[++i];
        }
    }

    std::cout << "📄 Loading: " << filename << "\n";
    std::cout << "⚙️  Sample Rate: " << sampleRate << " Hz\n";

    // Load CSV data
    std::vector<double> signalData;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ Cannot open file: " << filename << "\n";
        return 1;
    }

    std::string line;
    // Skip header if present
    std::getline(file, line);
    if (line.find("Time") == std::string::npos && line.find("Value") == std::string::npos) {
        // If first line is data, rewind
        file.seekg(0);
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::istringstream ss(line);
        std::string token;
        // Skip time column
        std::getline(ss, token, ','); 
        // Get value column
        std::getline(ss, token, ',');
        
        try {
            signalData.push_back(std::stod(token));
        } catch (...) {
            // Skip invalid lines
        }
    }
    file.close();

    std::cout << "📊 Loaded " << signalData.size() << " samples\n";

    if (signalData.empty()) {
        std::cerr << "❌ No valid data found in file\n";
        return 1;
    }

    // Create Engine
    FFTEngine engine;
    engine.setSampleRate(sampleRate);
    engine.setWindowType(windowType);

    // Run FFT
    std::cout << "🚀 Computing FFT...\n";
    FFTReport report = engine.compute(signalData);

    // Output Results
    if (plotMode == "ascii") {
        std::cout << "\n📈 Frequency Spectrum:\n";
        std::cout << report.toASCIIPlot(60, 15);
    } else {
        std::cout << report.toText();
    }

    return 0;
}
