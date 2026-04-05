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

// FluxScript C++ Plugin Example
// Compile with: g++ -shared -fPIC -o example_plugin.so example_plugin.cpp

#include <cmath>
#include <cstring>

// Plugin metadata (exported symbols)
extern "C" {

// Plugin information
const char* plugin_name() {
    return "example_plugin";
}

const char* plugin_version() {
    return "1.0.0";
}

const char* plugin_description() {
    return "Example FluxScript plugin with custom functions";
}

// Plugin initialization - called when plugin is loaded
int plugin_init() {
    return 0;  // Return 0 on success
}

// Plugin cleanup - called when plugin is unloaded
void plugin_cleanup() {
    // Cleanup resources
}

// ============================================================================
// Plugin Functions (available to FluxScript)
// ============================================================================

// Example: Fast sine approximation
double fast_sin(double x) {
    // Bhaskara I's sine approximation formula
    const double pi = 3.14159265358979323846;
    const double pi2 = pi * 2.0;
    
    // Normalize x to [0, 2*pi]
    x = x - pi2 * floor(x / pi2);
    
    // Bhaskara I's approximation
    double num = 16.0 * x * (pi - x);
    double denom = 5.0 * pi * pi - 4.0 * x * (pi - x);
    
    return num / denom;
}

// Example: Fast inverse square root
double fast_inv_sqrt(double x) {
    if (x <= 0.0) return 0.0;
    
    double x2 = x * 0.5;
    long long i = *reinterpret_cast<long long*>(&x);
    i = 0x5FE6EB50C7B537A9 - (i >> 1);  // Magic number for double precision
    double y = *reinterpret_cast<double*>(&i);
    
    // Newton-Raphson iteration
    return y * (1.5 - x2 * y * y);
}

// Example: Smoothstep function (useful for interpolation)
double smoothstep(double edge0, double edge1, double x) {
    double t = (x - edge0) / (edge1 - edge0);
    t = fmax(0.0, fmin(1.0, t));  // Clamp to [0, 1]
    return t * t * (3.0 - 2.0 * t);
}

// Example: Linear congruential generator (simple PRNG)
static unsigned long lcg_state = 12345;

double random_uniform() {
    lcg_state = (lcg_state * 1103515245 + 12345) & 0x7FFFFFFF;
    return static_cast<double>(lcg_state) / 0x7FFFFFFF;
}

void random_seed(double seed) {
    lcg_state = static_cast<unsigned long>(seed);
}

// Example: Bessel function J0 (approximation)
double bessel_j0(double x) {
    const double a1 = -0.1098628627e+2;
    const double a2 =  0.2734510407e+4;
    const double a3 = -0.2073370639e+5;
    const double a4 =  0.2093887211e+2;
    
    const double b1 = -0.1562499995e-1;
    const double b2 =  0.5464213086e+4;
    const double b3 = -0.3673428440e+5;
    const double b4 =  0.1071614105e+6;
    
    double ax = fabs(x);
    
    if (ax < 8.0) {
        double y = x * x;
        return (a1 + y*(a2 + y*(a3 + y*a4))) /
               (b1 + y*(b2 + y*(b3 + y*b4)));
    } else {
        double sqrtx = sqrt(ax);
        double z = 8.0 / ax;
        double y = z * z;
        double xx = ax - 0.785398164;
        
        return sqrt(0.636619772 / ax) *
               (cos(xx) * (1.0 + y*(-0.1098628627e+2 + y*(0.2734510407e+4 + 
                y*(-0.2073370639e+5 + y*0.2093887211e+2)))) -
                z * sin(xx) * (1.0 + y*(-0.1562499995e-1 + y*(0.5464213086e+4 +
                y*(-0.3673428440e+5 + y*0.1071614105e+6)))));
    }
}

// ============================================================================
// Function Registration
// ============================================================================

// Register all plugin functions with the FluxScript runtime
// This would be called by the ModuleLoader when the plugin is loaded

typedef void (*RegisterFunctionCallback)(const char* name, void* func);

void plugin_register_functions(RegisterFunctionCallback register_fn) {
    register_fn("fast_sin", reinterpret_cast<void*>(fast_sin));
    register_fn("fast_inv_sqrt", reinterpret_cast<void*>(fast_inv_sqrt));
    register_fn("smoothstep", reinterpret_cast<void*>(smoothstep));
    register_fn("random_uniform", reinterpret_cast<void*>(random_uniform));
    register_fn("random_seed", reinterpret_cast<void*>(random_seed));
    register_fn("bessel_j0", reinterpret_cast<void*>(bessel_j0));
}

} // extern "C"
