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

#include "flux/compiler/ast.h"
#include "flux/compiler/parser.h"

namespace Flux {

// ============ Try/Catch Parser ============

std::unique_ptr<ExprAST> Parser::ParseTryCatchExpr()
{
    getNextToken(); // eat try

    // ParseBlockExpr will eat the '{'
    auto tryBody = ParseBlockExpr();
    if (!tryBody)
        return nullptr;

    auto expr = std::make_unique<TryCatchExprAST>(std::move(tryBody));

    // Parse catch clauses
    while (CurTok == static_cast<int>(TokenType::tok_catch)) {
        getNextToken(); // eat catch

        std::string varName = "e"; // default name

        // Support both `catch (e)` and `catch e` syntax
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
        } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            varName = m_lexer.IdentifierStr;
            getNextToken();
        }

        // ParseBlockExpr will eat the '{'
        auto handler = ParseBlockExpr();
        if (!handler)
            return nullptr;

        expr->addCatch(varName, std::move(handler));
    }

    // Parse finally
    if (CurTok == static_cast<int>(TokenType::tok_finally)) {
        getNextToken(); // eat finally
        // ParseBlockExpr will eat the '{'
        auto finallyBody = ParseBlockExpr();
        if (!finallyBody)
            return nullptr;
        expr->setFinally(std::move(finallyBody));
    }

    return expr;
}

// ============ Throw Parser ============

std::unique_ptr<ExprAST> Parser::ParseThrowExpr()
{
    getNextToken(); // eat throw

    auto exception = ParseExpression();
    if (!exception)
        return nullptr;

    return std::make_unique<ThrowExprAST>(std::move(exception));
}

// ============ Assert Parser ============

std::unique_ptr<ExprAST> Parser::ParseAssertExpr()
{
    getNextToken(); // eat assert

    if (CurTok != '(') {
        ReportError("expected '(' after assert");
        return nullptr;
    }
    getNextToken(); // eat (

    auto condition = ParseExpression();
    if (!condition)
        return nullptr;

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

std::unique_ptr<ExprAST> Parser::ParseYieldExpr()
{
    getNextToken(); // eat yield

    auto value = ParseExpression();
    if (!value)
        return nullptr;

    return std::make_unique<YieldExprAST>(std::move(value));
}

// ============ Corner Case Parser ============

std::unique_ptr<ExprAST> Parser::ParseCornerExpr()
{
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

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {

            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string caseName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != ':') {
                    ReportError("expected ':' after corner case name");
                    return nullptr;
                }
                getNextToken(); // eat :

                auto caseExpr = ParseExpression();
                if (!caseExpr)
                    return nullptr;

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

std::unique_ptr<ExprAST> Parser::ParseMatchExpr()
{
    getNextToken(); // eat match

    auto value = ParseExpression();
    if (!value)
        return nullptr;

    auto expr = std::make_unique<MatchExprAST>(std::move(value));

    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {

            // Handle patterns: identifiers, numbers, wildcard _, default
            if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                CurTok == static_cast<int>(TokenType::tok_number) || CurTok == '_' ||
                CurTok == static_cast<int>(TokenType::tok_default)) {

                std::unique_ptr<ExprAST> pattern;

                // Handle wildcard _ specially (not a valid expression starter)
                if (CurTok == '_') {
                    getNextToken(); // eat _

                    if (CurTok != static_cast<int>(TokenType::tok_arrow)) {
                        ReportError("expected '=>' after match pattern");
                        return nullptr;
                    }
                    getNextToken(); // eat =>

                    auto result = ParseExpression();
                    if (!result)
                        return nullptr;

                    // Store as default arm (wildcard = default)
                    expr->setDefault(std::move(result));

                    if (CurTok == ',') {
                        getNextToken();
                    }
                    continue;
                } else {
                    pattern = ParseExpression();
                    if (!pattern)
                        return nullptr;
                }

                if (CurTok != static_cast<int>(TokenType::tok_arrow)) {
                    ReportError("expected '=>' after match pattern");
                    return nullptr;
                }
                getNextToken(); // eat =>

                auto result = ParseExpression();
                if (!result)
                    return nullptr;

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
                if (!defaultResult)
                    return nullptr;
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

std::unique_ptr<ExprAST> Parser::ParseForeachExpr()
{
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
    if (!iterable)
        return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        ReportError("expected 'do' after foreach iterable");
        return nullptr;
    }
    getNextToken(); // eat do

    auto body = ParseExpression();
    if (!body)
        return nullptr;

    return std::make_unique<ForeachExprAST>(varName, std::move(iterable), std::move(body));
}

// ============ Repeat-Until Parser ============

std::unique_ptr<ExprAST> Parser::ParseRepeatUntil()
{
    getNextToken(); // eat repeat

    auto body = ParseExpression();
    if (!body)
        return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_until)) {
        ReportError("expected 'until' after repeat body");
        return nullptr;
    }
    getNextToken(); // eat until

    auto condition = ParseExpression();
    if (!condition)
        return nullptr;

    return std::make_unique<RepeatUntilExprAST>(std::move(body), std::move(condition));
}

} // namespace Flux
