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

#ifndef FLUX_COMPILER_AST_DATA_H
#define FLUX_COMPILER_AST_DATA_H

#include "flux/compiler/ast_core.h"

namespace Flux {

class ArrayExprAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> Elements;

public:
    ArrayExprAST(std::vector<std::unique_ptr<ExprAST>> Elements) : Elements(std::move(Elements)) {}
    ~ArrayExprAST() = default;
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
    bool containsYield() const override
    {
        for (const auto& Elem : Elements) {
            if (Elem->containsYield())
                return true;
        }
        return false;
    }
};
class VectorExprAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> Elements;

public:
    VectorExprAST(std::vector<std::unique_ptr<ExprAST>> Elements) : Elements(std::move(Elements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getElements() const { return Elements; }
    bool containsYield() const override
    {
        for (const auto& Elem : Elements) {
            if (Elem->containsYield())
                return true;
        }
        return false;
    }
};

class MatrixExprAST : public ExprAST
{
    std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows;
    int NumRows;
    int NumCols;

public:
    MatrixExprAST(std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows, int rows, int cols)
        : Rows(std::move(Rows)), NumRows(rows), NumCols(cols)
    {
    }
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::vector<std::unique_ptr<ExprAST>>>& getRows() const { return Rows; }
    int getNumRows() const { return NumRows; }
    int getNumCols() const { return NumCols; }
    bool containsYield() const override
    {
        for (const auto& Row : Rows) {
            for (const auto& Elem : Row) {
                if (Elem->containsYield())
                    return true;
            }
        }
        return false;
    }
};

class SymbolicExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Expression;

public:
    SymbolicExprAST(std::unique_ptr<ExprAST> Expr) : Expression(std::move(Expr)) {}
    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getExpression() const { return Expression.get(); }
    bool containsYield() const override { return Expression && Expression->containsYield(); }
};
class IndexExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Array;
    std::unique_ptr<ExprAST> RowIndex; // Can be a RangeExprAST for slicing
    std::unique_ptr<ExprAST> ColIndex; // Optional, for matrix column indexing
    bool IsMatrixIndex;                // True if accessing m(row, col)
public:
    IndexExprAST(std::unique_ptr<ExprAST> Array, std::unique_ptr<ExprAST> RowIdx)
        : Array(std::move(Array)), RowIndex(std::move(RowIdx)), ColIndex(nullptr), IsMatrixIndex(false)
    {
    }

    IndexExprAST(std::unique_ptr<ExprAST> Array, std::unique_ptr<ExprAST> RowIndex, std::unique_ptr<ExprAST> ColIndex)
        : Array(std::move(Array)), RowIndex(std::move(RowIndex)), ColIndex(std::move(ColIndex)), IsMatrixIndex(true)
    {
    }

    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getArray() const { return Array.get(); }
    const ExprAST* getRowIndex() const { return RowIndex.get(); }
    const ExprAST* getColIndex() const { return ColIndex.get(); }
    ExprAST* getRowIndexMut() { return RowIndex.get(); }
    ExprAST* getColIndexMut() { return ColIndex.get(); }
    bool isMatrixIndex() const { return IsMatrixIndex; }
    bool containsYield() const override
    {
        if (Array && Array->containsYield())
            return true;
        if (RowIndex && RowIndex->containsYield())
            return true;
        if (ColIndex && ColIndex->containsYield())
            return true;
        return false;
    }
};

// Range expression for slicing: start:end or start:step:end
class RangeExprAST : public ExprAST
{
    std::unique_ptr<ExprAST> Start;
    std::unique_ptr<ExprAST> Step; // Optional (defaults to 1)
    std::unique_ptr<ExprAST> End;

public:
    RangeExprAST(std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End)
        : Start(std::move(Start)), Step(nullptr), End(std::move(End))
    {
    }

    RangeExprAST(std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> End)
        : Start(std::move(Start)), Step(std::move(Step)), End(std::move(End))
    {
    }

    TypedValue codegen(CodegenContext& context) override;
    const ExprAST* getStart() const { return Start.get(); }
    const ExprAST* getStep() const { return Step.get(); }
    const ExprAST* getEnd() const { return End.get(); }
    bool containsYield() const override
    {
        if (Start && Start->containsYield())
            return true;
        if (Step && Step->containsYield())
            return true;
        if (End && End->containsYield())
            return true;
        return false;
    }
};

class BlockExprAST : public ExprAST
{
    std::vector<std::unique_ptr<ExprAST>> Statements;

public:
    BlockExprAST(std::vector<std::unique_ptr<ExprAST>> Statements) : Statements(std::move(Statements)) {}
    TypedValue codegen(CodegenContext& context) override;
    const std::vector<std::unique_ptr<ExprAST>>& getStatements() const { return Statements; }
    bool containsYield() const override
    {
        for (const auto& Stmt : Statements) {
            if (Stmt->containsYield())
                return true;
        }
        return false;
    }
};

} // namespace Flux

#endif // FLUX_COMPILER_AST_DATA_H
