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

#ifndef FLUX_COMPILER_AST_SPICE_H
#define FLUX_COMPILER_AST_SPICE_H

#include "flux/compiler/ast_core.h"

namespace Flux {

class FunctionAST;

class VoltageExprAST : public ExprAST
{
    std::string NodeName;

public:
    VoltageExprAST(const std::string& NodeName) : NodeName(NodeName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNodeName() const { return NodeName; }
};

class CurrentExprAST : public ExprAST
{
    std::string BranchName;

public:
    CurrentExprAST(const std::string& BranchName) : BranchName(BranchName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getBranchName() const { return BranchName; }
};

class ParameterExprAST : public ExprAST
{
    std::string ParamName;

public:
    ParameterExprAST(const std::string& ParamName) : ParamName(ParamName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getParamName() const { return ParamName; }
    bool containsYield() const override { return false; }
};
class BuiltinVarExprAST : public ExprAST
{
    std::string Name;

public:
    explicit BuiltinVarExprAST(const std::string& Name) : Name(Name) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
};

// update(t, inputs) or update(t, inputs, outputs, state) function declaration
class UpdateFuncAST : public ExprAST
{
    std::string TimeVar;
    std::string InputsVar;
    std::string OutputsVar; // Optional: output array name
    std::string StateVar;   // Optional: state pointer name
    std::unique_ptr<ExprAST> Body;

public:
    UpdateFuncAST(const std::string& TimeVar, const std::string& InputsVar, std::unique_ptr<ExprAST> Body)
        : TimeVar(TimeVar), InputsVar(InputsVar), Body(std::move(Body))
    {
    }
    UpdateFuncAST(const std::string& TimeVar, const std::string& InputsVar, const std::string& OutputsVar,
                  const std::string& StateVar, std::unique_ptr<ExprAST> Body)
        : TimeVar(TimeVar), InputsVar(InputsVar), OutputsVar(OutputsVar), StateVar(StateVar), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getTimeVar() const { return TimeVar; }
    const std::string& getInputsVar() const { return InputsVar; }
    const std::string& getOutputsVar() const { return OutputsVar; }
    const std::string& getStateVar() const { return StateVar; }
    bool hasOutputs() const { return !OutputsVar.empty(); }
    bool hasState() const { return !StateVar.empty(); }
    const ExprAST* getBody() const { return Body.get(); }
};

// ============================================================================
// SPICE Behavioral Source AST Nodes
// ============================================================================

// B-source (arbitrary behavioral voltage/current source)
class BSourceExprAST : public ExprAST
{
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::unique_ptr<ExprAST> Expression; // The behavioral expression
    bool IsCurrent;                      // true for I-source, false for V-source
public:
    BSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   std::unique_ptr<ExprAST> Expr, bool IsCurrent = false)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode), Expression(std::move(Expr)), IsCurrent(IsCurrent)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const ExprAST* getExpression() const { return Expression.get(); }
    bool isCurrentSource() const { return IsCurrent; }
};

// E-device (Voltage-Controlled Voltage Source)
class ESourceExprAST : public ExprAST
{
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string ControlPosNode;
    std::string ControlNegNode;
    std::unique_ptr<ExprAST> Gain; // Can be expression or constant
public:
    ESourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& CtrlPosNode, const std::string& CtrlNegNode, std::unique_ptr<ExprAST> Gain)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode), ControlPosNode(CtrlPosNode),
          ControlNegNode(CtrlNegNode), Gain(std::move(Gain))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getControlPosNode() const { return ControlPosNode; }
    const std::string& getControlNegNode() const { return ControlNegNode; }
    const ExprAST* getGain() const { return Gain.get(); }
};

// F-device (Current-Controlled Current Source)
class FSourceExprAST : public ExprAST
{
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string VoltageSourceName; // Current measured through this V-source
    std::unique_ptr<ExprAST> Gain;

public:
    FSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& VSourceName, std::unique_ptr<ExprAST> Gain)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode), VoltageSourceName(VSourceName),
          Gain(std::move(Gain))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getVoltageSourceName() const { return VoltageSourceName; }
    const ExprAST* getGain() const { return Gain.get(); }
};

// G-device (Voltage-Controlled Current Source)
class GSourceExprAST : public ExprAST
{
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string ControlPosNode;
    std::string ControlNegNode;
    std::unique_ptr<ExprAST> Transconductance;

public:
    GSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& CtrlPosNode, const std::string& CtrlNegNode, std::unique_ptr<ExprAST> Transcond)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode), ControlPosNode(CtrlPosNode),
          ControlNegNode(CtrlNegNode), Transconductance(std::move(Transcond))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getControlPosNode() const { return ControlPosNode; }
    const std::string& getControlNegNode() const { return ControlNegNode; }
    const ExprAST* getTransconductance() const { return Transconductance.get(); }
};

// H-device (Current-Controlled Voltage Source)
class HSourceExprAST : public ExprAST
{
    std::string Name;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string VoltageSourceName; // Current measured through this V-source
    std::unique_ptr<ExprAST> Transresistance;

public:
    HSourceExprAST(const std::string& Name, const std::string& PosNode, const std::string& NegNode,
                   const std::string& VSourceName, std::unique_ptr<ExprAST> Transres)
        : Name(Name), PositiveNode(PosNode), NegativeNode(NegNode), VoltageSourceName(VSourceName),
          Transresistance(std::move(Transres))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getVoltageSourceName() const { return VoltageSourceName; }
    const ExprAST* getTransresistance() const { return Transresistance.get(); }
};

// ============================================================================
// SPICE Analysis Control AST Nodes
// ============================================================================

// Analysis type enumeration
enum class AnalysisType
{
    TRAN,   // Transient analysis
    DC,     // DC sweep
    AC,     // AC small-signal analysis
    NOISE,  // Noise analysis
    OP,     // Operating point
    TF,     // Transfer function
    SENS,   // Sensitivity analysis
    FOURIER // Fourier analysis
};

// Analysis directive
class AnalysisExprAST : public ExprAST
{
    AnalysisType Type;
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;

public:
    AnalysisExprAST(AnalysisType Type) : Type(Type) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    AnalysisType getAnalysisType() const { return Type; }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Measurement type enumeration
enum class MeasureType
{
    MAX,   // Maximum value
    MIN,   // Minimum value
    AVG,   // Average value
    RMS,   // Root mean square
    TRIG,  // Trigger (rise/fall time)
    TARG,  // Target (delay, etc.)
    WHEN,  // Time when condition is met
    FIND,  // Find value at specific time/condition
    DERIV, // Derivative
    INTEG  // Integral
};

// Measurement directive
class MeasureExprAST : public ExprAST
{
    std::string Name;
    MeasureType Type;
    std::unique_ptr<ExprAST> Expression;
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;

public:
    MeasureExprAST(const std::string& Name, MeasureType Type, std::unique_ptr<ExprAST> Expr)
        : Name(Name), Type(Type), Expression(std::move(Expr))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    const std::string& getName() const { return Name; }
    MeasureType getMeasureType() const { return Type; }
    const ExprAST* getExpression() const { return Expression.get(); }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Probe directive
class ProbeExprAST : public ExprAST
{
    std::string VariableName;
    std::string OutputName; // Optional custom name for output
public:
    ProbeExprAST(const std::string& VarName, const std::string& OutName = "")
        : VariableName(VarName), OutputName(OutName)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVariableName() const { return VariableName; }
    const std::string& getOutputName() const { return OutputName; }
};

// Save directive
class SaveExprAST : public ExprAST
{
    std::string VariableName;

public:
    explicit SaveExprAST(const std::string& VarName) : VariableName(VarName) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVariableName() const { return VariableName; }
};

// ============================================================================
// SPICE Subcircuit and Model AST Nodes
// ============================================================================

// Subcircuit definition
class SubcktExprAST : public ExprAST
{
    std::string Name;
    std::vector<std::string> Pins;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> Parameters;
    std::vector<std::unique_ptr<ExprAST>> Body;
    std::vector<std::unique_ptr<FunctionAST>> Functions;

public:
    SubcktExprAST(const std::string& Name, std::vector<std::string> Pins) : Name(Name), Pins(std::move(Pins)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    void addStatement(std::unique_ptr<ExprAST> Stmt);
    void addFunction(std::unique_ptr<FunctionAST> Func);
    const std::string& getName() const { return Name; }
    const std::vector<std::string>& getPins() const { return Pins; }
    const std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>& getParameters() const { return Parameters; }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    const std::vector<std::unique_ptr<FunctionAST>>& getFunctions() const { return Functions; }
};

// Model declaration
class ModelExprAST : public ExprAST
{
    std::string Name;
    std::string ModelType; // D, NPN, PNP, NMOS, PMOS, R, L, C, SW, T, K
    std::map<std::string, std::unique_ptr<ExprAST>> Parameters;

public:
    ModelExprAST(const std::string& Name, const std::string& Type) : Name(Name), ModelType(Type) {}
    TypedValue codegen(CodegenContext& context) override;
    void addParameter(const std::string& Name, std::unique_ptr<ExprAST> Value);
    const std::string& getName() const { return Name; }
    const std::string& getModelType() const { return ModelType; }
    const std::map<std::string, std::unique_ptr<ExprAST>>& getParameters() const { return Parameters; }
};

// Parameter declaration
class ParamExprAST : public ExprAST
{
    std::string Name;
    std::unique_ptr<ExprAST> Value;

public:
    ParamExprAST(const std::string& Name, std::unique_ptr<ExprAST> Value) : Name(Name), Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const ExprAST* getValue() const { return Value.get(); }
};

// Initial condition
class ICExprAST : public ExprAST
{
    std::string NodeName;
    std::unique_ptr<ExprAST> Value;

public:
    ICExprAST(const std::string& NodeName, std::unique_ptr<ExprAST> Value) : NodeName(NodeName), Value(std::move(Value))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getNodeName() const { return NodeName; }
    const ExprAST* getValue() const { return Value.get(); }
};

// Worst-Case Analysis
class WorstCaseExprAST : public ExprAST
{
    std::string OutputName;
    std::map<std::string, double> ComponentNominal;
    std::map<std::string, double> ComponentTolerance;

public:
    WorstCaseExprAST(const std::string& output) : OutputName(output) {}
    void addComponent(const std::string& name, double nominal, double tolerance)
    {
        ComponentNominal[name] = nominal;
        ComponentTolerance[name] = tolerance;
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
    const std::map<std::string, double>& getComponentNominal() const { return ComponentNominal; }
};

// Stability Analysis
class StabilityExprAST : public ExprAST
{
    std::string OutputName;

public:
    StabilityExprAST(const std::string& output) : OutputName(output) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// Sensitivity Analysis
class SensitivityExprAST : public ExprAST
{
    std::string OutputName;

public:
    SensitivityExprAST(const std::string& output) : OutputName(output) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// Optimization
class OptimizationExprAST : public ExprAST
{
    std::string OutputName;
    std::vector<std::string> VarNames;
    std::vector<double> InitVals;
    std::vector<double> MinVals;
    std::vector<double> MaxVals;

public:
    OptimizationExprAST(const std::string& outputName, const std::vector<std::string>& varNames,
                        const std::vector<double>& initVals, const std::vector<double>& minVals,
                        const std::vector<double>& maxVals)
        : OutputName(outputName), VarNames(varNames), InitVals(initVals), MinVals(minVals), MaxVals(maxVals)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getOutputName() const { return OutputName; }
};

// Bode Plot Analysis
class BodeExprAST : public ExprAST
{
    std::string OutputName;
    double FreqStart;
    double FreqEnd;
    int PointsPerDecade;

public:
    BodeExprAST(const std::string& outputName, double freqStart, double freqEnd, int pointsPerDecade)
        : OutputName(outputName), FreqStart(freqStart), FreqEnd(freqEnd), PointsPerDecade(pointsPerDecade)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// ASCII Plot
class PlotExprAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    PlotExprAST(std::vector<std::unique_ptr<ExprAST>> args) : Args(std::move(args)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// FFT Analysis
class FFTExprAST : public ExprAST
{
    std::string SignalName;

public:
    FFTExprAST(const std::string& signal) : SignalName(signal) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getSignalName() const { return SignalName; }
};
enum class ADeviceType
{
    BUF,     // Buffer
    NOT,     // Inverter
    AND,     // AND gate
    OR,      // OR gate
    NAND,    // NAND gate
    NOR,     // NOR gate
    XOR,     // XOR gate
    XNOR,    // XNOR gate
    SCHMITT, // Schmitt trigger
    DFF,     // D flip-flop
    JKFF,    // JK flip-flop
    SRFF,    // SR flip-flop
    DLATCH,  // D latch
    COUNTER  // Counter/frequency divider
};

// A-device instance: A1 [in1 in2] out d_and  or  A1 d clk q nq d_dff
class ADeviceExprAST : public ExprAST
{
    std::string InstanceName;
    ADeviceType DeviceType;
    std::vector<std::string> InputNodes;
    std::vector<std::string> OutputNodes;

public:
    ADeviceExprAST(const std::string& name, ADeviceType type) : InstanceName(name), DeviceType(type) {}
    TypedValue codegen(CodegenContext& context) override;

    void addInputNode(const std::string& node) { InputNodes.push_back(node); }
    void addOutputNode(const std::string& node) { OutputNodes.push_back(node); }

    const std::string& getInstanceName() const { return InstanceName; }
    ADeviceType getDeviceType() const { return DeviceType; }
    const std::vector<std::string>& getInputNodes() const { return InputNodes; }
    const std::vector<std::string>& getOutputNodes() const { return OutputNodes; }

    static std::string deviceTypeToString(ADeviceType type);
    static std::string deviceTypeToModelName(ADeviceType type);
    static int deviceTypeToInputCount(ADeviceType type);
    static int deviceTypeToOutputCount(ADeviceType type);
};

// ============================================================================
// VioMATRIXC Integration: WAVEFILE Voltage/Current Source
// ============================================================================

// WAVEFILE source: V1 out 0 WAVEFILE "audio.wav" CHAN 0
class WaveFileExprAST : public ExprAST
{
    std::string InstanceName;
    std::string PositiveNode;
    std::string NegativeNode;
    std::string FilePath;
    int Channel = 0;
    bool IsCurrent = false;

public:
    WaveFileExprAST(const std::string& name, const std::string& posNode, const std::string& negNode,
                    const std::string& filePath)
        : InstanceName(name), PositiveNode(posNode), NegativeNode(negNode), FilePath(filePath)
    {
    }
    TypedValue codegen(CodegenContext& context) override;

    void setChannel(int ch) { Channel = ch; }
    void setCurrent(bool isCurrent) { IsCurrent = isCurrent; }

    const std::string& getInstanceName() const { return InstanceName; }
    const std::string& getPositiveNode() const { return PositiveNode; }
    const std::string& getNegativeNode() const { return NegativeNode; }
    const std::string& getFilePath() const { return FilePath; }
    int getChannel() const { return Channel; }
    bool isCurrentSource() const { return IsCurrent; }
};
using SubcktAST = SubcktExprAST;
using ModelAST = ModelExprAST;
using AnalysisDeclAST = AnalysisExprAST;
using MeasureDeclAST = MeasureExprAST;
using ProbeDeclAST = ProbeExprAST;

} // namespace Flux

#endif // FLUX_COMPILER_AST_SPICE_H
