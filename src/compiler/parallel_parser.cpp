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
