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

// flux-monte-carlo: Statistical analysis and yield prediction
// Usage: flux-monte-carlo <circuit.flux> [--param Name:Nom:Tol] [--spec Min:Max] [--iterations N]

#include "flux/analysis/advanced_analysis.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace Flux::AdvancedAnalysis;

struct ParamDef
{
    std::string name;
    double nominal;
    double tolerance;
};

int main(int argc, char** argv)
{
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Monte Carlo Analyzer                \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-monte-carlo <circuit.flux> [options]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --param Name:Nominal:AbsTol  Add component parameter (e.g., R1:10000:100)\n";
        std::cerr << "  --spec Min:Max               Set specification limits (e.g., 4.5:5.5)\n";
        std::cerr << "  --iterations N               Set number of simulations (default: 100)\n";
        return 1;
    }

    std::string filename = argv[1];
    std::vector<ParamDef> params;
    int iterations = 100;
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
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = std::stoi(argv[++i]);
        } else if (arg == "--spec" && i + 1 < argc) {
            std::string val = argv[++i];
            std::replace(val.begin(), val.end(), ':', ' ');
            std::istringstream ss(val);
            ss >> specMin >> specMax;
        }
    }

    // Default demo if no params provided
    if (params.empty()) {
        std::cout << "  No parameters specified. Running demo with R1 and C1...\n";
        params.push_back({"R1", 10000.0, 100.0}); // 10k +/- 100
        params.push_back({"C1", 1e-9, 0.2e-9});   // 1nF +/- 20%
    }

    std::cout << " Analyzing: " << filename << "\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << " Parameters: " << params.size() << "\n";

    // Create Engine
    MonteCarloEngine mc;
    mc.setIterations(iterations);
    mc.setDistribution("gaussian");

    for (const auto& p : params) {
        mc.addParameter(p.name, p.nominal, p.tolerance);
        std::cout << "    " << p.name << " = " << p.nominal << "  " << p.tolerance << "\n";
    }

    if (specMin > -1e17) {
        mc.setSpecLimits(specMin, specMax);
        std::cout << " Spec Limits: [" << specMin << ", " << specMax << "]\n";
    }

    // Run Analysis
    std::cout << " Running Monte Carlo simulation...\n";
    MonteCarloResult result = mc.run(filename);

    // Output Results
    std::cout << "\n" << result.toString();

    return 0;
}
