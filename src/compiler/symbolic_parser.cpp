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

std::string getCurrentIdentifier(Parser& p) {
    // Access via public method - we'll use a workaround
    return "";
}

// ============ Symbolic Declaration Parser ============

std::unique_ptr<ExprAST> Parser::ParseSymDecl() {
    getNextToken(); // eat sym
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after sym");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken();
    
    return std::make_unique<SymDeclAST>(name);
}

std::unique_ptr<ExprAST> Parser::ParseSolveExpr() {
    getNextToken(); // eat solve
    if (CurTok != '(') { ReportError("expected '(' after solve"); return nullptr; }
    getNextToken(); // eat (
    
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    
    if (CurTok != ',') { ReportError("expected ',' after expression in solve"); return nullptr; }
    getNextToken(); // eat ,
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected variable name in solve"); return nullptr; }
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();
    
    if (CurTok != ')') { ReportError("expected ')' after solve"); return nullptr; }
    getNextToken();
    
    return std::make_unique<SolveExprAST>(std::move(expr), varName);
}

std::unique_ptr<ExprAST> Parser::ParseSimplifyExpr() {
    getNextToken(); // eat simplify
    if (CurTok != '(') { ReportError("expected '(' after simplify"); return nullptr; }
    getNextToken(); // eat (
    
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    
    if (CurTok != ')') { ReportError("expected ')' after simplify"); return nullptr; }
    getNextToken();
    
    return std::make_unique<SimplifyExprAST>(std::move(expr));
}

std::unique_ptr<ExprAST> Parser::ParseDifferentiateExpr() {
    getNextToken(); // eat differentiate
    if (CurTok != '(') { ReportError("expected '(' after differentiate"); return nullptr; }
    getNextToken(); // eat (
    
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    
    if (CurTok != ',') { ReportError("expected ',' after expression in differentiate"); return nullptr; }
    getNextToken(); // eat ,
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected variable name in differentiate"); return nullptr; }
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();
    
    if (CurTok != ')') { ReportError("expected ')' after differentiate"); return nullptr; }
    getNextToken();
    
    return std::make_unique<DifferentiateExprAST>(std::move(expr), varName);
}

std::unique_ptr<ExprAST> Parser::ParseSubstituteExpr() {
    getNextToken(); // eat substitute
    if (CurTok != '(') { ReportError("expected '(' after substitute"); return nullptr; }
    getNextToken(); // eat (
    
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    
    if (CurTok != ',') { ReportError("expected ',' after expression in substitute"); return nullptr; }
    getNextToken(); // eat ,
    
    if (CurTok != static_cast<int>(TokenType::tok_lbrace)) { ReportError("expected '{' for substitution map"); return nullptr; }
    getNextToken(); // eat {
    
    std::map<std::string, std::unique_ptr<ExprAST>> vals;
    while (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected variable name in substitution map"); return nullptr; }
        std::string varName = m_lexer.IdentifierStr;
        getNextToken();
        
        if (CurTok != static_cast<int>(TokenType::tok_colon)) { ReportError("expected ':' after variable name in substitution map"); return nullptr; }
        getNextToken(); // eat :
        
        auto val = ParseExpression();
        if (!val) return nullptr;
        vals[varName] = std::move(val);
        
        if (CurTok == ',') getNextToken();
    }
    getNextToken(); // eat }
    
    if (CurTok != ')') { ReportError("expected ')' after substitute"); return nullptr; }
    getNextToken();
    
    return std::make_unique<SubstituteExprAST>(std::move(expr), std::move(vals));
}

std::unique_ptr<ExprAST> Parser::ParseEvaluateExpr() {
    getNextToken(); // eat evaluate
    if (CurTok != '(') { ReportError("expected '(' after evaluate"); return nullptr; }
    getNextToken(); // eat (
    
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    
    std::map<std::string, std::unique_ptr<ExprAST>> vals;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_lbrace)) { ReportError("expected '{' for evaluation map"); return nullptr; }
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) { ReportError("expected variable name in evaluation map"); return nullptr; }
            std::string varName = m_lexer.IdentifierStr;
            getNextToken();
            
            if (CurTok != static_cast<int>(TokenType::tok_colon)) { ReportError("expected ':' after variable name in evaluation map"); return nullptr; }
            getNextToken(); // eat :
            
            auto val = ParseExpression();
            if (!val) return nullptr;
            vals[varName] = std::move(val);
            
            if (CurTok == ',') getNextToken();
        }
        getNextToken(); // eat }
    }
    
    if (CurTok != ')') { ReportError("expected ')' after evaluate"); return nullptr; }
    getNextToken();
    
    return std::make_unique<EvaluateExprAST>(std::move(expr), std::move(vals));
}

} // namespace Flux
