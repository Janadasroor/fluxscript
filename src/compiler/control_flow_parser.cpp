#include "flux/compiler/parser.h"
#include "flux/compiler/ast.h"

namespace Flux {

// ============ Switch Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseSwitchExpr() {
    getNextToken(); // eat switch
    
    if (CurTok != '(') {
        ReportError("expected '(' after switch");
        return nullptr;
    }
    
    getNextToken(); // eat (
    
    auto condition = ParseExpression();
    if (!condition) return nullptr;
    
    if (CurTok != ')') {
        ReportError("expected ')' after switch condition");
        return nullptr;
    }
    
    getNextToken(); // eat )
    
    std::vector<CaseClauseAST> cases;
    std::vector<std::unique_ptr<ExprAST>> defaultBody;
    
    // Parse cases
    while (CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (CurTok == static_cast<int>(TokenType::tok_case)) {
            getNextToken(); // eat case
            
            auto caseValue = ParseExpression();
            if (!caseValue) return nullptr;
            
            if (CurTok != ':') {
                ReportError("expected ':' after case value");
                return nullptr;
            }
            getNextToken(); // eat :
            
            // Parse case body
            std::vector<std::unique_ptr<ExprAST>> body;
            while (CurTok != static_cast<int>(TokenType::tok_eof) &&
                   CurTok != static_cast<int>(TokenType::tok_case) &&
                   CurTok != static_cast<int>(TokenType::tok_default) &&
                   CurTok != static_cast<int>(TokenType::tok_switch)) {
                
                if (CurTok == static_cast<int>(TokenType::tok_return)) {
                    getNextToken();
                    body.push_back(ParseExpression());
                    break;  // return ends the case
                } else if (CurTok == static_cast<int>(TokenType::tok_break)) {
                    body.push_back(std::make_unique<BreakExprAST>());
                    getNextToken();
                    break;  // break ends the case
                } else if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                    getNextToken();
                } else {
                    body.push_back(ParseExpression());
                    if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                        getNextToken();
                    }
                }
            }
            
            cases.emplace_back(std::move(caseValue), std::move(body));
            
        } else if (CurTok == static_cast<int>(TokenType::tok_default)) {
            getNextToken(); // eat default
            
            if (CurTok != ':') {
                ReportError("expected ':' after default");
                return nullptr;
            }
            getNextToken(); // eat :
            
            // Parse default body
            while (CurTok != static_cast<int>(TokenType::tok_eof) &&
                   CurTok != static_cast<int>(TokenType::tok_switch)) {
                
                if (CurTok == static_cast<int>(TokenType::tok_return)) {
                    getNextToken();
                    defaultBody.push_back(ParseExpression());
                    break;
                } else if (CurTok == static_cast<int>(TokenType::tok_break)) {
                    defaultBody.push_back(std::make_unique<BreakExprAST>());
                    getNextToken();
                    break;
                } else if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                    getNextToken();
                } else {
                    defaultBody.push_back(ParseExpression());
                    if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                        getNextToken();
                    }
                }
            }
            
        } else if (CurTok == static_cast<int>(TokenType::tok_rbrace) ||
                   CurTok == static_cast<int>(TokenType::tok_eof)) {
            break;
        } else {
            getNextToken();
        }
    }
    
    return std::make_unique<SwitchExprAST>(std::move(condition), std::move(cases), std::move(defaultBody));
}

// ============ Break Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseBreakExpr() {
    getNextToken(); // eat break
    return std::make_unique<BreakExprAST>();
}

// ============ Continue Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseContinueExpr() {
    getNextToken(); // eat continue
    return std::make_unique<ContinueExprAST>();
}

} // namespace Flux
