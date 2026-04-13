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

// Circuit Diff Tool Implementation
#include "flux/tooling/circuit_diff.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace Flux {
namespace CircuitDiff {

// ============================================================================
// CircuitDiffResult Report Generation
// ============================================================================

std::string CircuitDiffResult::toText() const {
    std::ostringstream oss;
    
    oss << "Circuit Diff Report\n";
    oss << "===================\n\n";
    oss << "Old: " << oldCircuit << "\n";
    oss << "New: " << newCircuit << "\n\n";
    
    oss << "Summary: " << summary << "\n\n";
    
    oss << "Changes:\n";
    oss << "  Components: +" << addedComponents << " -" << removedComponents 
        << " ~" << modifiedComponents << "\n";
    oss << "  Nets:       +" << addedNets << " -" << removedNets 
        << " ~" << modifiedNets << "\n";
    oss << "  Similarity: " << (similarityScore * 100) << "%\n\n";
    
    if (!componentDiffs.empty()) {
        oss << "Component Changes:\n";
        for (const auto& diff : componentDiffs) {
            switch (diff.change) {
                case ChangeType::Added:
                    oss << "  + Added " << diff.componentName << " (" << diff.componentType << ")\n";
                    break;
                case ChangeType::Removed:
                    oss << "  - Removed " << diff.componentName << " (" << diff.componentType << ")\n";
                    break;
                case ChangeType::Modified:
                    oss << "  ~ Modified " << diff.componentName << ":\n";
                    for (const auto& prop : diff.changedProperties) {
                        oss << "    " << prop << ": " << diff.oldValue.at(prop) 
                            << " -> " << diff.newValue.at(prop) << "\n";
                    }
                    break;
                default:
                    break;
            }
        }
        oss << "\n";
    }
    
    if (!performanceImpacts.empty()) {
        oss << "Performance Impact:\n";
        for (const auto& impact : performanceImpacts) {
            oss << "  " << impact.metric << ": " << impact.oldValue << " -> " 
                << impact.newValue << impact.unit << " (" 
                << (impact.changePercent > 0 ? "+" : "") << impact.changePercent << "%)\n";
        }
        oss << "\n";
    }
    
    if (!recommendations.empty()) {
        oss << "Recommendations:\n";
        for (const auto& rec : recommendations) {
            oss << "   " << rec << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string CircuitDiffResult::toMarkdown() const {
    std::ostringstream oss;
    
    oss << "# Circuit Diff Report\n\n";
    oss << "**Old:** `" << oldCircuit << "`  \n";
    oss << "**New:** `" << newCircuit << "`\n\n";
    
    oss << "## Summary\n\n";
    oss << summary << "\n\n";
    
    oss << "## Statistics\n\n";
    oss << "| Metric | Count |\n";
    oss << "|--------|-------|\n";
    oss << "| Added Components | " << addedComponents << " |\n";
    oss << "| Removed Components | " << removedComponents << " |\n";
    oss << "| Modified Components | " << modifiedComponents << " |\n";
    oss << "| Similarity | " << (similarityScore * 100) << "% |\n\n";
    
    if (!componentDiffs.empty()) {
        oss << "## Component Changes\n\n";
        for (const auto& diff : componentDiffs) {
            if (diff.change == ChangeType::Modified) {
                oss << "### " << diff.componentName << "\n\n";
                oss << "| Property | Old | New |\n";
                oss << "|----------|-----|-----|\n";
                for (const auto& prop : diff.changedProperties) {
                    oss << "| " << prop << " | " << diff.oldValue.at(prop) 
                        << " | " << diff.newValue.at(prop) << " |\n";
                }
                oss << "\n";
            }
        }
    }
    
    return oss.str();
}

std::string CircuitDiffResult::toJSON() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"oldCircuit\": \"" << oldCircuit << "\",\n";
    oss << "  \"newCircuit\": \"" << newCircuit << "\",\n";
    oss << "  \"similarity\": " << similarityScore << ",\n";
    oss << "  \"totalChanges\": " << totalChanges << ",\n";
    oss << "  \"addedComponents\": " << addedComponents << ",\n";
    oss << "  \"removedComponents\": " << removedComponents << ",\n";
    oss << "  \"modifiedComponents\": " << modifiedComponents << ",\n";
    oss << "  \"summary\": \"" << summary << "\"\n";
    oss << "}\n";
    
    return oss.str();
}

std::string CircuitDiffResult::toHTML() const {
    std::ostringstream oss;
    
    oss << "<!DOCTYPE html>\n<html>\n<head>\n";
    oss << "<title>Circuit Diff Report</title>\n";
    oss << "<style>\n";
    oss << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    oss << ".added { color: green; }\n";
    oss << ".removed { color: red; }\n";
    oss << ".modified { color: orange; }\n";
    oss << "table { border-collapse: collapse; width: 100%; }\n";
    oss << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    oss << "th { background-color: #4CAF50; color: white; }\n";
    oss << "</style>\n</head>\n<body>\n";
    
    oss << "<h1>Circuit Diff Report</h1>\n";
    oss << "<p><strong>Old:</strong> " << oldCircuit << "</p>\n";
    oss << "<p><strong>New:</strong> " << newCircuit << "</p>\n";
    oss << "<p><strong>Similarity:</strong> " << (similarityScore * 100) << "%</p>\n";
    
    oss << "<h2>Summary</h2>\n<p>" << summary << "</p>\n";
    
    oss << "<h2>Statistics</h2>\n";
    oss << "<table>\n";
    oss << "<tr><th>Metric</th><th>Count</th></tr>\n";
    oss << "<tr><td>Added Components</td><td class='added'>" << addedComponents << "</td></tr>\n";
    oss << "<tr><td>Removed Components</td><td class='removed'>" << removedComponents << "</td></tr>\n";
    oss << "<tr><td>Modified Components</td><td class='modified'>" << modifiedComponents << "</td></tr>\n";
    oss << "</table>\n";
    
    oss << "</body>\n</html>\n";
    
    return oss.str();
}

// ============================================================================
// CircuitDiffEngine Implementation
// ============================================================================

CircuitDiffEngine::CircuitDiffEngine() = default;
CircuitDiffEngine::~CircuitDiffEngine() = default;

void CircuitDiffEngine::setOptions(const DiffOptions& options) {
    m_options = options;
}

CircuitDiffResult CircuitDiffEngine::diff(const std::string& oldFile, const std::string& newFile) {
    // Read files
    std::ifstream oldStream(oldFile);
    std::ifstream newStream(newFile);
    
    CircuitDiffResult result;
    
    if (!oldStream.is_open()) {
        result.summary = "Error: Cannot open old file: " + oldFile;
        return result;
    }
    
    if (!newStream.is_open()) {
        result.summary = "Error: Cannot open new file: " + newFile;
        return result;
    }
    
    std::string oldCircuit((std::istreambuf_iterator<char>(oldStream)),
                            std::istreambuf_iterator<char>());
    std::string newCircuit((std::istreambuf_iterator<char>(newStream)),
                            std::istreambuf_iterator<char>());
    
    result.oldCircuit = oldFile;
    result.newCircuit = newFile;
    
    auto diffResult = diffStrings(oldCircuit, newCircuit);
    diffResult.oldCircuit = oldFile;
    diffResult.newCircuit = newFile;
    
    return diffResult;
}

CircuitDiffResult CircuitDiffEngine::diffStrings(const std::string& oldCircuit, 
                                                   const std::string& newCircuit) {
    CircuitDiffResult result;
    result.oldCircuit = "string";
    result.newCircuit = "string";
    
    // Parse components
    auto oldComponents = parseComponents(oldCircuit);
    auto newComponents = parseComponents(newCircuit);
    
    // Diff components
    result.componentDiffs = diffComponents(oldCircuit, newCircuit);
    
    // Count changes
    for (const auto& diff : result.componentDiffs) {
        switch (diff.change) {
            case ChangeType::Added: result.addedComponents++; break;
            case ChangeType::Removed: result.removedComponents++; break;
            case ChangeType::Modified: result.modifiedComponents++; break;
            default: break;
        }
    }
    
    // Parse and diff nets
    result.netDiffs = diffNets(oldCircuit, newCircuit);
    for (const auto& diff : result.netDiffs) {
        switch (diff.change) {
            case ChangeType::Added: result.addedNets++; break;
            case ChangeType::Removed: result.removedNets++; break;
            case ChangeType::Modified: result.modifiedNets++; break;
            default: break;
        }
    }
    
    // Calculate similarity
    result.similarityScore = calculateSimilarity(result);
    result.totalChanges = result.addedComponents + result.removedComponents + 
                          result.modifiedComponents + result.addedNets + 
                          result.removedNets + result.modifiedNets;
    
    // Generate summary
    result.summary = generateSummary(result);
    
    // Analyze performance impact
    if (m_options.showPerformanceImpact) {
        result.performanceImpacts = analyzePerformanceImpact(result);
    }
    
    // Generate recommendations
    if (m_options.showRecommendations) {
        result.recommendations = generateRecommendations(result);
    }
    
    return result;
}

CircuitDiffResult CircuitDiffEngine::diffAST(const std::string& oldAST, 
                                               const std::string& newAST) {
    // Would compare parsed ASTs directly
    // For now, treat as strings
    return diffStrings(oldAST, newAST);
}

double CircuitDiffEngine::calculateSimilarity(const CircuitDiffResult& result) {
    if (result.totalChanges == 0) return 1.0;
    
    // Simple similarity based on change count
    // More sophisticated version would use edit distance
    double maxChanges = 100;  // Normalize
    double similarity = 1.0 - (result.totalChanges / maxChanges);
    
    return std::max(0.0, std::min(1.0, similarity));
}

std::string CircuitDiffEngine::generateSummary(const CircuitDiffResult& result) {
    std::ostringstream oss;
    
    if (result.totalChanges == 0) {
        return "Circuits are identical";
    }
    
    oss << "Found " << result.totalChanges << " changes: ";
    
    std::vector<std::string> parts;
    if (result.addedComponents > 0) {
        parts.push_back(std::to_string(result.addedComponents) + " components added");
    }
    if (result.removedComponents > 0) {
        parts.push_back(std::to_string(result.removedComponents) + " components removed");
    }
    if (result.modifiedComponents > 0) {
        parts.push_back(std::to_string(result.modifiedComponents) + " components modified");
    }
    
    for (size_t i = 0; i < parts.size(); ++i) {
        oss << parts[i];
        if (i < parts.size() - 1) oss << ", ";
    }
    
    return oss.str();
}

std::vector<std::string> CircuitDiffEngine::generateRecommendations(const CircuitDiffResult& result) {
    std::vector<std::string> recommendations;
    
    // Check for removed decoupling capacitors
    for (const auto& diff : result.componentDiffs) {
        if (diff.change == ChangeType::Removed && diff.componentType == "capacitor") {
            recommendations.push_back("Verify that removed capacitor " + diff.componentName + 
                                     " is not needed for decoupling");
        }
    }
    
    // Check for significant value changes
    for (const auto& diff : result.componentDiffs) {
        if (diff.change == ChangeType::Modified) {
            for (const auto& prop : diff.changedProperties) {
                if (prop == "value") {
                    recommendations.push_back("Verify that " + diff.componentName + 
                                             " value change is intentional");
                }
            }
        }
    }
    
    // Check similarity score
    if (result.similarityScore < 0.5) {
        recommendations.push_back("Significant changes detected - consider thorough testing");
    }
    
    return recommendations;
}

std::vector<ComponentDiff> CircuitDiffEngine::diffComponents(const std::string& oldCircuit,
                                                              const std::string& newCircuit) {
    std::vector<ComponentDiff> diffs;
    
    auto oldComponents = parseComponents(oldCircuit);
    auto newComponents = parseComponents(newCircuit);
    
    // Find added and modified components
    for (const auto& newComp : newComponents) {
        const std::string& name = newComp.first;
        auto oldIt = oldComponents.find(name);
        
        if (oldIt == oldComponents.end()) {
            // Added
            ComponentDiff diff;
            diff.componentName = name;
            auto typeIt = newComp.second.find("type");
            diff.componentType = (typeIt != newComp.second.end()) ? typeIt->second : "unknown";
            diff.change = ChangeType::Added;
            diff.newValue = newComp.second;
            diffs.push_back(diff);
        } else {
            // Check for modifications
            ComponentDiff diff;
            diff.componentName = name;
            auto typeIt = newComp.second.find("type");
            diff.componentType = (typeIt != newComp.second.end()) ? typeIt->second : "unknown";
            diff.oldValue = oldIt->second;
            diff.newValue = newComp.second;
            
            for (const auto& prop : newComp.second) {
                auto oldPropIt = oldIt->second.find(prop.first);
                if (oldPropIt == oldIt->second.end() || oldPropIt->second != prop.second) {
                    diff.change = ChangeType::Modified;
                    diff.changedProperties.push_back(prop.first);
                }
            }
            
            if (diff.change == ChangeType::Modified) {
                diffs.push_back(diff);
            }
        }
    }
    
    // Find removed components
    for (const auto& oldComp : oldComponents) {
        if (newComponents.find(oldComp.first) == newComponents.end()) {
            ComponentDiff diff;
            diff.componentName = oldComp.first;
            auto typeIt = oldComp.second.find("type");
            diff.componentType = (typeIt != oldComp.second.end()) ? typeIt->second : "unknown";
            diff.change = ChangeType::Removed;
            diff.oldValue = oldComp.second;
            diffs.push_back(diff);
        }
    }
    
    return diffs;
}

std::vector<NetDiff> CircuitDiffEngine::diffNets(const std::string& oldCircuit,
                                                   const std::string& newCircuit) {
    std::vector<NetDiff> diffs;
    
    auto oldNets = parseNets(oldCircuit);
    auto newNets = parseNets(newCircuit);
    
    // Compare nets
    for (const auto& newNet : newNets) {
        auto oldIt = oldNets.find(newNet.first);
        if (oldIt == oldNets.end()) {
            NetDiff diff;
            diff.netName = newNet.first;
            diff.change = ChangeType::Added;
            diff.newConnections = newNet.second;
            diffs.push_back(diff);
        } else if (oldIt->second != newNet.second) {
            NetDiff diff;
            diff.netName = newNet.first;
            diff.change = ChangeType::Modified;
            diff.oldConnections = oldIt->second;
            diff.newConnections = newNet.second;
            
            // Find added/removed connections
            for (const auto& conn : newNet.second) {
                if (std::find(oldIt->second.begin(), oldIt->second.end(), conn) == oldIt->second.end()) {
                    diff.addedConnections.push_back(conn);
                }
            }
            for (const auto& conn : oldIt->second) {
                if (std::find(newNet.second.begin(), newNet.second.end(), conn) == newNet.second.end()) {
                    diff.removedConnections.push_back(conn);
                }
            }
            
            diffs.push_back(diff);
        }
    }
    
    // Find removed nets
    for (const auto& oldNet : oldNets) {
        if (newNets.find(oldNet.first) == newNets.end()) {
            NetDiff diff;
            diff.netName = oldNet.first;
            diff.change = ChangeType::Removed;
            diff.oldConnections = oldNet.second;
            diffs.push_back(diff);
        }
    }
    
    return diffs;
}

std::vector<PerformanceImpact> CircuitDiffEngine::analyzePerformanceImpact(const CircuitDiffResult& result) {
    std::vector<PerformanceImpact> impacts;
    
    // Stub - would analyze actual circuit changes and predict performance impact
    // For now, return empty
    
    return impacts;
}

std::map<std::string, std::map<std::string, std::string>> CircuitDiffEngine::parseComponents(const std::string& circuit) {
    std::map<std::string, std::map<std::string, std::string>> components;
    
    // Simple parser - looks for component declarations
    // Format: R1 node1 node2 10k
    // Format: C1 node1 node2 1uF
    
    std::istringstream stream(circuit);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '/') continue;
        
        std::istringstream lineStream(line);
        std::string name, type, node1, node2, value;
        
        if (lineStream >> name >> node1 >> node2 >> value) {
            // Determine component type from first letter
            char typeChar = toupper(name[0]);
            switch (typeChar) {
                case 'R': type = "resistor"; break;
                case 'C': type = "capacitor"; break;
                case 'L': type = "inductor"; break;
                case 'Q': type = "transistor"; break;
                case 'U': type = "ic"; break;
                case 'D': type = "diode"; break;
                case 'V': type = "voltage_source"; break;
                case 'I': type = "current_source"; break;
                default: type = "unknown"; break;
            }
            
            components[name] = {
                {"type", type},
                {"node1", node1},
                {"node2", node2},
                {"value", value}
            };
        }
    }
    
    return components;
}

std::map<std::string, std::vector<std::string>> CircuitDiffEngine::parseNets(const std::string& circuit) {
    std::map<std::string, std::vector<std::string>> nets;
    
    auto components = parseComponents(circuit);
    
    for (const auto& comp : components) {
        const auto& props = comp.second;
        auto it1 = props.find("node1");
        auto it2 = props.find("node2");
        
        if (it1 != props.end()) {
            nets[it1->second].push_back(comp.first);
        }
        if (it2 != props.end()) {
            nets[it2->second].push_back(comp.first);
        }
    }
    
    return nets;
}

std::string CircuitDiffEngine::formatValue(const std::string& value) {
    return value;
}

bool CircuitDiffEngine::valuesEqual(const std::string& oldVal, const std::string& newVal) {
    if (m_options.ignoreWhitespace) {
        std::string oldTrimmed = oldVal;
        std::string newTrimmed = newVal;
        oldTrimmed.erase(0, oldTrimmed.find_first_not_of(" \t"));
        oldTrimmed.erase(oldTrimmed.find_last_not_of(" \t") + 1);
        newTrimmed.erase(0, newTrimmed.find_first_not_of(" \t"));
        newTrimmed.erase(newTrimmed.find_last_not_of(" \t") + 1);
        return oldTrimmed == newTrimmed;
    }
    return oldVal == newVal;
}

// ============================================================================
// Convenience Functions
// ============================================================================

CircuitDiffResult circuit_diff(const std::string& oldFile, const std::string& newFile,
                                const DiffOptions& options) {
    CircuitDiffEngine engine;
    engine.setOptions(options);
    return engine.diff(oldFile, newFile);
}

std::string show_diff(const CircuitDiffResult& result, const std::string& format) {
    if (format == "markdown") return result.toMarkdown();
    if (format == "json") return result.toJSON();
    if (format == "html") return result.toHTML();
    return result.toText();
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" {

void* flux_diff_create() {
    return new CircuitDiffEngine();
}

void flux_diff_destroy(void* diff) {
    delete static_cast<CircuitDiffEngine*>(diff);
}

void flux_diff_set_options(void* diff, int ignoreWhitespace, int ignoreComments) {
    auto* engine = static_cast<CircuitDiffEngine*>(diff);
    DiffOptions options;
    options.ignoreWhitespace = (ignoreWhitespace != 0);
    options.ignoreComments = (ignoreComments != 0);
    engine->setOptions(options);
}

const char* flux_diff_files(void* diff, const char* oldFile, const char* newFile) {
    static std::string result;
    auto* engine = static_cast<CircuitDiffEngine*>(diff);
    auto diffResult = engine->diff(oldFile ? oldFile : "", newFile ? newFile : "");
    result = diffResult.toText();
    return result.c_str();
}

const char* flux_diff_strings(void* diff, const char* oldCircuit, const char* newCircuit) {
    static std::string result;
    auto* engine = static_cast<CircuitDiffEngine*>(diff);
    auto diffResult = engine->diffStrings(oldCircuit ? oldCircuit : "", 
                                           newCircuit ? newCircuit : "");
    result = diffResult.toText();
    return result.c_str();
}

const char* flux_diff_summary(void* diff) {
    static std::string result;
    // Would need to store last result - simplified for now
    result = "Circuit diff summary";
    return result.c_str();
}

double flux_diff_similarity(void* diff) {
    // Would need to store last result - simplified for now
    return 1.0;
}

}

} // namespace CircuitDiff
} // namespace Flux
