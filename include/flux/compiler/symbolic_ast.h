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
#include <vector>
#include <string>
#include <memory>

namespace Flux {

// Symbolic variable declaration
class SymDeclAST : public ExprAST {
    std::string VarName;
public:
    SymDeclAST(const std::string& name) : VarName(name) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::string& getVarName() const { return VarName; }
};

// Symbolic expression
class SymExprAST : public ExprAST {
    std::shared_ptr<class SymbolicExpr> Expr;
public:
    SymExprAST(std::shared_ptr<SymbolicExpr> expr) : Expr(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Solve equation
class SolveExprAST : public ExprAST {
    std::shared_ptr<SymbolicExpr> LHS;
    std::shared_ptr<SymbolicExpr> RHS;
    std::string Variable;
public:
    SolveExprAST(std::shared_ptr<SymbolicExpr> lhs, std::shared_ptr<SymbolicExpr> rhs, std::string var)
        : LHS(std::move(lhs)), RHS(std::move(rhs)), Variable(std::move(var)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Simplify expression
class SimplifyExprAST : public ExprAST {
    std::shared_ptr<SymbolicExpr> Expr;
public:
    SimplifyExprAST(std::shared_ptr<SymbolicExpr> expr) : Expr(std::move(expr)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Differentiate
class DifferentiateExprAST : public ExprAST {
    std::shared_ptr<SymbolicExpr> Expr;
    std::string Variable;
public:
    DifferentiateExprAST(std::shared_ptr<SymbolicExpr> expr, std::string var)
        : Expr(std::move(expr)), Variable(std::move(var)) {}
    TypedValue codegen(CodegenContext& context) override;
};

// Substitute values
class SubstituteExprAST : public ExprAST {
    std::shared_ptr<SymbolicExpr> Expr;
    std::map<std::string, double> Values;
public:
    SubstituteExprAST(std::shared_ptr<SymbolicExpr> expr, std::map<std::string, double> vals)
        : Expr(std::move(expr)), Values(std::move(vals)) {}
    TypedValue codegen(CodegenContext& context) override;
};

} // namespace Flux

#endif // FLUX_SYMBOLIC_AST_H
