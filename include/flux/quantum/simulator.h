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

#ifndef FLUX_QUANTUM_SIMULATOR_H
#define FLUX_QUANTUM_SIMULATOR_H

#include <string>
#include <vector>
#include <complex>
#include <map>
#include <memory>
#include <functional>

namespace Flux {
namespace Quantum {

// ============================================================================
// Quantum Data Structures
// ============================================================================

// Quantum state vector (2^n amplitudes for n qubits)
using StateVector = std::vector<std::complex<double>>;

// Density matrix for mixed states
using DensityMatrix = std::vector<std::vector<std::complex<double>>>;

// Quantum gate as unitary matrix
using QuantumGate = std::vector<std::vector<std::complex<double>>>;

// Measurement result
struct MeasurementResult {
    std::string outcome;      // e.g., "011" for 3-qubit measurement
    double probability;
    StateVector postState;
    
    MeasurementResult() : probability(0) {}
};

// Circuit operation
struct QuantumOperation {
    std::string name;
    std::vector<int> qubits;
    std::vector<double> params;  // For parameterized gates
    QuantumGate matrix;
    
    QuantumOperation() {}
};

// ============================================================================
// Quantum Gates
// ============================================================================

class QuantumGates {
public:
    // Single-qubit gates
    static QuantumGate I();   // Identity
    static QuantumGate X();   // Pauli-X (NOT)
    static QuantumGate Y();   // Pauli-Y
    static QuantumGate Z();   // Pauli-Z
    static QuantumGate H();   // Hadamard
    static QuantumGate S();   // S gate (sqrt(Z))
    static QuantumGate T();   // T gate (sqrt(S))
    static QuantumGate RX(double theta);  // Rotation X
    static QuantumGate RY(double theta);  // Rotation Y
    static QuantumGate RZ(double theta);  // Rotation Z
    
    // Two-qubit gates
    static QuantumGate CNOT();  // Controlled-NOT
    static QuantumGate CZ();    // Controlled-Z
    static QuantumGate SWAP();  // SWAP
    static QuantumGate iSWAP(); // iSWAP
    
    // Three-qubit gates
    static QuantumGate Toffoli();  // CCNOT
    static QuantumGate Fredkin();  // CSWAP
    
    // Multi-qubit gates
    static QuantumGate createControlled(const QuantumGate& gate, int controlQubits);
    
    // Gate utilities
    static QuantumGate tensorProduct(const QuantumGate& A, const QuantumGate& B);
    static QuantumGate applyToQubits(const QuantumGate& gate, int totalQubits, 
                                      const std::vector<int>& targetQubits);
};

// ============================================================================
// Quantum Circuit
// ============================================================================

class QuantumCircuit {
public:
    QuantumCircuit(int numQubits);
    ~QuantumCircuit();
    
    // Circuit construction
    void addGate(const std::string& name, const std::vector<int>& qubits,
                 const std::vector<double>& params = {});
    
    // Convenience methods
    void h(int qubit);
    void x(int qubit);
    void y(int qubit);
    void z(int qubit);
    void s(int qubit);
    void t(int qubit);
    void rx(int qubit, double theta);
    void ry(int qubit, double theta);
    void rz(int qubit, double theta);
    void cnot(int control, int target);
    void cz(int control, int target);
    void swap(int q1, int q2);
    void toffoli(int c1, int c2, int target);
    
    // Circuit info
    int numQubits() const { return m_numQubits; }
    int numGates() const { return m_operations.size(); }
    const std::vector<QuantumOperation>& operations() const { return m_operations; }
    
    // Export
    std::string toQASM() const;
    std::string toDiagram() const;
    
private:
    int m_numQubits;
    std::vector<QuantumOperation> m_operations;
};

// ============================================================================
// Quantum Simulator
// ============================================================================

class QuantumSimulator {
public:
    QuantumSimulator(int numQubits);
    ~QuantumSimulator();
    
    // State initialization
    void reset();
    void setState(const StateVector& state);
    StateVector getState() const { return m_state; }
    
    // Gate application
    void applyGate(const QuantumGate& gate, const std::vector<int>& qubits);
    void applyOperation(const QuantumOperation& op);
    
    // Circuit execution
    void execute(const QuantumCircuit& circuit);
    
    // Measurement
    MeasurementResult measure(int qubit);
    std::map<std::string, double> measureAll();
    std::string measureAndCollapse();
    
    // Expectation values
    double expectationValue(const QuantumGate& observable);
    
    // Fidelity
    double fidelity(const StateVector& targetState) const;
    
    // State info
    int numQubits() const { return m_numQubits; }
    double getProbability(const std::string& basisState) const;
    std::vector<double> getProbabilities() const;
    
    // Visualization
    std::string stateToText() const;
    void plotStateHistogram() const;
    
private:
    int m_numQubits;
    StateVector m_state;
    
    // Helper methods
    int basisStateToInt(const std::string& state) const;
    std::string intToBasisState(int index) const;
    void normalizeState();
};

// ============================================================================
// Quantum Algorithms
// ============================================================================

class QuantumAlgorithms {
public:
    // Basic algorithms
    static QuantumCircuit createBellState(int q0, int q1);
    static QuantumCircuit createGHZState(int numQubits);
    static QuantumCircuit quantumTeleportation();
    
    // Oracle-based algorithms
    static QuantumCircuit deutschJozsa(const std::function<int(int)>& oracle, int n);
    static QuantumCircuit groverSearch(const std::function<bool(int)>& oracle, int n);
    static QuantumCircuit simonAlgorithm(const std::function<int(int)>& oracle, int n);
    
    // Phase estimation
    static QuantumCircuit quantumPhaseEstimation(const QuantumGate& unitary, 
                                                  int precisionQubits);
    
    // Quantum Fourier Transform
    static QuantumCircuit qft(int numQubits);
    static QuantumCircuit iqft(int numQubits);
    
    // Variational algorithms
    static QuantumCircuit qaoa(const std::vector<double>& gamma, 
                               const std::vector<double>& beta,
                               int numQubits);
    static QuantumCircuit vqe(const std::vector<QuantumGate>& ansatz);
    
    // Error correction
    static QuantumCircuit threeQubitBitFlip();
    static QuantumCircuit shorCode();
};

// ============================================================================
// Quantum Chemistry
// ============================================================================

class QuantumChemistry {
public:
    // Molecular Hamiltonian
    struct MolecularHamiltonian {
        std::vector<std::vector<double>> oneBody;
        std::vector<std::vector<std::vector<std::vector<double>>>> twoBody;
        double nuclearRepulsion;
    };
    
    // Convert molecular Hamiltonian to qubit Hamiltonian
    static std::map<std::string, double> jordanWignerTransform(
        const MolecularHamiltonian& hamiltonian);
    
    // VQE for molecular ground state
    static double vqeGroundState(const MolecularHamiltonian& hamiltonian,
                                  int numQubits, int maxIterations);
    
    // Create ansatz circuits
    static QuantumCircuit uccsdAnsatz(int numQubits, const std::vector<double>& params);
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Circuit creation
    void* flux_quantum_circuit_create(int numQubits);
    void flux_quantum_circuit_destroy(void* circuit);
    void flux_quantum_h(void* circuit, int qubit);
    void flux_quantum_x(void* circuit, int qubit);
    void flux_quantum_cnot(void* circuit, int control, int target);
    void flux_quantum_toffoli(void* circuit, int c1, int c2, int target);
    
    // Simulation
    void* flux_quantum_simulator_create(int numQubits);
    void flux_quantum_simulator_destroy(void* sim);
    void flux_quantum_execute(void* sim, void* circuit);
    const char* flux_quantum_measure(void* sim);
    
    // Algorithms
    void* flux_quantum_bell_state();
    void* flux_quantum_ghz_state(int numQubits);
    void* flux_quantum_qft(int numQubits);
    void* flux_quantum_grover(int n);
}

} // namespace Quantum
} // namespace Flux

#endif // FLUX_QUANTUM_SIMULATOR_H
