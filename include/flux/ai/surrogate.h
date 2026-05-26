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

#ifndef FLUX_AI_SURROGATE_H
#define FLUX_AI_SURROGATE_H

#include <Eigen/Dense>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {
namespace AI {

struct NetworkArchitecture
{
    std::vector<int> layers;
    int epochs = 100;
    double learningRate = 0.01;
    std::string activation = "relu";
    std::string outputActivation = "linear";
};

struct TrainingData
{
    std::vector<std::vector<double>> inputs;
    std::vector<std::vector<double>> outputs;
    size_t numSamples() const { return inputs.size(); }
};

struct TrainingResult
{
    bool success = false;
    std::string message;
    std::vector<double> lossHistory;
    size_t epochsTrained = 0;
    double finalLoss = 0.0;
};

struct PredictionResult
{
    std::vector<double> prediction;
    double inferenceTime = 0.0;
};

typedef std::function<void(size_t, double)> LossCallback;

class NeuralNetworkSurrogate
{
public:
    NeuralNetworkSurrogate();
    explicit NeuralNetworkSurrogate(const NetworkArchitecture& arch);
    ~NeuralNetworkSurrogate();

    void setArchitecture(const NetworkArchitecture& arch);
    void setEpochs(int epochs) { m_arch.epochs = epochs; }
    void setSeed(unsigned seed) { m_seed = seed; }

    TrainingResult train(const TrainingData& data);
    TrainingResult train(const TrainingData& trainData, const TrainingData& valData);
    TrainingResult train(const TrainingData& data, LossCallback callback);

    PredictionResult predict(const std::vector<double>& input) const;
    std::vector<PredictionResult> predictBatch(const std::vector<std::vector<double>>& inputs) const;

    double calculateRMSE(const TrainingData& testData) const;
    double calculateMAE(const TrainingData& testData) const;
    double calculateR2(const TrainingData& testData) const;

    void save(const std::string& path) const;
    void load(const std::string& path);
    std::string toONNX() const;
    size_t numParameters() const;
    size_t memorySize() const;
    std::string getSummary() const;

    double computeLoss(const TrainingData& data) const;

private:
    void initializeWeights();
    Eigen::VectorXd forward(const Eigen::VectorXd& input) const;
    void backward(const Eigen::VectorXd& input, const Eigen::VectorXd& target);

    double relu(double x) const;
    double relu_derivative(double x) const;
    double tanh(double x) const;
    double tanh_derivative(double x) const;
    double sigmoid(double x) const;
    double sigmoid_derivative(double x) const;
    double linear(double x) const;

    NetworkArchitecture m_arch;
    bool m_trained;
    unsigned m_seed = 0;
    std::vector<Eigen::MatrixXd> m_weights;
    std::vector<Eigen::VectorXd> m_biases;
    mutable std::vector<Eigen::VectorXd> m_activations;
    std::vector<Eigen::VectorXd> m_deltas;
};

class SurrogateFactory
{
public:
    static SurrogateFactory& instance();
    static NetworkArchitecture suggestArchitecture(size_t inputDim, size_t outputDim, size_t numSamples);
    std::shared_ptr<NeuralNetworkSurrogate> createForCircuitSimulation();
    std::shared_ptr<NeuralNetworkSurrogate> createForEMAnalysis();
    std::shared_ptr<NeuralNetworkSurrogate> createForThermalAnalysis();
    static std::shared_ptr<NeuralNetworkSurrogate> loadPretrained(const std::string& name);
    std::vector<std::string> listPretrainedModels() const;
};

typedef std::vector<double> (*SimulationFunction)(const std::vector<double>&);

class TrainingDataGenerator
{
public:
    TrainingDataGenerator(SimulationFunction simFunc);
    ~TrainingDataGenerator();
    TrainingData generateRandomSamples(size_t numSamples,
                                       const std::map<std::string, std::pair<double, double>>& paramRanges);
    TrainingData generateLatinHypercube(size_t numSamples,
                                        const std::map<std::string, std::pair<double, double>>& paramRanges);
    TrainingData generateFullFactorial(const std::map<std::string, std::vector<double>>& paramValues);
    TrainingData adaptiveSampling(NeuralNetworkSurrogate& surrogate, size_t numAdditionalSamples,
                                  const std::map<std::string, std::pair<double, double>>& paramRanges);
    void normalize(TrainingData& data);
    void denormalize(TrainingData& data);
    TrainingData splitTrainVal(const TrainingData& data, double valFraction);

private:
    SimulationFunction m_simFunc;
};

class EnsembleModel
{
public:
    EnsembleModel();
    ~EnsembleModel();
    void addModel(std::shared_ptr<NeuralNetworkSurrogate> model, double weight = 1.0);
    PredictionResult predict(const std::vector<double>& input) const;
    static std::shared_ptr<EnsembleModel> selectBest(const std::vector<std::shared_ptr<NeuralNetworkSurrogate>>& models,
                                                     const TrainingData& testData);
};

// C interface
extern "C" {
void* flux_nn_create(const int* layers, int numLayers);
void flux_nn_destroy(void* nn);
void flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inputDim, int outputDim,
                   int epochs);
void flux_nn_predict(void* nn, const double* input, double* output, int inputDim, int outputDim);
void flux_nn_save(void* nn, const char* filename);
void* flux_nn_load(const char* filename);
void* flux_surrogate_create();
void flux_surrogate_train(void* surrogate, const char* circuit_type, const double* params, const double* results,
                          int numSamples);
void flux_surrogate_predict(void* surrogate, const double* params, double* results, int numParams);
}

} // namespace AI
} // namespace Flux

#endif // FLUX_AI_SURROGATE_H
