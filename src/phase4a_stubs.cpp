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

// Phase 4A Stub Implementations
// Package Manager, Debugger, Sensitivity Analysis, Natural Language

#include "flux/package/package_manager.h"
#include "flux/debug/debugger.h"
#include "flux/analysis/sensitivity.h"
#include "flux/nlp/natural_language.h"
#include <sstream>
#include <fstream>
#include <cmath>
#include <random>
#include <iostream>
#include <complex>
#include <cctype>

namespace Flux {
namespace PackageManager {

// PackageCLI (see src/package/package_manager.cpp for all other implementations)
int PackageCLI::run(int argc, char** argv) {
    std::cout << "FluxScript Package Manager v1.0\n\n";
    
    if (argc < 2) {
        printHelp();
        return 0;
    }
    
    std::string cmd = argv[1];
    PackageManager pkg;
    pkg.initialize(".");
    
    if (cmd == "install" || cmd == "i") {
        if (argc < 3) {
            std::cerr << "Error: Package name required\n";
            return 1;
        }
        pkg.install(argv[2], argc > 3 ? argv[3] : "");
    } else if (cmd == "search" || cmd == "s") {
        auto results = PackageRegistry::instance().search(argc > 2 ? argv[2] : "");
        std::cout << "Search results:\n";
        for (const auto& pkg : results) {
            std::cout << "  " << pkg.name << " - " << pkg.description 
                      << " (v" << pkg.latestVersion.toString() << ")\n";
        }
    } else if (cmd == "list" || cmd == "l") {
        auto installed = pkg.getInstalledPackages();
        std::cout << "Installed packages:\n";
        for (const auto& p : installed) {
            std::cout << "  " << p.name << " v" << p.version.toString() << "\n";
        }
    } else if (cmd == "update" || cmd == "u") {
        pkg.updateAll();
    } else if (cmd == "audit") {
        auto result = pkg.audit();
        std::cout << "Audit complete. " << (result.hasIssues ? "Issues found." : "No issues.") << "\n";
    } else if (cmd == "help" || cmd == "h") {
        printHelp();
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        printHelp();
        return 1;
    }
    
    return 0;
}

void PackageCLI::printHelp() {
    std::cout << "Usage: flux-pkg <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  install <package> [version]  Install a package\n";
    std::cout << "  search <query>               Search packages\n";
    std::cout << "  list                         List installed packages\n";
    std::cout << "  update                       Update all packages\n";
    std::cout << "  audit                        Audit dependencies\n";
    std::cout << "  help                         Show this help\n";
}

// C Interface
extern "C" {

void* flux_pkg_create(const char* projectPath) {
    auto* pkg = new PackageManager();
    pkg->initialize(projectPath ? projectPath : ".");
    return pkg;
}

void flux_pkg_destroy(void* pkg) {
    delete static_cast<PackageManager*>(pkg);
}

int flux_pkg_install(void* pkg, const char* packageName, const char* version) {
    return static_cast<PackageManager*>(pkg)->install(
        packageName, version ? version : "") ? 1 : 0;
}

int flux_pkg_uninstall(void* pkg, const char* packageName) {
    return static_cast<PackageManager*>(pkg)->uninstall(packageName) ? 1 : 0;
}

int flux_pkg_update(void* pkg, const char* packageName) {
    return static_cast<PackageManager*>(pkg)->update(packageName) ? 1 : 0;
}

int flux_pkg_is_installed(void* pkg, const char* packageName) {
    return static_cast<PackageManager*>(pkg)->isInstalled(packageName) ? 1 : 0;
}

const char* flux_pkg_search(void* pkg, const char* query) {
    static std::string result;
    auto results = PackageRegistry::instance().search(query ? query : "");
    result = "Found " + std::to_string(results.size()) + " packages";
    return result.c_str();
}

const char* flux_pkg_audit(void* pkg) {
    static std::string result = "No issues found";
    auto auditResult = static_cast<PackageManager*>(pkg)->audit();
    if (auditResult.hasIssues) {
        result = "Issues found";
    }
    return result.c_str();
}

}

} // namespace PackageManager
} // namespace Flux

namespace Flux {
namespace Debugger {

// CircuitDebugger
CircuitDebugger::CircuitDebugger() = default;
CircuitDebugger::~CircuitDebugger() = default;

void CircuitDebugger::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

void CircuitDebugger::loadCircuitFromNetlist(const std::string& netlist) {
    m_netlist = netlist;
}

void CircuitDebugger::addSymptom(const CircuitSymptom& symptom) {
    m_symptoms.push_back(symptom);
}

DebugReport CircuitDebugger::diagnose() {
    DebugReport report;
    report.circuitName = m_circuitFile;
    report.healthScore = 85;
    
    // Stub analysis
    if (!m_symptoms.empty() && m_symptoms[0].type == "clipping") {
        DebugIssue issue;
        issue.message = "Output clipping detected";
        issue.suggestion = "Check bias point and signal swing";
        issue.severity = IssueSeverity::Warning;
        issue.confidence = 0.8;
        report.issues.push_back(issue);
    }
    
    report.recommendations.push_back("Run DC operating point analysis");
    report.recommendations.push_back("Check component stress levels");
    
    return report;
}

DebugReport CircuitDebugger::analyze() {
    return diagnose();
}

std::vector<DebugIssue> CircuitDebugger::checkBiasPoints() {
    return {};
}

std::vector<DebugIssue> CircuitDebugger::checkStability() {
    return {};
}

std::string DebugReport::toString() const {
    std::ostringstream oss;
    oss << "Debug Report for " << circuitName << "\n";
    oss << "Health Score: " << healthScore << "/100\n";
    oss << "Issues: " << issues.size() << "\n";
    for (const auto& issue : issues) {
        oss << "  - " << issue.message << "\n";
    }
    return oss.str();
}

// DesignRuleChecker
DesignRuleChecker::DesignRuleChecker() = default;
DesignRuleChecker::~DesignRuleChecker() = default;

void DesignRuleChecker::enableVoltageStressRules() {}
void DesignRuleChecker::enablePowerDissipationRules() {}
void DesignRuleChecker::enableThermalRules() {}

std::vector<DebugIssue> DesignRuleChecker::runChecks() {
    return {};
}

bool DesignRuleChecker::hasViolations() const {
    return false;
}

// RootCauseAnalyzer
RootCauseAnalyzer::RootCauseAnalyzer() = default;
RootCauseAnalyzer::~RootCauseAnalyzer() = default;

void RootCauseAnalyzer::addEvidence(const std::string& evidence, double weight) {
    m_evidence[evidence] = weight;
}

std::vector<DebugIssue> RootCauseAnalyzer::findRootCauses() {
    return {};
}

// FixSuggester
FixSuggester::FixSuggester() = default;
FixSuggester::~FixSuggester() = default;

std::vector<FixSuggester::FixSuggestion> FixSuggester::suggestFixes(const DebugIssue& issue) {
    std::vector<FixSuggestion> suggestions;
    
    FixSuggestion sug;
    sug.description = "Suggested fix";
    sug.expectedImprovement = 0.5;
    suggestions.push_back(sug);
    
    return suggestions;
}

// DebugSession
DebugSession::DebugSession() : m_active(false) {}
DebugSession::~DebugSession() = default;

void DebugSession::start(const std::string& circuitFile) {
    m_active = true;
    m_circuitFile = circuitFile;
    m_debugger.loadCircuit(circuitFile);
}

void DebugSession::end() {
    m_active = false;
}

bool DebugSession::isActive() const {
    return m_active;
}

std::string DebugSession::executeCommand(const std::string& command) {
    return "Command executed: " + command;
}

std::string DebugSession::ask(const std::string& question) {
    return "Analysis: " + question;
}

// C Interface
extern "C" {

void* flux_debug_create() {
    return new CircuitDebugger();
}

void flux_debug_destroy(void* debug) {
    delete static_cast<CircuitDebugger*>(debug);
}

void flux_debug_load(void* debug, const char* circuitFile) {
    static_cast<CircuitDebugger*>(debug)->loadCircuit(circuitFile ? circuitFile : "");
}

void flux_debug_add_symptom(void* debug, const char* type, const char* description) {
    CircuitSymptom symptom;
    symptom.type = type ? type : "";
    symptom.description = description ? description : "";
    static_cast<CircuitDebugger*>(debug)->addSymptom(symptom);
}

const char* flux_debug_diagnose(void* debug) {
    static std::string result;
    auto report = static_cast<CircuitDebugger*>(debug)->diagnose();
    result = report.toString();
    return result.c_str();
}

const char* flux_debug_get_recommendations(void* debug) {
    static std::string result = "Run DC analysis\nCheck component stress";
    return result.c_str();
}

const char* flux_debug_ask(void* debug, const char* question) {
    static std::string result = "Analysis of: ";
    result += question ? question : "";
    return result.c_str();
}

void* flux_drc_create() {
    return new DesignRuleChecker();
}

void flux_drc_destroy(void* drc) {
    delete static_cast<DesignRuleChecker*>(drc);
}

void flux_drc_enable_rules(void* drc, const char* rules) {
    auto* checker = static_cast<DesignRuleChecker*>(drc);
    std::string r = rules ? rules : "";
    if (r.find("voltage") != std::string::npos) checker->enableVoltageStressRules();
    if (r.find("power") != std::string::npos) checker->enablePowerDissipationRules();
    if (r.find("thermal") != std::string::npos) checker->enableThermalRules();
}

const char* flux_drc_run(void* drc) {
    static std::string result = "No violations found";
    auto* checker = static_cast<DesignRuleChecker*>(drc);
    if (checker->hasViolations()) {
        result = "Violations found";
    }
    return result.c_str();
}

}

} // namespace Debugger
} // namespace Flux

namespace Flux {
namespace Sensitivity {

// SensitivityAnalyzer
SensitivityAnalyzer::SensitivityAnalyzer() : m_method(Method::FiniteDifference) {}
SensitivityAnalyzer::~SensitivityAnalyzer() = default;

void SensitivityAnalyzer::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

void SensitivityAnalyzer::setOutputVariable(const std::string& var) {
    m_outputVariable = var;
}

void SensitivityAnalyzer::setParameter(const std::string& param, double nominal, double tolerance) {
    m_parameters.push_back(param);
    m_nominalValues[param] = nominal;
    m_tolerances[param] = tolerance;
}

SensitivityReport SensitivityAnalyzer::analyze() {
    SensitivityReport report;
    report.circuitName = m_circuitFile;
    report.outputVariable = m_outputVariable;
    
    // Generate stub results
    for (const auto& param : m_parameters) {
        SensitivityResult result;
        result.outputVariable = m_outputVariable;
        result.parameter = param;
        result.absolute = 0.01 * m_nominalValues[param];
        result.relative = 0.01;
        result.normalized = 1.0;
        report.results.push_back(result);
    }
    
    report.meanSensitivity = 0.01;
    report.maxSensitivity = 0.02;
    report.minSensitivity = 0.005;
    
    return report;
}

std::string SensitivityReport::toString() const {
    std::ostringstream oss;
    oss << "Sensitivity Report: " << outputVariable << "\n";
    oss << "Parameters: " << results.size() << "\n";
    for (const auto& r : results) {
        oss << "  " << r.parameter << ": " << r.relative * 100 << "%\n";
    }
    return oss.str();
}

// ToleranceAnalyzer
ToleranceAnalyzer::ToleranceAnalyzer() : m_method(ToleranceMethod::RSS), m_numSamples(1000) {}
ToleranceAnalyzer::~ToleranceAnalyzer() = default;

void ToleranceAnalyzer::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

void ToleranceAnalyzer::setTolerance(const std::string& component, double tolerance) {
    m_tolerances[component] = tolerance;
}

ToleranceAnalysisResult ToleranceAnalyzer::analyze() {
    ToleranceAnalysisResult result;
    result.nominal = 5.0;
    result.min = 4.75;
    result.max = 5.25;
    result.stdDev = 0.05;
    return result;
}

double ToleranceAnalyzer::estimateYield(double specLower, double specUpper) {
    return 0.95;  // 95% yield
}

// ParameterOptimizer
ParameterOptimizer::ParameterOptimizer() : m_goal(Goal::Maximize), m_method(Method::BFGS),
                                            m_maxIterations(100), m_tolerance(1e-6) {}
ParameterOptimizer::~ParameterOptimizer() = default;

void ParameterOptimizer::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

void ParameterOptimizer::setObjective(const std::string& expression, Goal goal) {
    m_objective = expression;
    m_goal = goal;
}

void ParameterOptimizer::setParameterBounds(const std::string& param, double lower, double upper) {
    m_bounds[param] = {lower, upper};
}

ParameterOptimizer::OptimizationResult ParameterOptimizer::optimize() {
    OptimizationResult result;
    result.success = true;
    result.iterations = 15;
    result.objectiveValue = 0.95;
    result.message = "Optimization converged";
    return result;
}

// CornerAnalyzer
CornerAnalyzer::CornerAnalyzer() = default;
CornerAnalyzer::~CornerAnalyzer() = default;

void CornerAnalyzer::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

void CornerAnalyzer::useStandardCorners(const std::string& type) {
    if (type == "process" || type == "all") {
        std::map<std::string, double> tt_params = {{"temp", 25}, {"vcc", 5.0}};
        std::map<std::string, double> ff_params = {{"temp", -40}, {"vcc", 5.5}};
        std::map<std::string, double> ss_params = {{"temp", 125}, {"vcc", 4.5}};
        addCorner("tt", tt_params);
        addCorner("ff", ff_params);
        addCorner("ss", ss_params);
    }
}

void CornerAnalyzer::addCorner(const std::string& name, const std::map<std::string, double>& parameters) {
    m_corners.push_back({name, parameters});
}

std::vector<CornerAnalyzer::CornerResult> CornerAnalyzer::analyze(
    const std::vector<std::string>& outputs) {
    std::vector<CornerResult> results;
    for (const auto& corner : m_corners) {
        CornerResult r;
        r.cornerName = corner.first;
        r.parameterValues = corner.second;
        results.push_back(r);
    }
    return results;
}

// C Interface
extern "C" {

void* flux_sens_create() {
    return new SensitivityAnalyzer();
}

void flux_sens_destroy(void* sens) {
    delete static_cast<SensitivityAnalyzer*>(sens);
}

void flux_sens_load(void* sens, const char* circuitFile) {
    static_cast<SensitivityAnalyzer*>(sens)->loadCircuit(circuitFile ? circuitFile : "");
}

void flux_sens_set_output(void* sens, const char* output) {
    static_cast<SensitivityAnalyzer*>(sens)->setOutputVariable(output ? output : "");
}

void flux_sens_set_parameter(void* sens, const char* param, double nominal, double tol) {
    static_cast<SensitivityAnalyzer*>(sens)->setParameter(param ? param : "", nominal, tol);
}

void flux_sens_analyze(void* sens) {
    static_cast<SensitivityAnalyzer*>(sens)->analyze();
}

const char* flux_sens_get_results(void* sens) {
    static std::string result;
    auto report = static_cast<SensitivityAnalyzer*>(sens)->analyze();
    result = report.toString();
    return result.c_str();
}

double flux_sens_get_value(void* sens, const char* param) {
    return 0.01;
}

void* flux_tol_create() {
    return new ToleranceAnalyzer();
}

void flux_tol_destroy(void* tol) {
    delete static_cast<ToleranceAnalyzer*>(tol);
}

void flux_tol_load(void* tol, const char* circuitFile) {
    static_cast<ToleranceAnalyzer*>(tol)->loadCircuit(circuitFile ? circuitFile : "");
}

void flux_tol_set_tolerance(void* tol, const char* component, double tol_value) {
    static_cast<ToleranceAnalyzer*>(tol)->setTolerance(component ? component : "", tol_value);
}

void flux_tol_analyze(void* tol) {
    static_cast<ToleranceAnalyzer*>(tol)->analyze();
}

double flux_tol_get_yield(void* tol, double spec_low, double spec_high) {
    return static_cast<ToleranceAnalyzer*>(tol)->estimateYield(spec_low, spec_high);
}

void* flux_opt_create() {
    return new ParameterOptimizer();
}

void flux_opt_destroy(void* opt) {
    delete static_cast<ParameterOptimizer*>(opt);
}

void flux_opt_load(void* opt, const char* circuitFile) {
    static_cast<ParameterOptimizer*>(opt)->loadCircuit(circuitFile ? circuitFile : "");
}

void flux_opt_set_objective(void* opt, const char* expr, const char* goal) {
    std::string g = goal ? goal : "";
    ParameterOptimizer::Goal gval = ParameterOptimizer::Goal::Maximize;
    if (g == "minimize") gval = ParameterOptimizer::Goal::Minimize;
    if (g == "target") gval = ParameterOptimizer::Goal::Target;
    static_cast<ParameterOptimizer*>(opt)->setObjective(expr ? expr : "", gval);
}

void flux_opt_set_bounds(void* opt, const char* param, double lower, double upper) {
    static_cast<ParameterOptimizer*>(opt)->setParameterBounds(param ? param : "", lower, upper);
}

int flux_opt_optimize(void* opt) {
    return static_cast<ParameterOptimizer*>(opt)->optimize().success ? 1 : 0;
}

double flux_opt_get_value(void* opt, const char* param) {
    return 1.0;
}

void* flux_corner_create() {
    return new CornerAnalyzer();
}

void flux_corner_destroy(void* corner) {
    delete static_cast<CornerAnalyzer*>(corner);
}

void flux_corner_load(void* corner, const char* circuitFile) {
    static_cast<CornerAnalyzer*>(corner)->loadCircuit(circuitFile ? circuitFile : "");
}

void flux_corner_use_standard(void* corner, const char* type) {
    static_cast<CornerAnalyzer*>(corner)->useStandardCorners(type ? type : "");
}

const char* flux_corner_analyze(void* corner) {
    static std::string result = "Corner analysis complete";
    return result.c_str();
}

const char* flux_corner_get_worst(void* corner, const char* output) {
    static std::string result = "ss corner";
    return result.c_str();
}

}

} // namespace Sensitivity
} // namespace Flux

namespace Flux {
namespace NaturalLanguage {

// NLPProcessor
NLPProcessor::NLPProcessor() {
    // Initialize intent patterns
    m_intentPatterns["what"] = Intent::Query;
    m_intentPatterns["show"] = Intent::Query;
    m_intentPatterns["run"] = Intent::Command;
    m_intentPatterns["change"] = Intent::Modify;
    m_intentPatterns["why"] = Intent::Debug;
    m_intentPatterns["explain"] = Intent::Explain;
}

NLPProcessor::~NLPProcessor() = default;

void NLPProcessor::initialize(const std::string& domain) {
    // Domain-specific initialization
}

ParsedQuery NLPProcessor::parse(const std::string& query) {
    ParsedQuery parsed;
    
    // Simple keyword-based parsing
    std::string lower = query;
    for (auto& c : lower) c = tolower(c);
    
    if (lower.find("what") != std::string::npos || lower.find("show") != std::string::npos) {
        parsed.intent = Intent::Query;
        parsed.confidence = 0.9;
    } else if (lower.find("run") != std::string::npos || lower.find("simulate") != std::string::npos) {
        parsed.intent = Intent::Command;
        parsed.confidence = 0.9;
    } else if (lower.find("change") != std::string::npos || lower.find("set") != std::string::npos) {
        parsed.intent = Intent::Modify;
        parsed.confidence = 0.8;
    } else if (lower.find("why") != std::string::npos) {
        parsed.intent = Intent::Debug;
        parsed.confidence = 0.85;
    } else if (lower.find("explain") != std::string::npos) {
        parsed.intent = Intent::Explain;
        parsed.confidence = 0.9;
    } else {
        parsed.intent = Intent::Unknown;
        parsed.confidence = 0.5;
    }
    
    return parsed;
}

QueryResponse NLPProcessor::execute(const ParsedQuery& query) {
    QueryResponse response;
    response.success = true;
    
    switch (query.intent) {
        case Intent::Query:
            response.text = "Query result";
            response.type = "value";
            break;
        case Intent::Command:
            response.text = "Command executed";
            response.type = "status";
            break;
        case Intent::Debug:
            response.text = "Debug analysis complete";
            response.type = "explanation";
            break;
        default:
            response.text = "Response";
            response.type = "text";
    }
    
    return response;
}

QueryResponse NLPProcessor::query(const std::string& text) {
    ParsedQuery parsed = parse(text);
    return execute(parsed);
}

// CircuitExplainer
CircuitExplainer::CircuitExplainer() = default;
CircuitExplainer::~CircuitExplainer() = default;

void CircuitExplainer::loadCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
}

std::string CircuitExplainer::explainFunction() {
    return "This circuit performs signal amplification with filtering.";
}

std::string CircuitExplainer::explainOperation() {
    return "The input signal is amplified by Q1, then filtered by R2-C1.";
}

std::string CircuitExplainer::explainComponent(const std::string& component) {
    return component + " provides bias current for the amplifier stage.";
}

std::string CircuitExplainer::answerWhy(const std::string& question) {
    return "This is because of the circuit topology and component values.";
}

std::string CircuitExplainer::answerHow(const std::string& question) {
    return "The circuit works by amplifying the input signal through multiple stages.";
}

std::string CircuitExplainer::generateDocumentation() {
    return "# Circuit Documentation\n\n## Overview\n\nThis circuit...";
}

// VoiceInterface
VoiceInterface& VoiceInterface::instance() {
    static VoiceInterface instance;
    return instance;
}

VoiceInterface::VoiceInterface() : m_language("en-US") {}

void VoiceInterface::initialize() {}

std::string VoiceInterface::recognize(const std::vector<double>& audioData) {
    return "recognized command";
}

// ConversationalAssistant
ConversationalAssistant::ConversationalAssistant() : m_sessionActive(false), m_personality("professional") {}
ConversationalAssistant::~ConversationalAssistant() = default;

void ConversationalAssistant::startSession() {
    m_sessionActive = true;
}

std::string ConversationalAssistant::processMessage(const std::string& message) {
    return "I understand. Let me help you with: " + message;
}

void ConversationalAssistant::endSession() {
    m_sessionActive = false;
}

// C Interface
extern "C" {

void* flux_nlp_create() {
    return new NLPProcessor();
}

void flux_nlp_destroy(void* nlp) {
    delete static_cast<NLPProcessor*>(nlp);
}

void flux_nlp_initialize(void* nlp, const char* domain) {
    static_cast<NLPProcessor*>(nlp)->initialize(domain ? domain : "electronics");
}

const char* flux_nlp_query(void* nlp, const char* query) {
    static std::string result;
    auto response = static_cast<NLPProcessor*>(nlp)->query(query ? query : "");
    result = response.text;
    return result.c_str();
}

void* flux_explainer_create() {
    return new CircuitExplainer();
}

void flux_explainer_destroy(void* explainer) {
    delete static_cast<CircuitExplainer*>(explainer);
}

void flux_explainer_load(void* explainer, const char* circuitFile) {
    static_cast<CircuitExplainer*>(explainer)->loadCircuit(circuitFile ? circuitFile : "");
}

const char* flux_explainer_explain(void* explainer) {
    static std::string result;
    result = static_cast<CircuitExplainer*>(explainer)->explainFunction();
    return result.c_str();
}

const char* flux_explainer_answer(void* explainer, const char* question) {
    static std::string result;
    std::string q = question ? question : "";
    if (q.find("why") != std::string::npos) {
        result = static_cast<CircuitExplainer*>(explainer)->answerWhy(q);
    } else if (q.find("how") != std::string::npos) {
        result = static_cast<CircuitExplainer*>(explainer)->answerHow(q);
    } else {
        result = "Answer: " + q;
    }
    return result.c_str();
}

void* flux_voice_create() {
    return &VoiceInterface::instance();
}

void flux_voice_destroy(void* voice) {
    // Singleton - don't delete
}

void flux_voice_initialize(void* voice) {
    VoiceInterface::instance().initialize();
}

void* flux_assistant_create() {
    return new ConversationalAssistant();
}

void flux_assistant_destroy(void* assistant) {
    delete static_cast<ConversationalAssistant*>(assistant);
}

const char* flux_assistant_message(void* assistant, const char* message) {
    static std::string result;
    auto* asst = static_cast<ConversationalAssistant*>(assistant);
    asst->startSession();  // Always ensure session is active
    result = asst->processMessage(message ? message : "");
    return result.c_str();
}

}  // extern "C"

} // namespace NaturalLanguage
} // namespace Flux
