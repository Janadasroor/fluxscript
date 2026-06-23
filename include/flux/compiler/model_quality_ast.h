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

#ifndef FLUX_MODEL_QUALITY_AST_H
#define FLUX_MODEL_QUALITY_AST_H

#include "flux/compiler/ast.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

// ============================================================================
// Transient Assertion System
// ============================================================================

// Assertion: assert V(node) < bound [within time]
class AssertDeclAST : public ExprAST
{
    std::string Node;
    std::string Operator; // "<", ">", "<=", ">=", "=="
    double Bound;
    double WithinTime; // 0.0 means entire simulation
    std::string Message;

public:
    AssertDeclAST(const std::string& node, const std::string& op, double bound)
        : Node(node), Operator(op), Bound(bound), WithinTime(0.0)
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    void setWithinTime(double t) { WithinTime = t; }
    void setMessage(const std::string& msg) { Message = msg; }

    const std::string& getNode() const { return Node; }
    const std::string& getOperator() const { return Operator; }
    double getBound() const { return Bound; }
    double getWithinTime() const { return WithinTime; }
    const std::string& getMessage() const { return Message; }
};

// Settling time check: settle V(node) within tolerance% after time
class SettleDeclAST : public ExprAST
{
    std::string Node;
    double TolerancePercent;
    double AfterTime;

public:
    SettleDeclAST(const std::string& node, double tol, double afterTime)
        : Node(node), TolerancePercent(tol), AfterTime(afterTime)
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    double getTolerancePercent() const { return TolerancePercent; }
    double getAfterTime() const { return AfterTime; }
};

// ============================================================================
// Golden Waveform Regression Framework
// ============================================================================

// Golden waveform reference: golden "name" from file
class GoldenDeclAST : public ExprAST
{
    std::string Name;
    std::string Filename;
    std::vector<double> TimePoints;
    std::vector<double> Values;

public:
    GoldenDeclAST(const std::string& name, const std::string& filename) : Name(name), Filename(filename) {}

    TypedValue codegen(CodegenContext& context) override;

    void loadWaveform();
    void addPoint(double t, double v)
    {
        TimePoints.push_back(t);
        Values.push_back(v);
    }

    const std::string& getName() const { return Name; }
    const std::string& getFilename() const { return Filename; }
    const std::vector<double>& getTimePoints() const { return TimePoints; }
    const std::vector<double>& getValues() const { return Values; }
};

// Waveform comparison: compare V(node) with golden "name" tolerance tol%
class CompareDeclAST : public ExprAST
{
    std::string Node;
    std::string GoldenName;
    double TolerancePercent;

public:
    CompareDeclAST(const std::string& node, const std::string& golden, double tol)
        : Node(node), GoldenName(golden), TolerancePercent(tol)
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    const std::string& getGoldenName() const { return GoldenName; }
    double getTolerancePercent() const { return TolerancePercent; }
};

// ============================================================================
// Convergence Diagnostics Toolkit
// ============================================================================

// Convergence check: converge V(node) max_iter epsilon
class ConvergeDeclAST : public ExprAST
{
    std::string Node;
    int MaxIterations;
    double Epsilon;

public:
    ConvergeDeclAST(const std::string& node, int maxIter, double eps) : Node(node), MaxIterations(maxIter), Epsilon(eps)
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    int getMaxIterations() const { return MaxIterations; }
    double getEpsilon() const { return Epsilon; }
};

// Discontinuity detection: discontinuity V(node) threshold
class DiscontinuityDeclAST : public ExprAST
{
    std::string Node;
    double Threshold;

public:
    DiscontinuityDeclAST(const std::string& node, double threshold) : Node(node), Threshold(threshold) {}

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    double getThreshold() const { return Threshold; }
};

// Hidden state detection: state V(node) history_depth
class StateDeclAST : public ExprAST
{
    std::string Node;
    int HistoryDepth;

public:
    StateDeclAST(const std::string& node, int depth) : Node(node), HistoryDepth(depth) {}

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    int getHistoryDepth() const { return HistoryDepth; }
};

// Verification block: verify { assertions... }
class VerifyBlockAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> Checks;
    std::vector<std::unique_ptr<ExprAST>> Comparisons;
    std::vector<std::unique_ptr<ExprAST>> Diagnostics;

public:
    VerifyBlockAST() {}

    TypedValue codegen(CodegenContext& context) override;

    void addCheck(std::unique_ptr<ExprAST> check) { Checks.push_back(std::move(check)); }
    void addComparison(std::unique_ptr<ExprAST> comp) { Comparisons.push_back(std::move(comp)); }
    void addDiagnostic(std::unique_ptr<ExprAST> diag) { Diagnostics.push_back(std::move(diag)); }

    const std::vector<std::unique_ptr<ExprAST>>& getChecks() const { return Checks; }
    const std::vector<std::unique_ptr<ExprAST>>& getComparisons() const { return Comparisons; }
    const std::vector<std::unique_ptr<ExprAST>>& getDiagnostics() const { return Diagnostics; }
};

// Tolerance specification: tolerance abs=value rel=value%
class ToleranceDeclAST : public ExprAST
{
    double AbsoluteTolerance;
    double RelativeTolerancePercent;

public:
    ToleranceDeclAST(double absTol, double relTol) : AbsoluteTolerance(absTol), RelativeTolerancePercent(relTol) {}

    TypedValue codegen(CodegenContext& context) override;

    double getAbsoluteTolerance() const { return AbsoluteTolerance; }
    double getRelativeTolerancePercent() const { return RelativeTolerancePercent; }
};

// Standalone diagnostic directive: diagnostic V(node) [type="overshoot"] [threshold=0.1]
class DiagnosticDeclAST : public ExprAST
{
    std::string Node;
    std::string DiagnosticType;
    double Threshold;

public:
    DiagnosticDeclAST(const std::string& node, const std::string& type = "", double threshold = 0.0)
        : Node(node), DiagnosticType(type), Threshold(threshold)
    {
    }

    TypedValue codegen(CodegenContext& context) override;

    const std::string& getNode() const { return Node; }
    const std::string& getDiagnosticType() const { return DiagnosticType; }
    double getThreshold() const { return Threshold; }
};

} // namespace Flux

#endif // FLUX_MODEL_QUALITY_AST_H
