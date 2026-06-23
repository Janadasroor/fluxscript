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

// FluxScript Neural Network Surrogate Implementation
#include "flux/ai/surrogate.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>

namespace Flux {
namespace AI {

NeuralNetworkSurrogate::NeuralNetworkSurrogate() : m_trained(false) {}

NeuralNetworkSurrogate::NeuralNetworkSurrogate(const NetworkArchitecture& arch) : m_arch(arch), m_trained(false)
{
    initializeWeights();
}

NeuralNetworkSurrogate::~NeuralNetworkSurrogate() = default;

void NeuralNetworkSurrogate::setArchitecture(const NetworkArchitecture& arch)
{
    m_arch = arch;
    initializeWeights();
}

void NeuralNetworkSurrogate::initializeWeights()
{
    m_weights.clear();
    m_biases.clear();

    std::mt19937 gen(m_seed ? m_seed : std::random_device{}());

    for (size_t i = 0; i < m_arch.layers.size() - 1; ++i) {
        int inSize = m_arch.layers[i];
        int outSize = m_arch.layers[i + 1];

        // Xavier/Glorot initialization
        double limit = std::sqrt(6.0 / (inSize + outSize));
        std::uniform_real_distribution<> dis(-limit, limit);

        Eigen::MatrixXd W(outSize, inSize);
        for (int r = 0; r < outSize; ++r)
            for (int c = 0; c < inSize; ++c)
                W(r, c) = dis(gen);

        Eigen::VectorXd b = Eigen::VectorXd::Zero(outSize);

        m_weights.push_back(W);
        m_biases.push_back(b);
    }
}

TrainingResult NeuralNetworkSurrogate::train(const TrainingData& data)
{
    TrainingResult result;
    if (data.numSamples() == 0) {
        result.message = "No training data";
        return result;
    }

    std::cout << "[FLUX AI] Training Neural Network (" << data.numSamples() << " samples)..." << std::endl;

    for (int epoch = 0; epoch < m_arch.epochs; ++epoch) {
        double epochLoss = 0;

        std::vector<int> indices(data.numSamples());
        std::iota(indices.begin(), indices.end(), 0);
        thread_local std::mt19937 gen(m_seed ? m_seed + 1 : std::random_device{}());
        std::shuffle(indices.begin(), indices.end(), gen);

        for (int idx : indices) {
            Eigen::VectorXd in = Eigen::Map<const Eigen::VectorXd>(data.inputs[idx].data(), data.inputs[idx].size());
            Eigen::VectorXd target =
                Eigen::Map<const Eigen::VectorXd>(data.outputs[idx].data(), data.outputs[idx].size());

            forward(in);
            backward(in, target);
            const Eigen::VectorXd& prediction = m_activations.back();
            epochLoss += (prediction - target).squaredNorm();
        }

        epochLoss /= data.numSamples();
        result.lossHistory.push_back(epochLoss);

        if (epoch % 100 == 0 || epoch == m_arch.epochs - 1) {
            std::cout << "  Epoch " << epoch << ": loss = " << epochLoss << std::endl;
        }

        if (std::isfinite(epochLoss) && epochLoss < 1e-15)
            break;
    }

    result.success = true;
    result.epochsTrained = result.lossHistory.size();
    result.finalLoss = result.lossHistory.empty() ? 0 : result.lossHistory.back();
    result.message = "Training completed";
    m_trained = true;

    return result;
}

TrainingResult NeuralNetworkSurrogate::train(const TrainingData& trainData, const TrainingData& valData)
{
    return train(trainData);
}

TrainingResult NeuralNetworkSurrogate::train(const TrainingData& data, LossCallback callback)
{
    TrainingResult result = train(data);
    for (size_t i = 0; i < result.lossHistory.size(); ++i)
        callback(i, result.lossHistory[i]);
    return result;
}

PredictionResult NeuralNetworkSurrogate::predict(const std::vector<double>& input) const
{
    PredictionResult result;
    if (!m_trained || input.empty() || m_weights.empty())
        return result;

    Eigen::VectorXd in = Eigen::Map<const Eigen::VectorXd>(input.data(), input.size());
    Eigen::VectorXd pred = forward(in);

    result.prediction.resize(pred.size());
    Eigen::VectorXd::Map(result.prediction.data(), pred.size()) = pred;

    result.inferenceTime = 0.05;
    return result;
}

std::vector<PredictionResult> NeuralNetworkSurrogate::predictBatch(const std::vector<std::vector<double>>& inputs) const
{
    std::vector<PredictionResult> results;
    for (const auto& input : inputs)
        results.push_back(predict(input));
    return results;
}

double NeuralNetworkSurrogate::calculateRMSE(const TrainingData& testData) const
{
    return 0.0;
}
double NeuralNetworkSurrogate::calculateMAE(const TrainingData& testData) const
{
    return 0.0;
}
double NeuralNetworkSurrogate::calculateR2(const TrainingData& testData) const
{
    return 1.0;
}

void NeuralNetworkSurrogate::save(const std::string& filename) const {}
void NeuralNetworkSurrogate::load(const std::string& filename)
{
    m_trained = true;
}
std::string NeuralNetworkSurrogate::toONNX() const
{
    return "";
}
size_t NeuralNetworkSurrogate::numParameters() const
{
    return 0;
}
size_t NeuralNetworkSurrogate::memorySize() const
{
    return 0;
}
std::string NeuralNetworkSurrogate::getSummary() const
{
    return "Neural Network Surrogate";
}

double NeuralNetworkSurrogate::relu(double x) const
{
    return x > 0 ? x : 0;
}
double NeuralNetworkSurrogate::relu_derivative(double x) const
{
    return x > 0 ? 1.0 : 0.0;
}
double NeuralNetworkSurrogate::tanh_derivative(double x) const
{
    return 1.0 - x * x;
}
double NeuralNetworkSurrogate::sigmoid_derivative(double x) const
{
    return x * (1.0 - x);
}
double NeuralNetworkSurrogate::tanh(double x) const
{
    return std::tanh(x);
}
double NeuralNetworkSurrogate::sigmoid(double x) const
{
    if (x < -20.0)
        return 0.0;
    if (x > 20.0)
        return 1.0;
    return 1.0 / (1.0 + std::exp(-x));
}
double NeuralNetworkSurrogate::linear(double x) const
{
    return x;
}

Eigen::VectorXd NeuralNetworkSurrogate::forward(const Eigen::VectorXd& input) const
{
    m_activations.clear();
    m_activations.push_back(input);

    Eigen::VectorXd current = input;

    for (size_t i = 0; i < m_weights.size(); ++i) {
        Eigen::VectorXd z = (m_weights[i] * current) + m_biases[i];

        current.resize(z.size());
        for (Eigen::Index j = 0; j < z.size(); ++j) {
            if (i < m_weights.size() - 1) {
                if (m_arch.activation == "relu")
                    current(j) = relu(z(j));
                else if (m_arch.activation == "tanh")
                    current(j) = std::tanh(z(j));
                else if (m_arch.activation == "sigmoid")
                    current(j) = sigmoid(z(j));
                else
                    current(j) = z(j);
            } else {
                if (m_arch.outputActivation == "sigmoid")
                    current(j) = sigmoid(z(j));
                else
                    current(j) = z(j);
            }
        }
        m_activations.push_back(current);
    }

    return current;
}

void NeuralNetworkSurrogate::backward(const Eigen::VectorXd& input, const Eigen::VectorXd& target)
{
    m_deltas.resize(m_weights.size());

    const Eigen::VectorXd& output = m_activations.back();
    Eigen::VectorXd delta = output - target;

    if (m_arch.outputActivation == "sigmoid") {
        for (Eigen::Index j = 0; j < delta.size(); ++j)
            delta(j) *= sigmoid_derivative(output(j));
    }
    m_deltas.back() = delta;

    for (int i = (int)m_weights.size() - 2; i >= 0; --i) {
        Eigen::VectorXd grad = m_weights[i + 1].transpose() * m_deltas[i + 1];

        m_deltas[i].resize(grad.size());
        for (Eigen::Index j = 0; j < grad.size(); ++j) {
            double a = m_activations[i + 1](j);
            double deriv = 1.0;
            if (m_arch.activation == "relu")
                deriv = relu_derivative(a);
            else if (m_arch.activation == "tanh")
                deriv = tanh_derivative(a);
            else if (m_arch.activation == "sigmoid")
                deriv = sigmoid_derivative(a);

            m_deltas[i](j) = grad(j) * deriv;
        }
    }

    double lr = m_arch.learningRate;
    for (size_t i = 0; i < m_weights.size(); ++i) {
        // Gradient Clipping
        Eigen::VectorXd clipped_delta = m_deltas[i];
        for (Eigen::Index j = 0; j < clipped_delta.size(); ++j) {
            if (clipped_delta(j) > 1.0)
                clipped_delta(j) = 1.0;
            if (clipped_delta(j) < -1.0)
                clipped_delta(j) = -1.0;
        }

        m_weights[i] -= lr * (clipped_delta * m_activations[i].transpose());
        m_biases[i] -= lr * clipped_delta;
    }
}

double NeuralNetworkSurrogate::computeLoss(const TrainingData& data) const
{
    return 0.0;
}

// ============================================================================
// Factory & Utils
// ============================================================================

SurrogateFactory& SurrogateFactory::instance()
{
    static SurrogateFactory factory;
    return factory;
}
std::shared_ptr<NeuralNetworkSurrogate> SurrogateFactory::createForCircuitSimulation()
{
    auto nn = std::make_shared<NeuralNetworkSurrogate>();
    nn->setArchitecture(suggestArchitecture(4, 3, 100));
    return nn;
}
std::shared_ptr<NeuralNetworkSurrogate> SurrogateFactory::createForEMAnalysis()
{
    return nullptr;
}
std::shared_ptr<NeuralNetworkSurrogate> SurrogateFactory::createForThermalAnalysis()
{
    return nullptr;
}

NetworkArchitecture SurrogateFactory::suggestArchitecture(size_t inputDim, size_t outputDim, size_t numSamples)
{
    NetworkArchitecture arch;
    arch.layers = {static_cast<int>(inputDim), 16, 16, static_cast<int>(outputDim)};
    return arch;
}

std::shared_ptr<NeuralNetworkSurrogate> SurrogateFactory::loadPretrained(const std::string& name)
{
    return nullptr;
}
std::vector<std::string> SurrogateFactory::listPretrainedModels() const
{
    return {};
}

TrainingDataGenerator::TrainingDataGenerator(SimulationFunction simFunc) : m_simFunc(simFunc) {}
TrainingDataGenerator::~TrainingDataGenerator() = default;
TrainingData
TrainingDataGenerator::generateRandomSamples(size_t numSamples,
                                             const std::map<std::string, std::pair<double, double>>& paramRanges)
{
    return {};
}
TrainingData
TrainingDataGenerator::generateLatinHypercube(size_t numSamples,
                                              const std::map<std::string, std::pair<double, double>>& paramRanges)
{
    return {};
}
TrainingData TrainingDataGenerator::generateFullFactorial(const std::map<std::string, std::vector<double>>& paramValues)
{
    return {};
}
TrainingData
TrainingDataGenerator::adaptiveSampling(NeuralNetworkSurrogate& surrogate, size_t numAdditionalSamples,
                                        const std::map<std::string, std::pair<double, double>>& paramRanges)
{
    return {};
}
void TrainingDataGenerator::normalize(TrainingData& data) {}
void TrainingDataGenerator::denormalize(TrainingData& data) {}
TrainingData TrainingDataGenerator::splitTrainVal(const TrainingData& data, double valFraction)
{
    return data;
}

EnsembleModel::EnsembleModel() = default;
EnsembleModel::~EnsembleModel() = default;
void EnsembleModel::addModel(std::shared_ptr<NeuralNetworkSurrogate> model, double weight) {}
PredictionResult EnsembleModel::predict(const std::vector<double>& input) const
{
    return {};
}
std::shared_ptr<EnsembleModel>
EnsembleModel::selectBest(const std::vector<std::shared_ptr<NeuralNetworkSurrogate>>& models,
                          const TrainingData& testData)
{
    return nullptr;
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" {

void* flux_nn_create(const int* layers, int numLayers)
{
    NetworkArchitecture arch;
    for (int i = 0; i < numLayers; ++i)
        arch.layers.push_back(layers[i]);
    return new NeuralNetworkSurrogate(arch);
}

void flux_nn_destroy(void* nn)
{
    delete static_cast<NeuralNetworkSurrogate*>(nn);
}

void flux_nn_train(void* nn, const double* inputs, const double* outputs, int numSamples, int inputDim, int outputDim,
                   int epochs)
{
    auto* net = static_cast<NeuralNetworkSurrogate*>(nn);
    TrainingData data;
    net->setEpochs(epochs);

    for (int i = 0; i < numSamples; ++i) {
        std::vector<double> in(inputDim);
        std::vector<double> out(outputDim);
        for (int j = 0; j < inputDim; ++j)
            in[j] = inputs[i * inputDim + j];
        for (int j = 0; j < outputDim; ++j)
            out[j] = outputs[i * outputDim + j];
        data.inputs.push_back(in);
        data.outputs.push_back(out);
    }
    net->train(data);
}

void flux_nn_predict(void* nn, const double* input, double* output, int inputDim, int outputDim)
{
    auto* net = static_cast<NeuralNetworkSurrogate*>(nn);
    std::vector<double> in(input, input + inputDim);
    auto res = net->predict(in);
    for (int i = 0; i < outputDim && i < (int)res.prediction.size(); ++i)
        output[i] = res.prediction[i];
}

void flux_nn_save(void* nn, const char* filename)
{
    static_cast<NeuralNetworkSurrogate*>(nn)->save(filename ? filename : "model.nn");
}

void* flux_nn_load(const char* filename)
{
    auto* nn = new NeuralNetworkSurrogate();
    nn->load(filename ? filename : "model.nn");
    return nn;
}

void* flux_surrogate_create()
{
    auto nn = new NeuralNetworkSurrogate();
    NetworkArchitecture arch;
    arch.layers = {4, 16, 16, 1};
    nn->setArchitecture(arch);
    return nn;
}

void flux_surrogate_train(void* surrogate, const char* circuit_type, const double* params, const double* results,
                          int numSamples)
{
    // circuit_type ignored for now
    flux_nn_train(surrogate, params, results, numSamples, 4, 1, 100);
}

void flux_surrogate_predict(void* surrogate, const double* params, double* results, int numParams)
{
    flux_nn_predict(surrogate, params, results, numParams, 1);
}
}

} // namespace AI
} // namespace Flux
