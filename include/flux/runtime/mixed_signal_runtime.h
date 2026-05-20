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

#ifndef FLUX_MIXED_SIGNAL_RUNTIME_H
#define FLUX_MIXED_SIGNAL_RUNTIME_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>
#include <random>
#include <stdexcept>

/* ========================================================================
 * Section 7.2: Mixed-Signal & Modeling Extensions - Runtime
 * ======================================================================== */

// ============================================================================
// Event-driven constructs
// ============================================================================

// Previous value tracking for edge detection
struct EventState {
    double PrevValue = 0.0;
    double EventTime = 0.0;
    bool Initialized = false;
};

// Cross detection: returns 1.0 when signal crosses zero
extern "C" double flux_cross_detect(double value, int rise_fall);

// Above detection: returns 1.0 when value > threshold
extern "C" double flux_above_detect(double value, double threshold);

// Timer: returns elapsed time since last event
extern "C" double flux_timer_get();

// ============================================================================
// Real-valued digital modeling
// ============================================================================

// FSM object (opaque handle)
struct FSMObject {
    int CurrentState;
    std::string OutputFn;  // "moore" or "mealy"

    struct Transition {
        int CurrentState;
        int NextState;
        double (*Condition)(void*);  // Guard function
        double (*Output)(void*);     // Output function
    };
    std::vector<Transition> Transitions;
};

extern "C" void* flux_fsm_create(int initial_state, double output_fn);
extern "C" void flux_fsm_add_transition(void* fsm, int cur, int next,
                                         double (*cond)(void*), double (*out)(void*));

// Edge detection: returns 1.0 on edge, 0.0 otherwise
extern "C" double flux_edge_detect(double value, int edge_type);

// ============================================================================
// Noise modeling primitives
// ============================================================================

// Generic noise generation
extern "C" double flux_noise_generate(double type, double amplitude, double freq);

// White noise (Gaussian)
extern "C" double flux_white_noise(double amplitude);

// Flicker noise (1/f)
extern "C" double flux_flicker_noise(double amplitude, double corner_freq);

// Thermal noise (Johnson-Nyquist): sqrt(4*k*T*R*BW)
extern "C" double flux_thermal_noise(double resistance, double temperature);

// ============================================================================
// Piecewise and table-based models
// ============================================================================

// Piecewise interpolation context
struct PiecewiseContext {
    std::string Interpolation;  // "linear", "cubic", "spline", "step"
    struct Point { double X, Y; };
    std::vector<Point> Points;
};

extern "C" void* flux_piecewise_create(double interpolation);
extern "C" void flux_piecewise_add_point(void* pw, double x, double y);
extern "C" double flux_piecewise_eval(void* pw, double x);

// Table lookup
struct TableObject {
    std::map<double, double> Entries;
    double DefaultValue = 0.0;
    bool HasDefault = false;
};

extern "C" void* flux_table_create();
extern "C" void flux_table_add_entry(void* table, double key, double value);
extern "C" void flux_table_set_default(void* table, double value);
extern "C" double flux_table_lookup(void* table, double key);

// CSV import
extern "C" void* flux_csv_import(double filename, double options_json);

#endif // FLUX_MIXED_SIGNAL_RUNTIME_H
