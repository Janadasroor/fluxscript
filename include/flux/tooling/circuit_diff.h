#ifndef FLUX_CIRCUIT_DIFF_H
#define FLUX_CIRCUIT_DIFF_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace Flux {
namespace CircuitDiff {

// Component difference
enum class ChangeType {
    Added,
    Removed,
    Modified,
    Unchanged
};

struct ComponentDiff {
    std::string componentName;
    std::string componentType;
    ChangeType change;
    std::map<std::string, std::string> oldValue;
    std::map<std::string, std::string> newValue;
    std::vector<std::string> changedProperties;
    
    ComponentDiff() : change(ChangeType::Unchanged) {}
};

// Net difference
struct NetDiff {
    std::string netName;
    ChangeType change;
    std::vector<std::string> oldConnections;
    std::vector<std::string> newConnections;
    std::vector<std::string> addedConnections;
    std::vector<std::string> removedConnections;
    
    NetDiff() : change(ChangeType::Unchanged) {}
};

// Performance impact
struct PerformanceImpact {
    std::string metric;
    double oldValue;
    double newValue;
    double changePercent;
    std::string unit;
    std::string severity;  // "low", "medium", "high"
    
    PerformanceImpact() : oldValue(0), newValue(0), changePercent(0) {}
};

// Circuit diff result
struct CircuitDiffResult {
    std::string oldCircuit;
    std::string newCircuit;
    
    // Component changes
    std::vector<ComponentDiff> componentDiffs;
    int addedComponents;
    int removedComponents;
    int modifiedComponents;
    
    // Net changes
    std::vector<NetDiff> netDiffs;
    int addedNets;
    int removedNets;
    int modifiedNets;
    
    // Performance impact
    std::vector<PerformanceImpact> performanceImpacts;
    
    // Summary
    std::string summary;
    std::vector<std::string> keyChanges;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    
    // Statistics
    double similarityScore;  // 0-1 (1 = identical)
    int totalChanges;
    
    CircuitDiffResult() : addedComponents(0), removedComponents(0), 
                          modifiedComponents(0), addedNets(0), removedNets(0),
                          modifiedNets(0), similarityScore(1.0), totalChanges(0) {}
    
    // Generate reports
    std::string toText() const;
    std::string toMarkdown() const;
    std::string toJSON() const;
    std::string toHTML() const;
};

// Diff options
struct DiffOptions {
    bool ignoreWhitespace;
    bool ignoreComments;
    bool ignoreOrder;
    bool showPerformanceImpact;
    bool showRecommendations;
    std::string outputFormat;  // "text", "markdown", "json", "html"
    int contextLines;
    
    DiffOptions() : ignoreWhitespace(false), ignoreComments(false),
                    ignoreOrder(true), showPerformanceImpact(true),
                    showRecommendations(true), outputFormat("text"),
                    contextLines(3) {}
};

// Circuit Diff Engine
class CircuitDiffEngine {
public:
    CircuitDiffEngine();
    ~CircuitDiffEngine();
    
    // Set options
    void setOptions(const DiffOptions& options);
    
    // Compare two circuit files
    CircuitDiffResult diff(const std::string& oldFile, const std::string& newFile);
    
    // Compare two circuit strings
    CircuitDiffResult diffStrings(const std::string& oldCircuit, const std::string& newCircuit);
    
    // Compare parsed circuits
    CircuitDiffResult diffAST(const std::string& oldAST, const std::string& newAST);
    
    // Get similarity score
    double calculateSimilarity(const CircuitDiffResult& result);
    
    // Generate summary
    std::string generateSummary(const CircuitDiffResult& result);
    
    // Generate recommendations
    std::vector<std::string> generateRecommendations(const CircuitDiffResult& result);
    
    // Visual diff (for GUI)
    struct VisualDiff {
        std::string oldSVG;
        std::string newSVG;
        std::string diffSVG;  // Highlighted differences
    };
    
    VisualDiff generateVisualDiff(const CircuitDiffResult& result);
    
private:
    DiffOptions m_options;
    
    // Internal diff methods
    std::vector<ComponentDiff> diffComponents(const std::string& oldCircuit,
                                               const std::string& newCircuit);
    std::vector<NetDiff> diffNets(const std::string& oldCircuit,
                                   const std::string& newCircuit);
    std::vector<PerformanceImpact> analyzePerformanceImpact(const CircuitDiffResult& result);
    
    // Helper methods
    std::map<std::string, std::map<std::string, std::string>> parseComponents(const std::string& circuit);
    std::map<std::string, std::vector<std::string>> parseNets(const std::string& circuit);
    std::string formatValue(const std::string& value);
    bool valuesEqual(const std::string& oldVal, const std::string& newVal);
};

// Convenience functions
CircuitDiffResult circuit_diff(const std::string& oldFile, const std::string& newFile,
                                const DiffOptions& options = DiffOptions());

std::string show_diff(const CircuitDiffResult& result, const std::string& format = "text");

// C Interface for FluxScript JIT
extern "C" {
    void* flux_diff_create();
    void flux_diff_destroy(void* diff);
    void flux_diff_set_options(void* diff, int ignoreWhitespace, int ignoreComments);
    const char* flux_diff_files(void* diff, const char* oldFile, const char* newFile);
    const char* flux_diff_strings(void* diff, const char* oldCircuit, const char* newCircuit);
    const char* flux_diff_summary(void* diff);
    double flux_diff_similarity(void* diff);
}

} // namespace CircuitDiff
} // namespace Flux

#endif // FLUX_CIRCUIT_DIFF_H
