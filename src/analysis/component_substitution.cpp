// Component Substitution Engine Implementation
#include "flux/analysis/component_substitution.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Flux {
namespace Substitution {

// ============================================================================
// ComponentDatabase Implementation
// ============================================================================

ComponentDatabase& ComponentDatabase::instance() {
    static ComponentDatabase db;
    return db;
}

ComponentDatabase::ComponentDatabase() {
    // Initialize with some common components
    ComponentSpec resistor;
    resistor.type = "resistor";
    resistor.value = "10k";
    resistor.tolerance = "1%";
    resistor.power = "0.25W";
    resistor.package = "0603";
    resistor.partNumber = "RC0603FR-0710KL";
    resistor.manufacturer = "Yageo";
    resistor.price = 0.005;
    resistor.availability = 100000;
    addComponent(resistor);
    
    ComponentSpec capacitor;
    capacitor.type = "capacitor";
    capacitor.value = "1uF";
    capacitor.tolerance = "10%";
    capacitor.voltage = "50V";
    capacitor.package = "0603";
    capacitor.partNumber = "GRM188R71H105KA12D";
    capacitor.manufacturer = "Murata";
    capacitor.price = 0.02;
    capacitor.availability = 50000;
    addComponent(capacitor);
}

void ComponentDatabase::addComponent(const ComponentSpec& spec) {
    m_components[spec.partNumber] = spec;
    m_byType[spec.type].push_back(spec.partNumber);
}

std::vector<ComponentSpec> ComponentDatabase::search(const std::string& query) {
    std::vector<ComponentSpec> results;
    
    for (const auto& pair : m_components) {
        const auto& spec = pair.second;
        
        // Search in part number, manufacturer, type
        if (spec.partNumber.find(query) != std::string::npos ||
            spec.manufacturer.find(query) != std::string::npos ||
            spec.type.find(query) != std::string::npos) {
            results.push_back(spec);
        }
    }
    
    return results;
}

std::vector<ComponentSpec> ComponentDatabase::getByPartNumber(const std::string& partNum) {
    std::vector<ComponentSpec> results;
    
    auto it = m_components.find(partNum);
    if (it != m_components.end()) {
        results.push_back(it->second);
    }
    
    // Also check equivalents
    if (it != m_components.end()) {
        for (const auto& equiv : it->second.equivalents) {
            auto equivIt = m_components.find(equiv);
            if (equivIt != m_components.end()) {
                results.push_back(equivIt->second);
            }
        }
    }
    
    return results;
}

std::vector<ComponentSpec> ComponentDatabase::getByType(const std::string& type) {
    std::vector<ComponentSpec> results;
    
    auto it = m_byType.find(type);
    if (it != m_byType.end()) {
        for (const auto& partNum : it->second) {
            auto compIt = m_components.find(partNum);
            if (compIt != m_components.end()) {
                results.push_back(compIt->second);
            }
        }
    }
    
    return results;
}

void ComponentDatabase::updateAvailability(const std::string& partNum, int quantity) {
    auto it = m_components.find(partNum);
    if (it != m_components.end()) {
        it->second.availability = quantity;
    }
}

void ComponentDatabase::updatePrice(const std::string& partNum, double price) {
    auto it = m_components.find(partNum);
    if (it != m_components.end()) {
        it->second.price = price;
    }
}

bool ComponentDatabase::importFromDigiKey(const std::string& apiKey) {
    // Stub - would integrate with DigiKey API
    return true;
}

bool ComponentDatabase::importFromMouser(const std::string& apiKey) {
    // Stub - would integrate with Mouser API
    return true;
}

bool ComponentDatabase::importFromLCSC(const std::string& apiKey) {
    // Stub - would integrate with LCSC API
    return true;
}

void ComponentDatabase::exportToCSV(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "PartNumber,Type,Value,Tolerance,Package,Manufacturer,Price,Availability\n";
    for (const auto& pair : m_components) {
        const auto& spec = pair.second;
        file << spec.partNumber << ","
             << spec.type << ","
             << spec.value << ","
             << spec.tolerance << ","
             << spec.package << ","
             << spec.manufacturer << ","
             << spec.price << ","
             << spec.availability << "\n";
    }
    
    file.close();
}

void ComponentDatabase::exportToJSON(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "{\n  \"components\": [\n";
    bool first = true;
    for (const auto& pair : m_components) {
        if (!first) file << ",\n";
        first = false;
        
        const auto& spec = pair.second;
        file << "    {\n";
        file << "      \"partNumber\": \"" << spec.partNumber << "\",\n";
        file << "      \"type\": \"" << spec.type << "\",\n";
        file << "      \"value\": \"" << spec.value << "\",\n";
        file << "      \"manufacturer\": \"" << spec.manufacturer << "\",\n";
        file << "      \"price\": " << spec.price << ",\n";
        file << "      \"availability\": " << spec.availability << "\n";
        file << "    }";
    }
    file << "\n  ]\n}\n";
    
    file.close();
}

// ============================================================================
// SubstitutionEngine Implementation
// ============================================================================

SubstitutionEngine::SubstitutionEngine() 
    : m_minAvailability(100), m_maxPriceIncrease(0.5) {}

SubstitutionEngine::~SubstitutionEngine() = default;

std::vector<SubstitutionSuggestion> SubstitutionEngine::findSubstitutes(
    const ComponentSpec& original,
    const std::vector<std::string>& preferredManufacturers)
{
    std::vector<SubstitutionSuggestion> suggestions;
    
    auto& db = ComponentDatabase::instance();
    auto candidates = db.getByType(original.type);
    
    for (const auto& candidate : candidates) {
        if (candidate.partNumber == original.partNumber) continue;
        
        // Check availability
        if (candidate.availability < m_minAvailability) continue;
        
        // Calculate compatibility
        double score = calculateCompatibilityScore(original, candidate);
        
        if (score < 0.7) continue;  // Too different
        
        SubstitutionSuggestion suggestion;
        suggestion.originalPart = original.partNumber;
        suggestion.suggestedPart = candidate.partNumber;
        suggestion.compatibilityScore = score;
        suggestion.priceDiff = candidate.price - original.price;
        suggestion.availabilityScore = candidate.availability / 1000;
        
        // Check for preferred manufacturer
        if (!preferredManufacturers.empty()) {
            auto it = std::find(preferredManufacturers.begin(), 
                               preferredManufacturers.end(),
                               candidate.manufacturer);
            if (it != preferredManufacturers.end()) {
                suggestion.compatibilityScore += 0.1;  // Bonus
            }
        }
        
        // Identify differences
        if (candidate.tolerance != original.tolerance) {
            suggestion.differences.push_back("Tolerance: " + candidate.tolerance + 
                                            " vs " + original.tolerance);
        }
        if (candidate.package != original.package) {
            suggestion.differences.push_back("Package: " + candidate.package + 
                                            " vs " + original.package);
            suggestion.warnings.push_back("Footprint may differ");
        }
        
        suggestions.push_back(suggestion);
    }
    
    return rankSuggestions(suggestions);
}

std::vector<SubstitutionSuggestion> SubstitutionEngine::findSubstitutesByPartNumber(
    const std::string& partNumber)
{
    auto& db = ComponentDatabase::instance();
    auto originals = db.getByPartNumber(partNumber);
    
    if (originals.empty()) {
        return {};
    }
    
    return findSubstitutes(originals[0], m_preferredManufacturers);
}

SubstitutionEngine::BOMSubstitutionResult SubstitutionEngine::substituteBOM(
    const std::vector<ComponentSpec>& bom)
{
    BOMSubstitutionResult result;
    result.totalComponents = bom.size();
    result.substitutedComponents = 0;
    result.originalCost = 0;
    result.newCost = 0;
    
    for (const auto& component : bom) {
        result.originalCost += component.price;
        
        auto substitutes = findSubstitutes(component);
        
        if (!substitutes.empty() && substitutes[0].compatibilityScore > 0.9) {
            const auto& best = substitutes[0];
            result.suggestions.push_back(best);
            result.substitutedComponents++;
            result.newCost += component.price + best.priceDiff;
        } else {
            result.newCost += component.price;
        }
    }
    
    result.savings = result.originalCost - result.newCost;
    
    return result;
}

bool SubstitutionEngine::isCompatible(const ComponentSpec& original, 
                                       const ComponentSpec& substitute)
{
    return calculateCompatibilityScore(original, substitute) >= 0.7;
}

double SubstitutionEngine::calculateCompatibilityScore(const ComponentSpec& original, 
                                                        const ComponentSpec& substitute)
{
    double score = 1.0;
    
    // Type must match
    if (original.type != substitute.type) {
        return 0.0;
    }
    
    // Value should match (simplified comparison)
    if (original.value != substitute.value) {
        score -= 0.3;
    }
    
    // Tolerance: substitute should be equal or better
    int origTol = std::stoi(original.tolerance.substr(0, original.tolerance.find('%')));
    int subTol = std::stoi(substitute.tolerance.substr(0, substitute.tolerance.find('%')));
    if (subTol > origTol) {
        score -= 0.2;
    }
    
    // Voltage: substitute should be equal or higher
    if (!original.voltage.empty() && !substitute.voltage.empty()) {
        int origV = std::stoi(original.voltage.substr(0, original.voltage.find('V')));
        int subV = std::stoi(substitute.voltage.substr(0, substitute.voltage.find('V')));
        if (subV < origV) {
            score -= 0.3;
        }
    }
    
    // Package: should match for drop-in replacement
    if (original.package != substitute.package) {
        score -= 0.2;
    }
    
    // Power: substitute should be equal or higher
    if (!original.power.empty() && !substitute.power.empty()) {
        // Simplified power comparison
    }
    
    // Availability bonus
    if (substitute.availability > original.availability) {
        score += 0.1;
    }
    
    // Price penalty if much more expensive
    if (substitute.price > original.price * (1 + m_maxPriceIncrease)) {
        score -= 0.2;
    }
    
    return std::max(0.0, std::min(1.0, score));
}

std::vector<SubstitutionSuggestion> SubstitutionEngine::rankSuggestions(
    std::vector<SubstitutionSuggestion>& suggestions)
{
    std::sort(suggestions.begin(), suggestions.end(),
        [](const SubstitutionSuggestion& a, const SubstitutionSuggestion& b) {
            // Rank by: compatibility > availability > price
            return a.compatibilityScore > b.compatibilityScore;
        });
    
    return suggestions;
}

void SubstitutionEngine::checkParametricCompatibility(const ComponentSpec& original,
                                                       const ComponentSpec& substitute,
                                                       SubstitutionSuggestion& suggestion)
{
    // Detailed parametric checking
    // Stub for future implementation
}

// ============================================================================
// BOMAnalyzer Implementation
// ============================================================================

BOMAnalyzer::BOMAnalyzer() = default;
BOMAnalyzer::~BOMAnalyzer() = default;

void BOMAnalyzer::loadFromCircuit(const std::string& circuitFile) {
    m_circuitFile = circuitFile;
    // Parse circuit file and extract components
    // Stub for future implementation
}

void BOMAnalyzer::loadFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    std::getline(file, line);  // Skip header
    
    while (std::getline(file, line)) {
        ComponentSpec comp;
        std::istringstream ss(line);
        std::string token;
        
        std::getline(ss, token, ',');
        comp.partNumber = token;
        
        std::getline(ss, token, ',');
        comp.type = token;
        
        std::getline(ss, token, ',');
        comp.value = token;
        
        m_bom.push_back(comp);
    }
    
    file.close();
}

void BOMAnalyzer::loadFromJSON(const std::string& filename) {
    // Parse JSON BOM file
    // Stub for future implementation
}

BOMAnalyzer::BOMAnalysis BOMAnalyzer::analyze() {
    BOMAnalysis analysis;
    analysis.totalComponents = m_bom.size();
    analysis.totalCost = 0;
    
    auto& db = ComponentDatabase::instance();
    
    for (const auto& comp : m_bom) {
        // Count by type
        analysis.byType[comp.type]++;
        
        // Count by package
        if (!comp.package.empty()) {
            analysis.byPackage[comp.package]++;
        }
        
        // Count by manufacturer
        if (!comp.manufacturer.empty()) {
            analysis.byManufacturer[comp.manufacturer]++;
        }
        
        // Get cost and availability from database
        auto specs = db.getByPartNumber(comp.partNumber);
        if (!specs.empty()) {
            analysis.totalCost += specs[0].price;
            
            if (specs[0].availability < 1000) {
                analysis.hardToFind.push_back(comp.partNumber);
            }
            
            if (specs[0].price > 1.0) {
                analysis.expensive.push_back(comp.partNumber);
            }
        }
    }
    
    analysis.uniqueComponents = m_bom.size();  // Simplified
    
    return analysis;
}

std::vector<std::string> BOMAnalyzer::suggestOptimizations() {
    std::vector<std::string> suggestions;
    
    auto analysis = analyze();
    
    // Suggest consolidating packages
    if (analysis.byPackage.size() > 5) {
        suggestions.push_back("Consider consolidating package types to reduce assembly cost");
    }
    
    // Suggest alternative manufacturers
    if (analysis.byManufacturer.size() < 2) {
        suggestions.push_back("Consider second-source components for supply chain resilience");
    }
    
    // Flag hard-to-find components
    if (!analysis.hardToFind.empty()) {
        suggestions.push_back(std::to_string(analysis.hardToFind.size()) + 
                             " components have low availability - consider substitutes");
    }
    
    // Flag expensive components
    if (!analysis.expensive.empty()) {
        suggestions.push_back(std::to_string(analysis.expensive.size()) + 
                             " components are high-cost - review for cost optimization");
    }
    
    return suggestions;
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" {

void* flux_comp_db_create() {
    return &ComponentDatabase::instance();
}

void flux_comp_db_destroy(void* db) {
    // Singleton - don't delete
}

const char* flux_comp_db_search(void* db, const char* query) {
    static std::string result;
    auto results = static_cast<ComponentDatabase*>(db)->search(query ? query : "");
    result = "Found " + std::to_string(results.size()) + " components";
    return result.c_str();
}

void* flux_sub_engine_create() {
    return new SubstitutionEngine();
}

void flux_sub_engine_destroy(void* engine) {
    delete static_cast<SubstitutionEngine*>(engine);
}

const char* flux_sub_find(void* engine, const char* partNumber) {
    static std::string result;
    auto suggestions = static_cast<SubstitutionEngine*>(engine)->findSubstitutesByPartNumber(
        partNumber ? partNumber : "");
    result = "Found " + std::to_string(suggestions.size()) + " substitutes";
    return result.c_str();
}

const char* flux_sub_bom_substitute(void* engine, const char* bomFile) {
    static std::string result;
    // Would load BOM and find substitutes
    result = "BOM substitution complete";
    return result.c_str();
}

void* flux_bom_analyzer_create() {
    return new BOMAnalyzer();
}

void flux_bom_analyzer_destroy(void* analyzer) {
    delete static_cast<BOMAnalyzer*>(analyzer);
}

void flux_bom_load(void* analyzer, const char* filename) {
    std::string file = filename ? filename : "";
    if (file.find(".csv") != std::string::npos) {
        static_cast<BOMAnalyzer*>(analyzer)->loadFromCSV(file);
    } else if (file.find(".json") != std::string::npos) {
        static_cast<BOMAnalyzer*>(analyzer)->loadFromJSON(file);
    }
}

const char* flux_bom_analyze(void* analyzer) {
    static std::string result;
    auto analysis = static_cast<BOMAnalyzer*>(analyzer)->analyze();
    result = "BOM Analysis: " + std::to_string(analysis.totalComponents) + 
             " components, $" + std::to_string(analysis.totalCost) + " total";
    return result.c_str();
}

}

} // namespace Substitution
} // namespace Flux
