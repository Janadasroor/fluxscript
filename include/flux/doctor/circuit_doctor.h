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

#ifndef FLUX_CIRCUIT_DOCTOR_H
#define FLUX_CIRCUIT_DOCTOR_H

#include <string>
#include <vector>
#include <map>

namespace Flux {
namespace Doctor {

// 
//  Diagnostic Severity Levels                          
// 
enum class Severity {
    Info,       // Helpful tips
    Warning,    // Potential issues
    Error       // Critical problems
};

// 
//  Diagnostic Issue Structure                          
// 
struct DiagnosticIssue {
    std::string rule;           // Rule name (e.g., "FloatingNode")
    Severity severity;          // Info, Warning, or Error
    std::string message;        // What's wrong
    std::string suggestion;     // How to fix it
    std::string location;       // Line number or component name
    std::string component;      // Affected component
    
    std::string severityToString() const;
    std::string severityIcon() const;
    std::string toText() const;
    std::string toMarkdown() const;
};

// 
//  Diagnostic Report                                   
// 
struct DiagnosticReport {
    std::string circuitFile;
    std::vector<DiagnosticIssue> issues;
    int errorCount;
    int warningCount;
    int infoCount;
    bool passesDesignRules;
    std::string summary;
    
    DiagnosticReport() : errorCount(0), warningCount(0), infoCount(0), 
                         passesDesignRules(true) {}
    
    std::string toText() const;
    std::string toMarkdown() const;
    std::string toJSON() const;
};

// 
//  Circuit Doctor Class                                
// 
class CircuitDoctor {
public:
    CircuitDoctor();
    ~CircuitDoctor();
    
    // Configuration
    void setMinSeverity(Severity severity);
    void enableRule(const std::string& ruleName);
    void disableRule(const std::string& ruleName);
    
    // Run diagnostics
    DiagnosticReport analyze(const std::string& circuitFile);
    DiagnosticReport analyzeFromString(const std::string& netlist);
    
private:
    Severity m_minSeverity;
    std::vector<std::string> m_enabledRules;
    
    // Diagnostic Rules
    void checkFloatingNodes(const std::string& netlist, DiagnosticReport& report);
    void checkDCPaths(const std::string& netlist, DiagnosticReport& report);
    void checkPowerDissipation(const std::string& netlist, DiagnosticReport& report);
    void checkShortCircuits(const std::string& netlist, DiagnosticReport& report);
    void checkUnrealisticValues(const std::string& netlist, DiagnosticReport& report);
    void checkMissingDecoupling(const std::string& netlist, DiagnosticReport& report);
    void checkStabilityHeuristics(const std::string& netlist, DiagnosticReport& report);
    void checkComponentStress(const std::string& netlist, DiagnosticReport& report);
    
    // Helper methods
    struct ParsedComponent {
        std::string name;
        std::string type;
        std::string node1;
        std::string node2;
        double value;
        int lineNumber;
    };
    
    std::vector<ParsedComponent> parseComponents(const std::string& netlist);
    std::map<std::string, std::vector<std::string>> buildNodeMap(const std::string& netlist);
    double parseValue(const std::string& valueStr);
    std::string getComponentType(char prefix);
};

// 
//  Convenience Functions                                
// 
DiagnosticReport circuit_doctor(const std::string& circuitFile);

// 
//  C Interface for JIT                                  
// 
extern "C" {
    void* flux_doctor_create();
    void flux_doctor_destroy(void* doctor);
    void flux_doctor_set_severity(void* doctor, int severity);
    const char* flux_doctor_analyze(void* doctor, const char* circuitFile);
    const char* flux_doctor_get_markdown(void* doctor);
    int flux_doctor_get_error_count(void* doctor);
    int flux_doctor_get_warning_count(void* doctor);
    bool flux_doctor_passes(void* doctor);
}

} // namespace Doctor
} // namespace Flux

#endif // FLUX_CIRCUIT_DOCTOR_H
