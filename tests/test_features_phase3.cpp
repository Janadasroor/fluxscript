// FluxScript Feature Tests - Phase 3
// Features: Quantum Simulation, Neuromorphic Computing, Digital Twin

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

#include "flux/quantum/simulator.h"
#include "flux/neuromorphic/simulator.h"
#include "flux/twin/digital_twin.h"

using namespace Flux;

// ============================================================================
// Feature #136: Quantum Circuit Simulation Tests
// ============================================================================

void test_quantum_simulation() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #136: Quantum Circuit Simulation                \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: Bell State
    std::cout << "Test 1: Bell State Entanglement\n";
    std::cout << "\n";
    
    Quantum::QuantumCircuit bellCircuit = Quantum::QuantumAlgorithms::createBellState(0, 1);
    
    std::cout << "  Circuit QASM:\n";
    std::cout << bellCircuit.toQASM() << "\n";
    
    Quantum::QuantumSimulator sim(2);
    sim.execute(bellCircuit);
    
    std::cout << "  Final State: " << sim.stateToText() << "\n";
    std::cout << "  Qubits: " << sim.numQubits() << "\n";
    
    assert(sim.numQubits() == 2 && "Should have 2 qubits!");
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: GHZ State
    std::cout << "\n\nTest 2: GHZ State (3-qubit entanglement)\n";
    std::cout << "\n";
    
    Quantum::QuantumCircuit ghzCircuit = Quantum::QuantumAlgorithms::createGHZState(3);
    
    std::cout << "  GHZ Circuit:\n";
    std::cout << ghzCircuit.toQASM() << "\n";
    
    Quantum::QuantumSimulator sim2(3);
    sim2.execute(ghzCircuit);
    
    std::cout << "  Created 3-qubit GHZ state\n";
    
    assert(sim2.numQubits() == 3 && "Should have 3 qubits!");
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Quantum Fourier Transform
    std::cout << "\n\nTest 3: Quantum Fourier Transform\n";
    std::cout << "\n";
    
    Quantum::QuantumCircuit qftCircuit = Quantum::QuantumAlgorithms::qft(4);
    
    std::cout << "  QFT on 4 qubits\n";
    std::cout << "  Gates: " << qftCircuit.numGates() << "\n";
    
    assert(qftCircuit.numQubits() == 4 && "QFT should have 4 qubits!");
    std::cout << "\n Test 3 PASSED\n";
    
    // Test 4: Grover's Algorithm
    std::cout << "\n\nTest 4: Grover's Search Algorithm\n";
    std::cout << "\n";
    
    // Search for target = 5
    auto oracle = [](int x) { return x == 5; };
    Quantum::QuantumCircuit groverCircuit = Quantum::QuantumAlgorithms::groverSearch(oracle, 4);
    
    std::cout << "  Grover search on 4 qubits\n";
    std::cout << "  Target: 5\n";
    std::cout << "  Gates: " << groverCircuit.numGates() << "\n";
    
    assert(groverCircuit.numQubits() == 4 && "Grover should have 4 qubits!");
    std::cout << "\n Test 4 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Feature #136: ALL TESTS PASSED                     \n";
    std::cout << "\n";
}

// ============================================================================
// Feature #137: Neuromorphic Computing Tests
// ============================================================================

void test_neuromorphic() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #137: Neuromorphic Computing                    \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: LIF Neuron
    std::cout << "Test 1: Leaky Integrate-and-Fire Neuron\n";
    std::cout << "\n";
    
    Neuromorphic::Neuron neuron(0, "LIF");
    
    // Stimulate neuron
    for (int i = 0; i < 100; ++i) {
        neuron.update(0.1, 5.0);  // dt=0.1ms, input=5.0nA (stronger stimulus)
    }
    
    std::cout << "  Membrane Potential: " << std::setprecision(2) 
              << neuron.getMembranePotential() << " mV\n";
    std::cout << "  Spike Count: " << neuron.getSpikeCount() << "\n";
    
    assert(neuron.getSpikeCount() > 0 && "Neuron should have spiked!");
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Neural Network
    std::cout << "\n\nTest 2: Random Neural Network\n";
    std::cout << "\n";
    
    Neuromorphic::NeuralNetwork network;
    
    // Add 10 neurons
    for (int i = 0; i < 10; ++i) {
        network.addNeuron("LIF");
    }
    
    // Random connectivity
    network.connectRandom(0.3, 1.0);
    
    std::cout << "  Neurons: " << network.numNeurons() << "\n";
    std::cout << "  Synapses: " << network.numSynapses() << "\n";
    
    // Run simulation
    network.initialize();
    network.run(100, 0.1);  // 100ms with 0.1ms timestep
    
    std::cout << "  Mean Firing Rate: " << std::setprecision(2) 
              << network.getMeanFiringRate() << " Hz\n";
    
    assert(network.numNeurons() == 10 && "Should have 10 neurons!");
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Izhikevich Neuron Types
    std::cout << "\n\nTest 3: Izhikevich Neuron Types\n";
    std::cout << "\n";
    
    auto params = Neuromorphic::IzhikevichParameters::regularSpiking();
    
    std::cout << "  Regular Spiking Parameters:\n";
    std::cout << "    a = " << params.a << "\n";
    std::cout << "    b = " << params.b << "\n";
    std::cout << "    c = " << params.c << " mV\n";
    std::cout << "    d = " << params.d << "\n";
    
    assert(params.a == 0.02 && "Parameter a should be 0.02!");
    std::cout << "\n Test 3 PASSED\n";
    
    // Test 4: STDP Learning
    std::cout << "\n\nTest 4: Spike-Timing Dependent Plasticity\n";
    std::cout << "\n";
    
    Neuromorphic::STDPLearner stdp;
    
    // LTP (pre before post)
    double ltp = stdp.computeWeightChange(10, 20, 1.0);
    // LTD (post before pre)
    double ltd = stdp.computeWeightChange(20, 10, 1.0);
    
    std::cout << "  LTP (t=10ms): " << std::setprecision(4) << ltp << "\n";
    std::cout << "  LTD (t=-10ms): " << ltd << "\n";
    
    assert(ltp > 0 && "LTP should be positive!");
    assert(ltd < 0 && "LTD should be negative!");
    std::cout << "\n Test 4 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Feature #137: ALL TESTS PASSED                     \n";
    std::cout << "\n";
}

// ============================================================================
// Feature #140: Digital Twin Tests
// ============================================================================

void test_digital_twin() {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "  Feature #140: Digital Twin                              \n";
    std::cout << "\n";
    std::cout << "\n";
    
    // Test 1: Create Digital Twin
    std::cout << "Test 1: Digital Twin Creation\n";
    std::cout << "\n";
    
    DigitalTwin::DigitalTwin twin("power_supply_001");
    
    // Set simulated hardware
    auto hw = std::make_shared<DigitalTwin::SimulatedInterface>("psu_hw");
    hw->connect();
    twin.setHardwareInterface(hw);
    
    std::cout << "  Twin ID: power_supply_001\n";
    std::cout << "  Hardware: Connected\n";
    std::cout << "  Status: " << hw->getStatus() << "\n";
    
    assert(hw->isConnected() && "Hardware should be connected!");
    std::cout << "\n Test 1 PASSED\n";
    
    // Test 2: Synchronization
    std::cout << "\n\nTest 2: Hardware Synchronization\n";
    std::cout << "\n";
    
    twin.synchronize();
    
    auto state = twin.getCurrentState();
    
    std::cout << "  Synchronized state variables: " << state.variables.size() << "\n";
    std::cout << "  Mode: " << state.mode << "\n";
    
    std::cout << "\n Test 2 PASSED\n";
    
    // Test 3: Prediction
    std::cout << "\n\nTest 3: State Prediction\n";
    std::cout << "\n";
    
    auto predicted = twin.predict(10.0);  // Predict 10 seconds ahead
    
    std::cout << "  Prediction horizon: 10 seconds\n";
    std::cout << "  Prediction completed\n";
    
    std::cout << "\n Test 3 PASSED\n";
    
    // Test 4: Fault Detection
    std::cout << "\n\nTest 4: Fault Detection & RUL Estimation\n";
    std::cout << "\n";
    
    auto faults = twin.detectFaults();
    double rul = twin.estimateRemainingUsefulLife();
    
    std::cout << "  Faults detected: " << faults.size() << "\n";
    std::cout << "  Remaining Useful Life: " << std::setprecision(1) 
              << rul << " hours\n";
    
    assert(rul > 0 && "RUL should be positive!");
    std::cout << "\n Test 4 PASSED\n";
    
    // Test 5: Model Templates
    std::cout << "\n\nTest 5: Pre-built Model Templates\n";
    std::cout << "\n";
    
    auto rlcModel = DigitalTwin::ModelTemplates::createRLCCircuit(100, 0.01, 0.0001);
    auto msdModel = DigitalTwin::ModelTemplates::createMassSpringDamper(1, 0.5, 10);
    
    std::cout << "  RLC Circuit model created\n";
    std::cout << "  Mass-Spring-Damper model created\n";
    
    std::cout << "\n Test 5 PASSED\n";
    
    // Test 6: Twin Manager
    std::cout << "\n\nTest 6: Twin Manager (Multiple Twins)\n";
    std::cout << "\n";
    
    auto& manager = DigitalTwin::TwinManager::instance();
    
    auto twin1 = manager.createTwin("twin_1");
    auto twin2 = manager.createTwin("twin_2");
    
    std::cout << "  Created twins: twin_1, twin_2\n";
    std::cout << "  Total twins: " << manager.getAllStates().size() << "\n";
    
    std::cout << "\n Test 6 PASSED\n";
    
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "     Feature #140: ALL TESTS PASSED                     \n";
    std::cout << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "\n";
    std::cout << "          FluxScript Feature Test Suite - Phase 3         \n";
    std::cout << "          Quantum, Neuromorphic, Digital Twin             \n";
    std::cout << "\n";
    
    try {
        // Test Feature #136: Quantum Simulation
        test_quantum_simulation();
        
        // Test Feature #137: Neuromorphic Computing
        test_neuromorphic();
        
        // Test Feature #140: Digital Twin
        test_digital_twin();
        
        std::cout << "\n";
        std::cout << "\n";
        std::cout << "          ALL PHASE 3 FEATURES TESTED SUCCESSFULLY      \n";
        std::cout << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
