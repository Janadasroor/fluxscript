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

#ifndef FLUX_SYMBOLIC_AST_H
#define FLUX_SYMBOLIC_AST_H

#include "flux/compiler/ast.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

// Symbolic variable declaration
class SymDeclAST : public ExprAST
{
    std::string VarName;

public:
    SymDeclAST(const std::string& name) : VarName(name) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
};

// Symbolic expression handle
class SymExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expr;

public:
    SymExprAST(std::unique_ptr<ExprAST> expr) : Expr(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Simplify expression
class SimplifyExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;

public:
    SimplifyExprAST(std::unique_ptr<ExprAST> expr) : Expression(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Differentiate
class DifferentiateExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::string Variable;

public:
    DifferentiateExprAST(std::unique_ptr<ExprAST> expr, std::string var)
        : Expression(std::move(expr)), Variable(std::move(var))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Substitute values
class SubstituteExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::map<std::string, std::unique_ptr<ExprAST>> Values;

public:
    SubstituteExprAST(std::unique_ptr<ExprAST> expr, std::map<std::string, std::unique_ptr<ExprAST>> vals)
        : Expression(std::move(expr)), Values(std::move(vals))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Evaluate symbolic expression to a number
class EvaluateExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::map<std::string, std::unique_ptr<ExprAST>> Values;

public:
    EvaluateExprAST(std::unique_ptr<ExprAST> expr, std::map<std::string, std::unique_ptr<ExprAST>> vals)
        : Expression(std::move(expr)), Values(std::move(vals))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Jacobian matrix
class JacobianExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expressions; // VectorExprAST
    std::unique_ptr<ExprAST> Variables;   // VectorExprAST
public:
    JacobianExprAST(std::unique_ptr<ExprAST> exprs, std::unique_ptr<ExprAST> vars)
        : Expressions(std::move(exprs)), Variables(std::move(vars))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Partial Differential Equation
class PDEExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Equation;
    std::vector<std::string> IndependentVars;

public:
    PDEExprAST(std::unique_ptr<ExprAST> eq, std::vector<std::string> vars)
        : Equation(std::move(eq)), IndependentVars(std::move(vars))
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

// Partial Derivative
class PartialDiffExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;
    std::string Variable;
    int Order;

public:
    PartialDiffExprAST(std::unique_ptr<ExprAST> expr, std::string var, int order = 1)
        : Expression(std::move(expr)), Variable(std::move(var)), Order(order)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
};

} // namespace Flux

#endif // FLUX_SYMBOLIC_AST_H
