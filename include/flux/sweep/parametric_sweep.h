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

#ifndef FLUX_SWEEP_H
#define FLUX_SWEEP_H

#include <string>
#include <vector>

namespace Flux {
namespace Sweep {

//
//  Sweep Result Structure
//

struct SweepPoint
{
    double componentValue;
    double outputValue;
    bool simulationSuccess;
};

struct SweepReport
{
    std::string circuitFile;
    std::string componentName;
    double startValue;
    double stopValue;
    int points;
    std::string measureExpression;
    std::vector<SweepPoint> data;

    double getMinOutput() const;
    double getMaxOutput() const;
    double getMinValuePoint() const;
    double getMaxValuePoint() const;

    std::string toText() const;
    std::string toMarkdown() const;
};

//
//  Sweep Engine
//

class SweepEngine
{
public:
    SweepEngine();
    ~SweepEngine();

    // Configuration
    void setCircuitFile(const std::string& file);
    void setComponent(const std::string& name);
    void setRange(double start, double stop, int points);
    void setMeasure(const std::string& expression);

    // Execution
    SweepReport run();

private:
    std::string m_circuitFile;
    std::string m_componentName;
    double m_startValue;
    double m_stopValue;
    int m_numPoints;
    std::string m_measureExpression;

    // Helper: Run simulation for specific value
    double runSingleSimulation(const std::string& netlist, double value);
    std::string modifyNetlist(const std::string& original, double value);
};

} // namespace Sweep
} // namespace Flux

#endif // FLUX_SWEEP_H
