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

#include "flux/runtime/mixed_signal_runtime.h"
#include <algorithm>
#include <cstring>

// For thermal noise: Boltzmann constant
#ifndef BOLTZMANN_K
#define BOLTZMANN_K 1.380649e-23  // J/K
#endif

template <typename To, typename From>
inline To bit_cast(const From& src) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

extern "C" {

/* ========================================================================
 * Event-driven constructs
 * ======================================================================== */

static EventState g_eventState;

double flux_cross_detect(double value, int rise_fall) {
    if (!g_eventState.Initialized) {
        g_eventState.PrevValue = value;
        g_eventState.Initialized = true;
        return 0.0;
    }

    bool crossed = false;
    if (rise_fall == 0) {
        // Any crossing
        crossed = (g_eventState.PrevValue < 0.0 && value >= 0.0) ||
                  (g_eventState.PrevValue > 0.0 && value <= 0.0);
    } else if (rise_fall == 1) {
        // Rising only
        crossed = (g_eventState.PrevValue < 0.0 && value >= 0.0);
    } else if (rise_fall == -1) {
        // Falling only
        crossed = (g_eventState.PrevValue > 0.0 && value <= 0.0);
    }

    g_eventState.PrevValue = value;
    return crossed ? 1.0 : 0.0;
}

double flux_above_detect(double value, double threshold) {
    if (!g_eventState.Initialized) {
        g_eventState.PrevValue = value;
        g_eventState.Initialized = true;
    }

    bool isAbove = (value > threshold);
    g_eventState.PrevValue = value;
    return isAbove ? 1.0 : 0.0;
}

double flux_timer_get() {
    // Returns simulated time (in a real system, this would be tied to the simulation clock)
    // For now, return a placeholder that can be overridden by the simulation environment
    return g_eventState.EventTime;
}

/* ========================================================================
 * Real-valued digital modeling
 * ======================================================================== */

void* flux_fsm_create(int initial_state, double output_fn) {
    auto* fsm = new FSMObject();
    fsm->CurrentState = initial_state;
    const char* output_fn_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(output_fn));
    fsm->OutputFn = output_fn_ptr ? output_fn_ptr : "moore";
    return static_cast<void*>(fsm);
}

void flux_fsm_add_transition(void* fsm, int cur, int next,
                              double (*cond)(void*), double (*out)(void*)) {
    if (!fsm) return;
    auto* f = static_cast<FSMObject*>(fsm);
    FSMObject::Transition t;
    t.CurrentState = cur;
    t.NextState = next;
    t.Condition = cond;
    t.Output = out;
    f->Transitions.push_back(t);
}

double flux_edge_detect(double value, int edge_type) {
    if (!g_eventState.Initialized) {
        g_eventState.PrevValue = value;
        g_eventState.Initialized = true;
        return 0.0;
    }

    bool edge = false;
    if (edge_type == 0) {
        // Any edge
        edge = (g_eventState.PrevValue < 0.5 && value >= 0.5) ||
               (g_eventState.PrevValue >= 0.5 && value < 0.5);
    } else if (edge_type == 1) {
        // Positive edge
        edge = (g_eventState.PrevValue < 0.5 && value >= 0.5);
    } else if (edge_type == -1) {
        // Negative edge
        edge = (g_eventState.PrevValue >= 0.5 && value < 0.5);
    }

    g_eventState.PrevValue = value;
    return edge ? 1.0 : 0.0;
}

/* ========================================================================
 * Noise modeling primitives
 * ======================================================================== */

// Thread-local random state
static thread_local std::mt19937_64 g_rng(std::random_device{}());
static thread_local std::normal_distribution<double> g_normalDist(0.0, 1.0);

double flux_noise_generate(double type, double amplitude, double freq) {
    const char* type_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(type));
    if (!type_ptr) return 0.0;

    std::string t(type_ptr);
    if (t == "white") {
        return flux_white_noise(amplitude);
    } else if (t == "flicker") {
        return flux_flicker_noise(amplitude, freq);
    } else if (t == "thermal") {
        // For generic thermal, amplitude is resistance, freq is temperature
        return flux_thermal_noise(amplitude, freq);
    }
    return 0.0;
}

double flux_white_noise(double amplitude) {
    return amplitude * g_normalDist(g_rng);
}

double flux_flicker_noise(double amplitude, double corner_freq) {
    // Simplified 1/f noise model
    // V_flicker = amplitude * sqrt(corner_freq / f) * gaussian
    // For simulation, we use a simplified approach
    double gaussian = g_normalDist(g_rng);
    // The actual frequency-dependent behavior would need simulation context
    // For now, return amplitude-weighted Gaussian with 1/f characteristic
    double flickerFactor = (corner_freq > 0.0) ? std::sqrt(corner_freq) : 1.0;
    return amplitude * flickerFactor * gaussian * 0.1;  // 0.1 is a scaling factor
}

double flux_thermal_noise(double resistance, double temperature) {
    // Johnson-Nyquist noise: Vn = sqrt(4 * k * T * R * BW)
    // Assuming 1 Hz bandwidth for per-sample noise
    double bw = 1.0;  // 1 Hz bandwidth
    double variance = 4.0 * BOLTZMANN_K * temperature * resistance * bw;
    double stdDev = std::sqrt(variance);
    return stdDev * g_normalDist(g_rng);
}

/* ========================================================================
 * Piecewise and table-based models
 * ======================================================================== */

void* flux_piecewise_create(double interpolation) {
    auto* pw = new PiecewiseContext();
    const char* interp_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(interpolation));
    pw->Interpolation = interp_ptr ? interp_ptr : "linear";
    return static_cast<void*>(pw);
}

void flux_piecewise_add_point(void* pw, double x, double y) {
    if (!pw) return;
    auto* ctx = static_cast<PiecewiseContext*>(pw);
    ctx->Points.push_back({x, y});
    // Keep points sorted by x
    std::sort(ctx->Points.begin(), ctx->Points.end(),
              [](const PiecewiseContext::Point& a, const PiecewiseContext::Point& b) {
                  return a.X < b.X;
              });
}

static double linearInterp(double x0, double y0, double x1, double y1, double x) {
    if (x1 == x0) return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

static double stepInterp(const std::vector<PiecewiseContext::Point>& points, double x) {
    // Return y of the nearest point with x_point <= x
    double result = points.empty() ? 0.0 : points.back().Y;
    for (const auto& pt : points) {
        if (pt.X <= x) {
            result = pt.Y;
        } else {
            break;
        }
    }
    return result;
}

static double cubicInterp(double x0, double y0, double x1, double y1,
                          double x2, double y2, double x3, double y3, double x) {
    // Catmull-Rom spline interpolation
    double t = (x - x1) / (x2 - x1);
    double t2 = t * t;
    double t3 = t2 * t;

    double c0 = y1;
    double c1 = 0.5 * (y2 - y0);
    double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return c0 + c1 * t + c2 * t2 + c3 * t3;
}

double flux_piecewise_eval(void* pw, double x) {
    if (!pw) return 0.0;
    auto* ctx = static_cast<PiecewiseContext*>(pw);

    if (ctx->Points.empty()) return 0.0;
    if (ctx->Points.size() == 1) return ctx->Points[0].Y;

    if (ctx->Interpolation == "step") {
        return stepInterp(ctx->Points, x);
    }

    // Find segment containing x
    size_t i = 0;
    for (; i < ctx->Points.size() - 1; ++i) {
        if (ctx->Points[i].X <= x && ctx->Points[i+1].X >= x) {
            break;
        }
    }

    if (ctx->Interpolation == "linear") {
        if (i >= ctx->Points.size() - 1) {
            // Extrapolate using last segment
            i = ctx->Points.size() - 2;
        }
        const auto& p0 = ctx->Points[i];
        const auto& p1 = ctx->Points[i+1];
        return linearInterp(p0.X, p0.Y, p1.X, p1.Y, x);
    } else if (ctx->Interpolation == "cubic" || ctx->Interpolation == "spline") {
        // Need 4 points for cubic interpolation
        size_t i0 = (i > 0) ? i - 1 : i;
        size_t i3 = (i + 2 < ctx->Points.size()) ? i + 2 : ctx->Points.size() - 1;
        size_t i1 = i;
        size_t i2 = i + 1;

        const auto& p0 = ctx->Points[i0];
        const auto& p1 = ctx->Points[i1];
        const auto& p2 = ctx->Points[i2];
        const auto& p3 = ctx->Points[i3];

        return cubicInterp(p0.X, p0.Y, p1.X, p1.Y, p2.X, p2.Y, p3.X, p3.Y, x);
    }

    // Default to linear
    if (i >= ctx->Points.size() - 1) i = ctx->Points.size() - 2;
    const auto& p0 = ctx->Points[i];
    const auto& p1 = ctx->Points[i+1];
    return linearInterp(p0.X, p0.Y, p1.X, p1.Y, x);
}

/* ========================================================================
 * Table lookup
 * ======================================================================== */

void* flux_table_create() {
    return static_cast<void*>(new TableObject());
}

void flux_table_add_entry(void* table, double key, double value) {
    if (!table) return;
    auto* t = static_cast<TableObject*>(table);
    t->Entries[key] = value;
}

void flux_table_set_default(void* table, double value) {
    if (!table) return;
    auto* t = static_cast<TableObject*>(table);
    t->DefaultValue = value;
    t->HasDefault = true;
}

double flux_table_lookup(void* table, double key) {
    if (!table) return 0.0;
    auto* t = static_cast<TableObject*>(table);

    auto it = t->Entries.find(key);
    if (it != t->Entries.end()) {
        return it->second;
    }

    // Try interpolation between nearest keys
    if (!t->Entries.empty()) {
        auto upper = t->Entries.upper_bound(key);
        if (upper == t->Entries.end()) {
            // Key is beyond all entries, return last
            return t->Entries.rbegin()->second;
        }
        if (upper == t->Entries.begin()) {
            // Key is before all entries, return first
            return t->Entries.begin()->second;
        }

        auto lower = std::prev(upper);
        // Linear interpolation between lower and upper
        double x0 = lower->first, y0 = lower->second;
        double x1 = upper->first, y1 = upper->second;
        if (x1 == x0) return y0;
        return y0 + (y1 - y0) * (key - x0) / (x1 - x0);
    }

    if (t->HasDefault) {
        return t->DefaultValue;
    }

    return 0.0;  // Default if no match
}

/* ========================================================================
 * CSV import
 * ======================================================================== */

void* flux_csv_import(double filename, double options_json) {
    const char* filename_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(filename));
    if (!filename_ptr) return nullptr;

    const char* options_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(options_json));
    // In a full implementation, this would:
    // 1. Parse the options_ptr for delimiter, header row, etc.
    // 2. Open and parse the CSV file
    // 3. Return a data structure that can be queried

    // For now, return a placeholder indicating success
    static int csvHandleCounter = 0;
    return reinterpret_cast<void*>(++csvHandleCounter);
}

/* ========================================================================
 * Units and dimensional analysis
 * ======================================================================== */

// Simple unit registry for runtime
static const std::map<std::string, std::string> g_unitDimensions = {
    {"V", "M*L^2*T^-3*I^-1"},        // Voltage
    {"mV", "M*L^2*T^-3*I^-1"},
    {"kV", "M*L^2*T^-3*I^-1"},
    {"A", "I"},                       // Current
    {"mA", "I"},
    {"A", "I"},
    {"", "M*L^2*T^-3*I^-2"},        // Resistance
    {"k", "M*L^2*T^-3*I^-2"},
    {"M", "M*L^2*T^-3*I^-2"},
    {"F", "M^-1*L^-2*T^4*I^2"},      // Capacitance
    {"F", "M^-1*L^-2*T^4*I^2"},
    {"nF", "M^-1*L^-2*T^4*I^2"},
    {"pF", "M^-1*L^-2*T^4*I^2"},
    {"H", "M*L^2*T^-2*I^-2"},        // Inductance
    {"mH", "M*L^2*T^-2*I^-2"},
    {"H", "M*L^2*T^-2*I^-2"},
    {"Hz", "T^-1"},                   // Frequency
    {"kHz", "T^-1"},
    {"MHz", "T^-1"},
    {"s", "T"},                       // Time
    {"ms", "T"},
    {"s", "T"},
    {"ns", "T"},
    {"W", "M*L^2*T^-3"},             // Power
    {"mW", "M*L^2*T^-3"},
    {"J", "M*L^2*T^-2"},             // Energy
};

// Unit scale factors
static const std::map<std::string, double> g_unitScales = {
    {"V", 1.0}, {"mV", 1e-3}, {"kV", 1e3}, {"V", 1e-6}, {"uV", 1e-6},
    {"A", 1.0}, {"mA", 1e-3}, {"A", 1e-6}, {"uA", 1e-6}, {"nA", 1e-9},
    {"", 1.0}, {"k", 1e3}, {"M", 1e6}, {"m", 1e-3},
    {"F", 1.0}, {"F", 1e-6}, {"uF", 1e-6}, {"nF", 1e-9}, {"pF", 1e-12},
    {"H", 1.0}, {"mH", 1e-3}, {"H", 1e-6}, {"uH", 1e-6},
    {"Hz", 1.0}, {"kHz", 1e3}, {"MHz", 1e6}, {"GHz", 1e9},
    {"s", 1.0}, {"ms", 1e-3}, {"s", 1e-6}, {"us", 1e-6}, {"ns", 1e-9}, {"ps", 1e-12},
    {"W", 1.0}, {"mW", 1e-3}, {"kW", 1e3},
    {"J", 1.0}, {"mJ", 1e-3}, {"kJ", 1e3},
};

// Helper for unit lookup using strcmp
static const char* get_unit_dimension(const char* unit_str) {
    if (!unit_str) return nullptr;
    for (auto const& [unit, dim] : g_unitDimensions) {
        if (std::strcmp(unit.c_str(), unit_str) == 0) {
            return dim.c_str();
        }
    }
    return nullptr;
}

void* flux_unit_create(double value, double unit_str_ptr) {
    auto* obj = (QuantifiedValueObject*)malloc(sizeof(QuantifiedValueObject));
    if (!obj) return nullptr;
    obj->Value = value;
    const char* unit_str = reinterpret_cast<const char*>(bit_cast<uint64_t>(unit_str_ptr));
    if (unit_str) {
        strncpy(obj->UnitStr, unit_str, 63);
        obj->UnitStr[63] = '\0';
    } else {
        obj->UnitStr[0] = '\0';
    }
    return static_cast<void*>(obj);
}

const char* flux_dimension(void* quant) {
    if (!quant) return "";
    auto* obj = static_cast<QuantifiedValueObject*>(quant);

    const char* dim = get_unit_dimension(obj->UnitStr);
    if (dim) return dim;

    return "dimensionless";
}

double flux_convert(double value, double from, double to) {
    const char* from_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(from));
    const char* to_ptr = reinterpret_cast<const char*>(bit_cast<uint64_t>(to));
    if (!from_ptr || !to_ptr) return value;

    std::string fromStr(from_ptr);
    std::string toStr(to_ptr);

    // Check if units are compatible
    auto dimIt1 = g_unitDimensions.find(fromStr);
    auto dimIt2 = g_unitDimensions.find(toStr);

    if (dimIt1 != g_unitDimensions.end() && dimIt2 != g_unitDimensions.end()) {
        if (dimIt1->second != dimIt2->second) {
            // Incompatible units - return original value
            return value;
        }
    }

    // Convert: value * scale(from) / scale(to)
    auto scaleIt1 = g_unitScales.find(fromStr);
    auto scaleIt2 = g_unitScales.find(toStr);

    double fromScale = (scaleIt1 != g_unitScales.end()) ? scaleIt1->second : 1.0;
    double toScale = (scaleIt2 != g_unitScales.end()) ? scaleIt2->second : 1.0;

    return value * fromScale / toScale;
}

double flux_has_unit(void* quant, double unit_str_ptr) {
    const char* unit_str = reinterpret_cast<const char*>(bit_cast<uint64_t>(unit_str_ptr));
    if (!quant || !unit_str) return 0.0;
    auto* obj = static_cast<QuantifiedValueObject*>(quant);

    // Exact match
    if (std::strcmp(obj->UnitStr, unit_str) == 0) return 1.0;

    // Check if same dimension (compatible units)
    const char* dim1 = get_unit_dimension(obj->UnitStr);
    const char* dim2 = get_unit_dimension(unit_str);

    if (dim1 && dim2) {
        if (std::strcmp(dim1, dim2) == 0) {
            return 1.0;  // Compatible units
        }
    }

    return 0.0;
}

} // extern "C"
