// AI Circuit Doctor Tests
#include <iostream>
#include <cassert>
#include <fstream>
#include "flux/doctor/circuit_doctor.h"

using namespace Flux::Doctor;

void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 1: Floating Node Detection                     │
// └─────────────────────────────────────────────────────┘
void test_floating_nodes() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 1: Floating Node Detection                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Circuit with floating node
    std::string circuit = R"(
Vin in 0 DC=5
R1 in out 10k
R2 float out 10k
)";
    
    CircuitDoctor doctor;
    auto report = doctor.analyzeFromString(circuit);
    
    std::cout << report.toText();
    
    assert(report.errorCount > 0 && "Should detect floating node!");
    assert(!report.passesDesignRules && "Should fail design rules!");
    
    std::cout << "\n✅ Test 1 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 2: DC Path Check                               │
// └─────────────────────────────────────────────────────┘
void test_dc_paths() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 2: DC Path to Ground Check                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Circuit without ground
    std::string circuit = R"(
Vin in out DC=5
R1 out load 10k
)";
    
    CircuitDoctor doctor;
    auto report = doctor.analyzeFromString(circuit);
    
    std::cout << report.toText();
    
    assert(report.errorCount > 0 && "Should detect missing ground path!");
    
    std::cout << "\n✅ Test 2 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 3: Short Circuit Detection                     │
// └─────────────────────────────────────────────────────┘
void test_short_circuits() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 3: Short Circuit Detection                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Circuit with short
    std::string circuit = R"(
Vin in 0 DC=5
R1 out out 10k
R2 in 0 10k
)";
    
    CircuitDoctor doctor;
    auto report = doctor.analyzeFromString(circuit);
    
    std::cout << report.toText();
    
    assert(report.errorCount > 0 && "Should detect short circuit!");
    
    std::cout << "\n✅ Test 3 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 4: Good Circuit (No Issues)                    │
// └─────────────────────────────────────────────────────┘
void test_good_circuit() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 4: Good Circuit (No Issues)                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Good voltage divider
    std::string circuit = R"(
Vin in 0 DC=5
R1 in out 10k
R2 out 0 10k
)";
    
    CircuitDoctor doctor;
    auto report = doctor.analyzeFromString(circuit);
    
    std::cout << report.toText();
    
    assert(report.errorCount == 0 && "Good circuit should have no errors!");
    assert(report.passesDesignRules && "Good circuit should pass!");
    
    std::cout << "\n✅ Test 4 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 5: Output Formats                              │
// └─────────────────────────────────────────────────────┘
void test_output_formats() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 5: Output Formats                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    std::string circuit = R"(
Vin in 0 DC=5
R1 in out 10k
R2 out 0 10k
)";
    
    CircuitDoctor doctor;
    auto report = doctor.analyzeFromString(circuit);
    
    // Test Text
    std::string text = report.toText();
    assert(text.find("DIAGNOSTIC REPORT") != std::string::npos && "Should have text header!");
    std::cout << "✅ Text output works\n";
    
    // Test Markdown
    std::string md = report.toMarkdown();
    assert(md.find("# AI Circuit Doctor") != std::string::npos && "Should have markdown header!");
    std::cout << "✅ Markdown output works\n";
    
    // Test JSON
    std::string json = report.toJSON();
    assert(json.find("\"passes\"") != std::string::npos && "Should have JSON structure!");
    std::cout << "✅ JSON output works\n";
    
    std::cout << "\n✅ Test 5 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Test 6: C Interface                                 │
// └─────────────────────────────────────────────────────┘
void test_c_interface() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test 6: C Interface for JIT                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    void* doctor = flux_doctor_create();
    assert(doctor != nullptr && "Should create doctor!");
    
    flux_doctor_set_severity(doctor, 1);  // Warning
    
    const char* result = flux_doctor_analyze(doctor, "test");
    assert(result != nullptr && "Should return result!");
    
    int errors = flux_doctor_get_error_count(doctor);
    int warnings = flux_doctor_get_warning_count(doctor);
    bool passes = flux_doctor_passes(doctor);
    
    std::cout << "  Errors: " << errors << ", Warnings: " << warnings << ", Passes: " << passes << "\n";
    
    flux_doctor_destroy(doctor);
    
    std::cout << "\n✅ Test 6 PASSED\n";
}

// ┌─────────────────────────────────────────────────────┐
// │ Main Test Runner                                    │
// └─────────────────────────────────────────────────────┘
int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║          AI Circuit Doctor - Test Suite                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    try {
        test_floating_nodes();
        test_dc_paths();
        test_short_circuits();
        test_good_circuit();
        test_output_formats();
        test_c_interface();
        
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║          ALL CIRCUIT DOCTOR TESTS PASSED ✅              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
