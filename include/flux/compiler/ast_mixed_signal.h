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

#ifndef FLUX_COMPILER_AST_MIXED_SIGNAL_H
#define FLUX_COMPILER_AST_MIXED_SIGNAL_H

#include "flux/compiler/ast_core.h"

namespace Flux {

class TrainExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Model;
    std::unique_ptr<ExprAST> Inputs;
    std::unique_ptr<ExprAST> Outputs;
    int Epochs;

public:
    TrainExprAST(std::unique_ptr<ExprAST> model, std::unique_ptr<ExprAST> in, std::unique_ptr<ExprAST> out, int epochs)
        : Model(std::move(model)), Inputs(std::move(in)), Outputs(std::move(out)), Epochs(epochs)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class PredictExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Model;
    std::unique_ptr<ExprAST> Input;

public:
    PredictExprAST(std::unique_ptr<ExprAST> model, std::unique_ptr<ExprAST> in)
        : Model(std::move(model)), Input(std::move(in))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class GoalExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::unique_ptr<ExprAST> Target;

public:
    GoalExprAST(std::unique_ptr<ExprAST> expr, std::unique_ptr<ExprAST> target)
        : Expression(std::move(expr)), Target(std::move(target))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class ThermalBlockAST : public ExprAST
{
    std::unique_ptr<ExprAST> Power;
    std::unique_ptr<ExprAST> Resistance;
    std::unique_ptr<ExprAST> Capacitance;

public:
    ThermalBlockAST(std::unique_ptr<ExprAST> power, std::unique_ptr<ExprAST> res, std::unique_ptr<ExprAST> cap)
        : Power(std::move(power)), Resistance(std::move(res)), Capacitance(std::move(cap))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class MonteCarloExprAST : public ExprAST
{
    std::string OutputName;
    std::vector<std::string> ComponentNames;
    std::vector<double> Nominals;
    std::vector<double> Tolerances;
    int Iterations;

public:
    MonteCarloExprAST(const std::string& outputName, const std::vector<std::string>& componentNames,
                      const std::vector<double>& nominals, const std::vector<double>& tolerances, int iterations)
        : OutputName(outputName), ComponentNames(componentNames), Nominals(nominals), Tolerances(tolerances),
          Iterations(iterations)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class VirtualProbeExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Signal;
    std::string ProbeName;

public:
    VirtualProbeExprAST(std::unique_ptr<ExprAST> sig, std::string name)
        : Signal(std::move(sig)), ProbeName(std::move(name))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class HotSwapExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> SubcktName;
    std::unique_ptr<ExprAST> Model;

public:
    HotSwapExprAST(std::unique_ptr<ExprAST> name, std::unique_ptr<ExprAST> model)
        : SubcktName(std::move(name)), Model(std::move(model))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

// Corner case analysis
class CornerExprAST : public ExprAST
{
    std::string Variable;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Cases;

public:
    CornerExprAST(std::string Var) : Variable(std::move(Var)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addCase(const std::string& name, std::unique_ptr<ExprAST> expr);
    bool containsYield() const override
    {
        for (const auto& C : Cases)
            if (C.second->containsYield())
                return true;
        return false;
    }
};
class CrossExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    int RiseFall; // 0=any edge, 1=rising, -1=falling
public:
    CrossExprAST(std::unique_ptr<ExprAST> Expr, int RiseFall = 0) : Expression(std::move(Expr)), RiseFall(RiseFall) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    int getRiseFall() const { return RiseFall; }
};

// above(expr, threshold) - Threshold detection (returns 1 when expr > threshold)
class AboveExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::unique_ptr<ExprAST> Threshold;

public:
    AboveExprAST(std::unique_ptr<ExprAST> Expr, std::unique_ptr<ExprAST> Thresh)
        : Expression(std::move(Expr)), Threshold(std::move(Thresh))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    const ExprAST* getThreshold() const { return Threshold.get(); }
};

// timer() - Returns elapsed simulation time since last event
class TimerExprAST : public ExprAST
{
public:
    TimerExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

// --- Real-valued digital modeling ---

// FSM state transition table entry
struct FSMTransition
{
    int CurrentState;
    int NextState;
    std::unique_ptr<ExprAST> Condition; // Guard expression
    std::unique_ptr<ExprAST> Output;    // Output value (Moore/Mealy)
    FSMTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out)
        : CurrentState(cur), NextState(next), Condition(std::move(cond)), Output(std::move(out))
    {
    }
};

// fsm(initial_state, transitions[], output_fn) - Finite State Machine
class FSMExprAST : public ExprAST
{
    int InitialState;
    std::vector<FSMTransition> Transitions;
    std::string OutputFn; // "moore" or "mealy"
public:
    FSMExprAST(int initState, std::string outputFn) : InitialState(initState), OutputFn(std::move(outputFn)) {}
    void addTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out);
    TypedValue codegen(CodegenContext& context) override;
    int getInitialState() const { return InitialState; }
    const std::vector<FSMTransition>& getTransitions() const { return Transitions; }
    const std::string& getOutputFn() const { return OutputFn; }
};

// edge(expr, type) - Edge trigger detection
// type: 1=posedge, -1=negedge, 0=any
class EdgeExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    int EdgeType;        // 1=posedge, -1=negedge, 0=any
    std::string EdgeStr; // "posedge", "negedge", or ""
public:
    EdgeExprAST(std::unique_ptr<ExprAST> Expr, int type, std::string edgeStr)
        : Expression(std::move(Expr)), EdgeType(type), EdgeStr(std::move(edgeStr))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    int getEdgeType() const { return EdgeType; }
    const std::string& getEdgeStr() const { return EdgeStr; }
};

// triggered(event_expr, body) - Execute body only on event
class TriggeredExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> EventExpr;
    std::unique_ptr<ExprAST> Body;

public:
    TriggeredExprAST(std::unique_ptr<ExprAST> Event, std::unique_ptr<ExprAST> B)
        : EventExpr(std::move(Event)), Body(std::move(B))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getEventExpr() const { return EventExpr.get(); }
    const ExprAST* getBody() const { return Body.get(); }
};

// --- Noise modeling primitives ---

// noise(type, amplitude, [freq]) - Noise generation function
// type: "white", "flicker", "thermal"
class NoiseExprAST : public ExprAST
{
    std::string NoiseType; // "white", "flicker", "thermal"
    std::unique_ptr<ExprAST> Amplitude;
    std::unique_ptr<ExprAST> Frequency; // Optional, for flicker noise corner freq
public:
    NoiseExprAST(std::string type, std::unique_ptr<ExprAST> amp, std::unique_ptr<ExprAST> freq = nullptr)
        : NoiseType(std::move(type)), Amplitude(std::move(amp)), Frequency(std::move(freq))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNoiseType() const { return NoiseType; }
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
    const ExprAST* getFrequency() const { return Frequency.get(); }
};

// white_noise(amplitude)
class WhiteNoiseExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Amplitude;

public:
    WhiteNoiseExprAST(std::unique_ptr<ExprAST> amp) : Amplitude(std::move(amp)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
};

// flicker_noise(amplitude, corner_freq)
class FlickerNoiseExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Amplitude;
    std::unique_ptr<ExprAST> CornerFreq;

public:
    FlickerNoiseExprAST(std::unique_ptr<ExprAST> amp, std::unique_ptr<ExprAST> cf)
        : Amplitude(std::move(amp)), CornerFreq(std::move(cf))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getAmplitude() const { return Amplitude.get(); }
    const ExprAST* getCornerFreq() const { return CornerFreq.get(); }
};

// thermal_noise(resistance, temperature)
class ThermalNoiseExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Resistance;
    std::unique_ptr<ExprAST> Temperature; // Kelvin, defaults to 300.15
public:
    ThermalNoiseExprAST(std::unique_ptr<ExprAST> res, std::unique_ptr<ExprAST> temp)
        : Resistance(std::move(res)), Temperature(std::move(temp))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getResistance() const { return Resistance.get(); }
    const ExprAST* getTemperature() const { return Temperature.get(); }
};

// --- Piecewise and table-based models ---

// Piecewise data point
struct PiecewisePoint
{
    std::unique_ptr<ExprAST> X;
    std::unique_ptr<ExprAST> Y;
    PiecewisePoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y) : X(std::move(x)), Y(std::move(y)) {}
};

// piecewise(points..., [interpolation], x) - Piecewise function with optional interpolation
// interpolation: "linear", "cubic", "spline", "step"
class PiecewiseExprAST : public ExprAST
{
    std::vector<PiecewisePoint> Points;
    std::string Interpolation;       // "linear", "cubic", "spline", "step"
    std::unique_ptr<ExprAST> QueryX; // The x value to evaluate
public:
    PiecewiseExprAST(std::string interp) : Interpolation(std::move(interp)) {}
    void addPoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y);
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<PiecewisePoint>& getPoints() const { return Points; }
    const std::string& getInterpolation() const { return Interpolation; }
    const ExprAST* getQueryX() const { return QueryX.get(); }
    void setQueryX(std::unique_ptr<ExprAST> x) { QueryX = std::move(x); }
};

// Table entry
struct TableEntry
{
    std::unique_ptr<ExprAST> Key;
    std::unique_ptr<ExprAST> Value;
    TableEntry(std::unique_ptr<ExprAST> k, std::unique_ptr<ExprAST> v) : Key(std::move(k)), Value(std::move(v)) {}
};

// table(entries..., default, key) - Table lookup with optional default
class TableExprAST : public ExprAST
{
    std::vector<TableEntry> Entries;
    std::unique_ptr<ExprAST> DefaultValue;
    std::unique_ptr<ExprAST> QueryKey;

public:
    TableExprAST() {}
    void addEntry(std::unique_ptr<ExprAST> key, std::unique_ptr<ExprAST> val);
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<TableEntry>& getEntries() const { return Entries; }
    const ExprAST* getDefaultValue() const { return DefaultValue.get(); }
    void setDefaultValue(std::unique_ptr<ExprAST> d) { DefaultValue = std::move(d); }
    const ExprAST* getQueryKey() const { return QueryKey.get(); }
    void setQueryKey(std::unique_ptr<ExprAST> k) { QueryKey = std::move(k); }
};

// csv_import(filename, [options]) - Import data from CSV
class CsvImportExprAST : public ExprAST
{
    std::string Filename;
    std::map<std::string, std::string> Options; // delimiter, header, skip_rows, etc.
public:
    CsvImportExprAST(std::string filename, std::map<std::string, std::string> opts)
        : Filename(std::move(filename)), Options(std::move(opts))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getFilename() const { return Filename; }
    const std::map<std::string, std::string>& getOptions() const { return Options; }
};

// --- Units and dimensional analysis ---

// unit(value, unit_string) - Annotate value with unit
class UnitExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;
    std::string UnitStr; // e.g., "V", "mA", "k", "F"
public:
    UnitExprAST(std::unique_ptr<ExprAST> val, std::string unit) : Value(std::move(val)), UnitStr(std::move(unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getUnitStr() const { return UnitStr; }
};

// dimension(expr) - Return dimensional analysis string
class DimensionExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;

public:
    DimensionExprAST(std::unique_ptr<ExprAST> expr) : Expression(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
};

// convert(value, from_unit, to_unit) - Unit conversion
class ConvertExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;
    std::string FromUnit;
    std::string ToUnit;

public:
    ConvertExprAST(std::unique_ptr<ExprAST> val, std::string from, std::string to)
        : Value(std::move(val)), FromUnit(std::move(from)), ToUnit(std::move(to))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getFromUnit() const { return FromUnit; }
    const std::string& getToUnit() const { return ToUnit; }
};

// has_unit(value, unit_string) - Check if value has specified unit
class HasUnitExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;
    std::string UnitStr;

public:
    HasUnitExprAST(std::unique_ptr<ExprAST> val, std::string unit) : Value(std::move(val)), UnitStr(std::move(unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getValue() const { return Value.get(); }
    const std::string& getUnitStr() const { return UnitStr; }
};
class PlotDeclAST : public ExprAST
{
    std::vector<std::string> Signals;
    std::string Title;
    std::vector<std::string> Colors;
    bool GridEnabled = false;
    bool AutoScale = false;

public:
    PlotDeclAST(std::vector<std::string> signalNames) : Signals(std::move(signalNames)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setTitle(const std::string& title) { Title = title; }
    void setColors(std::vector<std::string> colors) { Colors = std::move(colors); }
    void setGrid(bool enabled) { GridEnabled = enabled; }
    void setAutoScale(bool enabled) { AutoScale = enabled; }
};

class BenchmarkDeclAST : public ExprAST
{
    std::string Circuit;
    std::vector<std::string> Comparators;
    std::vector<std::string> Metrics;

public:
    BenchmarkDeclAST(std::string circuit) : Circuit(std::move(circuit)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setComparators(std::vector<std::string> comparators) { Comparators = std::move(comparators); }
    void setMetrics(std::vector<std::string> metrics) { Metrics = std::move(metrics); }
};

class OptimizeDeclAST : public ExprAST
{
    std::string Circuit;
    std::map<std::string, std::string> Goals;
    std::vector<std::string> TuneParams;
    std::string Algorithm;
    int MaxIterations = 0;
    bool GPUAccelerated = false;

public:
    OptimizeDeclAST(std::string circuit) : Circuit(std::move(circuit)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setGoals(std::map<std::string, std::string> goals) { Goals = std::move(goals); }
    void setTuneParams(std::vector<std::string> params) { TuneParams = std::move(params); }
    void setAlgorithm(const std::string& algo) { Algorithm = algo; }
    void setMaxIterations(int iter) { MaxIterations = iter; }
    void setGPUAccelerated(bool enabled) { GPUAccelerated = enabled; }
};

class SweepDeclAST : public ExprAST
{
    std::string Signal;
    std::map<std::string, std::map<std::string, std::string>> Controls;

public:
    SweepDeclAST(std::string signal) : Signal(std::move(signal)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addControl(const std::string& name, const std::string& type, const std::map<std::string, std::string>& params);
};

class ReportDeclAST : public ExprAST
{
    std::string Filename;
    std::vector<std::string> Sections;
    bool IncludePNG = false;
    bool IncludeCSV = false;

public:
    ReportDeclAST(std::string filename) : Filename(std::move(filename)) {}
    TypedValue codegen(CodegenContext& context) override;
    void setSections(std::vector<std::string> sections) { Sections = std::move(sections); }
    void setIncludePNG(bool include) { IncludePNG = include; }
    void setIncludeCSV(bool include) { IncludeCSV = include; }
};
class SpawnExprAST : public ExprAST
{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    SpawnExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args)
        : Callee(callee), Args(std::move(args))
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return Args; }
};

class JoinExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Handle;

public:
    JoinExprAST(std::unique_ptr<ExprAST> handle) : Handle(std::move(handle)) {}

    TypedValue codegen(CodegenContext& context) override;

    const ExprAST* getHandle() const { return Handle.get(); }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_MIXED_SIGNAL_H
