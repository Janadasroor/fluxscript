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

// flux-instrument: Test and control laboratory instruments via SCPI
// Usage: flux-instrument <IP_ADDRESS> <COMMAND>

#include "flux/instruments/instrument.h"
#include <iostream>
#include <string>

using namespace Flux::Instruments;

int main(int argc, char** argv)
{
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Instrument Controller               \n";
    std::cout << "\n";
    std::cout << "\n";

    if (argc < 2) {
        std::cerr << "Usage: flux-instrument <IP_ADDRESS> [COMMAND]\n";
        return 1;
    }

    std::string ip = argv[1];

    // If no command, just test connectivity
    if (argc == 2) {
        Instrument inst;
        if (!inst.connect(ip, 5025))
            return 1;
        std::cout << "Connected to: " << inst.getID() << "\n";
        return 0;
    }

    // Handle commands
    std::string arg = argv[2];

    // Raw SCPI command
    if (arg.find("--") == std::string::npos || arg.find("?") != std::string::npos) {
        Instrument inst;
        if (!inst.connect(ip, 5025))
            return 1;
        std::cout << "Response: " << inst.query(arg) << "\n";
    }
    // Power Supply Helper
    else if (arg == "--ps") {
        PowerSupply ps;
        if (!ps.connect(ip, 5025))
            return 1;
        std::cout << "Device: " << ps.getID() << "\n";

        ps.setOutput(true);

        for (int i = 3; i < argc; ++i) {
            std::string opt = argv[i];
            if (opt == "--set-volt" && i + 1 < argc) {
                double v = std::stod(argv[++i]);
                ps.setVoltage(v);
                std::cout << "Set Voltage to " << v << "V\n";
            } else if (opt == "--measure") {
                std::cout << "Measured: " << ps.measureVoltage() << "V, " << ps.measureCurrent() << "A\n";
            }
        }
    }

    return 0;
}
