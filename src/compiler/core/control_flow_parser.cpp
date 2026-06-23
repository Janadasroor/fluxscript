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

// ============ Switch Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseSwitchExpr()
{
    getNextToken(); // eat switch

    if (CurTok != '(') {
        ReportError("expected '(' after switch");
        return nullptr;
    }

    getNextToken(); // eat (

    auto condition = ParseExpression();
    if (!condition)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after switch condition");
        return nullptr;
    }

    getNextToken(); // eat )

    // Expect opening brace of switch body
    if (CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        ReportError("expected '{' after switch condition");
        return nullptr;
    }
    getNextToken(); // eat {

    std::vector<CaseClauseAST> cases;
    std::vector<std::unique_ptr<ExprAST>> defaultBody;

    // Parse cases — supports both `case value: body` and `expr => body` syntax,
    // as well as `default:` / `~ => body` / `_ => body` for default.
    while (CurTok != static_cast<int>(TokenType::tok_eof)) {
        // Skip commas between case clauses
        if (CurTok == ',') {
            getNextToken();
            continue;
        }

        // Check for bare `~` or `_` as default indicator
        bool isDefault = (CurTok == static_cast<int>(TokenType::tok_bitwise_not));
        if (isDefault) {
            getNextToken(); // eat ~ or _

            if (CurTok != static_cast<int>(TokenType::tok_fat_arrow)) {
                ReportError("expected '=>' after '~'");
                return nullptr;
            }
            getNextToken(); // eat =>

            // Parse default body
            if (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
                defaultBody.push_back(ParseExpression());
            }
            continue;
        }

        if (CurTok == static_cast<int>(TokenType::tok_case)) {
            getNextToken(); // eat case

            // Temporarily remove ':' as a binary range operator so it is not
            // consumed by ParseExpression (which would turn `case 2.0:` into a
            // RangeExprAST). The ':' after the case value is the case separator.
            int savedColonPrec = m_binopPrecedence[static_cast<int>(TokenType::tok_colon)];
            m_binopPrecedence[static_cast<int>(TokenType::tok_colon)] = 0;
            auto caseValue = ParseExpression();
            m_binopPrecedence[static_cast<int>(TokenType::tok_colon)] = savedColonPrec;
            if (!caseValue)
                return nullptr;

            if (CurTok != static_cast<int>(TokenType::tok_colon)) {
                ReportError("expected ':' after case value");
                return nullptr;
            }
            getNextToken(); // eat :

            // Parse case body
            std::vector<std::unique_ptr<ExprAST>> body;
            while (CurTok != static_cast<int>(TokenType::tok_eof) && CurTok != static_cast<int>(TokenType::tok_case) &&
                   CurTok != static_cast<int>(TokenType::tok_default) &&
                   CurTok != static_cast<int>(TokenType::tok_switch) &&
                   CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != ',' &&
                   CurTok != static_cast<int>(TokenType::tok_end)) {

                if (CurTok == static_cast<int>(TokenType::tok_return)) {
                    getNextToken();
                    body.push_back(std::make_unique<ReturnExprAST>(ParseExpression()));
                    break;
                } else if (CurTok == static_cast<int>(TokenType::tok_break)) {
                    body.push_back(std::make_unique<BreakExprAST>());
                    getNextToken();
                    break;
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

            if (CurTok != static_cast<int>(TokenType::tok_colon)) {
                ReportError("expected ':' after default");
                return nullptr;
            }
            getNextToken(); // eat :

            // Parse default body
            while (CurTok != static_cast<int>(TokenType::tok_eof) &&
                   CurTok != static_cast<int>(TokenType::tok_switch) &&
                   CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != ',' &&
                   CurTok != static_cast<int>(TokenType::tok_end)) {

                if (CurTok == static_cast<int>(TokenType::tok_return)) {
                    getNextToken();
                    defaultBody.push_back(std::make_unique<ReturnExprAST>(ParseExpression()));
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

        } else if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat the closing }
            break;
        } else if (CurTok == static_cast<int>(TokenType::tok_eof) || CurTok == static_cast<int>(TokenType::tok_end)) {
            break;
        } else if (CurTok == static_cast<int>(TokenType::tok_fat_arrow)) {
            ReportError("expected case value or '~' before '=>'");
            return nullptr;
        } else {
            // Bare expression case value syntax: `value => body`
            // Temporarily remove ':' as a binary range operator, similar to above
            int savedColonPrec = m_binopPrecedence[static_cast<int>(TokenType::tok_colon)];
            m_binopPrecedence[static_cast<int>(TokenType::tok_colon)] = 0;
            auto caseValue = ParseExpression();
            m_binopPrecedence[static_cast<int>(TokenType::tok_colon)] = savedColonPrec;
            if (!caseValue)
                return nullptr;

            if (CurTok != static_cast<int>(TokenType::tok_fat_arrow)) {
                ReportError("expected '=>' after case value");
                return nullptr;
            }
            getNextToken(); // eat =>

            // Parse case body (single expression)
            std::vector<std::unique_ptr<ExprAST>> body;
            if (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
                body.push_back(ParseExpression());
            }

            cases.emplace_back(std::move(caseValue), std::move(body));
        }
    }

    return std::make_unique<SwitchExprAST>(std::move(condition), std::move(cases), std::move(defaultBody));
}

// ============ Break Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseBreakExpr()
{
    getNextToken(); // eat break
    return std::make_unique<BreakExprAST>();
}

// ============ Continue Statement Parser ============

std::unique_ptr<ExprAST> Parser::ParseContinueExpr()
{
    getNextToken(); // eat continue
    return std::make_unique<ContinueExprAST>();
}

} // namespace Flux
