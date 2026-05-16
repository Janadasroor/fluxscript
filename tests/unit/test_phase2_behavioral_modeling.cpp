// FluxScript Phase 2: High-Performance Behavioral Modeling Tests
// Tests for SmartSignalItem, JIT Manager, Simulation Hook, Zero-Copy Data

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "flux/jit/smart_signal.h"
#include "flux/jit/jit_manager.h"
#include "flux/jit/simulation_hook.h"
#include "flux/jit/zero_copy_data.h"

using namespace Flux;

// ============================================================================
// Test #1: SmartSignalItem - Parametric Signal Generation
// ============================================================================

void test_smart_signal_parametric() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #1: SmartSignalItem - Parametric Generation       \n";
    std::cout << "\n";

    // Test 1.1: Sine wave generation
    std::cout << "\nTest 1.1: Sine Wave Generation\n";
    std::cout << "\n";

    SmartSignalItem sine_signal("sine_test", SignalSourceType::Parametric);
    
    SignalParams params;
    params.dc_offset = 0.0;
    params.amplitude = 1.0;
    params.frequency = 1e3;  // 1 kHz
    params.phase = 0.0;
    params.t_start = 0.0;
    params.t_stop = 1e-3;    // 1 ms
    params.sample_rate = 1e5; // 100 kHz
    
    sine_signal.setWaveformType(ParametricWaveformType::Sine);
    sine_signal.setParameters(params);
    
    auto result = sine_signal.generateSignal();
    
    assert(result.success && "Sine generation failed!");
    assert(result.num_points > 0 && "No points generated!");
    std::cout << "  Generated " << result.num_points << " points in " 
              << std::setprecision(2) << result.generation_time_ms << " ms\n";
    
    // Verify sine properties
    double min_val = sine_signal.min();
    double max_val = sine_signal.max();
    double avg_val = sine_signal.average();
    double rms_val = sine_signal.rms();
    
    std::cout << "  Min: " << std::setprecision(4) << min_val << "\n";
    std::cout << "  Max: " << max_val << "\n";
    std::cout << "  Average: " << avg_val << "\n";
    std::cout << "  RMS: " << rms_val << "\n";
    
    assert(std::abs(min_val - (-1.0)) < 0.01 && "Min should be ~-1.0");
    assert(std::abs(max_val - 1.0) < 0.01 && "Max should be ~1.0");
    assert(std::abs(avg_val) < 0.01 && "Average should be ~0");
    assert(std::abs(rms_val - 0.707) < 0.01 && "RMS should be ~0.707");
    
    std::cout << "\n Test 1.1 PASSED\n";

    // Test 1.2: Square wave generation
    std::cout << "\n\nTest 1.2: Square Wave Generation\n";
    std::cout << "\n";

    SmartSignalItem square_signal("square_test", SignalSourceType::Parametric);
    params.amplitude = 2.0;
    params.dc_offset = 1.0;
    params.frequency = 500;
    
    square_signal.setWaveformType(ParametricWaveformType::Square);
    square_signal.setParameters(params);
    
    result = square_signal.generateSignal();
    assert(result.success && "Square generation failed!");
    
    std::cout << "  Generated " << result.num_points << " points\n";
    std::cout << "  Min: " << square_signal.min() << "\n";
    std::cout << "  Max: " << square_signal.max() << "\n";
    
    assert(std::abs(square_signal.min() - (-1.0)) < 0.01 && "Min should be ~-1.0");
    assert(std::abs(square_signal.max() - 3.0) < 0.01 && "Max should be ~3.0 (2+1)");
    
    std::cout << "\n Test 1.2 PASSED\n";

    // Test 1.3: CSV export
    std::cout << "\n\nTest 1.3: CSV Export\n";
    std::cout << "\n";

    std::string csv = sine_signal.toCSV();
    assert(csv.find("time,sine_test") != std::string::npos && "CSV header incorrect!");
    assert(csv.length() > 100 && "CSV too short!");
    
    std::cout << "  CSV size: " << csv.length() << " bytes\n";
    std::cout << "  First line: " << csv.substr(0, csv.find('\n')) << "\n";
    
    std::cout << "\n Test 1.3 PASSED\n";

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #1: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Test #2: SmartSignalManager - Signal Group Management
// ============================================================================

void test_smart_signal_manager() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #2: SmartSignalManager - Group Management         \n";
    std::cout << "\n";

    auto& manager = SmartSignalManager::instance();

    // Test 2.1: Add and retrieve signals
    std::cout << "\nTest 2.1: Signal Management\n";
    std::cout << "\n";

    auto sig1 = std::make_shared<SmartSignalItem>("test_sig1", SignalSourceType::Parametric);
    SignalParams p1;
    p1.amplitude = 1.0;
    p1.frequency = 1e3;
    sig1->setWaveformType(ParametricWaveformType::Sine);
    sig1->setParameters(p1);
    
    assert(manager.addSignal(sig1) && "Failed to add signal!");
    assert(manager.hasSignal("test_sig1") && "Signal not found!");
    assert(manager.signalCount() == 1 && "Signal count incorrect!");
    
    std::cout << "  Added signal: test_sig1\n";
    std::cout << "  Signal count: " << manager.signalCount() << "\n";
    
    std::cout << "\n Test 2.1 PASSED\n";

    // Test 2.2: Group management
    std::cout << "\n\nTest 2.2: Group Management\n";
    std::cout << "\n";

    assert(manager.createGroup("test_group") && "Failed to create group!");
    assert(manager.addToGroup("test_group", "test_sig1") && "Failed to add to group!");
    
    auto* group = manager.getGroup("test_group");
    assert(group != nullptr && "Group not found!");
    assert(group->signals.size() == 1 && "Group signal count incorrect!");
    
    std::cout << "  Created group: test_group\n";
    std::cout << "  Group size: " << group->signals.size() << "\n";
    
    std::cout << "\n Test 2.2 PASSED\n";

    // Cleanup
    manager.removeSignal("test_sig1");

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #2: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Test #3: JIT Manager - Component Registration & Compilation
// ============================================================================

void test_jit_manager() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #3: JIT Manager - Component Management            \n";
    std::cout << "\n";

    auto& jit_mgr = JITManager::instance();
    jit_mgr.initialize();

    // Test 3.1: Register and compile component
    std::cout << "\nTest 3.1: Component Registration\n";
    std::cout << "\n";

    std::string error;
    // Simple test: function that returns its second argument
    // Note: we use "process" since "update" is a reserved keyword
    std::string source =
        "def process(t, x) {\n"
        "    x\n"
        "}\n";
    
    assert(jit_mgr.registerComponent("test_comp", source, 1, 1, &error) && "Registration failed!");
    std::cout << "  Registered component: test_comp\n";
    
    assert(jit_mgr.compileComponent("test_comp", &error) && "Compilation failed!");
    std::cout << "  Compiled component successfully\n";
    
    // Look up the "process" function instead of "update"
    auto* comp = jit_mgr.getComponent("test_comp");
    assert(comp && comp->update_func && "Component or update_func is null!");
    
    std::cout << "\n Test 3.1 PASSED\n";

    // Test 3.2: Evaluate component
    std::cout << "\n\nTest 3.2: Component Evaluation\n";
    std::cout << "\n";

    double inputs[1] = { 5.0 };
    double outputs[1] = { 0.0 };
    
    bool success = jit_mgr.evaluateComponent("test_comp", 0.0, 1e-6, inputs, outputs);
    assert(success && "Evaluation failed!");
    
    std::cout << "  Input: " << inputs[0] << "\n";
    std::cout << "  Output: " << outputs[0] << "\n";
    
    auto stats = jit_mgr.getStatistics();
    std::cout << "  Total evaluations: " << stats.total_evaluations << "\n";
    std::cout << "  Avg eval time: " << std::setprecision(2) << stats.avg_eval_time_us << " s\n";
    
    assert(outputs[0] == 5.0 && "Output should match input!");
    
    std::cout << "\n Test 3.2 PASSED\n";

    // Test 3.3: Hot reload with real FluxScript source
    std::cout << "\n\nTest 3.3: Hot Reload\n";
    std::cout << "\n";

    // Use standard FluxScript syntax: def proc(t, x) { ... }
    // Note: "update" is a reserved keyword, so we use "proc"
    std::string new_source =
        "def proc(t, x) {\n"
        "    sin(t * 6.28318)\n"
        "}\n";

    assert(jit_mgr.recompileComponent("test_comp", new_source, &error) && "Hot reload failed!");

    inputs[0] = 0.25;  // t=0.25, sin(0.25*2*pi) = sin(pi/2) = 1.0
    outputs[0] = 0.0;
    success = jit_mgr.evaluateComponent("test_comp", 0.25, 1e-6, inputs, outputs);
    assert(success && "Evaluation after reload failed!");

    std::cout << "  Input: t=" << inputs[0] << "\n";
    std::cout << "  Output: " << std::setprecision(4) << outputs[0] << " (expected: ~1.0)\n";

    assert(std::abs(outputs[0] - 1.0) < 0.01 && "Hot reload output incorrect!");

    std::cout << "\n Test 3.3 PASSED\n";

    // Cleanup
    jit_mgr.unregisterComponent("test_comp");
    jit_mgr.finalize();

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #3: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Test #4: Simulation Hook - Transient Loop Integration
// ============================================================================

void test_simulation_hook() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #4: Simulation Hook - Transient Integration       \n";
    std::cout << "\n";

    auto& hook = NgspiceSimulationHook::instance();
    auto& jit_mgr = JITManager::instance();

    // Test 4.1: Node binding
    std::cout << "\nTest 4.1: Node Binding\n";
    std::cout << "\n";

    double node_voltage = 0.0;
    double node_current = 0.0;
    
    assert(hook.bindNode("V(out)", &node_voltage, &node_current) && "Node binding failed!");
    assert(hook.isNodeBound("V(out)") && "Node not bound!");
    
    std::cout << "  Bound node: V(out)\n";
    std::cout << "  Voltage ptr: " << (&node_voltage) << "\n";
    std::cout << "  Current ptr: " << (&node_current) << "\n";
    
    std::cout << "\n Test 4.1 PASSED\n";

    // Test 4.2: JIT component integration
    std::cout << "\n\nTest 4.2: JIT Component Integration\n";
    std::cout << "\n";

    jit_mgr.initialize();
    // Use syntax supported by the parser (node names, not array indices)
    std::string source = "update(t, inputs, outputs, state) {\n"
                         "    sin(t * 6.28318 * 1000)\n"
                         "}\n";
    std::string error;
    
    jit_mgr.registerComponent("oscillator", source, 1, 1, &error);
    jit_mgr.compileComponent("oscillator", &error);
    
    std::vector<std::string> input_nodes = {"V(in)"};
    std::vector<std::string> output_nodes = {"V(out)"};
    
    assert(hook.integrateJITComponent("oscillator", input_nodes, output_nodes) && "Integration failed!");
    
    std::cout << "  Integrated JIT component: oscillator\n";
    std::cout << "  Input nodes: " << input_nodes.size() << "\n";
    std::cout << "  Output nodes: " << output_nodes.size() << "\n";
    
    std::cout << "\n Test 4.2 PASSED\n";

    // Test 4.3: Transient timestep simulation
    std::cout << "\n\nTest 4.3: Transient Timestep\n";
    std::cout << "\n";

    TimestepData timestep;
    timestep.current_time = 0.0;
    timestep.dt = 1e-6;
    timestep.iteration = 0;
    timestep.converged = true;
    
    bool success = hook.onTransientTimestep(timestep);
    assert(success && "Timestep failed!");
    
    std::cout << "  Timestep 0: success\n";
    std::cout << "  Timestep count: " << hook.getTimestepCount() << "\n";
    std::cout << "  Total eval time: " << std::setprecision(2) << hook.getTotalEvalTime() << " s\n";
    
    std::cout << "\n Test 4.3 PASSED\n";

    // Cleanup
    jit_mgr.unregisterComponent("oscillator");
    jit_mgr.finalize();
    hook.unbindNode("V(out)");

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #4: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Test #5: Zero-Copy Data Interface
// ============================================================================

void test_zero_copy_data() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Test #5: Zero-Copy Data Interface                      \n";
    std::cout << "\n";

    auto& zcd = ZeroCopyDataManager::instance();
    zcd.initialize();

    // Test 5.1: Buffer registration
    std::cout << "\nTest 5.1: Buffer Registration\n";
    std::cout << "\n";

    std::vector<double> voltage_buffer(100, 0.0);
    std::vector<double> current_buffer(100, 0.0);
    
    std::string error;
    assert(zcd.registerBuffer("node_voltages", voltage_buffer.data(), 100, true, &error) && "Buffer registration failed!");
    assert(zcd.registerBuffer("branch_currents", current_buffer.data(), 100, false, &error) && "Buffer registration failed!");
    
    std::cout << "  Registered buffer: node_voltages (100 elements, RW)\n";
    std::cout << "  Registered buffer: branch_currents (100 elements, RO)\n";
    std::cout << "  Buffer count: " << zcd.getBufferNames().size() << "\n";
    
    std::cout << "\n Test 5.1 PASSED\n";

    // Test 5.2: Node mapping
    std::cout << "\n\nTest 5.2: Node Mapping\n";
    std::cout << "\n";

    assert(zcd.mapNode("V(out)", "node_voltages", 0, &error) && "Node mapping failed!");
    assert(zcd.mapNode("V(in)", "node_voltages", 1, &error) && "Node mapping failed!");
    
    auto* mapping = zcd.getNodeMapping("V(out)");
    assert(mapping != nullptr && "Node mapping not found!");
    assert(mapping->direct_ptr != nullptr && "Direct pointer is null!");
    
    std::cout << "  Mapped V(out) -> node_voltages[0]\n";
    std::cout << "  Mapped V(in) -> node_voltages[1]\n";
    std::cout << "  Mapped nodes: " << zcd.getMappedNodeCount() << "\n";
    
    std::cout << "\n Test 5.2 PASSED\n";

    // Test 5.3: Zero-copy read/write
    std::cout << "\n\nTest 5.3: Zero-Copy Operations\n";
    std::cout << "\n";

    // Write to node
    zcd.writeVoltage("V(out)", 3.3);
    double value = zcd.readVoltage("V(out)");
    
    std::cout << "  Write V(out): 3.3 V\n";
    std::cout << "  Read V(out): " << std::setprecision(2) << value << " V\n";
    
    assert(std::abs(value - 3.3) < 0.01 && "Zero-copy read/write failed!");
    
    // Verify direct buffer access
    assert(std::abs(voltage_buffer[0] - 3.3) < 0.01 && "Buffer not updated!");
    std::cout << "  Buffer[0]: " << voltage_buffer[0] << " V (direct access verified)\n";
    
    // Statistics
    std::cout << "  Read count: " << zcd.getReadCount() << "\n";
    std::cout << "  Write count: " << zcd.getWriteCount() << "\n";
    std::cout << "  Avg read latency: " << std::setprecision(1) << zcd.getAverageReadLatencyNs() << " ns\n";
    std::cout << "  Avg write latency: " << zcd.getAverageWriteLatencyNs() << " ns\n";
    
    std::cout << "\n Test 5.3 PASSED\n";

    // Test 5.4: Bulk operations
    std::cout << "\n\nTest 5.4: Bulk Operations\n";
    std::cout << "\n";

    std::vector<double> test_values = {1.0, 2.0, 3.0, 4.0, 5.0};
    zcd.writeAllVoltages(test_values);
    auto read_back = zcd.readAllVoltages();
    
    std::cout << "  Wrote 5 values\n";
    std::cout << "  Read back " << read_back.size() << " values\n";
    std::cout << "  First: " << read_back[0] << ", Last: " << read_back[4] << "\n";
    
    assert(read_back.size() >= 5 && "Bulk read failed!");
    assert(std::abs(read_back[0] - 1.0) < 0.01 && "Bulk write failed!");
    
    std::cout << "\n Test 5.4 PASSED\n";

    // Cleanup
    zcd.unregisterBuffer("node_voltages");
    zcd.unregisterBuffer("branch_currents");
    zcd.finalize();

    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Test #5: ALL TESTS PASSED                          \n";
    std::cout << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "    Phase 2: High-Performance Behavioral Modeling Tests   \n";
    std::cout << "\n";

    try {
        // Test SmartSignalItem parametric generation
        test_smart_signal_parametric();

        // Test SmartSignalManager
        test_smart_signal_manager();

        // Test JIT Manager
        test_jit_manager();

        // Test Simulation Hook
        test_simulation_hook();

        // Test Zero-Copy Data
        test_zero_copy_data();

        std::cout << "\n";
        std::cout << "\n";
        std::cout << "     ALL PHASE 2 BEHAVIORAL MODELING TESTS PASSED       \n";
        std::cout << "\n";
        std::cout << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
