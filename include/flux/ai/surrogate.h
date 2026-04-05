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

// FluxScript AI/ML Toolbox - Neural Network Surrogate
// Feature: Train neural networks to approximate expensive simulations

#ifndef FLUX_AI_SURROGATE_H
#define FLUX_AI_SURROGATE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace Flux {
namespace AI {

// ============================================================================
// Neural Network Data Structures
// ============================================================================

struct NetworkArchitecture {
    std::vector<int> layers;          // neurons per layer
    std::string activation;           // "relu", "tanh", "sigmoid"
    std::string outputActivation;     // for output layer
    double learningRate;
    int batchSize;
    int epochs;
    double regularization;
    
    NetworkArchitecture() : activation("relu"), outputActivation("linear"),
                            learningRate(0.001), batchSize(32), epochs(100),
                            regularization(0.0001) {}
};

struct TrainingData {
    std::vector<std::vector<double>> inputs;   // N x D matrix
    std::vector<std::vector<double>> outputs;  // N x K matrix
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    
    size_t numSamples() const { return inputs.size(); }
    size_t inputDim() const { return inputs.empty() ? 0 : inputs[0].size(); }
    size_t outputDim() const { return outputs.empty() ? 0 : outputs[0].size(); }
};

struct TrainingResult {
    bool success;
    int epochsTrained;
    double finalLoss;
    double validationLoss;
    std::vector<double> lossHistory;
    std::string message;
    
    TrainingResult() : success(false), epochsTrained(0), 
                       finalLoss(0), validationLoss(0) {}
};

struct PredictionResult {
    std::vector<double> prediction;
    std::vector<double> uncertainty;  // if available
    double inferenceTime;  // ms
    
    PredictionResult() : inferenceTime(0) {}
};

// ============================================================================
// Neural Network Surrogate Model
// ============================================================================

class NeuralNetworkSurrogate {
public:
    NeuralNetworkSurrogate();
    explicit NeuralNetworkSurrogate(const NetworkArchitecture& arch);
    ~NeuralNetworkSurrogate();
    
    // Architecture
    void setArchitecture(const NetworkArchitecture& arch);
    const NetworkArchitecture& architecture() const { return m_arch; }
    
    // Training
    TrainingResult train(const TrainingData& data);
    TrainingResult train(const TrainingData& trainData, const TrainingData& valData);
    
    // Training with callback
    using LossCallback = std::function<void(int epoch, double loss)>;
    TrainingResult train(const TrainingData& data, LossCallback callback);
    
    // Prediction
    PredictionResult predict(const std::vector<double>& input) const;
    std::vector<PredictionResult> predictBatch(
        const std::vector<std::vector<double>>& inputs) const;
    
    // Performance metrics
    double calculateRMSE(const TrainingData& testData) const;
    double calculateMAE(const TrainingData& testData) const;
    double calculateR2(const TrainingData& testData) const;
    
    // Export/Import
    void save(const std::string& filename) const;
    void load(const std::string& filename);
    std::string toONNX() const;
    
    // Model info
    size_t numParameters() const;
    size_t memorySize() const;
    
    // Validation
    bool isValid() const { return m_trained; }
    std::string getSummary() const;
    
private:
    NetworkArchitecture m_arch;
    bool m_trained;
    
    // Model weights (simplified representation)
    std::vector<std::vector<double>> m_weights;
    std::vector<std::vector<double>> m_biases;
    
    // Training state
    std::vector<double> m_inputMean;
    std::vector<double> m_inputStd;
    std::vector<double> m_outputMean;
    std::vector<double> m_outputStd;
    
    // Internal methods
    void initializeWeights();
    std::vector<double> forward(const std::vector<double>& input) const;
    void backward(const std::vector<double>& input, 
                  const std::vector<double>& target);
    double computeLoss(const TrainingData& data) const;
    
    // Activation functions
    static double relu(double x);
    static double tanh(double x);
    static double sigmoid(double x);
    static double linear(double x);
};

// ============================================================================
// Surrogate Model Factory
// ============================================================================

class SurrogateFactory {
public:
    static SurrogateFactory& instance();
    
    // Create surrogate for specific use case
    std::shared_ptr<NeuralNetworkSurrogate> createForCircuitSimulation();
    std::shared_ptr<NeuralNetworkSurrogate> createForEMAnalysis();
    std::shared_ptr<NeuralNetworkSurrogate> createForThermalAnalysis();
    
    // Auto-architecture search
    NetworkArchitecture suggestArchitecture(size_t inputDim, size_t outputDim,
                                             size_t numSamples);
    
    // Pre-trained models
    std::shared_ptr<NeuralNetworkSurrogate> loadPretrained(const std::string& name);
    std::vector<std::string> listPretrainedModels() const;
    
private:
    SurrogateFactory() = default;
    std::map<std::string, std::shared_ptr<NeuralNetworkSurrogate>> m_pretrained;
};

// ============================================================================
// Training Data Generator
// ============================================================================

class TrainingDataGenerator {
public:
    using SimulationFunction = std::function<std::vector<double>(
        const std::vector<double>& params)>;
    
    TrainingDataGenerator(SimulationFunction simFunc);
    ~TrainingDataGenerator();
    
    // Generate training data
    TrainingData generateRandomSamples(size_t numSamples,
                                        const std::map<std::string, std::pair<double, double>>& paramRanges);
    
    // Design of experiments
    TrainingData generateLatinHypercube(size_t numSamples,
                                         const std::map<std::string, std::pair<double, double>>& paramRanges);
    TrainingData generateFullFactorial(
        const std::map<std::string, std::vector<double>>& paramValues);
    
    // Adaptive sampling
    TrainingData adaptiveSampling(NeuralNetworkSurrogate& surrogate,
                                   size_t numAdditionalSamples,
                                   const std::map<std::string, std::pair<double, double>>& paramRanges);
    
    // Data utilities
    void normalize(TrainingData& data);
    void denormalize(TrainingData& data);
    TrainingData splitTrainVal(const TrainingData& data, double valFraction = 0.2);
    
private:
    SimulationFunction m_simFunc;
};

// ============================================================================
// Model Comparison and Ensemble
// ============================================================================

struct ModelComparison {
    std::string modelName;
    double rmse;
    double mae;
    double r2;
    double inferenceTime;  // ms
    size_t numParameters;
};

class EnsembleModel {
public:
    EnsembleModel();
    ~EnsembleModel();
    
    // Add models to ensemble
    void addModel(std::shared_ptr<NeuralNetworkSurrogate> model, double weight = 1.0);
    
    // Prediction with uncertainty
    PredictionResult predict(const std::vector<double>& input) const;
    
    // Model selection
    static std::shared_ptr<EnsembleModel> selectBest(
        const std::vector<std::shared_ptr<NeuralNetworkSurrogate>>& models,
        const TrainingData& testData);
    
private:
    std::vector<std::pair<std::shared_ptr<NeuralNetworkSurrogate>, double>> m_models;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Neural network creation
    void* flux_nn_create(const int* layers, int numLayers);
    void flux_nn_destroy(void* nn);
    
    // Training
    void flux_nn_train(void* nn, const double* inputs, const double* outputs, 
                       int numSamples, int inputDim, int outputDim, int epochs);
    
    // Prediction
    void flux_nn_predict(void* nn, const double* input, double* output, 
                         int inputDim, int outputDim);
    
    // Save/Load
    void flux_nn_save(void* nn, const char* filename);
    void* flux_nn_load(const char* filename);
    
    // Surrogate for circuit simulation
    void* flux_surrogate_create();
    void flux_surrogate_train(void* surrogate, const char* circuit_type,
                               const double* params, const double* results,
                               int numSamples);
    void flux_surrogate_predict(void* surrogate, const double* params, 
                                 double* results, int numParams);
}

} // namespace AI
} // namespace Flux

#endif // FLUX_AI_SURROGATE_H
