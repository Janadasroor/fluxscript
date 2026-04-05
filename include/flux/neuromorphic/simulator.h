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

#ifndef FLUX_NEUROMORPHIC_SIMULATOR_H
#define FLUX_NEUROMORPHIC_SIMULATOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <random>

namespace Flux {
namespace Neuromorphic {

// ============================================================================
// Neuron Models
// ============================================================================

// Base neuron state
struct NeuronState {
    double membrane_potential;  // Vm (mV)
    double recovery_variable;    // For adaptive models
    double conductance;          // Synaptic conductance
    double last_spike_time;      // Time of last spike (ms)
    int spike_count;             // Total spikes
    
    NeuronState() : membrane_potential(-70), recovery_variable(0),
                    conductance(0), last_spike_time(-1), spike_count(0) {}
};

// Leaky Integrate-and-Fire (LIF) neuron parameters
struct LIFParameters {
    double tau_m;      // Membrane time constant (ms)
    double tau_ref;    // Refractory period (ms)
    double v_rest;     // Resting potential (mV)
    double v_thresh;   // Threshold potential (mV)
    double v_reset;    // Reset potential (mV)
    double i_bias;     // Bias current (nA)
    
    LIFParameters() : tau_m(20), tau_ref(2), v_rest(-70), 
                      v_thresh(-55), v_reset(-75), i_bias(0) {}
};

// Hodgkin-Huxley neuron parameters
struct HHParameters {
    double C_m;        // Membrane capacitance (μF/cm²)
    double g_Na;       // Sodium conductance (mS/cm²)
    double g_K;        // Potassium conductance (mS/cm²)
    double g_L;        // Leak conductance (mS/cm²)
    double E_Na;       // Sodium reversal potential (mV)
    double E_K;        // Potassium reversal potential (mV)
    double E_L;        // Leak reversal potential (mV)
    
    HHParameters() : C_m(1.0), g_Na(120), g_K(36), g_L(0.3),
                     E_Na(50), E_K(-77), E_L(-54.4) {}
};

// Izhikevich neuron parameters
struct IzhikevichParameters {
    double a, b, c, d;  // Model parameters
    double v_rest;
    double v_thresh;
    
    IzhikevichParameters() : a(0.02), b(0.2), c(-65), d(8),
                              v_rest(-65), v_thresh(30) {}
    
    // Preset configurations
    static IzhikevichParameters regularSpiking();
    static IzhikevichParameters fastSpiking();
    static IzhikevichParameters thalamocortical();
    static IzhikevichParameters corticalBurst();
};

// ============================================================================
// Synapse Models
// ============================================================================

struct SynapseState {
    double conductance;    // Synaptic conductance (nS)
    double reversal_potential;  // E_syn (mV)
    double last_update;    // Last update time
    
    SynapseState() : conductance(0), reversal_potential(0), last_update(0) {}
};

struct SynapseParameters {
    double tau_rise;     // Rise time constant (ms)
    double tau_decay;    // Decay time constant (ms)
    double g_max;        // Maximum conductance (nS)
    double u;            // Release probability
    double tau_facil;    // Facilitation time constant (ms)
    double tau_recov;    // Recovery time constant (ms)
    
    SynapseParameters() : tau_rise(0.5), tau_decay(3.0), g_max(1.0),
                           u(0.5), tau_facil(0), tau_recov(800) {}
};

// Plasticity rules
enum class PlasticityRule {
    None,
    STDP,      // Spike-timing dependent plasticity
    Hebbian,   // Classic Hebbian
    AntiHebbian,
    Homeostatic
};

struct PlasticityParameters {
    PlasticityRule rule;
    double tau_plus;   // STDP: LTP time constant (ms)
    double tau_minus;  // STDP: LTD time constant (ms)
    double A_plus;     // LTP amplitude
    double A_minus;    // LTD amplitude
    double eta;        // Learning rate
    
    PlasticityParameters() : rule(PlasticityRule::None),
                              tau_plus(20), tau_minus(20),
                              A_plus(0.005), A_minus(0.0052), eta(0.01) {}
};

// ============================================================================
// Neuron Class
// ============================================================================

class Neuron {
public:
    Neuron(int id, const std::string& type = "LIF");
    ~Neuron();
    
    // Configuration
    void setLIFParameters(const LIFParameters& params);
    void setHHParameters(const HHParameters& params);
    void setIzhikevichParameters(const IzhikevichParameters& params);
    
    // Simulation
    void update(double dt, double input_current);
    void reset();
    
    // Spike handling
    bool hasSpiked() const { return m_spiked; }
    void resetSpikeFlag() { m_spiked = false; }
    double getLastSpikeTime() const { return m_state.last_spike_time; }
    int getSpikeCount() const { return m_state.spike_count; }
    
    // State access
    int id() const { return m_id; }
    double getMembranePotential() const { return m_state.membrane_potential; }
    const NeuronState& state() const { return m_state; }
    const std::string& type() const { return m_type; }
    
    // Input current
    void addInputCurrent(double current) { m_inputCurrent += current; }
    void clearInputCurrent() { m_inputCurrent = 0; }
    
private:
    int m_id;
    std::string m_type;
    NeuronState m_state;
    double m_inputCurrent;
    bool m_spiked;
    double m_refractoryTime;
    
    // Model-specific state
    double m_m;  // HH: Na activation
    double m_h;  // HH: Na inactivation
    double m_n;  // HH: K activation
    
    // Update methods
    void updateLIF(double dt);
    void updateHH(double dt);
    void updateIzhikevich(double dt);
};

// ============================================================================
// Synapse Class
// ============================================================================

class Synapse {
public:
    Synapse(int preId, int postId, const std::string& type = "excitatory");
    ~Synapse();
    
    // Configuration
    void setParameters(const SynapseParameters& params);
    void setPlasticity(const PlasticityParameters& plasticity);
    
    // Spike handling
    void onPreSpike(double time);
    void onPostSpike(double time);
    
    // Update
    void update(double dt);
    
    // Current calculation
    double getCurrent(double postVm) const;
    
    // Access
    int preId() const { return m_preId; }
    int postId() const { return m_postId; }
    double getWeight() const { return m_params.g_max; }
    void setWeight(double weight) { m_params.g_max = weight; }
    
    // Plasticity
    void applySTDP(double preTime, double postTime);
    
private:
    int m_preId;
    int m_postId;
    std::string m_type;
    SynapseParameters m_params;
    PlasticityParameters m_plasticity;
    SynapseState m_state;
    
    // STDP traces
    double m_preTrace;
    double m_postTrace;
};

// ============================================================================
// Neural Network
// ============================================================================

class NeuralNetwork {
public:
    NeuralNetwork();
    ~NeuralNetwork();
    
    // Network construction
    int addNeuron(const std::string& type = "LIF");
    void addSynapse(int preId, int postId, const std::string& type = "excitatory");
    
    // Network topology
    void connectRandom(double probability, double weight = 1.0);
    void connectAllToAll(double weight = 1.0);
    void connectSmallWorld(int k, double rewiringProb);
    void connectScaleFree(int m0, int m);
    
    // Input/Output
    void setInputNeurons(const std::vector<int>& neuronIds);
    void setOutputNeurons(const std::vector<int>& neuronIds);
    
    // Stimulation
    void injectCurrent(int neuronId, double current);
    void stimulateInput(const std::vector<double>& pattern);
    
    // Simulation
    void initialize();
    void step(double dt);
    void run(double duration, double dt);
    
    // Recording
    struct Recording {
        std::vector<double> time;
        std::vector<double> voltage;
        std::vector<double> spike_times;
    };
    
    void startRecording(int neuronId);
    void stopRecording();
    const Recording& getRecording(int neuronId) const;
    
    // Analysis
    std::vector<double> getFiringRates() const;
    std::vector<std::vector<double>> getSpikeTrain() const;
    double getMeanFiringRate() const;
    
    // Network info
    int numNeurons() const { return m_neurons.size(); }
    int numSynapses() const { return m_synapses.size(); }
    Neuron* getNeuron(int id) { return m_neurons[id].get(); }
    
    // Plasticity
    void enablePlasticity(bool enabled) { m_plasticityEnabled = enabled; }
    
private:
    std::vector<std::unique_ptr<Neuron>> m_neurons;
    std::vector<std::unique_ptr<Synapse>> m_synapses;
    std::map<int, Recording> m_recordings;
    std::vector<int> m_inputNeurons;
    std::vector<int> m_outputNeurons;
    bool m_plasticityEnabled;
    double m_currentTime;
    
    std::mt19937 m_rng;
};

// ============================================================================
// Spike-Timing Dependent Plasticity (STDP)
// ============================================================================

class STDPLearner {
public:
    STDPLearner();
    ~STDPLearner();
    
    // Configuration
    void setParameters(double tauPlus, double tauMinus,
                       double APlus, double AMinus);
    
    // Weight update
    double computeWeightChange(double preTime, double postTime,
                                double currentWeight);
    
    // Batch update
    void applyToSynapse(Synapse& synapse, double preTime, double postTime);
    
private:
    double m_tauPlus;
    double m_tauMinus;
    double m_APlus;
    double m_AMinus;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Network creation
    void* flux_nn_create();
    void flux_nn_destroy(void* network);
    int flux_nn_add_neuron(void* network, const char* type);
    void flux_nn_connect(void* network, int pre, int post);
    
    // Simulation
    void flux_nn_initialize(void* network);
    void flux_nn_step(void* network, double dt);
    void flux_nn_run(void* network, double duration, double dt);
    
    // Recording
    double flux_nn_get_voltage(void* network, int neuronId);
    int flux_nn_get_spike_count(void* network, int neuronId);
    
    // Neuron parameters
    void flux_nn_set_lif_params(void* network, int neuronId,
                                 double tau_m, double v_thresh, double v_reset);
}

} // namespace Neuromorphic
} // namespace Flux

#endif // FLUX_NEUROMORPHIC_SIMULATOR_H
