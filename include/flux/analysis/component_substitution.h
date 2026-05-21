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

#ifndef FLUX_COMPONENT_SUBSTITUTION_H
#define FLUX_COMPONENT_SUBSTITUTION_H

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Flux {
namespace Substitution {

// Component parameters
struct ComponentSpec
{
    std::string type;      // "resistor", "capacitor", etc.
    std::string value;     // "10k", "1uF", etc.
    std::string tolerance; // "1%", "5%", etc.
    std::string voltage;   // "50V", "100V", etc.
    std::string power;     // "0.25W", "1W", etc.
    std::string package;   // "0603", "SOT-23", etc.
    std::string manufacturer;
    std::string partNumber;
    double price;
    int availability;                  // Stock quantity
    std::set<std::string> equivalents; // Equivalent part numbers

    ComponentSpec() : price(0), availability(0) {}
};

// Substitution suggestion
struct SubstitutionSuggestion
{
    std::string originalPart;
    std::string suggestedPart;
    double compatibilityScore;            // 0-1 (1 = perfect match)
    std::vector<std::string> differences; // What's different
    std::vector<std::string> warnings;    // Compatibility warnings
    double priceDiff;                     // Price difference
    int availabilityScore;                // Higher = better availability

    SubstitutionSuggestion() : compatibilityScore(0), priceDiff(0), availabilityScore(0) {}
};

// Component Database
class ComponentDatabase
{
public:
    static ComponentDatabase& instance();

    // Search components
    std::vector<ComponentSpec> search(const std::string& query);
    std::vector<ComponentSpec> getByPartNumber(const std::string& partNum);
    std::vector<ComponentSpec> getByType(const std::string& type);

    // Add/update components
    void addComponent(const ComponentSpec& spec);
    void updateAvailability(const std::string& partNum, int quantity);
    void updatePrice(const std::string& partNum, double price);

    // Import from distributor
    bool importFromDigiKey(const std::string& apiKey);
    bool importFromMouser(const std::string& apiKey);
    bool importFromLCSC(const std::string& apiKey);

    // Export
    void exportToCSV(const std::string& filename);
    void exportToJSON(const std::string& filename);

private:
    ComponentDatabase();
    std::map<std::string, ComponentSpec> m_components;
    std::map<std::string, std::vector<std::string>> m_byType;
};

// Substitution Engine
class SubstitutionEngine
{
public:
    SubstitutionEngine();
    ~SubstitutionEngine();

    // Find substitutes for a component
    std::vector<SubstitutionSuggestion> findSubstitutes(const ComponentSpec& original,
                                                        const std::vector<std::string>& preferredManufacturers = {});

    // Find substitutes by part number
    std::vector<SubstitutionSuggestion> findSubstitutesByPartNumber(const std::string& partNumber);

    // Batch substitution for entire BOM
    struct BOMSubstitutionResult
    {
        int totalComponents;
        int substitutedComponents;
        double originalCost;
        double newCost;
        double savings;
        std::vector<SubstitutionSuggestion> suggestions;
    };

    BOMSubstitutionResult substituteBOM(const std::vector<ComponentSpec>& bom);

    // Compatibility checking
    bool isCompatible(const ComponentSpec& original, const ComponentSpec& substitute);
    double calculateCompatibilityScore(const ComponentSpec& original, const ComponentSpec& substitute);

    // Configuration
    void setMinAvailability(int qty) { m_minAvailability = qty; }
    void setMaxPriceIncrease(double percent) { m_maxPriceIncrease = percent; }
    void setPreferredManufacturers(const std::vector<std::string>& mfrs) { m_preferredManufacturers = mfrs; }

private:
    int m_minAvailability;
    double m_maxPriceIncrease;
    std::vector<std::string> m_preferredManufacturers;

    std::vector<SubstitutionSuggestion> rankSuggestions(std::vector<SubstitutionSuggestion>& suggestions);
    void checkParametricCompatibility(const ComponentSpec& original, const ComponentSpec& substitute,
                                      SubstitutionSuggestion& suggestion);
};

// BOM Analyzer
class BOMAnalyzer
{
public:
    BOMAnalyzer();
    ~BOMAnalyzer();

    // Load BOM
    void loadFromCircuit(const std::string& circuitFile);
    void loadFromCSV(const std::string& filename);
    void loadFromJSON(const std::string& filename);

    // Analysis
    struct BOMAnalysis
    {
        int totalComponents;
        int uniqueComponents;
        double totalCost;
        std::map<std::string, int> byType;
        std::map<std::string, int> byPackage;
        std::map<std::string, int> byManufacturer;
        std::vector<std::string> hardToFind; // Low availability
        std::vector<std::string> expensive;  // High cost items
    };

    BOMAnalysis analyze();

    // Optimization suggestions
    std::vector<std::string> suggestOptimizations();

    // Get BOM
    const std::vector<ComponentSpec>& getBOM() const { return m_bom; }

private:
    std::vector<ComponentSpec> m_bom;
    std::string m_circuitFile;
};

// C Interface for FluxScript JIT
extern "C" {
// Database
void* flux_comp_db_create();
void flux_comp_db_destroy(void* db);
const char* flux_comp_db_search(void* db, const char* query);

// Substitution
void* flux_sub_engine_create();
void flux_sub_engine_destroy(void* engine);
const char* flux_sub_find(void* engine, const char* partNumber);
const char* flux_sub_bom_substitute(void* engine, const char* bomFile);

// BOM Analysis
void* flux_bom_analyzer_create();
void flux_bom_analyzer_destroy(void* analyzer);
void flux_bom_load(void* analyzer, const char* filename);
const char* flux_bom_analyze(void* analyzer);
}

} // namespace Substitution
} // namespace Flux

#endif // FLUX_COMPONENT_SUBSTITUTION_H
