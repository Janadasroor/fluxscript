// Circuit Diff Tool Tests
#include <iostream>
#include <cassert>
#include <fstream>
#include "flux/tooling/circuit_diff.h"

using namespace Flux::CircuitDiff;

// Helper to create test circuit files
void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

void test_basic_diff() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #1: Circuit Diff Tool - Basic Tests             \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: Identical circuits
    std::cout << "Test 1: Identical Circuits\n";
    std::cout << "\n";
    
    std::string circuit1 = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    std::string circuit2 = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    
    CircuitDiffEngine engine;
    auto result = engine.diffStrings(circuit1, circuit2);
    
    std::cout << "  Similarity: " << (result.similarityScore * 100) << "%\n";
    std::cout << "  Total changes: " << result.totalChanges << "\n";
    
    assert(result.totalChanges == 0 && "Identical circuits should have no changes!");
    assert(result.similarityScore == 1.0 && "Similarity should be 100%!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Added component
    std::cout << "\n\nTest 2: Added Component\n";
    std::cout << "\n";
    
    std::string circuit3 = "R1 node1 node2 10k\n";
    std::string circuit4 = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    
    auto result2 = engine.diffStrings(circuit3, circuit4);
    
    std::cout << "  Added components: " << result2.addedComponents << "\n";
    std::cout << "  Summary: " << result2.summary << "\n";
    
    assert(result2.addedComponents == 1 && "Should have 1 added component!");
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Removed component
    std::cout << "\n\nTest 3: Removed Component\n";
    std::cout << "\n";
    
    std::string circuit5 = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    std::string circuit6 = "R1 node1 node2 10k\n";
    
    auto result3 = engine.diffStrings(circuit5, circuit6);
    
    std::cout << "  Removed components: " << result3.removedComponents << "\n";
    std::cout << "  Summary: " << result3.summary << "\n";
    
    assert(result3.removedComponents == 1 && "Should have 1 removed component!");
    
    std::cout << "\n Test 3 PASSED\n";
    
    // Test 4: Modified component
    std::cout << "\n\nTest 4: Modified Component\n";
    std::cout << "\n";
    
    std::string circuit7 = "R1 node1 node2 10k\n";
    std::string circuit8 = "R1 node1 node2 15k\n";
    
    auto result4 = engine.diffStrings(circuit7, circuit8);
    
    std::cout << "  Modified components: " << result4.modifiedComponents << "\n";
    std::cout << "  Summary: " << result4.summary << "\n";
    
    assert(result4.modifiedComponents == 1 && "Should have 1 modified component!");
    
    std::cout << "\n Test 4 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Circuit Diff: ALL TESTS PASSED                     \n";
    std::cout << "\n";
}

void test_file_diff() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #1: Circuit Diff Tool - File Tests              \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Create test files
    createTestFile("/tmp/circuit_old.flux", "R1 node1 node2 10k\nC1 node2 node3 1uF\nL1 node3 node4 100uH\n");
    createTestFile("/tmp/circuit_new.flux", "R1 node1 node2 15k\nC1 node2 node3 1uF\nC2 node3 node4 10nF\n");
    
    std::cout << "Test 1: File Diff\n";
    std::cout << "\n";
    
    auto result = circuit_diff("/tmp/circuit_old.flux", "/tmp/circuit_new.flux");
    
    std::cout << result.toText();
    
    assert(result.totalChanges > 0 && "Should have changes!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Markdown output
    std::cout << "\n\nTest 2: Markdown Output\n";
    std::cout << "\n";
    
    std::string markdown = result.toMarkdown();
    std::cout << markdown;
    
    assert(markdown.find("# Circuit Diff Report") != std::string::npos && "Should have markdown header!");
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: JSON output
    std::cout << "\n\nTest 3: JSON Output\n";
    std::cout << "\n";
    
    std::string json = result.toJSON();
    std::cout << json;
    
    assert(json.find("\"similarity\"") != std::string::npos && "Should have JSON similarity!");
    
    std::cout << "\n Test 3 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     File Diff: ALL TESTS PASSED                        \n";
    std::cout << "\n";
}

void test_recommendations() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #1: Circuit Diff Tool - Recommendations         \n";
    std::cout << "\n";
    std::cout << "\n";
    
    std::cout << "Test 1: Smart Recommendations\n";
    std::cout << "\n";
    
    std::string circuit1 = "R1 node1 node2 10k\nC1 node2 node3 1uF\n";
    std::string circuit2 = "R1 node1 node2 15k\n";  // Removed capacitor
    
    CircuitDiffEngine engine;
    auto result = engine.diffStrings(circuit1, circuit2);
    
    std::cout << "  Recommendations:\n";
    for (const auto& rec : result.recommendations) {
        std::cout << "     " << rec << "\n";
    }
    
    assert(!result.recommendations.empty() && "Should have recommendations!");
    
    std::cout << "\n Test 1 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Recommendations: ALL TESTS PASSED                  \n";
    std::cout << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          Circuit Diff Tool - Test Suite                  \n";
    std::cout << "\n";
    
    try {
        test_basic_diff();
        test_file_diff();
        test_recommendations();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL CIRCUIT DIFF TESTS PASSED                 \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
