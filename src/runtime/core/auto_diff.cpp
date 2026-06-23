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

#include "flux/runtime/auto_diff.h"
#include <cmath>
#include <iostream>

namespace Flux {
namespace Autodiff {

// ============================================================================
// Runtime Functions for JIT Integration
// ============================================================================

extern "C" double flux_derivative(double (*func)(double), double x)
{
    // Wrap the C function in a std::function
    auto wrapper = [func](double t) -> double { return func(t); };

    // Use forward mode AD
    Dual x_dual = Dual::variable(x);

    // For proper AD, we need to trace through the function
    // This is a simplified implementation
    double result = func(x);

    // Compute numerical derivative as fallback
    double h = 1e-8;
    double f_x = func(x);
    double f_xh = func(x + h);
    double derivative = (f_xh - f_x) / h;

    return derivative;
}

extern "C" void flux_gradient(double (*func)(double*, int), double* x, double* grad, int n)
{
    const double h = 1e-8;
    double f0 = func(x, n);

    for (int i = 0; i < n; ++i) {
        double original = x[i];
        x[i] = original + h;
        double fi = func(x, n);
        x[i] = original;

        grad[i] = (fi - f0) / h;
    }
}

extern "C" double flux_numerical_derivative(double (*func)(double), double x, double h)
{
    double f_x = func(x);
    double f_xh = func(x + h);
    return (f_xh - f_x) / h;
}

} // namespace Autodiff
} // namespace Flux
