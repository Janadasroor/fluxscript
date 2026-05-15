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

#ifndef FLUX_NETLIST_GENERATOR_H
#define FLUX_NETLIST_GENERATOR_H

#include "flux/compiler/ast.h"
#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace Flux {

class NetlistGenerator {
public:
    NetlistGenerator();
    ~NetlistGenerator();
    
    // Generate complete netlist from AST
    std::string generateNetlist(
        const std::vector<std::unique_ptr<FunctionAST>>& functions,
        const std::vector<std::unique_ptr<SubcktAST>>& subckts,
        const std::vector<std::unique_ptr<ModelAST>>& models,
        const std::vector<std::unique_ptr<AnalysisExprAST>>& analyses,
        const std::vector<std::unique_ptr<MeasureExprAST>>& measures);
    
    // Generate specific SPICE cards
    std::string generateTitle(const std::string& title);
    std::string generateInclude(const std::string& file);
    std::string generateLib(const std::string& lib);
    std::string generateParam(const std::string& name, double value);
    std::string generateAnalysisCard(const AnalysisExprAST* analysis);
    std::string generateMeasureCard(const MeasureExprAST* measure);
    std::string generateProbeCard(const ProbeDeclAST* probe);
    std::string generateSaveCard(const std::vector<std::string>& signalNames);

    std::string generateSubcktCard(const SubcktAST* subckt);
    std::string generateModelCard(const ModelAST* model);
    std::string generateEnds();
    
    // Generate component instances
    std::string generateResistor(const std::string& name, const std::string& n1, 
                                  const std::string& n2, double value);
    std::string generateCapacitor(const std::string& name, const std::string& n1,
                                   const std::string& n2, double value);
    std::string generateInductor(const std::string& name, const std::string& n1,
                                  const std::string& n2, double value);
    std::string generateDiode(const std::string& name, const std::string& n1,
                               const std::string& n2, const std::string& model);
    std::string generateBJT(const std::string& name, const std::string& nc,
                             const std::string& nb, const std::string& ne,
                             const std::string& ns, const std::string& model);
    std::string generateMOSFET(const std::string& name, const std::string& nd,
                                const std::string& ng, const std::string& ns,
                                const std::string& nb, const std::string& model);
    
    // Generate behavioral sources
    std::string generateBSource(const std::string& name, 
                                const std::vector<std::string>& nodes,
                                const std::string& type,
                                const std::string& expression);
    std::string generateBVoltage(const std::string& name,
                                  const std::string& nPlus,
                                  const std::string& nMinus,
                                  const std::string& expression);
    std::string generateBCurrent(const std::string& name,
                                  const std::string& nPlus,
                                  const std::string& nMinus,
                                  const std::string& expression);

    // Generate E-source (VCVS)
    std::string generateESource(const std::string& name,
                                 const std::string& nPlus,
                                 const std::string& nMinus,
                                 const std::string& cnPlus,
                                 const std::string& cnMinus,
                                 double gain);

    // Generate F-source (CCCS)
    std::string generateFSource(const std::string& name,
                                 const std::string& nPlus,
                                 const std::string& nMinus,
                                 const std::string& vSourceName,
                                 double gain);

    // Generate G-source (VCCS)
    std::string generateGSource(const std::string& name,
                                 const std::string& nPlus,
                                 const std::string& nMinus,
                                 const std::string& cnPlus,
                                 const std::string& cnMinus,
                                 double transconductance);

    // Generate H-source (CCVS)
    std::string generateHSource(const std::string& name,
                                 const std::string& nPlus,
                                 const std::string& nMinus,
                                 const std::string& vSourceName,
                                 double transresistance);

    // Generate A-device (XSPICE digital gate/flip-flop)
    std::string generateADevice(const std::string& name,
                                ADeviceType deviceType,
                                const std::vector<std::string>& inputNodes,
                                const std::vector<std::string>& outputNodes);

    // Generate WAVEFILE voltage/current source
    std::string generateWaveFile(const std::string& name,
                                  const std::string& nPlus,
                                  const std::string& nMinus,
                                  const std::string& filePath,
                                  int channel,
                                  bool isCurrent);

    // Generate subcircuit instance
    std::string generateSubcktInstance(const std::string& name,
                                        const std::string& subcktName,
                                        const std::vector<std::string>& nodes,
                                        const std::map<std::string, double>& params);

public:
    // Public for testing
    std::string expressionToSpice(const ExprAST* expr);

private:
    std::ostringstream m_netlist;
    int m_nodeCounter;
    std::map<std::string, int> m_nodeMap;
    
    std::string getInternalNode(const std::string& name);
    std::string valueToSpice(double value);
};

} // namespace Flux

#endif // FLUX_NETLIST_GENERATOR_H
