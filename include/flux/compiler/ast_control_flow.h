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

#ifndef FLUX_COMPILER_AST_CONTROL_FLOW_H
#define FLUX_COMPILER_AST_CONTROL_FLOW_H

#include "flux/compiler/ast_core.h"

namespace Flux {

class IfExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond, Then, Else;

public:
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getThen() const { return Then.get(); }
    const ExprAST* getElse() const { return Else.get(); }
    bool containsYield() const override
    {
        if (Cond->containsYield())
            return true;
        if (Then->containsYield())
            return true;
        if (Else && Else->containsYield())
            return true;
        return false;
    }
};

class ForExprAST : public ExprAST
{
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step;
    std::unique_ptr<ExprAST> Body;

public:
    ForExprAST(const std::string& VarName, std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
               std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)), Step(std::move(Step)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
    const ExprAST* getStart() const { return Start.get(); }
    const ExprAST* getEnd() const { return End.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override
    {
        if (Start->containsYield())
            return true;
        if (End->containsYield())
            return true;
        if (Step && Step->containsYield())
            return true;
        if (Body->containsYield())
            return true;
        return false;
    }
};
class WhileExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond, Body;

public:
    WhileExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Body)
        : Cond(std::move(Cond)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override { return Cond->containsYield() || Body->containsYield(); }
};

class DoWhileExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Body, Cond;

public:
    DoWhileExprAST(std::unique_ptr<ExprAST> Body, std::unique_ptr<ExprAST> Cond)
        : Body(std::move(Body)), Cond(std::move(Cond))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getBody() const { return Body.get(); }
    const ExprAST* getCond() const { return Cond.get(); }
    bool containsYield() const override { return Body->containsYield() || Cond->containsYield(); }
};

// ============================================================================
// Statement-based Control Flow (void-returning, no PHI nodes)
// Syntax: if (cond) { stmts } else { stmts }
//         for (init; cond; step) { stmts }
//         while (cond) { stmts }
// ============================================================================

class IfStmtAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>> ThenBody;
    std::vector<std::unique_ptr<ExprAST>> ElseBody;

public:
    IfStmtAST(std::unique_ptr<ExprAST> Cond, std::vector<std::unique_ptr<ExprAST>> ThenBody,
              std::vector<std::unique_ptr<ExprAST>> ElseBody)
        : Cond(std::move(Cond)), ThenBody(std::move(ThenBody)), ElseBody(std::move(ElseBody))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getThenBody() const { return ThenBody; }
    const std::vector<std::unique_ptr<ExprAST>>& getElseBody() const { return ElseBody; }
    bool containsYield() const override
    {
        if (Cond->containsYield())
            return true;
        for (const auto& Stmt : ThenBody) {
            if (Stmt->containsYield())
                return true;
        }
        for (const auto& Stmt : ElseBody) {
            if (Stmt->containsYield())
                return true;
        }
        return false;
    }
};

class ForStmtAST : public ExprAST
{
    std::unique_ptr<ExprAST> Init;
    std::unique_ptr<ExprAST> Cond;
    std::unique_ptr<ExprAST> Step;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    ForStmtAST(std::unique_ptr<ExprAST> Init, std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Step,
               std::vector<std::unique_ptr<ExprAST>> Body)
        : Init(std::move(Init)), Cond(std::move(Cond)), Step(std::move(Step)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getInit() const { return Init.get(); }
    const ExprAST* getCond() const { return Cond.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    bool containsYield() const override
    {
        if (Init && Init->containsYield())
            return true;
        if (Cond && Cond->containsYield())
            return true;
        if (Step && Step->containsYield())
            return true;
        for (const auto& Stmt : Body) {
            if (Stmt->containsYield())
                return true;
        }
        return false;
    }
};

class WhileStmtAST : public ExprAST
{
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    WhileStmtAST(std::unique_ptr<ExprAST> Cond, std::vector<std::unique_ptr<ExprAST>> Body)
        : Cond(std::move(Cond)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
    bool containsYield() const override
    {
        if (Cond && Cond->containsYield())
            return true;
        for (const auto& Stmt : Body) {
            if (Stmt->containsYield())
                return true;
        }
        return false;
    }
};
class CaseClauseAST
{
    std::unique_ptr<ExprAST> Value;
    std::vector<std::unique_ptr<ExprAST>> Body;

public:
    CaseClauseAST(std::unique_ptr<ExprAST> Value, std::vector<std::unique_ptr<ExprAST>> Body)
        : Value(std::move(Value)), Body(std::move(Body))
    {
    }
    ExprAST* getValue() const { return Value.get(); }
    const std::vector<std::unique_ptr<ExprAST>>& getBody() const { return Body; }
};

class SwitchExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Condition;
    std::vector<CaseClauseAST> Cases;
    std::vector<std::unique_ptr<ExprAST>> DefaultBody;

public:
    SwitchExprAST(std::unique_ptr<ExprAST> Cond, std::vector<CaseClauseAST> Cases,
                  std::vector<std::unique_ptr<ExprAST>> DefaultBody)
        : Condition(std::move(Cond)), Cases(std::move(Cases)), DefaultBody(std::move(DefaultBody))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    ExprAST* getCondition() const { return Condition.get(); }
    const std::vector<CaseClauseAST>& getCases() const { return Cases; }
    const std::vector<std::unique_ptr<ExprAST>>& getDefaultBody() const { return DefaultBody; }
    bool containsYield() const override
    {
        if (Condition && Condition->containsYield())
            return true;
        for (const auto& Clause : Cases) {
            if (Clause.getValue() && Clause.getValue()->containsYield())
                return true;
            for (const auto& Stmt : Clause.getBody()) {
                if (Stmt->containsYield())
                    return true;
            }
        }
        for (const auto& Stmt : DefaultBody) {
            if (Stmt->containsYield())
                return true;
        }
        return false;
    }
};

// Break and Continue statements
class BreakExprAST : public ExprAST
{
public:
    BreakExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

class ContinueExprAST : public ExprAST
{
public:
    ContinueExprAST() {}
    TypedValue codegen(CodegenContext& context) override;
};

class ReturnExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Val;

public:
    ReturnExprAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getVal() const { return Val.get(); }
    bool containsYield() const override { return Val && Val->containsYield(); }
};

// Try/Catch/Finally
class TryCatchExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> TryBody;
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> CatchClauses; // (exception_var, handler)
    std::unique_ptr<ExprAST> FinallyBody;

public:
    TryCatchExprAST(std::unique_ptr<ExprAST> TryBody) : TryBody(std::move(TryBody)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addCatch(const std::string& var, std::unique_ptr<ExprAST> handler);
    void setFinally(std::unique_ptr<ExprAST> body);
    const ExprAST* getTryBody() const { return TryBody.get(); }
    const std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>>& getCatchClauses() const
    {
        return CatchClauses;
    }
    const ExprAST* getFinallyBody() const { return FinallyBody.get(); }
    bool containsYield() const override
    {
        if (TryBody->containsYield())
            return true;
        for (const auto& C : CatchClauses)
            if (C.second->containsYield())
                return true;
        if (FinallyBody && FinallyBody->containsYield())
            return true;
        return false;
    }
};

// Throw exception
class ThrowExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Exception;

public:
    ThrowExprAST(std::unique_ptr<ExprAST> Exception) : Exception(std::move(Exception)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Exception->containsYield(); }
};

// `expr?` — early-return propagation for Result/Option enums.
// If the inner expression's tag is the error/None variant, branch to a
// per-function early-return block passing the value through unchanged.
// Otherwise, extract the Ok/Some payload and use it as the result.
class TryPropagateExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Inner;

public:
    explicit TryPropagateExprAST(std::unique_ptr<ExprAST> Inner) : Inner(std::move(Inner)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return Inner->containsYield(); }
};

// Assert
class AssertExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Condition;
    std::string Message;

public:
    AssertExprAST(std::unique_ptr<ExprAST> Cond, std::string Msg) : Condition(std::move(Cond)), Message(std::move(Msg))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Yield (generator)
class YieldExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;

public:
    YieldExprAST(std::unique_ptr<ExprAST> Value) : Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsYield() const override { return true; }
};

// Await (async)
class AwaitExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;

public:
    AwaitExprAST(std::unique_ptr<ExprAST> Value) : Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    bool containsAwait() const override { return true; }
    const ExprAST* getExpr() const { return Value.get(); }
};
class MatchExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Value;
    std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>, std::unique_ptr<ExprAST>>> Arms; // (patterns, result)
    std::unique_ptr<ExprAST> DefaultArm;
    std::vector<std::vector<std::string>> Bindings; // variable names for payload extraction per arm (empty means none)
    // Named-field bindings per arm: pairs of (field_name, binding_var_name) for `{ field: var }` patterns.
    // Empty inner vector means the arm uses positional or no bindings.
    std::vector<std::vector<std::pair<std::string, std::string>>> NamedFieldBindings;

public:
    MatchExprAST(std::unique_ptr<ExprAST> Value) : Value(std::move(Value)) {}
    TypedValue codegen(CodegenContext& context) override;
    void addArm(std::vector<std::unique_ptr<ExprAST>> patterns, std::unique_ptr<ExprAST> result,
                const std::vector<std::string>& bindings = {},
                const std::vector<std::pair<std::string, std::string>>& namedBindings = {});
    void setDefault(std::unique_ptr<ExprAST> arm);
    const ExprAST* getValue() const { return Value.get(); }
    const std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>, std::unique_ptr<ExprAST>>>& getArms() const
    {
        return Arms;
    }
    const std::vector<std::vector<std::string>>& getBindings() const { return Bindings; }
    const std::vector<std::vector<std::pair<std::string, std::string>>>& getNamedFieldBindings() const
    {
        return NamedFieldBindings;
    }
    const ExprAST* getDefaultArm() const { return DefaultArm.get(); }
    bool containsYield() const override
    {
        if (Value->containsYield())
            return true;
        for (const auto& A : Arms)
            if (A.second->containsYield())
                return true;
        if (DefaultArm && DefaultArm->containsYield())
            return true;
        return false;
    }
};

// Foreach loop
class ForeachExprAST : public ExprAST
{
    std::string VarName;
    std::unique_ptr<ExprAST> Iterable;
    std::unique_ptr<ExprAST> Body;

public:
    ForeachExprAST(std::string Var, std::unique_ptr<ExprAST> Iterable, std::unique_ptr<ExprAST> Body)
        : VarName(std::move(Var)), Iterable(std::move(Iterable)), Body(std::move(Body))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getIterable() const { return Iterable.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override { return Body->containsYield(); }
};

// Repeat-Until loop
class RepeatUntilExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Body;
    std::unique_ptr<ExprAST> Condition;

public:
    RepeatUntilExprAST(std::unique_ptr<ExprAST> Body, std::unique_ptr<ExprAST> Cond)
        : Body(std::move(Body)), Condition(std::move(Cond))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getBody() const { return Body.get(); }
    const ExprAST* getCond() const { return Condition.get(); }
    bool containsYield() const override { return Body->containsYield() || Condition->containsYield(); }
};

// Parallel for loop
class ParallelForExprAST : public ExprAST
{
    std::string VarName;
    std::unique_ptr<ExprAST> Start;
    std::unique_ptr<ExprAST> End;
    std::unique_ptr<ExprAST> Body;
    int ChunkSize; // Work chunk size for load balancing
public:
    ParallelForExprAST(std::string Var, std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
                       std::unique_ptr<ExprAST> Body, int ChunkSize = 1)
        : VarName(std::move(Var)), Start(std::move(Start)), End(std::move(End)), Body(std::move(Body)),
          ChunkSize(ChunkSize)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getStart() const { return Start.get(); }
    const ExprAST* getEnd() const { return End.get(); }
    const ExprAST* getBody() const { return Body.get(); }
    bool containsYield() const override { return Body->containsYield(); }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_CONTROL_FLOW_H
