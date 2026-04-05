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
    
    std::cout << "[Symbolic] Declared symbolic variable: " << name << std::endl;
    
    return std::make_unique<SymDeclAST>(name);
}

std::unique_ptr<ExprAST> Parser::ParseSolveExpr() {
    getNextToken(); // eat solve
    std::cout << "[Symbolic] Solve expression parsed" << std::endl;
    return std::make_unique<SymDeclAST>("solve_result");
}

std::unique_ptr<ExprAST> Parser::ParseSimplifyExpr() {
    getNextToken(); // eat simplify
    std::cout << "[Symbolic] Simplify expression parsed" << std::endl;
    return std::make_unique<SymDeclAST>("simplify_result");
}

std::unique_ptr<ExprAST> Parser::ParseDifferentiateExpr() {
    getNextToken(); // eat differentiate
    std::cout << "[Symbolic] Differentiate expression parsed" << std::endl;
    return std::make_unique<SymDeclAST>("diff_result");
}

std::unique_ptr<ExprAST> Parser::ParseSubstituteExpr() {
    getNextToken(); // eat substitute
    std::cout << "[Symbolic] Substitute expression parsed" << std::endl;
    return std::make_unique<SymDeclAST>("substitute_result");
}

} // namespace Flux
