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

// flux-optimize: Automated circuit optimization
// Usage: flux-optimize <circuit.flux> [--var Name:Init:Min:Max] [--target Name=Value:Weight]

#include "flux/analysis/advanced_analysis.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace Flux::AdvancedAnalysis;

struct VarDef
{
    std::string name;
    double init, min, max;
};

struct TargetDef
{
    std::string name;
    double value;
    double weight;
};

int main(int argc, char** argv)
{
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Circuit Optimizer                   \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-optimize <circuit.flux> [options]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --var Name:Init:Min:Max    Define optimization variable\n";
        std::cerr << "  --target Name=Value:Weight Define optimization target\n";
        std::cerr << "\nExample:\n";
        std::cerr << "  flux-optimize filter.flux --var Rval:10k:1k:100k --var Cval:1n:100p:10n --target cutoff=1000\n";
        return 1;
    }

    std::string filename = argv[1];
    std::vector<VarDef> variables;
    std::vector<TargetDef> targets;

    // Parse Arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--var" && i + 1 < argc) {
            std::string val = argv[++i];
            // Format: Name:Init:Min:Max
            std::replace(val.begin(), val.end(), ':', ' ');
            std::istringstream ss(val);
            VarDef v;
            if (ss >> v.name >> v.init >> v.min >> v.max) {
                variables.push_back(v);
            }
        } else if (arg == "--target" && i + 1 < argc) {
            std::string val = argv[++i];
            // Format: Name=Value:Weight
            size_t eq = val.find('=');
            size_t cln = val.find(':');
            if (eq != std::string::npos) {
                TargetDef t;
                t.name = val.substr(0, eq);
                std::string rhs = val.substr(eq + 1);
                if (cln != std::string::npos) {
                    t.value = std::stod(rhs.substr(0, cln - eq - 1));
                    t.weight = std::stod(rhs.substr(cln + 1));
                } else {
                    t.value = std::stod(rhs);
                    t.weight = 1.0;
                }
                targets.push_back(t);
            }
        }
    }

    // Default demo if no args provided
    if (variables.empty()) {
        std::cout << "  No variables specified. Running demo optimization...\n";
        variables.push_back({"Rval", 10000.0, 1000.0, 100000.0});
        variables.push_back({"Cval", 1e-9, 1e-12, 1e-6});
        targets.push_back({"cutoff", 1000.0, 1.0});
    }

    std::cout << " Optimizing: " << filename << "\n";
    std::cout << " Variables: " << variables.size() << " | Targets: " << targets.size() << "\n";

    // Create Optimizer
    CircuitOptimizer optimizer;
    for (const auto& v : variables) {
        optimizer.addVariable(v.name, v.init, v.min, v.max);
    }
    for (const auto& t : targets) {
        optimizer.addTarget(t.name, t.value, t.weight);
    }

    // Run Optimization
    std::cout << " Running optimization...\n";
    OptimizationResult result = optimizer.optimize(filename);

    // Output Results
    std::cout << result.toString();

    return 0;
}
