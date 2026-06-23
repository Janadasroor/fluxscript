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

// ============ Await Parser ============

std::unique_ptr<ExprAST> Parser::ParseAwaitExpr()
{
    getNextToken(); // eat await

    auto value = ParseExpression();
    if (!value)
        return nullptr;

    return std::make_unique<AwaitExprAST>(std::move(value));
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

                if (CurTok != static_cast<int>(TokenType::tok_colon)) {
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

static void flattenOrPatterns(std::unique_ptr<ExprAST> pattern, std::vector<std::unique_ptr<ExprAST>>& flattened)
{
    if (!pattern)
        return;
    if (auto* binExpr = dynamic_cast<BinaryExprAST*>(pattern.get())) {
        if (binExpr->getOp() == static_cast<int>(TokenType::tok_bitwise_or)) {
            flattenOrPatterns(binExpr->takeLHS(), flattened);
            flattenOrPatterns(binExpr->takeRHS(), flattened);
            return;
        }
    }
    flattened.push_back(std::move(pattern));
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
            // Handle patterns: identifiers, numbers, integers, wildcard _
            if (CurTok == static_cast<int>(TokenType::tok_identifier) ||
                CurTok == static_cast<int>(TokenType::tok_number) ||
                CurTok == static_cast<int>(TokenType::tok_integer) || CurTok == '_') {

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
                }

                std::vector<std::unique_ptr<ExprAST>> patterns;
                std::vector<std::string> bindings;
                std::vector<std::pair<std::string, std::string>> namedBindings;

                std::vector<std::unique_ptr<ExprAST>> rawPatternsList;
                while (true) {
                    m_parsingMatchPattern = true;
                    auto rawPattern = ParseExpression();
                    m_parsingMatchPattern = false;
                    if (!rawPattern)
                        return nullptr;
                    flattenOrPatterns(std::move(rawPattern), rawPatternsList);

                    if (CurTok == static_cast<int>(TokenType::tok_bitwise_or)) {
                        getNextToken(); // eat |
                    } else {
                        break;
                    }
                }

                for (auto& pattern : rawPatternsList) {
                    // Named-field pattern: `Enum.Variant { field1: var1, field2: var2 }`
                    // After parsing the variant as a MemberExprAST, if next token is '{',
                    // parse the braced named-field bindings.
                    bool hasNamedFields = false;
                    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
                        // Verify the pattern is Enum.Variant (a MemberExprAST on a known enum)
                        MemberExprAST* memberPat = dynamic_cast<MemberExprAST*>(pattern.get());
                        if (memberPat) {
                            VariableExprAST* objPat = dynamic_cast<VariableExprAST*>(memberPat->getObject());
                            if (objPat && m_knownEnumTypeNames.count(objPat->getName())) {
                                hasNamedFields = true;
                                getNextToken(); // eat {
                                // Parse field: var pairs
                                while (CurTok != static_cast<int>(TokenType::tok_rbrace) &&
                                       CurTok != static_cast<int>(TokenType::tok_eof)) {
                                    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                                        ReportError("expected field name in match named-field pattern");
                                        return nullptr;
                                    }
                                    std::string fieldName = m_lexer.IdentifierStr;
                                    getNextToken(); // eat field name
                                    if (CurTok != static_cast<int>(TokenType::tok_colon)) {
                                        ReportError("expected ':' after field name in match named-field pattern");
                                        return nullptr;
                                    }
                                    getNextToken(); // eat :
                                    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                                        ReportError("expected binding variable name in match named-field pattern");
                                        return nullptr;
                                    }
                                    std::string varName = m_lexer.IdentifierStr;
                                    getNextToken(); // eat var name
                                    namedBindings.push_back({fieldName, varName});
                                    // Also populate positional bindings for compatibility
                                    if (std::find(bindings.begin(), bindings.end(), varName) == bindings.end()) {
                                        bindings.push_back(varName);
                                    }
                                    if (CurTok == ',')
                                        getNextToken(); // eat ,
                                }
                                if (CurTok == static_cast<int>(TokenType::tok_rbrace))
                                    getNextToken(); // eat }
                            }
                        }
                    }

                    // Positional binding extraction from call-style patterns (e.g., Option.Some(v))
                    // Only if we didn't parse named fields
                    if (!hasNamedFields) {
                        if (auto* call = dynamic_cast<CallExprAST*>(pattern.get())) {
                            if (call->hasCalleeExpr()) {
                                if (auto* member = dynamic_cast<MemberExprAST*>(call->getCalleeExpr())) {
                                    if (auto* obj = dynamic_cast<VariableExprAST*>(member->getObject())) {
                                        if (m_knownEnumTypeNames.count(obj->getName())) {
                                            const auto& args = call->getArgs();
                                            for (auto& arg : args) {
                                                if (auto* var = dynamic_cast<VariableExprAST*>(arg.get())) {
                                                    std::string varName = var->getName();
                                                    if (std::find(bindings.begin(), bindings.end(), varName) ==
                                                        bindings.end()) {
                                                        bindings.push_back(varName);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    patterns.push_back(std::move(pattern));
                }

                if (CurTok != static_cast<int>(TokenType::tok_arrow)) {
                    ReportError("expected '=>' after match pattern");
                    return nullptr;
                }
                getNextToken(); // eat =>

                auto result = ParseExpression();
                if (!result)
                    return nullptr;

                expr->addArm(std::move(patterns), std::move(result), bindings, namedBindings);

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

// ============ Do-While Parser ============

std::unique_ptr<ExprAST> Parser::ParseDoWhile()
{
    getNextToken(); // eat do

    auto body = ParseExpression();
    if (!body)
        return nullptr;

    if (CurTok != static_cast<int>(TokenType::tok_while)) {
        ReportError("expected 'while' after do body");
        return nullptr;
    }
    getNextToken(); // eat while

    auto condition = ParseExpression();
    if (!condition)
        return nullptr;

    return std::make_unique<DoWhileExprAST>(std::move(body), std::move(condition));
}

std::unique_ptr<ExprAST> Parser::ParseCrossExpr()
{
    getNextToken(); // eat cross

    if (CurTok != '(') {
        ReportError("expected '(' after cross");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    int risefall = 0;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok == static_cast<int>(TokenType::tok_number)) {
            risefall = static_cast<int>(m_lexer.NumVal);
            getNextToken();
        } else if (CurTok == static_cast<int>(TokenType::tok_integer)) {
            risefall = static_cast<int>(m_lexer.IntVal);
            getNextToken();
        } else {
            ReportError("expected edge specifier (1=rising, -1=falling, 0=any)");
            return nullptr;
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after cross expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<CrossExprAST>(std::move(expr), risefall);
}

} // namespace Flux
