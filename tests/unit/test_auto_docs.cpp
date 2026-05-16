// Auto-Documentation Generator Tests
#include <iostream>
#include <cassert>
#include <fstream>
#include "flux/tooling/auto_docs.h"

using namespace Flux::AutoDocs;

void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

void test_docs_basic() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #3: Auto-Documentation Generator - Basic Tests  \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: Generate from string
    std::cout << "Test 1: Generate Documentation from String\n";
    std::cout << "\n";
    
    std::string circuitContent = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    
    DocGenerator gen;
    DocConfig config;
    config.projectName = "Test Filter";
    config.projectVersion = "1.0.0";
    config.author = "Test Engineer";
    gen.setConfig(config);
    
    auto doc = gen.generateFromString(circuitContent);
    
    std::cout << "  Generated: " << doc.generatedDate << "\n";
    std::cout << "  Components: " << doc.totalComponents << "\n";
    
    assert(doc.totalComponents == 2 && "Should have 2 components!");
    assert(!doc.generatedDate.empty() && "Should have date!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: BOM Extraction
    std::cout << "\n\nTest 2: BOM Extraction\n";
    std::cout << "\n";
    
    auto bom = doc.bom;
    
    std::cout << "  BOM Entries:\n";
    for (const auto& entry : bom) {
        std::cout << "    " << entry.designator << ": " << entry.type 
                  << " " << entry.value << "\n";
    }
    
    assert(bom.size() == 2 && "Should have 2 BOM entries!");
    assert(bom[0].designator == "R1" && "First should be R1!");
    assert(bom[1].designator == "C1" && "Second should be C1!");
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Markdown Output
    std::cout << "\n\nTest 3: Markdown Output\n";
    std::cout << "\n";
    
    std::string markdown = doc.toMarkdown();
    
    assert(markdown.find("# Test Filter") != std::string::npos && "Should have title!");
    assert(markdown.find("| Designator |") != std::string::npos && "Should have BOM table!");
    
    std::cout << "  Generated markdown length: " << markdown.length() << " chars\n";
    
    std::cout << "\n Test 3 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Auto-Docs: BASIC TESTS PASSED                      \n";
    std::cout << "\n";
}

void test_docs_output_formats() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #3: Auto-Docs - Output Formats                  \n";
    std::cout << "\n";
    std::cout << "\n";
    
    std::string circuitContent = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    
    DocGenerator gen;
    DocConfig config;
    config.projectName = "Test Circuit";
    gen.setConfig(config);
    
    auto doc = gen.generateFromString(circuitContent);
    
    // Test 1: HTML Output
    std::cout << "Test 1: HTML Output\n";
    std::cout << "\n";
    
    std::string html = doc.toHTML();
    
    assert(html.find("<!DOCTYPE html>") != std::string::npos && "Should have HTML doctype!");
    assert(html.find("<title>") != std::string::npos && "Should have title!");
    assert(html.find("<table>") != std::string::npos && "Should have table!");
    
    std::cout << "  Generated HTML length: " << html.length() << " chars\n";
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: RST Output
    std::cout << "\n\nTest 2: RST Output\n";
    std::cout << "\n";
    
    std::string rst = doc.toRST();
    
    assert(rst.find("====") != std::string::npos && "Should have RST headers!");
    assert(rst.find("-----") != std::string::npos && "Should have RST underlines!");
    
    std::cout << "  Generated RST length: " << rst.length() << " chars\n";
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Text Output
    std::cout << "\n\nTest 3: Plain Text Output\n";
    std::cout << "\n";
    
    std::string text = doc.toText();
    
    assert(text.find("TECHNICAL DOCUMENTATION") != std::string::npos && "Should have header!");
    assert(text.find("R1:") != std::string::npos && "Should list components!");
    
    std::cout << "  Generated text length: " << text.length() << " chars\n";
    
    std::cout << "\n Test 3 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Auto-Docs: OUTPUT FORMAT TESTS PASSED              \n";
    std::cout << "\n";
}

void test_docs_file_generation() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #3: Auto-Docs - File Generation                 \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Create test circuit file
    createTestFile("/tmp/test_circuit.flux", 
                   "R1 node1 node2 10k\nC1 node2 node3 1uF\nL1 node3 node4 100uH\n");
    
    std::cout << "Test 1: Generate from File\n";
    std::cout << "\n";
    
    DocConfig config;
    config.projectName = "File Test Circuit";
    config.projectVersion = "1.0.0";
    config.author = "Auto";
    
    auto doc = generate_docs("/tmp/test_circuit.flux", config);
    
    std::cout << "  Components found: " << doc.totalComponents << "\n";
    std::cout << "  Generated: " << doc.generatedDate << "\n";
    
    assert(doc.totalComponents == 3 && "Should have 3 components from file!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Full Markdown Report
    std::cout << "\n\nTest 2: Full Markdown Report\n";
    std::cout << "\n";
    
    std::string markdown = doc.toMarkdown();
    std::cout << markdown;
    
    assert(markdown.find("Bill of Materials") != std::string::npos && "Should have BOM section!");
    assert(markdown.find("Troubleshooting") != std::string::npos && "Should have troubleshooting!");
    
    std::cout << "\n Test 2 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Auto-Docs: FILE GENERATION TESTS PASSED            \n";
    std::cout << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          Auto-Documentation Generator - Test Suite       \n";
    std::cout << "\n";
    
    try {
        test_docs_basic();
        test_docs_output_formats();
        test_docs_file_generation();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL AUTO-DOCS TESTS PASSED                    \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
