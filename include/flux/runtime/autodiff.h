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

#ifndef FLUX_AUTODIFF_H
#define FLUX_AUTODIFF_H

#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <string>
#include <cmath>

namespace Flux {
namespace Autodiff {

// ============================================================================
// Dual Numbers for Forward Mode AD
// ============================================================================

struct Dual {
    double value;   // Function value
    double dual;    // Derivative (dual part)
    
    Dual() : value(0.0), dual(0.0) {}
    Dual(double v, double d = 0.0) : value(v), dual(d) {}
    
    // Constants
    static Dual constant(double v) { return Dual(v, 0.0); }
    static Dual variable(double v) { return Dual(v, 1.0); }  // Seed derivative = 1
    
    // Arithmetic operations
    Dual operator+(const Dual& other) const {
        return Dual(value + other.value, dual + other.dual);
    }
    
    Dual operator-(const Dual& other) const {
        return Dual(value - other.value, dual - other.dual);
    }
    
    Dual operator*(const Dual& other) const {
        // Product rule: (uv)' = u'v + uv'
        return Dual(
            value * other.value,
            dual * other.value + value * other.dual
        );
    }
    
    Dual operator/(const Dual& other) const {
        // Quotient rule: (u/v)' = (u'v - uv') / v²
        return Dual(
            value / other.value,
            (dual * other.value - value * other.dual) / (other.value * other.value)
        );
    }
    
    Dual operator-() const {
        return Dual(-value, -dual);
    }
    
    // Comparison (only compares value part)
    bool operator<(const Dual& other) const { return value < other.value; }
    bool operator>(const Dual& other) const { return value > other.value; }
    bool operator<=(const Dual& other) const { return value <= other.value; }
    bool operator>=(const Dual& other) const { return value >= other.value; }
    bool operator==(const Dual& other) const { return value == other.value; }
    bool operator!=(const Dual& other) const { return value != other.value; }
};

// Elementary functions for dual numbers
namespace DualMath {
    inline Dual sin(const Dual& x) {
        return Dual(std::sin(x.value), x.dual * std::cos(x.value));
    }
    
    inline Dual cos(const Dual& x) {
        return Dual(std::cos(x.value), -x.dual * std::sin(x.value));
    }
    
    inline Dual tan(const Dual& x) {
        double cosVal = std::cos(x.value);
        return Dual(std::tan(x.value), x.dual / (cosVal * cosVal));
    }
    
    inline Dual asin(const Dual& x) {
        return Dual(std::asin(x.value), x.dual / std::sqrt(1.0 - x.value * x.value));
    }
    
    inline Dual acos(const Dual& x) {
        return Dual(std::acos(x.value), -x.dual / std::sqrt(1.0 - x.value * x.value));
    }
    
    inline Dual atan(const Dual& x) {
        return Dual(std::atan(x.value), x.dual / (1.0 + x.value * x.value));
    }
    
    inline Dual atan2(const Dual& y, const Dual& x) {
        double denom = x.value * x.value + y.value * y.value;
        return Dual(
            std::atan2(y.value, x.value),
            (y.dual * x.value - x.dual * y.value) / denom
        );
    }
    
    inline Dual exp(const Dual& x) {
        double expVal = std::exp(x.value);
        return Dual(expVal, x.dual * expVal);
    }
    
    inline Dual log(const Dual& x) {
        return Dual(std::log(x.value), x.dual / x.value);
    }
    
    inline Dual log10(const Dual& x) {
        double ln10 = std::log(10.0);
        return Dual(std::log10(x.value), x.dual / (x.value * ln10));
    }
    
    inline Dual sqrt(const Dual& x) {
        double sqrtVal = std::sqrt(x.value);
        return Dual(sqrtVal, x.dual / (2.0 * sqrtVal));
    }
    
    inline Dual pow(const Dual& base, const Dual& exp) {
        // d/dx (x^y) = y * x^(y-1) for constant y
        // d/dy (x^y) = x^y * ln(x) for constant x
        // General: d/dt (x^y) = x^y * (y' * ln(x) + y * x'/x)
        double powVal = std::pow(base.value, exp.value);
        if (base.value <= 0) {
            return Dual(powVal, 0.0);  // Avoid domain errors
        }
        return Dual(
            powVal,
            powVal * (exp.dual * std::log(base.value) + exp.value * base.dual / base.value)
        );
    }
    
    inline Dual pow(const Dual& base, double exp) {
        return pow(base, Dual::constant(exp));
    }
    
    inline Dual abs(const Dual& x) {
        if (x.value > 0) return x;
        if (x.value < 0) return -x;
        return Dual(0.0, 0.0);  // Undefined at 0
    }
    
    inline Dual sinh(const Dual& x) {
        return Dual(std::sinh(x.value), x.dual * std::cosh(x.value));
    }
    
    inline Dual cosh(const Dual& x) {
        return Dual(std::cosh(x.value), x.dual * std::sinh(x.value));
    }
    
    inline Dual tanh(const Dual& x) {
        double coshVal = std::cosh(x.value);
        return Dual(std::tanh(x.value), x.dual / (coshVal * coshVal));
    }
}

// ============================================================================
// Forward Mode Automatic Differentiation
// ============================================================================

class ForwardModeAD {
public:
    // Compute derivative of f at x using dual numbers
    static double derivative(const std::function<double(double)>& f, double x) {
        // Use numerical differentiation as fallback
        const double h = 1e-8;
        return (f(x + h) - f(x)) / h;
    }
    
    // Compute derivative with custom seed (numerical)
    static double derivativeWithSeed(const std::function<double(double)>& f, 
                                      double x, double seed = 1.0) {
        const double h = 1e-8;
        return (f(x + h * seed) - f(x)) / h;
    }
    
    // Compute gradient of multi-variable function (numerical)
    static std::vector<double> gradient(
        const std::function<double(const std::vector<double>&)>& f,
        const std::vector<double>& x) 
    {
        const double h = 1e-8;
        size_t n = x.size();
        std::vector<double> grad(n);
        double f0 = f(x);
        
        std::vector<double> xPlus = x;
        for (size_t i = 0; i < n; ++i) {
            xPlus[i] = x[i] + h;
            grad[i] = (f(xPlus) - f0) / h;
            xPlus[i] = x[i];  // Reset
        }
        
        return grad;
    }
    
    // Compute Jacobian matrix (numerical)
    static std::vector<std::vector<double>> jacobian(
        const std::function<std::vector<double>(const std::vector<double>&)>& f,
        const std::vector<double>& x)
    {
        const double h = 1e-8;
        size_t n = x.size();
        std::vector<double> y = f(x);
        size_t m = y.size();
        
        std::vector<std::vector<double>> J(m, std::vector<double>(n, 0.0));
        std::vector<double> xPlus = x;
        
        for (size_t i = 0; i < n; ++i) {
            xPlus[i] = x[i] + h;
            std::vector<double> yPlus = f(xPlus);
            for (size_t k = 0; k < m; ++k) {
                J[k][i] = (yPlus[k] - y[k]) / h;
            }
            xPlus[i] = x[i];  // Reset
        }
        
        return J;
    }
    
    // Higher-order derivatives (numerical)
    static double secondDerivative(const std::function<double(double)>& f, double x) {
        const double h = 1e-5;
        return (f(x + h) - 2 * f(x) + f(x - h)) / (h * h);
    }
    
    // Dual number based differentiation (for internal use)
    static Dual evalDual(const std::function<Dual(const Dual&)>& f, const Dual& x) {
        return f(x);
    }
};

// ============================================================================
// Reverse Mode AD (Backpropagation)
// ============================================================================

// Computational graph node
struct Node {
    double value;
    double gradient = 0.0;
    std::vector<std::shared_ptr<Node>> inputs;
    std::function<void()> backwardFunc;
    bool visited = false;
    
    Node(double v) : value(v) {}
    virtual ~Node() = default;
};

// Variable that tracks operations
class Var : public std::enable_shared_from_this<Var> {
public:
    std::shared_ptr<Node> node;
    
    Var(double value = 0.0) {
        node = std::make_shared<Node>(value);
    }
    
    double value() const { return node->value; }
    double grad() const { return node->gradient; }
    
    void zeroGrad() {
        node->gradient = 0.0;
    }
    
    // Arithmetic operations
    std::shared_ptr<Var> operator+(const std::shared_ptr<Var>& other) const {
        auto result = std::make_shared<Var>(node->value + other->node->value);
        result->node->inputs = {node, other->node};
        result->node->backwardFunc = [this, other, result]() {
            node->gradient += result->node->gradient;
            other->node->gradient += result->node->gradient;
        };
        return result;
    }
    
    std::shared_ptr<Var> operator-(const std::shared_ptr<Var>& other) const {
        auto result = std::make_shared<Var>(node->value - other->node->value);
        result->node->inputs = {node, other->node};
        result->node->backwardFunc = [this, other, result]() {
            node->gradient += result->node->gradient;
            other->node->gradient -= result->node->gradient;
        };
        return result;
    }
    
    std::shared_ptr<Var> operator*(const std::shared_ptr<Var>& other) const {
        auto result = std::make_shared<Var>(node->value * other->node->value);
        result->node->inputs = {node, other->node};
        result->node->backwardFunc = [this, other, result]() {
            node->gradient += result->node->gradient * other->node->value;
            other->node->gradient += result->node->gradient * node->value;
        };
        return result;
    }
    
    std::shared_ptr<Var> operator/(const std::shared_ptr<Var>& other) const {
        auto result = std::make_shared<Var>(node->value / other->node->value);
        result->node->inputs = {node, other->node};
        result->node->backwardFunc = [this, other, result]() {
            node->gradient += result->node->gradient / other->node->value;
            other->node->gradient -= result->node->gradient * node->value / 
                                     (other->node->value * other->node->value);
        };
        return result;
    }
    
    std::shared_ptr<Var> operator-() const {
        auto result = std::make_shared<Var>(-node->value);
        result->node->inputs = {node};
        result->node->backwardFunc = [this, result]() {
            node->gradient -= result->node->gradient;
        };
        return result;
    }
    
    // Elementary functions
    static std::shared_ptr<Var> sin(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::sin(x->node->value));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result]() {
            x->node->gradient += result->node->gradient * std::cos(x->node->value);
        };
        return result;
    }
    
    static std::shared_ptr<Var> cos(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::cos(x->node->value));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result]() {
            x->node->gradient -= result->node->gradient * std::sin(x->node->value);
        };
        return result;
    }
    
    static std::shared_ptr<Var> exp(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::exp(x->node->value));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result]() {
            x->node->gradient += result->node->gradient * result->node->value;
        };
        return result;
    }
    
    static std::shared_ptr<Var> log(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::log(x->node->value));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result]() {
            x->node->gradient += result->node->gradient / x->node->value;
        };
        return result;
    }
    
    static std::shared_ptr<Var> sqrt(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::sqrt(x->node->value));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result]() {
            x->node->gradient += result->node->gradient / (2.0 * result->node->value);
        };
        return result;
    }
    
    static std::shared_ptr<Var> pow(const std::shared_ptr<Var>& x, double exp) {
        auto result = std::make_shared<Var>(std::pow(x->node->value, exp));
        result->node->inputs = {x->node};
        result->node->backwardFunc = [x, result, exp]() {
            x->node->gradient += result->node->gradient * exp * 
                                 std::pow(x->node->value, exp - 1);
        };
        return result;
    }
    
    static std::shared_ptr<Var> tanh(const std::shared_ptr<Var>& x) {
        auto result = std::make_shared<Var>(std::tanh(x->node->value));
        result->node->inputs = {x->node};
        double tanhVal = result->node->value;
        result->node->backwardFunc = [x, result, tanhVal]() {
            x->node->gradient += result->node->gradient * (1.0 - tanhVal * tanhVal);
        };
        return result;
    }
};

// Reverse mode AD engine
class ReverseModeAD {
public:
    // Backpropagate from output
    static void backward(const std::shared_ptr<Var>& output) {
        // Collect all nodes in topological order
        std::vector<std::shared_ptr<Node>> nodes;
        std::map<Node*, bool> visited;
        
        std::function<void(std::shared_ptr<Node>)> collect = [&](std::shared_ptr<Node> n) {
            if (visited[n.get()]) return;
            visited[n.get()] = true;
            
            for (auto& input : n->inputs) {
                collect(input);
            }
            nodes.push_back(n);
        };
        
        collect(output->node);
        
        // Set gradient of output to 1
        output->node->gradient = 1.0;
        
        // Backpropagate in reverse topological order
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            if ((*it)->backwardFunc) {
                (*it)->backwardFunc();
            }
        }
    }
    
    // Compute gradient
    static std::vector<double> gradient(
        const std::shared_ptr<Var>& output,
        const std::vector<std::shared_ptr<Var>>& inputs)
    {
        // Zero all gradients
        for (auto& input : inputs) {
            input->zeroGrad();
        }
        
        // Backpropagate
        backward(output);
        
        // Extract gradients
        std::vector<double> grads(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            grads[i] = inputs[i]->grad();
        }
        
        return grads;
    }
};

// ============================================================================
// Runtime Functions for JIT Integration
// ============================================================================

extern "C" {
    // Forward mode derivative computation
    double flux_derivative(double (*func)(double), double x);
    
    // Gradient computation
    void flux_gradient(double (*func)(double*, int), double* x, double* grad, int n);
    
    // Numerical derivative (for comparison/testing)
    double flux_numerical_derivative(double (*func)(double), double x, double h = 1e-8);
}

} // namespace Autodiff
} // namespace Flux

#endif // FLUX_AUTODIFF_H
