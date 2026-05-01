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

// flux-worstcase: Automated worst-case corner analysis
// Usage: flux-worstcase <circuit.flux> [--param Name:Nominal:Tolerance] [--spec Min:Max]

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include "flux/analysis/advanced_analysis.h"

using namespace Flux::AdvancedAnalysis;

struct ParamDef {
    std::string name;
    double nominal;
    double tolerance;
};

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Worst-Case Analyzer                 \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-worstcase <circuit.flux> [options]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --param Name:Nominal:AbsTol  Add component parameter (e.g., R1:10000:100)\n";
        std::cerr << "  --spec Min:Max               Set specification limits (e.g., 4.5:5.5)\n";
        return 1;
    }

    std::string filename = argv[1];
    std::vector<ParamDef> params;
    double specMin = -1e18;
    double specMax = 1e18;

    // Parse Arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--param" && i + 1 < argc) {
            std::string val = argv[++i];
            // Format: Name:Nom:Tol
            std::replace(val.begin(), val.end(), ':', ' ');
            std::istringstream ss(val);
            ParamDef p;
            if (ss >> p.name >> p.nominal >> p.tolerance) {
                params.push_back(p);
            }
        } else if (arg == "--spec" && i + 1 < argc) {
            std::string val = argv[++i];
            std::replace(val.begin(), val.end(), ':', ' ');
            std::istringstream ss(val);
            ss >> specMin >> specMax;
        }
    }

    // Default demo if no params provided
    if (params.empty()) {
        std::cout << "  No parameters specified. Running demo with R1 and R2...\n";
        params.push_back({"R1", 10000.0, 100.0});   // 10k +/- 1%
        params.push_back({"R2", 20000.0, 200.0});   // 20k +/- 1%
    }

    std::cout << " Analyzing: " << filename << "\n";
    std::cout << " Parameters: " << params.size() << " (Testing " << (1 << params.size()) << " corners)\n";

    // Create Analyzer
    WorstCaseAnalyzer analyzer;
    for (const auto& p : params) {
        analyzer.addComponent(p.name, p.nominal, p.tolerance);
        std::cout << "    " << p.name << " = " << p.nominal << "  " << p.tolerance << "\n";
    }

    if (specMin > -1e17) {
        analyzer.setSpecLimits(specMin, specMax);
        std::cout << " Spec Limits: [" << specMin << ", " << specMax << "]\n";
    }

    // Run Analysis
    std::cout << " Running worst-case analysis...\n";
    WorstCaseResult result = analyzer.analyze(filename);

    // Output Results
    std::cout << "\n" << result.toString();

    return 0;
}
