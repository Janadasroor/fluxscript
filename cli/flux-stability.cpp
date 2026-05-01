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

// flux-stability: Analyze circuit stability (Phase Margin, Gain Margin, Bandwidth)
// Usage: flux-stability <circuit.flux>

#include <iostream>
#include <fstream>
#include <string>
#include "flux/analysis/advanced_analysis.h"

using namespace Flux::AdvancedAnalysis;

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Stability Analyzer                  \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-stability <circuit.flux>\n";
        std::cerr << "\nExample: flux-stability amplifier.flux\n";
        return 1;
    }

    std::string filename = argv[1];
    std::cout << " Analyzing: " << filename << "\n";

    // Create Analyzer
    StabilityAnalyzer analyzer;

    // Run Analysis
    std::cout << " Running stability analysis...\n";
    StabilityResult result = analyzer.analyze(filename);

    // Output Results
    std::cout << result.toString();

    // Also save Markdown version
    std::string mdFile = "stability_report.md";
    std::ofstream mdStream(mdFile);
    if (mdStream.is_open()) {
        mdStream << result.toMarkdown();
        mdStream.close();
        std::cout << "\n Markdown report saved to: " << mdFile << "\n";
    }

    return 0;
}
