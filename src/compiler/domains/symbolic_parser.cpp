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

#include "flux/compiler/parser.h"
#include "flux/compiler/symbolic_ast.h"
#include "flux/runtime/symbolic_engine.h"
#include <iostream>

namespace Flux {

// Helper to get current identifier
std::string getCurrentIdentifier(Parser& p);
double getCurrentNumber(Parser& p);
int getCurrentToken(Parser& p);
void getNextToken(Parser& p);

std::string getCurrentIdentifier(Parser& p)
{
    // Access via public method - we'll use a workaround
    return "";
}

// ============ Symbolic Declaration Parser ============

std::unique_ptr<ExprAST> Parser::ParseSymDecl()
{
    getNextToken(); // eat sym

    std::vector<std::unique_ptr<ExprAST>> decls;

    while (true) {
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected variable name after sym");
            return nullptr;
        }

        std::string name = m_lexer.IdentifierStr;
        getNextToken();
        decls.push_back(std::make_unique<SymDeclAST>(name));

        if (CurTok == ',') {
            getNextToken(); // eat ,
        } else {
            break;
        }
    }

    if (decls.size() == 1)
        return std::move(decls[0]);
    return std::make_unique<BlockExprAST>(std::move(decls));
}

std::unique_ptr<ExprAST> Parser::ParseSimplifyExpr()
{
    getNextToken(); // eat simplify
    if (CurTok != '(') {
        ReportError("expected '(' after simplify");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after simplify");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<SimplifyExprAST>(std::move(expr));
}

std::unique_ptr<ExprAST> Parser::ParseDifferentiateExpr()
{
    getNextToken(); // eat differentiate
    if (CurTok != '(') {
        ReportError("expected '(' after differentiate");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in differentiate");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name in differentiate");
        return nullptr;
    }
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after differentiate");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<DifferentiateExprAST>(std::move(expr), varName);
}

std::unique_ptr<ExprAST> Parser::ParseSubstituteExpr()
{
    getNextToken(); // eat substitute
    if (CurTok != '(') {
        ReportError("expected '(' after substitute");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in substitute");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        ReportError("expected '{' for substitution map");
        return nullptr;
    }
    getNextToken(); // eat {

    std::map<std::string, std::unique_ptr<ExprAST>> vals;
    while (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected variable name in substitution map");
            return nullptr;
        }
        std::string varName = m_lexer.IdentifierStr;
        getNextToken();

        if (CurTok != static_cast<int>(TokenType::tok_colon)) {
            ReportError("expected ':' after variable name in substitution map");
            return nullptr;
        }
        getNextToken(); // eat :

        auto val = ParseExpression();
        if (!val)
            return nullptr;
        vals[varName] = std::move(val);

        if (CurTok == ',')
            getNextToken();
    }
    getNextToken(); // eat }

    if (CurTok != ')') {
        ReportError("expected ')' after substitute");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<SubstituteExprAST>(std::move(expr), std::move(vals));
}

std::unique_ptr<ExprAST> Parser::ParseEvaluateExpr()
{
    return nullptr;
}

std::unique_ptr<ExprAST> Parser::ParseJacobianExpr()
{
    return nullptr;
}

std::unique_ptr<ExprAST> Parser::ParsePDEExpr()
{
    getNextToken(); // eat pde
    if (CurTok != '(') {
        ReportError("expected '(' after pde");
        return nullptr;
    }
    getNextToken(); // eat (

    auto eq = ParseExpression();
    if (!eq)
        return nullptr;

    std::vector<std::string> vars;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected independent variable name in pde");
            return nullptr;
        }
        vars.push_back(m_lexer.IdentifierStr);
        getNextToken();

        while (CurTok == ',') {
            getNextToken();
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected independent variable name in pde");
                return nullptr;
            }
            vars.push_back(m_lexer.IdentifierStr);
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after pde");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<PDEExprAST>(std::move(eq), std::move(vars));
}

std::unique_ptr<ExprAST> Parser::ParsePartialDiffExpr()
{
    getNextToken(); // eat pdiff
    if (CurTok != '(') {
        ReportError("expected '(' after pdiff");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in pdiff");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name in pdiff");
        return nullptr;
    }
    std::string var = m_lexer.IdentifierStr;
    getNextToken();

    int order = 1;
    if (CurTok == ',') {
        getNextToken();
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected derivative order in pdiff");
            return nullptr;
        }
        order = (int)m_lexer.NumVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after pdiff");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<PartialDiffExprAST>(std::move(expr), var, order);
}

std::unique_ptr<ExprAST> Parser::ParseIntegrateExpr()
{
    getNextToken(); // eat integrate
    if (CurTok != '(') {
        ReportError("expected '(' after integrate");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in integrate");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name in integrate");
        return nullptr;
    }
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after integrate");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<IntegrateExprAST>(std::move(expr), varName);
}

std::unique_ptr<ExprAST> Parser::ParseLaplaceExpr()
{
    getNextToken(); // eat laplace
    if (CurTok != '(') {
        ReportError("expected '(' after laplace");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in laplace");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected time variable name in laplace");
        return nullptr;
    }
    std::string tVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' after time variable in laplace");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected s-domain variable name in laplace");
        return nullptr;
    }
    std::string sVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after laplace");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<LaplaceExprAST>(std::move(expr), tVar, sVar);
}

std::unique_ptr<ExprAST> Parser::ParseInverseLaplaceExpr()
{
    getNextToken(); // eat inverse_laplace
    if (CurTok != '(') {
        ReportError("expected '(' after inverse_laplace");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in inverse_laplace");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected s-domain variable name in inverse_laplace");
        return nullptr;
    }
    std::string sVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' after s variable in inverse_laplace");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected time variable name in inverse_laplace");
        return nullptr;
    }
    std::string tVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after inverse_laplace");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<InverseLaplaceExprAST>(std::move(expr), sVar, tVar);
}

// expand(expr)
std::unique_ptr<ExprAST> Parser::ParseExpandExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat expand

    if (CurTok != '(') {
        ReportError("expected '(' after expand");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after expand expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<ExpandExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

// factor(expr)
std::unique_ptr<ExprAST> Parser::ParseFactorExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat factor

    if (CurTok != '(') {
        ReportError("expected '(' after factor");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after factor expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<FactorExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

// collect(expr, var)
std::unique_ptr<ExprAST> Parser::ParseCollectExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat collect

    if (CurTok != '(') {
        ReportError("expected '(' after collect");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in collect");
        return nullptr;
    }
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name in collect");
        return nullptr;
    }
    std::string var = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after collect arguments");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<CollectExprAST>(std::move(expr), var);
    Res->setLocation(line, col);
    return Res;
}

// numerator(expr)
std::unique_ptr<ExprAST> Parser::ParseNumeratorExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat numerator

    if (CurTok != '(') {
        ReportError("expected '(' after numerator");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after numerator expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<NumeratorExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

// denominator(expr)
std::unique_ptr<ExprAST> Parser::ParseDenominatorExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat denominator

    if (CurTok != '(') {
        ReportError("expected '(' after denominator");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after denominator expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<DenominatorExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

// poles(expr)
std::unique_ptr<ExprAST> Parser::ParsePolesExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat poles

    if (CurTok != '(') {
        ReportError("expected '(' after poles");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after poles expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<PolesExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

// zeros(expr)
std::unique_ptr<ExprAST> Parser::ParseZerosExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat zeros

    if (CurTok != '(') {
        ReportError("expected '(' after zeros");
        return nullptr;
    }
    getNextToken();

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after zeros expression");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<ZerosExprAST>(std::move(expr));
    Res->setLocation(line, col);
    return Res;
}

} // namespace Flux
