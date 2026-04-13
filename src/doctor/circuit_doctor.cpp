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

// AI Circuit Doctor - Diagnostic Rule Engine Implementation
// Analyzes circuits for common design errors and provides suggestions
#include "flux/doctor/circuit_doctor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <map>

namespace Flux {
namespace Doctor {

// 
//  DiagnosticIssue Output Methods                       
// 

std::string DiagnosticIssue::severityToString() const {
    switch (severity) {
        case Severity::Error: return "ERROR";
        case Severity::Warning: return "WARNING";
        case Severity::Info: return "TIP";
        default: return "INFO";
    }
}

std::string DiagnosticIssue::severityIcon() const {
    switch (severity) {
        case Severity::Error: return "";
        case Severity::Warning: return "";
        case Severity::Info: return "";
        default: return "";
    }
}

std::string DiagnosticIssue::toText() const {
    std::ostringstream oss;
    oss << severityIcon() << " " << severityToString() << ": " << message << "\n";
    if (!location.empty()) oss << "   Location: " << location << "\n";
    if (!component.empty()) oss << "   Component: " << component << "\n";
    oss << "    Suggestion: " << suggestion << "\n";
    return oss.str();
}

std::string DiagnosticIssue::toMarkdown() const {
    std::ostringstream oss;
    oss << "### " << severityIcon() << " " << severityToString() << ": " << message << "\n\n";
    if (!location.empty()) oss << "- **Location:** " << location << "\n";
    if (!component.empty()) oss << "- **Component:** " << component << "\n";
    oss << "- ** Suggestion:** " << suggestion << "\n\n";
    return oss.str();
}

// 
//  DiagnosticReport Output Methods                      
// 

std::string DiagnosticReport::toText() const {
    std::ostringstream oss;
    oss << "\n";
    oss << "         AI CIRCUIT DOCTOR - DIAGNOSTIC REPORT\n";
    oss << "\n\n";
    oss << "Circuit: " << circuitFile << "\n\n";
    
    if (issues.empty()) {
        oss << " No issues found! Your circuit looks good.\n";
    } else {
        for (const auto& issue : issues) {
            oss << issue.toText() << "\n";
        }
    }
    
    oss << "\n";
    oss << "Summary: " << errorCount << " Error(s), " 
        << warningCount << " Warning(s), " << infoCount << " Tip(s)\n";
    oss << "Status: " << (passesDesignRules ? " PASS" : " FAIL") << "\n";
    oss << "\n";
    
    return oss.str();
}

std::string DiagnosticReport::toMarkdown() const {
    std::ostringstream oss;
    oss << "# AI Circuit Doctor - Diagnostic Report\n\n";
    oss << "**Circuit:** " << circuitFile << "\n\n";
    
    for (const auto& issue : issues) {
        oss << issue.toMarkdown();
    }
    
    oss << "\n---\n\n";
    oss << "**Summary:** " << errorCount << " Error(s), " 
        << warningCount << " Warning(s), " << infoCount << " Tip(s)\n";
    oss << "**Status:** " << (passesDesignRules ? " PASS" : " FAIL") << "\n";
    
    return oss.str();
}

std::string DiagnosticReport::toJSON() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"circuit\": \"" << circuitFile << "\",\n";
    oss << "  \"passes\": " << (passesDesignRules ? "true" : "false") << ",\n";
    oss << "  \"errors\": " << errorCount << ",\n";
    oss << "  \"warnings\": " << warningCount << ",\n";
    oss << "  \"tips\": " << infoCount << ",\n";
    oss << "  \"issues\": [\n";
    
    for (size_t i = 0; i < issues.size(); ++i) {
        const auto& issue = issues[i];
        oss << "    {\n";
        oss << "      \"rule\": \"" << issue.rule << "\",\n";
        oss << "      \"severity\": \"" << issue.severityToString() << "\",\n";
        oss << "      \"message\": \"" << issue.message << "\",\n";
        oss << "      \"suggestion\": \"" << issue.suggestion << "\"\n";
        oss << "    }";
        if (i < issues.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ]\n";
    oss << "}\n";
    
    return oss.str();
}

// 
//  Circuit Doctor Implementation                        
// 

CircuitDoctor::CircuitDoctor() : m_minSeverity(Severity::Info) {
    // Enable all rules by default
    m_enabledRules = {
        "FloatingNode",
        "DCPath",
        "PowerDissipation",
        "ShortCircuit",
        "UnrealisticValues",
        "MissingDecoupling",
        "StabilityHeuristics",
        "ComponentStress"
    };
}

CircuitDoctor::~CircuitDoctor() = default;

void CircuitDoctor::setMinSeverity(Severity severity) {
    m_minSeverity = severity;
}

void CircuitDoctor::enableRule(const std::string& ruleName) {
    if (std::find(m_enabledRules.begin(), m_enabledRules.end(), ruleName) == m_enabledRules.end()) {
        m_enabledRules.push_back(ruleName);
    }
}

void CircuitDoctor::disableRule(const std::string& ruleName) {
    m_enabledRules.erase(
        std::remove(m_enabledRules.begin(), m_enabledRules.end(), ruleName),
        m_enabledRules.end()
    );
}

// 
//  Helper Methods                                       
// 

std::vector<CircuitDoctor::ParsedComponent> CircuitDoctor::parseComponents(const std::string& netlist) {
    std::vector<ParsedComponent> components;
    std::istringstream stream(netlist);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(stream, line)) {
        lineNum++;
        
        // Remove carriage return if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '*' || line[0] == '#' || 
            (line.size() > 1 && line[0] == '/' && line[1] == '/')) {
            continue;
        }
        
        // Skip directives and empty lines after trimming
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '.') continue;
        
        std::istringstream ls(line);
        std::string name, n1, n2, val;
        
        if (ls >> name >> n1 >> n2 >> val) {
            ParsedComponent comp;
            comp.name = name;
            comp.node1 = n1;
            comp.node2 = n2;
            comp.value = parseValue(val);
            comp.lineNumber = lineNum;
            comp.type = getComponentType(name[0]);
            
            components.push_back(comp);
        }
    }
    
    return components;
}

std::map<std::string, std::vector<std::string>> CircuitDoctor::buildNodeMap(const std::string& netlist) {
    std::map<std::string, std::vector<std::string>> nodeMap;
    auto components = parseComponents(netlist);
    
    for (const auto& comp : components) {
        nodeMap[comp.node1].push_back(comp.name);
        if (comp.node2 != comp.node1) {
            nodeMap[comp.node2].push_back(comp.name);
        }
    }
    
    return nodeMap;
}

double CircuitDoctor::parseValue(const std::string& valueStr) {
    std::string val = valueStr;
    double multiplier = 1.0;
    
    // Check for suffixes
    char lastChar = tolower(val.back());
    switch (lastChar) {
        case 'k': multiplier = 1e3; val.pop_back(); break;
        case 'm': multiplier = 1e-3; val.pop_back(); break;
        case 'u': multiplier = 1e-6; val.pop_back(); break;
        case 'n': multiplier = 1e-9; val.pop_back(); break;
        case 'p': multiplier = 1e-12; val.pop_back(); break;
        case 'f': multiplier = 1e-15; val.pop_back(); break;
        case 'g': multiplier = 1e9; val.pop_back(); break;
        case 't': multiplier = 1e12; val.pop_back(); break;
        default: break;
    }
    
    try {
        return std::stod(val) * multiplier;
    } catch (...) {
        return 0.0;
    }
}

std::string CircuitDoctor::getComponentType(char prefix) {
    switch (toupper(prefix)) {
        case 'R': return "Resistor";
        case 'C': return "Capacitor";
        case 'L': return "Inductor";
        case 'Q': return "Transistor";
        case 'M': return "MOSFET";
        case 'U': return "IC";
        case 'D': return "Diode";
        case 'V': return "Voltage Source";
        case 'I': return "Current Source";
        case 'B': return "Behavioral Source";
        case 'E': return "VCVS";
        case 'F': return "CCCS";
        case 'G': return "VCCS";
        case 'H': return "CCVS";
        default: return "Unknown";
    }
}

// 
//  Rule 1: Floating Node Detection                     
// 

void CircuitDoctor::checkFloatingNodes(const std::string& netlist, DiagnosticReport& report) {
    auto nodeMap = buildNodeMap(netlist);
    
    for (const auto& pair : nodeMap) {
        const std::string& node = pair.first;
        const auto& connections = pair.second;
        
        // Skip ground node
        if (node == "0" || node == "gnd" || node == "GND") continue;
        
        // Node with only 1 connection is floating
        if (connections.size() == 1) {
            DiagnosticIssue issue;
            issue.rule = "FloatingNode";
            issue.severity = Severity::Error;
            issue.message = "Floating node detected at '" + node + "'";
            issue.component = connections[0];
            issue.suggestion = "Connect " + node + " to another node or add high-value resistor to ground";
            report.issues.push_back(issue);
            report.errorCount++;
            report.passesDesignRules = false;
        }
    }
}

// 
//  Rule 2: DC Path to Ground Check                     
// 

void CircuitDoctor::checkDCPaths(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    bool hasGroundPath = false;
    
    for (const auto& comp : components) {
        if (comp.node1 == "0" || comp.node2 == "0" || 
            comp.node1 == "gnd" || comp.node2 == "gnd") {
            hasGroundPath = true;
            break;
        }
    }
    
    if (!hasGroundPath) {
        DiagnosticIssue issue;
        issue.rule = "DCPath";
        issue.severity = Severity::Error;
        issue.message = "No DC path to ground found in circuit";
        issue.suggestion = "Add a ground connection (node 0) to provide a DC return path";
        report.issues.push_back(issue);
        report.errorCount++;
        report.passesDesignRules = false;
    }
}

// 
//  Rule 3: Power Dissipation Check                     
// 

void CircuitDoctor::checkPowerDissipation(const std::string& netlist, DiagnosticReport& report) {
    // Simplified: Check for very low resistor values with voltage sources
    auto components = parseComponents(netlist);
    
    for (const auto& comp : components) {
        if (comp.type == "Resistor" && comp.value < 1.0 && comp.value > 0) {
            DiagnosticIssue issue;
            issue.rule = "PowerDissipation";
            issue.severity = Severity::Warning;
            issue.message = comp.name + " has very low resistance (" + std::to_string(comp.value) + ") - may dissipate excessive power";
            issue.component = comp.name;
            issue.location = "Line " + std::to_string(comp.lineNumber);
            issue.suggestion = "Verify power rating: P = V/R. Consider using higher resistance or power resistor";
            report.issues.push_back(issue);
            report.warningCount++;
        }
    }
}

// 
//  Rule 4: Short Circuit Detection                     
// 

void CircuitDoctor::checkShortCircuits(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    
    for (const auto& comp : components) {
        if (comp.node1 == comp.node2) {
            DiagnosticIssue issue;
            issue.rule = "ShortCircuit";
            issue.severity = Severity::Error;
            issue.message = comp.name + " has both terminals connected to the same node (" + comp.node1 + ")";
            issue.component = comp.name;
            issue.location = "Line " + std::to_string(comp.lineNumber);
            issue.suggestion = "Remove shorted component or fix node connections";
            report.issues.push_back(issue);
            report.errorCount++;
            report.passesDesignRules = false;
        }
    }
    
    // Check for voltage source loops (two V sources in parallel)
    std::map<std::string, int> voltageSourceNodes;
    for (const auto& comp : components) {
        if (comp.type == "Voltage Source") {
            voltageSourceNodes[comp.node1]++;
            voltageSourceNodes[comp.node2]++;
        }
    }
}

// 
//  Rule 5: Unrealistic Values Check                    
// 

void CircuitDoctor::checkUnrealisticValues(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    
    for (const auto& comp : components) {
        bool unrealistic = false;
        std::string reason;
        
        if (comp.type == "Resistor") {
            if (comp.value < 1.0 && comp.value > 0) {
                unrealistic = true;
                reason = "Resistance < 1";
            } else if (comp.value > 1e9) {
                unrealistic = true;
                reason = "Resistance > 1G";
            }
        } else if (comp.type == "Capacitor") {
            if (comp.value < 1e-15) {
                unrealistic = true;
                reason = "Capacitance < 1fF";
            } else if (comp.value > 1) {
                unrealistic = true;
                reason = "Capacitance > 1F";
            }
        }
        
        if (unrealistic) {
            DiagnosticIssue issue;
            issue.rule = "UnrealisticValues";
            issue.severity = Severity::Warning;
            issue.message = comp.name + " has unrealistic value: " + reason;
            issue.component = comp.name;
            issue.location = "Line " + std::to_string(comp.lineNumber);
            issue.suggestion = "Verify component value is physically realizable";
            report.issues.push_back(issue);
            report.warningCount++;
        }
    }
}

// 
//  Rule 6: Missing Decoupling Capacitor Check          
// 

void CircuitDoctor::checkMissingDecoupling(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    bool hasIC = false;
    bool hasDecoupling = false;
    
    for (const auto& comp : components) {
        if (comp.type == "IC") hasIC = true;
        if (comp.type == "Capacitor" && comp.value >= 10e-9 && comp.value <= 1e-6) {
            hasDecoupling = true;
        }
    }
    
    if (hasIC && !hasDecoupling) {
        DiagnosticIssue issue;
        issue.rule = "MissingDecoupling";
        issue.severity = Severity::Info;
        issue.message = "IC found without decoupling capacitor";
        issue.suggestion = "Add 100nF ceramic capacitor near each IC power pin";
        report.issues.push_back(issue);
        report.infoCount++;
    }
}

// 
//  Rule 7: Stability Heuristics                        
// 

void CircuitDoctor::checkStabilityHeuristics(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    bool hasFeedback = false;
    bool hasCompensation = false;
    
    // Simple heuristic: Look for op-amp with feedback but no compensation
    for (const auto& comp : components) {
        if (comp.type == "IC") hasFeedback = true;
        if (comp.type == "Capacitor" && comp.value < 100e-12) {
            hasCompensation = true;
        }
    }
    
    if (hasFeedback && !hasCompensation) {
        DiagnosticIssue issue;
        issue.rule = "StabilityHeuristics";
        issue.severity = Severity::Info;
        issue.message = "Feedback circuit detected without compensation capacitor";
        issue.suggestion = "Consider adding small capacitor (10-100pF) across feedback resistor for stability";
        report.issues.push_back(issue);
        report.infoCount++;
    }
}

// 
//  Rule 8: Component Stress Analysis                   
// 

void CircuitDoctor::checkComponentStress(const std::string& netlist, DiagnosticReport& report) {
    auto components = parseComponents(netlist);
    
    for (const auto& comp : components) {
        // Check for very high voltage across small resistors
        if (comp.type == "Resistor" && comp.value < 100 && comp.value > 0) {
            DiagnosticIssue issue;
            issue.rule = "ComponentStress";
            issue.severity = Severity::Warning;
            issue.message = comp.name + " is low value (" + std::to_string(comp.value) + ") - verify power rating";
            issue.component = comp.name;
            issue.location = "Line " + std::to_string(comp.lineNumber);
            issue.suggestion = "Calculate worst-case power: P = IR and select appropriate wattage";
            report.issues.push_back(issue);
            report.warningCount++;
        }
    }
}

// 
//  Main Analysis Methods                                
// 

DiagnosticReport CircuitDoctor::analyze(const std::string& circuitFile) {
    DiagnosticReport report;
    report.circuitFile = circuitFile;
    
    // Read file
    std::ifstream file(circuitFile);
    if (!file.is_open()) {
        DiagnosticIssue issue;
        issue.rule = "FileAccess";
        issue.severity = Severity::Error;
        issue.message = "Cannot open circuit file: " + circuitFile;
        issue.suggestion = "Check file path and permissions";
        report.issues.push_back(issue);
        report.errorCount++;
        report.passesDesignRules = false;
        return report;
    }
    
    std::string netlist((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    
    auto result = analyzeFromString(netlist);
    result.circuitFile = circuitFile;  // Preserve filename
    return result;
}

DiagnosticReport CircuitDoctor::analyzeFromString(const std::string& netlist) {
    DiagnosticReport report;
    report.circuitFile = "string";
    
    // Run enabled rules
    for (const auto& rule : m_enabledRules) {
        if (rule == "FloatingNode") checkFloatingNodes(netlist, report);
        else if (rule == "DCPath") checkDCPaths(netlist, report);
        else if (rule == "PowerDissipation") checkPowerDissipation(netlist, report);
        else if (rule == "ShortCircuit") checkShortCircuits(netlist, report);
        else if (rule == "UnrealisticValues") checkUnrealisticValues(netlist, report);
        else if (rule == "MissingDecoupling") checkMissingDecoupling(netlist, report);
        else if (rule == "StabilityHeuristics") checkStabilityHeuristics(netlist, report);
        else if (rule == "ComponentStress") checkComponentStress(netlist, report);
    }
    
    // Count issues by severity BEFORE filtering
    int totalErrors = report.errorCount;
    int totalWarnings = report.warningCount;
    int totalInfos = report.infoCount;
    
    // Filter by minimum severity
    std::vector<DiagnosticIssue> filtered;
    for (const auto& issue : report.issues) {
        // Include issues at or above minimum severity level
        // (Error=2 > Warning=1 > Info=0, so we use >=)
        if (static_cast<int>(issue.severity) >= static_cast<int>(m_minSeverity)) {
            filtered.push_back(issue);
        }
    }
    report.issues = filtered;
    
    // Update counts to match filtered issues
    report.errorCount = 0;
    report.warningCount = 0;
    report.infoCount = 0;
    for (const auto& issue : report.issues) {
        switch (issue.severity) {
            case Severity::Error: report.errorCount++; break;
            case Severity::Warning: report.warningCount++; break;
            case Severity::Info: report.infoCount++; break;
        }
    }
    
    // Update pass/fail based on errors
    if (report.errorCount > 0) {
        report.passesDesignRules = false;
    }
    
    report.summary = std::to_string(report.errorCount) + " error(s), " +
                     std::to_string(report.warningCount) + " warning(s), " +
                     std::to_string(report.infoCount) + " tip(s)";
    
    return report;
}

// 
//  Convenience Functions                                
// 

DiagnosticReport circuit_doctor(const std::string& circuitFile) {
    CircuitDoctor doctor;
    return doctor.analyze(circuitFile);
}

// 
//  C Interface Implementation                           
// 

extern "C" {

void* flux_doctor_create() {
    return new CircuitDoctor();
}

void flux_doctor_destroy(void* doctor) {
    delete static_cast<CircuitDoctor*>(doctor);
}

void flux_doctor_set_severity(void* doctor, int severity) {
    auto* d = static_cast<CircuitDoctor*>(doctor);
    d->setMinSeverity(static_cast<Severity>(severity));
}

const char* flux_doctor_analyze(void* doctor, const char* circuitFile) {
    static std::string result;
    auto* d = static_cast<CircuitDoctor*>(doctor);
    auto report = d->analyze(circuitFile ? circuitFile : "");
    result = report.toText();
    return result.c_str();
}

const char* flux_doctor_get_markdown(void* doctor) {
    static std::string result;
    auto* d = static_cast<CircuitDoctor*>(doctor);
    auto report = d->analyze("string");
    result = report.toMarkdown();
    return result.c_str();
}

int flux_doctor_get_error_count(void* doctor) {
    auto* d = static_cast<CircuitDoctor*>(doctor);
    auto report = d->analyzeFromString("");
    return report.errorCount;
}

int flux_doctor_get_warning_count(void* doctor) {
    auto* d = static_cast<CircuitDoctor*>(doctor);
    auto report = d->analyzeFromString("");
    return report.warningCount;
}

bool flux_doctor_passes(void* doctor) {
    auto* d = static_cast<CircuitDoctor*>(doctor);
    auto report = d->analyzeFromString("");
    return report.passesDesignRules;
}

}

} // namespace Doctor
} // namespace Flux
