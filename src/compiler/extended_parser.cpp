#include "flux/compiler/parser.h"
#include "flux/compiler/ast.h"

namespace Flux {

// ============ Try/Catch Parser ============

std::unique_ptr<ExprAST> Parser::ParseTryCatchExpr() {
    getNextToken(); // eat try
    
    if (CurTok != '{') {
        ReportError("expected '{' after try");
        return nullptr;
    }
    
    getNextToken(); // eat {
    auto tryBody = ParseBlockExpr();
    if (!tryBody) return nullptr;
    
    auto expr = std::make_unique<TryCatchExprAST>(std::move(tryBody));
    
    // Parse catch clauses
    while (CurTok == static_cast<int>(TokenType::tok_catch)) {
        getNextToken(); // eat catch
        
        std::string varName;
        if (CurTok == '(') {
            getNextToken(); // eat (
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                varName = m_lexer.IdentifierStr;
                getNextToken();
            }
            if (CurTok != ')') {
                ReportError("expected ')' after catch variable");
                return nullptr;
            }
            getNextToken(); // eat )
        }
        
        if (CurTok != '{') {
            ReportError("expected '{' after catch");
            return nullptr;
        }
        getNextToken(); // eat {
        auto handler = ParseBlockExpr();
        if (!handler) return nullptr;
        
        expr->addCatch(varName.empty() ? "e" : varName, std::move(handler));
    }
    
    // Parse finally
    if (CurTok == static_cast<int>(TokenType::tok_finally)) {
        getNextToken(); // eat finally
        if (CurTok != '{') {
            ReportError("expected '{' after finally");
            return nullptr;
        }
        getNextToken(); // eat {
        auto finallyBody = ParseBlockExpr();
        if (!finallyBody) return nullptr;
        expr->setFinally(std::move(finallyBody));
    }
    
    return expr;
}

// ============ Throw Parser ============

std::unique_ptr<ExprAST> Parser::ParseThrowExpr() {
    getNextToken(); // eat throw
    
    auto exception = ParseExpression();
    if (!exception) return nullptr;
    
    return std::make_unique<ThrowExprAST>(std::move(exception));
}

// ============ Assert Parser ============

std::unique_ptr<ExprAST> Parser::ParseAssertExpr() {
    getNextToken(); // eat assert
    
    if (CurTok != '(') {
        ReportError("expected '(' after assert");
        return nullptr;
    }
    getNextToken(); // eat (
    
    auto condition = ParseExpression();
    if (!condition) return nullptr;
    
    std::string message;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            message = m_lexer.StringVal;
            getNextToken();
        }
    }
    
    if (CurTok != ')') {
        ReportError("expected ')' after assert");
        return nullptr;
    }
    getNextToken(); // eat )
    
    return std::make_unique<AssertExprAST>(std::move(condition), std::move(message));
}

// ============ Yield Parser ============

std::unique_ptr<ExprAST> Parser::ParseYieldExpr() {
    getNextToken(); // eat yield
    
    auto value = ParseExpression();
    if (!value) return nullptr;
    
    return std::make_unique<YieldExprAST>(std::move(value));
}

// ============ Corner Case Parser ============

std::unique_ptr<ExprAST> Parser::ParseCornerExpr() {
    getNextToken(); // eat corner
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after corner");
        return nullptr;
    }
    
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();
    
    auto expr = std::make_unique<CornerExprAST>(varName);
    
    if (CurTok == '{') {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_rbrace) &&
               CurTok != static_cast<int>(TokenType::tok_eof)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string caseName = m_lexer.IdentifierStr;
                getNextToken();
                
                if (CurTok != ':') {
                    ReportError("expected ':' after corner case name");
                    return nullptr;
                }
                getNextToken(); // eat :
                
                auto caseExpr = ParseExpression();
                if (!caseExpr) return nullptr;
                
                expr->addCase(caseName, std::move(caseExpr));
                
                if (CurTok == ',') {
                    getNextToken();
                }
            } else {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken();
        }
    }
    
    return expr;
}

// ============ Pattern Match Parser ============

std::unique_ptr<ExprAST> Parser::ParseMatchExpr() {
    getNextToken(); // eat match
    
    auto value = ParseExpression();
    if (!value) return nullptr;
    
    auto expr = std::make_unique<MatchExprAST>(std::move(value));
    
    if (CurTok == '{') {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_rbrace) &&
               CurTok != static_cast<int>(TokenType::tok_eof)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                CurTok == static_cast<int>(TokenType::tok_number)) {
                
                auto pattern = ParseExpression();
                if (!pattern) return nullptr;
                
                if (CurTok != static_cast<int>(TokenType::tok_arrow)) {
                    ReportError("expected '=>' after match pattern");
                    return nullptr;
                }
                getNextToken(); // eat =>
                
                auto result = ParseExpression();
                if (!result) return nullptr;
                
                expr->addArm(std::move(pattern), std::move(result));
                
                if (CurTok == ',') {
                    getNextToken();
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_default)) {
                getNextToken(); // eat default
                if (CurTok != static_cast<int>(TokenType::tok_arrow)) {
                    ReportError("expected '=>' after default");
                    return nullptr;
                }
                getNextToken(); // eat =>
                
                auto defaultResult = ParseExpression();
                if (!defaultResult) return nullptr;
                expr->setDefault(std::move(defaultResult));
            } else {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken();
        }
    }
    
    return expr;
}

// ============ Foreach Parser ============

std::unique_ptr<ExprAST> Parser::ParseForeachExpr() {
    getNextToken(); // eat foreach
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after foreach");
        return nullptr;
    }
    
    std::string varName = m_lexer.IdentifierStr;
    getNextToken();
    
    if (CurTok != static_cast<int>(TokenType::tok_in)) {
        ReportError("expected 'in' after foreach variable");
        return nullptr;
    }
    getNextToken(); // eat in
    
    auto iterable = ParseExpression();
    if (!iterable) return nullptr;
    
    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        ReportError("expected 'do' after foreach iterable");
        return nullptr;
    }
    getNextToken(); // eat do
    
    auto body = ParseExpression();
    if (!body) return nullptr;
    
    return std::make_unique<ForeachExprAST>(varName, std::move(iterable), std::move(body));
}

// ============ Repeat-Until Parser ============

std::unique_ptr<ExprAST> Parser::ParseRepeatUntil() {
    getNextToken(); // eat repeat
    
    auto body = ParseExpression();
    if (!body) return nullptr;
    
    if (CurTok != static_cast<int>(TokenType::tok_until)) {
        ReportError("expected 'until' after repeat body");
        return nullptr;
    }
    getNextToken(); // eat until
    
    auto condition = ParseExpression();
    if (!condition) return nullptr;
    
    return std::make_unique<RepeatUntilExprAST>(std::move(body), std::move(condition));
}

} // namespace Flux
