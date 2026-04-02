#ifndef FLUX_DEBUGGER_H
#define FLUX_DEBUGGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <complex>

namespace Flux {
namespace Debugger {

// ============================================================================
// Debug Data Structures
// ============================================================================

enum class IssueSeverity {
    Info,
    Warning,
    Error,
    Critical
};

enum class IssueCategory {
    Design,
    Simulation,
    Performance,
    Safety,
    Manufacturing,
    Compliance
};

struct DebugIssue {
    std::string id;
    std::string message;
    std::string suggestion;
    IssueSeverity severity;
    IssueCategory category;
    std::string location;  // Component or net name
    std::vector<std::string> relatedComponents;
    double confidence;  // 0-1 confidence score
    
    DebugIssue() : severity(IssueSeverity::Info), confidence(0) {}
};

struct CircuitSymptom {
    std::string type;  // "clipping", "oscillation", "low_gain", etc.
    std::string description;
    std::map<std::string, double> measurements;
    std::vector<std::string> affectedNets;
};

struct DebugReport {
    std::string circuitName;
    std::vector<DebugIssue> issues;
    std::vector<DebugIssue> criticalIssues;
    std::vector<DebugIssue> warnings;
    std::vector<std::string> recommendations;
    double healthScore;  // 0-100
    
    std::string toString() const;
    std::string toMarkdown() const;
    std::string toJSON() const;
};

// ============================================================================
// Automated Debugger
// ============================================================================

class CircuitDebugger {
public:
    CircuitDebugger();
    ~CircuitDebugger();
    
    // Load circuit
    void loadCircuit(const std::string& circuitFile);
    void loadCircuitFromNetlist(const std::string& netlist);
    
    // Symptom-based debugging
    void addSymptom(const CircuitSymptom& symptom);
    DebugReport diagnose();
    
    // Automatic analysis
    DebugReport analyze();
    DebugReport analyzeDC();
    DebugReport analyzeAC();
    DebugReport analyzeTransient();
    DebugReport analyzeStability();
    
    // Specific checks
    std::vector<DebugIssue> checkBiasPoints();
    std::vector<DebugIssue> checkSignalSwing();
    std::vector<DebugIssue> checkStability();
    std::vector<DebugIssue> checkPowerDissipation();
    std::vector<DebugIssue> checkComponentStress();
    std::vector<DebugIssue> checkDesignRules();
    
    // Get suggestions
    std::vector<std::string> getRecommendations() const;
    DebugIssue getMostLikelyCause() const;
    
    // Interactive debugging
    void askQuestion(const std::string& question);
    std::string getAnswer() const;
    
private:
    std::string m_circuitFile;
    std::string m_netlist;
    std::vector<CircuitSymptom> m_symptoms;
    std::vector<DebugIssue> m_issues;
    
    // Analysis results
    std::map<std::string, double> m_dcOperatingPoint;
    std::map<std::string, std::complex<double>> m_acResponse;
    std::vector<double> m_transientResponse;
    
    // Internal methods
    void analyzeSymptoms();
    void correlateIssues();
    void calculateHealthScore();
    void generateRecommendations();
    
    // Pattern matching
    bool matchPattern(const std::string& pattern, const CircuitSymptom& symptom);
    DebugIssue createIssue(const std::string& pattern);
};

// ============================================================================
// Design Rule Checker
// ============================================================================

class DesignRuleChecker {
public:
    DesignRuleChecker();
    ~DesignRuleChecker();
    
    // Load rules
    void loadRules(const std::string& rulesFile);
    void loadStandardRules(const std::string& standard);  // "IPC", "JEDEC", etc.
    
    // Add custom rules
    void addRule(const std::string& name, const std::string& expression,
                 IssueSeverity severity);
    
    // Run checks
    std::vector<DebugIssue> runChecks();
    std::vector<DebugIssue> runChecks(const std::vector<std::string>& ruleNames);
    
    // Pre-defined rule sets
    void enableVoltageStressRules();
    void enablePowerDissipationRules();
    void enableThermalRules();
    void enableSignalIntegrityRules();
    void enableEMIRules();
    
    // Get results
    std::vector<DebugIssue> getViolations() const;
    bool hasViolations() const;
    bool hasCriticalViolations() const;
    
private:
    std::vector<std::string> m_rules;
    std::vector<DebugIssue> m_violations;
};

// ============================================================================
// Root Cause Analyzer
// ============================================================================

class RootCauseAnalyzer {
public:
    RootCauseAnalyzer();
    ~RootCauseAnalyzer();
    
    // Add evidence
    void addEvidence(const std::string& evidence, double weight);
    void addMeasurement(const std::string& net, double value);
    void addObservation(const std::string& observation);
    
    // Analyze
    std::vector<DebugIssue> findRootCauses();
    DebugIssue getMostProbableCause();
    
    // Bayesian analysis
    void setPriorProbability(const std::string& cause, double probability);
    double getPosteriorProbability(const std::string& cause);
    
    // Fault tree analysis
    void buildFaultTree(const std::string& topEvent);
    std::vector<std::string> getMinimalCutSets();
    
private:
    std::map<std::string, double> m_evidence;
    std::map<std::string, double> m_priors;
    std::map<std::string, double> m_posteriors;
};

// ============================================================================
// Fix Suggester
// ============================================================================

class FixSuggester {
public:
    FixSuggester();
    ~FixSuggester();
    
    // Generate fix for issue
    struct FixSuggestion {
        std::string description;
        std::string beforeValue;
        std::string afterValue;
        std::string component;
        double expectedImprovement;
        std::vector<std::string> sideEffects;
    };
    
    std::vector<FixSuggestion> suggestFixes(const DebugIssue& issue);
    std::vector<FixSuggestion> suggestFixes(const std::string& symptom);
    
    // Apply fix
    bool applyFix(const FixSuggestion& fix);
    bool applyFix(const std::string& component, const std::string& newValue);
    
    // Optimization-based suggestions
    std::vector<FixSuggestion> optimizeFor(const std::string& objective,
                                            const std::vector<std::string>& constraints);
    
private:
    std::vector<FixSuggestion> m_suggestions;
};

// ============================================================================
// Interactive Debug Session
// ============================================================================

class DebugSession {
public:
    DebugSession();
    ~DebugSession();
    
    // Session control
    void start(const std::string& circuitFile);
    void end();
    bool isActive() const;
    
    // Commands
    std::string executeCommand(const std::string& command);
    std::vector<std::string> getAvailableCommands() const;
    
    // Natural language queries
    std::string ask(const std::string& question);
    
    // State
    void printState();
    void printOperatingPoint();
    void printNodeVoltage(const std::string& node);
    void printComponentCurrent(const std::string& component);
    
    // Navigation
    void goToComponent(const std::string& component);
    void goToNet(const std::string& net);
    void showConnected(const std::string& componentOrNet);
    
private:
    bool m_active;
    std::string m_circuitFile;
    CircuitDebugger m_debugger;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Debugger creation
    void* flux_debug_create();
    void flux_debug_destroy(void* debug);
    
    // Load circuit
    void flux_debug_load(void* debug, const char* circuitFile);
    
    // Add symptom
    void flux_debug_add_symptom(void* debug, const char* type, const char* description);
    
    // Diagnose
    const char* flux_debug_diagnose(void* debug);
    
    // Get recommendations
    const char* flux_debug_get_recommendations(void* debug);
    
    // Ask question
    const char* flux_debug_ask(void* debug, const char* question);
    
    // Design rule check
    void* flux_drc_create();
    void flux_drc_destroy(void* drc);
    void flux_drc_enable_rules(void* drc, const char* rules);
    const char* flux_drc_run(void* drc);
}

} // namespace Debugger
} // namespace Flux

#endif // FLUX_DEBUGGER_H
