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

// flux-doctor: AI Circuit Doctor - Automatic diagnostic analysis
// Usage: flux-doctor <circuit.flux> [--severity warning|error] [--format text|json|markdown]

#include "flux/doctor/circuit_doctor.h"
#include <iostream>
#include <string>

using namespace Flux::Doctor;

int main(int argc, char** argv)
{
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript AI Circuit Doctor                   \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-doctor <circuit.flux> [options]\n";
        std::cerr << "\nOptions:\n";
        std::cerr << "  --severity warning|error   Minimum severity level to show\n";
        std::cerr << "  --format text|json|markdown Output format (default: text)\n";
        std::cerr << "\nExample:\n";
        std::cerr << "  flux-doctor amplifier.flux --severity warning --format text\n";
        return 1;
    }

    std::string filename = argv[1];
    Severity minSeverity = Severity::Info;
    std::string format = "text";

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--severity" && i + 1 < argc) {
            std::string sev = argv[++i];
            if (sev == "error")
                minSeverity = Severity::Error;
            else if (sev == "warning")
                minSeverity = Severity::Warning;
            else if (sev == "info")
                minSeverity = Severity::Info;
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        }
    }

    std::cout << " Analyzing: " << filename << "\n";
    std::cout << " Running 8 diagnostic rules...\n\n";

    // Create Doctor
    CircuitDoctor doctor;
    doctor.setMinSeverity(minSeverity);

    // Run Analysis
    DiagnosticReport report = doctor.analyze(filename);

    // Output Results
    if (format == "json") {
        std::cout << report.toJSON();
    } else if (format == "markdown") {
        std::cout << report.toMarkdown();
    } else {
        std::cout << report.toText();
    }

    return 0;
}
