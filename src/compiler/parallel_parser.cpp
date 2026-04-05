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
#include "flux/compiler/ast.h"

namespace Flux {

// ============ Parallel For Parser ============

std::unique_ptr<ExprAST> Parser::ParseParallelForExpr() {
    getNextToken(); // eat parallel
    
    if (CurTok != static_cast<int>(TokenType::tok_for)) {
        ReportError("expected 'for' after parallel");
        return nullptr;
    }
    getNextToken(); // eat for
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after parallel for");
        return nullptr;
    }
    
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();
    
    if (CurTok != static_cast<int>(TokenType::tok_in)) {
        ReportError("expected 'in' after parallel for variable");
        return nullptr;
    }
    getNextToken(); // eat in
    
    auto Start = ParseExpression();
    if (!Start) return nullptr;
    
    if (CurTok != ',') {
        ReportError("expected ',' in parallel for loop");
        return nullptr;
    }
    getNextToken(); // eat ,
    
    auto End = ParseExpression();
    if (!End) return nullptr;
    
    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        ReportError("expected 'do' after parallel for range");
        return nullptr;
    }
    getNextToken(); // eat do
    
    auto Body = ParseExpression();
    if (!Body) return nullptr;
    
    return std::make_unique<ParallelForExprAST>(VarName, std::move(Start), std::move(End), std::move(Body));
}

} // namespace Flux
