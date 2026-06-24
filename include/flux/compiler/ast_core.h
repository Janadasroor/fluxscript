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

#ifndef FLUX_COMPILER_AST_CORE_H
#define FLUX_COMPILER_AST_CORE_H

#include "flux/compiler/codegen_context.h"
#include "flux/compiler/lexer.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

class PrototypeAST;
class FunctionAST;
class StructDeclAST;
class EnumDeclAST;
class ImplDeclAST;
class TraitDeclAST;

class ExprAST
{
    int Line = 0;
    int Col = 0;

public:
    virtual ~ExprAST() = default;
    virtual TypedValue codegen(CodegenContext& context) = 0;
    void setLocation(int L, int C)
    {
        Line = L;
        Col = C;
    }
    int getLine() const { return Line; }
    int getCol() const { return Col; }
    virtual bool containsYield() const { return false; }
    virtual bool containsAwait() const { return false; }
};

class NumberExprAST : public ExprAST
{
    double Val;
    std::string Unit;

public:
    NumberExprAST(double Val, std::string Unit = "") : Val(Val), Unit(std::move(Unit)) {}
    TypedValue codegen(CodegenContext& context) override;
    double getValue() const { return Val; }
    const std::string& getUnit() const { return Unit; }
};

class IntExprAST : public ExprAST
{
    int64_t Val;

public:
    IntExprAST(int64_t Val) : Val(Val) {}
    TypedValue codegen(CodegenContext& context) override;
    int64_t getValue() const { return Val; }
};

class FixedExprAST : public ExprAST
{
    double Val;
    int Bits;
    int Fract;

public:
    FixedExprAST(double Val, int Bits, int Fract) : Val(Val), Bits(Bits), Fract(Fract) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return false; }
};

class ToFixedExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;
    int Bits;
    int Fract;

public:
    ToFixedExprAST(std::unique_ptr<ExprAST> Val, int Bits, int Fract) : Value(std::move(Val)), Bits(Bits), Fract(Fract)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Value->containsYield(); }
};

class ComplexExprAST : public ExprAST
{
    double Real;
    double Imag;

public:
    ComplexExprAST(double Real, double Imag) : Real(Real), Imag(Imag) {}
    TypedValue codegen(CodegenContext& context) override;
    double getReal() const { return Real; }
    double getImag() const { return Imag; }
};

class PhasorExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Magnitude;
    std::unique_ptr<ExprAST> PhaseDeg;

public:
    PhasorExprAST(std::unique_ptr<ExprAST> mag, std::unique_ptr<ExprAST> phase)
        : Magnitude(std::move(mag)), PhaseDeg(std::move(phase))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

class StringExprAST : public ExprAST
{
    std::string Val;

public:
    StringExprAST(const std::string& Val) : Val(Val) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getValue() const { return Val; }
};

struct InterpolatedPart
{
    bool isExpr;
    std::string text;
    std::unique_ptr<ExprAST> expr;
};

class InterpolatedStringExprAST : public ExprAST
{
    std::vector<InterpolatedPart> Parts;

public:
    InterpolatedStringExprAST(std::vector<InterpolatedPart> parts) : Parts(std::move(parts)) {}
    TypedValue codegen(CodegenContext& context) override;
};

class BoolExprAST : public ExprAST
{
    bool Val;

public:
    BoolExprAST(bool Val) : Val(Val) {}
    TypedValue codegen(CodegenContext& context) override;
    bool getValue() const { return Val; }
};

class ImportExprAST : public ExprAST
{
    std::string ModuleName;
    std::string VersionSpec;          // Version specification (e.g., "1.0.0", ">=1.0.0")
    std::string Alias;                // Optional namespace alias
    std::vector<std::string> Symbols; // Specific symbols to import

public:
    ImportExprAST(const std::string& ModuleName, const std::string& VersionSpec = "", const std::string& Alias = "",
                  const std::vector<std::string>& Symbols = {})
        : ModuleName(ModuleName), VersionSpec(VersionSpec), Alias(Alias), Symbols(Symbols)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getModuleName() const { return ModuleName; }
    const std::string& getVersionSpec() const { return VersionSpec; }
    const std::string& getAlias() const { return Alias; }
    const std::vector<std::string>& getSymbols() const { return Symbols; }
};
class DebugStmtAST : public ExprAST
{
    std::string CircuitFile;
    std::string Symptom;
    std::map<std::string, std::string> Options;

public:
    DebugStmtAST(const std::string& circuit, const std::string& symptom, const std::map<std::string, std::string>& opts)
        : CircuitFile(circuit), Symptom(symptom), Options(opts)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Sensitivity analysis AST
class SensitivityStmtAST : public ExprAST
{
    std::string OutputVar;
    std::vector<std::string> Params;
    std::string CircuitFile;

public:
    SensitivityStmtAST(const std::string& output, const std::vector<std::string>& params,
                       const std::string& circuit = "")
        : OutputVar(output), Params(params), CircuitFile(circuit)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Ask question AST
class AskExprAST : public ExprAST
{
    std::string Question;

public:
    explicit AskExprAST(const std::string& question) : Question(question) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Explain circuit AST
class ExplainExprAST : public ExprAST
{
    std::string CircuitFile;
    std::string Aspect;

public:
    ExplainExprAST(const std::string& circuit, const std::string& aspect = "") : CircuitFile(circuit), Aspect(aspect) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Component substitution AST
class SubstituteStmtAST : public ExprAST
{
    std::string PartNumber;
    std::vector<std::string> Options;

public:
    SubstituteStmtAST(const std::string& part, const std::vector<std::string>& opts) : PartNumber(part), Options(opts)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};
class VariableExprAST : public ExprAST
{
    std::string Name;
    std::vector<FluxType> TypeArgs;

public:
    VariableExprAST(const std::string& Name) : Name(Name) {}
    VariableExprAST(const std::string& Name, std::vector<FluxType> TypeArgs) : Name(Name), TypeArgs(std::move(TypeArgs))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getName() const { return Name; }
    const std::vector<FluxType>& getTypeArgs() const { return TypeArgs; }
    void setTypeArgs(std::vector<FluxType> Args) { TypeArgs = std::move(Args); }
};

class MemberExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Object;
    std::string MemberName;

public:
    MemberExprAST(std::unique_ptr<ExprAST> Object, const std::string& MemberName)
        : Object(std::move(Object)), MemberName(MemberName)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    ExprAST* getObject() { return Object.get(); }
    const ExprAST* getObject() const { return Object.get(); }
    const std::string& getMemberName() const { return MemberName; }
    bool containsYield() const override { return Object->containsYield(); }
};

class BinaryExprAST : public ExprAST
{
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    int getOp() const { return Op; }
    const ExprAST* getLHS() const { return LHS.get(); }
    const ExprAST* getRHS() const { return RHS.get(); }
    std::unique_ptr<ExprAST> takeLHS() { return std::move(LHS); }
    std::unique_ptr<ExprAST> takeRHS() { return std::move(RHS); }
    bool containsYield() const override { return LHS->containsYield() || RHS->containsYield(); }
};

class UnaryExprAST : public ExprAST
{
    int Op; // Changed to int to support multi-character operators
    std::unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(int Op, std::unique_ptr<ExprAST> Operand) : Op(Op), Operand(std::move(Operand)) {}
    TypedValue codegen(CodegenContext& context) override;
    int getOp() const { return Op; }
    const ExprAST* getOperand() const { return Operand.get(); }
    bool containsYield() const override { return Operand->containsYield(); }
};

class TransposeExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Operand;

public:
    TransposeExprAST(std::unique_ptr<ExprAST> Operand) : Operand(std::move(Operand)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getOperand() const { return Operand.get(); }
    bool containsYield() const override { return Operand->containsYield(); }
};

class CallExprAST : public ExprAST
{
    std::string Callee;
    std::unique_ptr<ExprAST> CalleeExpr; // expression-based callee (e.g., obj.method)
    std::vector<std::unique_ptr<ExprAST>> Args;
    std::vector<FluxType> GenericTypeArgs; // Explicit type args for generic calls (e.g., foo[Int, Double](x))

public:
    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args))
    {
    }

    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args,
                std::vector<FluxType> GenericTypeArgs)
        : Callee(Callee), Args(std::move(Args)), GenericTypeArgs(std::move(GenericTypeArgs))
    {
    }

    // Expression-based callee (e.g., obj.method(args))
    CallExprAST(std::unique_ptr<ExprAST> CalleeExpr, std::vector<std::unique_ptr<ExprAST>> Args)
        : CalleeExpr(std::move(CalleeExpr)), Args(std::move(Args))
    {
    }

    TypedValue codegen(CodegenContext& context) override;
    const std::string& getCallee() const { return Callee; }
    ExprAST* getCalleeExpr() const { return CalleeExpr.get(); }
    bool hasCalleeExpr() const { return CalleeExpr != nullptr; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return Args; }
    void prependArg(std::unique_ptr<ExprAST> Arg) { Args.insert(Args.begin(), std::move(Arg)); }
    const std::vector<FluxType>& getGenericTypeArgs() const { return GenericTypeArgs; }
    void setGenericTypeArgs(const std::vector<FluxType>& TArgs) { GenericTypeArgs = TArgs; }
    bool hasGenericTypeArgs() const { return !GenericTypeArgs.empty(); }
    bool containsYield() const override
    {
        if (CalleeExpr && CalleeExpr->containsYield())
            return true;
        for (const auto& Arg : Args) {
            if (Arg->containsYield())
                return true;
        }
        return false;
    }
};

class AssignExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> LHS;
    std::unique_ptr<ExprAST> Val;
    int Op;

public:
    AssignExprAST(std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> Val, int Op = 0)
        : LHS(std::move(LHS)), Val(std::move(Val)), Op(Op)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getLHS() const { return LHS.get(); }
    const ExprAST* getValueExpr() const { return Val.get(); }
    int getAssignmentOp() const { return Op; }
    bool containsYield() const override { return LHS->containsYield() || Val->containsYield(); }
};
class LetExprAST : public ExprAST
{
    std::string VarName;
    FluxType Type;
    std::unique_ptr<ExprAST> Init;
    std::unique_ptr<ExprAST> Body;

public:
    LetExprAST(const std::string& VarName, FluxType Type, std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Type(Type), Init(std::move(Init)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
    const FluxType& getDeclaredType() const { return Type; }
    const ExprAST* getInit() const { return Init.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override { return (Init && Init->containsYield()) || (Body && Body->containsYield()); }
};

class LambdaExprAST : public ExprAST
{
    std::vector<std::string> Args;
    std::unique_ptr<ExprAST> Body;

public:
    LambdaExprAST(std::vector<std::string> Args, std::unique_ptr<ExprAST> Body)
        : Args(std::move(Args)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::string>& getArgs() const { return Args; }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override { return Body && Body->containsYield(); }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_CORE_H
