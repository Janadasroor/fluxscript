#ifndef FLUX_AUTO_DOCS_H
#define FLUX_AUTO_DOCS_H

#include <string>
#include <vector>
#include <map>

namespace Flux {
namespace AutoDocs {

// Documentation Section Types
enum class DocSection {
    Overview,
    TheoryOfOperation,
    ComponentList,
    DesignEquations,
    SimulationResults,
    TestProcedures,
    Troubleshooting,
    RevisionHistory
};

// Documentation Configuration
struct DocConfig {
    std::string outputFormat;      // "markdown", "html", "pdf", "rst"
    std::string language;          // "en", "zh", "de", etc.
    std::string projectName;
    std::string projectVersion;
    std::string author;
    bool includeBOM;
    bool includeSimulation;
    bool includeTestProcedures;
    bool includeTroubleshooting;
    std::string templateName;      // Custom template to use
    bool includeSchematics;
    bool includeWaveforms;
    
    DocConfig() : outputFormat("markdown"), language("en"),
                  includeBOM(true), includeSimulation(true),
                  includeTestProcedures(true), includeTroubleshooting(true),
                  includeSchematics(false), includeWaveforms(false) {}
};

// Component Entry for BOM
struct BOMEntry {
    std::string designator;
    std::string type;
    std::string value;
    std::string tolerance;
    std::string package;
    std::string manufacturer;
    std::string partNumber;
    int quantity;
    std::string description;
    
    BOMEntry() : quantity(1) {}
};

// Documentation Block
struct DocBlock {
    std::string title;
    std::string content;
    std::vector<std::string> bulletPoints;
    std::vector<std::pair<std::string, std::string>> tableRows;
    std::string codeExample;
};

// Complete Documentation Package
struct Documentation {
    std::string circuitFile;
    DocConfig config;
    
    // Sections
    DocBlock overview;
    DocBlock theoryOfOperation;
    std::vector<BOMEntry> bom;
    DocBlock designEquations;
    DocBlock simulationResults;
    DocBlock testProcedures;
    DocBlock troubleshooting;
    
    // Metadata
    std::string generatedDate;
    std::string toolVersion;
    
    // Statistics
    int totalComponents;
    int totalPages;
    
    Documentation() : totalComponents(0), totalPages(0) {}
    
    // Output methods
    std::string toMarkdown() const;
    std::string toHTML() const;
    std::string toRST() const;
    std::string toText() const;
    std::string toPDF() const;  // Stub - would require PDF library
};

// Documentation Generator
class DocGenerator {
public:
    DocGenerator();
    ~DocGenerator();
    
    // Set configuration
    void setConfig(const DocConfig& config);
    
    // Generate documentation from circuit file
    Documentation generateFromCircuit(const std::string& circuitFile);
    
    // Generate from circuit string
    Documentation generateFromString(const std::string& circuitContent);
    
    // Customize templates
    void addTemplate(const std::string& name, const std::string& templateContent);
    
private:
    DocConfig m_config;
    std::map<std::string, std::string> m_templates;
    
    // Internal generation methods
    DocBlock generateOverview(const std::string& circuitFile);
    DocBlock generateTheoryOfOperation(const std::string& circuitContent);
    std::vector<BOMEntry> extractBOM(const std::string& circuitContent);
    DocBlock generateDesignEquations(const std::string& circuitContent);
    DocBlock generateTestProcedures(const std::string& circuitContent);
    DocBlock generateTroubleshooting(const std::string& circuitContent);
    
    // Helper methods
    std::string inferCircuitType(const std::string& content);
    std::string generateDescription(const std::string& circuitType);
    std::vector<std::string> getCommonTopologies(const std::string& type);
};

// Convenience functions
Documentation generate_docs(const std::string& circuitFile, 
                            const DocConfig& config = DocConfig());

// C Interface for FluxScript JIT
extern "C" {
    void* flux_docs_create();
    void flux_docs_destroy(void* docs);
    void flux_docs_set_format(void* docs, const char* format);
    const char* flux_docs_generate(void* docs, const char* circuitFile);
    const char* flux_docs_get_markdown(void* docs);
}

} // namespace AutoDocs
} // namespace Flux

#endif // FLUX_AUTO_DOCS_H
