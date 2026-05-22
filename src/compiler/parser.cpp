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
#include <iostream>

namespace Flux {

Parser::Parser(const std::string& input) : m_lexer(input), m_hasError(false)
{
    m_binopPrecedence['='] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_plus_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_minus_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_star_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_slash_equal)] = 2;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_or)] = 5;
    m_binopPrecedence[static_cast<int>(TokenType::tok_logical_and)] = 10;
    m_binopPrecedence['|'] = 11;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_or)] = 11;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_xor)] = 12;
    m_binopPrecedence['&'] = 13;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_and)] = 13;
    m_binopPrecedence[static_cast<int>(TokenType::tok_equal)] = 15;
    m_binopPrecedence[static_cast<int>(TokenType::tok_not_equal)] = 15;
    m_binopPrecedence['<'] = 20;
    m_binopPrecedence['>'] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_less_equal)] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_greater_equal)] = 20;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_shl)] = 25;
    m_binopPrecedence[static_cast<int>(TokenType::tok_bitwise_shr)] = 25;
    m_binopPrecedence['+'] = 30;
    m_binopPrecedence['-'] = 30;
    m_binopPrecedence['*'] = 40;
    m_binopPrecedence['/'] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_mul)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_div)] = 40;
    m_binopPrecedence[static_cast<int>(TokenType::tok_ew_power)] = 50;
    m_binopPrecedence[static_cast<int>(TokenType::tok_power)] = 50;
    m_binopPrecedence['['] = 60;
    m_binopPrecedence[static_cast<int>(TokenType::tok_colon)] = 35;
    getNextToken();
}

int Parser::getNextToken()
{
    return CurTok = m_lexer.getNextToken();
}

int Parser::GetTokPrecedence()
{
    int tokPrec = m_binopPrecedence[CurTok];
    if (tokPrec <= 0)
        return -1;
    return tokPrec;
}

void Parser::ReportError(const std::string& message)
{
    m_lexer.reportError(message);
    m_hasError = true;
}

void Parser::SkipToSynchronizationPoint()
{
    while (CurTok != static_cast<int>(TokenType::tok_eof) && !IsSynchronizationToken(CurTok))
        getNextToken();
}

bool Parser::IsSynchronizationToken(int token)
{
    switch (static_cast<TokenType>(token)) {
    case TokenType::tok_def:
    case TokenType::tok_extern:
    case TokenType::tok_var:
    case TokenType::tok_let:
    case TokenType::tok_return:
    case TokenType::tok_if:
    case TokenType::tok_for:
    case TokenType::tok_while:
    case TokenType::tok_rbrace:
    case TokenType::tok_semicolon:
        return true;
    default:
        return token == static_cast<int>(TokenType::tok_semicolon);
    }
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    if (CurTok == static_cast<int>(TokenType::tok_integer)) {
        auto Result = std::make_unique<IntExprAST>(m_lexer.IntVal);
        getNextToken();
        Result->setLocation(line, col);
        return Result;
    }
    auto Result = std::make_unique<NumberExprAST>(m_lexer.NumVal, m_lexer.StringVal);
    getNextToken();
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseFixedExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat fixed

    if (CurTok != '(') {
        ReportError("expected '(' after fixed");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in fixed");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected bit-width in fixed");
        return nullptr;
    }
    int bits = (int)m_lexer.NumVal;
    getNextToken(); // eat bits

    if (CurTok != ',') {
        ReportError("expected ',' in fixed");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected fraction-width in fixed");
        return nullptr;
    }
    int fract = (int)m_lexer.NumVal;
    getNextToken(); // eat fract

    if (CurTok != ')') {
        ReportError("expected ')' after fixed");
        return nullptr;
    }
    getNextToken(); // eat )

    std::unique_ptr<ExprAST> Result;
    if (auto* num = dynamic_cast<NumberExprAST*>(Val.get())) {
        Result = std::make_unique<FixedExprAST>(num->getValue(), bits, fract);
    } else {
        Result = std::make_unique<ToFixedExprAST>(std::move(Val), bits, fract);
    }
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseImaginaryExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    auto Result = std::make_unique<ComplexExprAST>(0.0, m_lexer.NumVal);
    getNextToken();
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseStringExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    std::string raw = m_lexer.StringVal;
    getNextToken();

    // Check for string interpolation ${...}
    std::vector<InterpolatedPart> parts;
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t dollar = raw.find("${", pos);
        if (dollar == std::string::npos) {
            parts.push_back({false, raw.substr(pos), nullptr});
            break;
        }
        // Static text before ${}
        if (dollar > pos)
            parts.push_back({false, raw.substr(pos, dollar - pos), nullptr});

        // Find matching } (handle nested braces)
        size_t exprStart = dollar + 2;
        int depth = 1;
        size_t end = exprStart;
        bool inStr = false;
        for (; end < raw.size() && depth > 0; ++end) {
            if (inStr) {
                if (raw[end] == '"' && (end == 0 || raw[end - 1] != '\\'))
                    inStr = false;
            } else {
                if (raw[end] == '"')
                    inStr = true;
                else if (raw[end] == '{')
                    ++depth;
                else if (raw[end] == '}')
                    --depth;
            }
        }
        if (depth != 0) {
            // Unterminated ${} — treat as literal text
            parts.push_back({false, raw.substr(dollar), nullptr});
            break;
        }
        std::string exprText = raw.substr(exprStart, end - exprStart - 1);

        // Parse expression with sub-lexer
        {
            Lexer subLexer(exprText);
            std::swap(m_lexer, subLexer);
            getNextToken();
            auto expr = ParseExpression();
            if (expr)
                parts.push_back({true, "", std::move(expr)});
            std::swap(m_lexer, subLexer);
        }
        pos = end;
    }

    if (parts.empty() || (parts.size() == 1 && !parts[0].isExpr)) {
        auto Result = std::make_unique<StringExprAST>(parts.empty() ? "" : parts[0].text);
        Result->setLocation(line, col);
        return Result;
    }

    auto Result = std::make_unique<InterpolatedStringExprAST>(std::move(parts));
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseBoolExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    bool val = (CurTok == static_cast<int>(TokenType::tok_true));
    getNextToken();
    auto Result = std::make_unique<BoolExprAST>(val);
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseImport()
{
    getNextToken(); // eat import

    std::string moduleName;
    std::string versionSpec;
    std::string alias;
    std::vector<std::string> symbols;

    if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
        CurTok != static_cast<int>(TokenType::tok_analysis) &&
        CurTok != static_cast<int>(TokenType::tok_measure)) {
        ReportError("expected module name after import");
        return nullptr;
    }

    moduleName = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok == '@') {
        getNextToken();

        if (CurTok == static_cast<int>(TokenType::tok_number)) {
            versionSpec = std::to_string(static_cast<int>(m_lexer.NumVal));
            getNextToken();

            if (CurTok == '.') {
                getNextToken();
                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                    versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                    getNextToken();

                    if (CurTok == '.') {
                        getNextToken();
                        if (CurTok == static_cast<int>(TokenType::tok_number)) {
                            versionSpec += "." + std::to_string(static_cast<int>(m_lexer.NumVal));
                            getNextToken();
                        }
                    }
                }
            }
        }
    }

    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "as") {
        getNextToken();

        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected alias name after 'as'");
            return nullptr;
        }

        alias = m_lexer.IdentifierStr;
        getNextToken();
    }

    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken();

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                symbols.push_back(m_lexer.IdentifierStr);
                getNextToken();

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

    return std::make_unique<ImportExprAST>(moduleName, versionSpec, alias, symbols);
}

// Parse: from <module> import <symbol1>, <symbol2>, ...
std::unique_ptr<ExprAST> Parser::ParseFromImport()
{
    getNextToken(); // eat from

    std::string moduleName;

    // Module name can be a string literal or an identifier
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        moduleName = m_lexer.StringVal;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        moduleName = m_lexer.IdentifierStr;
        getNextToken();
    } else {
        ReportError("expected module name after 'from'");
        return nullptr;
    }

    // Expect 'import' keyword
    if (CurTok != static_cast<int>(TokenType::tok_import)) {
        ReportError("expected 'import' after module name in from-import");
        return nullptr;
    }
    getNextToken(); // eat import

    // Parse comma-separated symbol list (no braces)
    std::vector<std::string> symbols;
    while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        symbols.push_back(m_lexer.IdentifierStr);
        getNextToken();
        if (CurTok == ',') {
            getNextToken();
        } else {
            break;
        }
    }

    if (symbols.empty()) {
        ReportError("expected at least one symbol name after 'import'");
        return nullptr;
    }

    return std::make_unique<ImportExprAST>(moduleName, "", "", symbols);
}

std::unique_ptr<ExprAST> Parser::ParseParenExpr()
{
    getNextToken();
    auto V = ParseExpression();
    if (!V)
        return nullptr;
    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();
    return V;
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    std::string IdName = m_lexer.IdentifierStr;

    // Explicit check for statement keywords that might be misparsed as identifiers
    if (IdName == "analysis") {
        int peek = m_lexer.peekToken();
        if (peek == static_cast<int>(TokenType::tok_tran) ||
            peek == static_cast<int>(TokenType::tok_dc) ||
            peek == static_cast<int>(TokenType::tok_ac) ||
            peek == static_cast<int>(TokenType::tok_noise) ||
            peek == static_cast<int>(TokenType::tok_op) ||
            peek == static_cast<int>(TokenType::tok_sens) ||
            peek == static_cast<int>(TokenType::tok_fourier) ||
            peek == static_cast<int>(TokenType::tok_lbrace) ||
            (peek == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "tf"))
            return ParseAnalysis();
    }
    if (IdName == "measure")
        return ParseMeasure();
    if (IdName == "model")
        return ParseModel();
    if (IdName == "subckt")
        return ParseSubckt();
    if (IdName == "state") {
        return ParseStateDecl();
    } else if (IdName == "ic") {
        return ParseIC();
    } else if (IdName == "dt") {
        return ParseBuiltinVar();
    }

    getNextToken();

    // Handle namespace qualifier: math::fft
    if (CurTok == static_cast<int>(TokenType::tok_namespace_sep)) {
        getNextToken(); // eat ::
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected identifier after ::");
            return nullptr;
        }
        std::string memberName = m_lexer.IdentifierStr;
        getNextToken(); // eat member identifier

        // Create namespace-qualified variable name
        std::string qualifiedName = IdName + "::" + memberName;

        // Check if followed by function call
        if (CurTok == '(') {
            getNextToken();
            std::vector<std::unique_ptr<ExprAST>> Args;
            if (CurTok != ')') {
                while (true) {
                    if (auto Arg = ParseExpression())
                        Args.push_back(std::move(Arg));
                    else
                        return nullptr;
                    if (CurTok == ')')
                        break;
                    if (CurTok != ',') {
                        ReportError("expected ')' or ',' in argument list");
                        return nullptr;
                    }
                    getNextToken();
                }
            }
            getNextToken();
            // Call with qualified name
            auto Result = std::make_unique<CallExprAST>(qualifiedName, std::move(Args));
            Result->setLocation(line, col);
            return Result;
        }

        auto Result = std::make_unique<VariableExprAST>(qualifiedName);
        Result->setLocation(line, col);
        return Result;
    }

    // Handle V(node), I(branch), and P(param) as special expressions for SPICE support
    if (CurTok == '(' && (IdName == "V" || IdName == "I" || IdName == "P")) {
        getNextToken(); // eat (
        if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
            CurTok != static_cast<int>(TokenType::tok_number) && CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected name or string in V() / I() / P()");
            return nullptr;
        }

        std::string name;
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            name = m_lexer.IdentifierStr;
        } else if (CurTok == static_cast<int>(TokenType::tok_string)) {
            name = m_lexer.StringVal;
        } else {
            name = std::to_string(static_cast<int>(m_lexer.NumVal));
        }
        getNextToken(); // eat name/string

        if (CurTok != ')') {
            ReportError("expected ')' after name");
            return nullptr;
        }
        getNextToken(); // eat )

        if (IdName == "V" || IdName == "v") {
            auto Result = std::make_unique<VoltageExprAST>(name);
            Result->setLocation(line, col);
            return Result;
        } else if (IdName == "I" || IdName == "i") {
            auto Result = std::make_unique<CurrentExprAST>(name);
            Result->setLocation(line, col);
            return Result;
        } else {
            auto Result = std::make_unique<ParameterExprAST>(name);
            Result->setLocation(line, col);
            return Result;
        }
    }

    if (CurTok == '(') {
        getNextToken();
        std::vector<std::unique_ptr<ExprAST>> Args;
        if (CurTok != ')') {
            while (true) {
                if (auto Arg = ParseExpression())
                    Args.push_back(std::move(Arg));
                else
                    return nullptr;
                if (CurTok == ')')
                    break;
                if (CurTok != ',') {
                    ReportError("expected ')' or ',' in argument list");
                    return nullptr;
                }
                getNextToken();
            }
        }
        getNextToken();
        auto Result = std::make_unique<CallExprAST>(IdName, std::move(Args));
        Result->setLocation(line, col);
        return Result;
    }
    auto Result = std::make_unique<VariableExprAST>(IdName);
    Result->setLocation(line, col);
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseIfExpr()
{
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_then)) {
        ReportError("expected then");
        return nullptr;
    }
    getNextToken();
    auto Then = ParseExpression();
    if (!Then)
        return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_else)) {
        ReportError("expected else");
        return nullptr;
    }
    getNextToken();
    auto Else = ParseExpression();
    if (!Else)
        return nullptr;
    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

std::unique_ptr<ExprAST> Parser::ParseForExpr()
{
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected identifier after for");
        return nullptr;
    }
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_in)) {
        ReportError("expected 'in' after for");
        return nullptr;
    }
    getNextToken();
    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != ',') {
        ReportError("expected ',' after for start value");
        return nullptr;
    }
    getNextToken();
    auto End = ParseExpression();
    if (!End)
        return nullptr;
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }
    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        ReportError("expected 'do' after for");
        return nullptr;
    }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body)
        return nullptr;
    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseWhileExpr()
{
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_do)) {
        ReportError("expected 'do' after while");
        return nullptr;
    }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body)
        return nullptr;
    return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
}

// ============================================================================
// Statement-based Control Flow Parsers
// Syntax: if (cond) { stmts } else { stmts }
//         for (init; cond; step) { stmts }
//         while (cond) { stmts }
// ============================================================================

std::vector<std::unique_ptr<ExprAST>> Parser::ParseStmtBlock()
{
    std::vector<std::unique_ptr<ExprAST>> stmts;
    if (CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        ReportError("expected '{' to start statement block");
        return stmts;
    }
    getNextToken(); // eat {

    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (auto Expr = ParseExpression()) {
            stmts.push_back(std::move(Expr));
        } else {
            getNextToken();
            continue;
        }
        if (CurTok == static_cast<int>(TokenType::tok_semicolon))
            getNextToken();
    }

    if (CurTok == static_cast<int>(TokenType::tok_rbrace))
        getNextToken(); // eat }

    return stmts;
}

std::unique_ptr<ExprAST> Parser::ParseIfStmt()
{
    getNextToken(); // eat if

    // Expect parentheses: if (cond) { ... }
    if (CurTok != '(') {
        ReportError("expected '(' after if");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after if condition");
        return nullptr;
    }
    getNextToken(); // eat )

    // Then block — must be a {}-block, but may be empty
    auto ThenBody = ParseStmtBlock();

    // Optional else block
    std::vector<std::unique_ptr<ExprAST>> ElseBody;
    if (CurTok == static_cast<int>(TokenType::tok_else)) {
        getNextToken(); // eat else
        if (CurTok == static_cast<int>(TokenType::tok_if)) {
            // else if  wrap as a nested IfStmt
            auto ElseIf = ParseIfStmt();
            if (!ElseIf)
                return nullptr;
            ElseBody.push_back(std::move(ElseIf));
        } else {
            ElseBody = ParseStmtBlock();
        }
    }

    return std::make_unique<IfStmtAST>(std::move(Cond), std::move(ThenBody), std::move(ElseBody));
}

std::unique_ptr<ExprAST> Parser::ParseForStmt()
{
    getNextToken(); // eat for

    // Expect parentheses: for (init; cond; step) { ... }
    if (CurTok != '(') {
        ReportError("expected '(' after for");
        return nullptr;
    }
    getNextToken(); // eat (

    // Init: can be empty (for (;;)), expression, or variable declaration
    std::unique_ptr<ExprAST> Init;
    if (CurTok != static_cast<int>(TokenType::tok_semicolon)) {
        Init = ParseExpression();
        if (!Init)
            return nullptr;
    }
    if (CurTok != static_cast<int>(TokenType::tok_semicolon)) {
        ReportError("expected ';' after for init");
        return nullptr;
    }
    getNextToken(); // eat ;

    // Condition: can be empty (treated as true)
    std::unique_ptr<ExprAST> Cond;
    if (CurTok != static_cast<int>(TokenType::tok_semicolon)) {
        Cond = ParseExpression();
        if (!Cond)
            return nullptr;
    } else {
        Cond = std::make_unique<NumberExprAST>(1.0); // always true
    }
    if (CurTok != static_cast<int>(TokenType::tok_semicolon)) {
        ReportError("expected ';' after for condition");
        return nullptr;
    }
    getNextToken(); // eat ;

    // Step: can be empty
    std::unique_ptr<ExprAST> Step;
    if (CurTok != ')') {
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }
    if (CurTok != ')') {
        ReportError("expected ')' after for step");
        return nullptr;
    }
    getNextToken(); // eat )

    // Body block
    auto Body = ParseStmtBlock();
    if (Body.empty())
        return nullptr;

    return std::make_unique<ForStmtAST>(std::move(Init), std::move(Cond), std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseWhileStmt()
{
    getNextToken(); // eat while

    // Expect parentheses: while (cond) { ... }
    if (CurTok != '(') {
        ReportError("expected '(' after while");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after while condition");
        return nullptr;
    }
    getNextToken(); // eat )

    // Body block
    auto Body = ParseStmtBlock();
    if (Body.empty())
        return nullptr;

    return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseLetExpr()
{
    bool isLet = (CurTok == static_cast<int>(TokenType::tok_let));
    getNextToken();
    if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
        CurTok != static_cast<int>(TokenType::tok_state) &&
        CurTok != static_cast<int>(TokenType::tok_ic) &&
        CurTok != static_cast<int>(TokenType::tok_dt_var) &&
        CurTok != static_cast<int>(TokenType::tok_analysis)) {
        ReportError("expected identifier after let/var");
        return nullptr;
    }
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    FluxType Type(TypeKind::Auto);
    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
        getNextToken(); // eat :
        Type = FluxType::fromToken(CurTok);
        // Check if the token is a unit type name (e.g., Voltage, Current, Ohm)
        if (Type.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_identifier)) {
            FluxType unitType = FluxType::fromUnitName(m_lexer.IdentifierStr);
            if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
                unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
                unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
                Type = unitType;
            }
        }
        getNextToken(); // eat type keyword
    }
    if (CurTok != '=') {
        ReportError("expected '=' after let/var");
        return nullptr;
    }
    getNextToken();
    auto Init = ParseExpression();
    if (!Init)
        return nullptr;

    // In block contexts, 'in' is optional. If not present, we assume it's part of a sequence.
    if (CurTok == static_cast<int>(TokenType::tok_in)) {
        getNextToken();
        auto Body = ParseExpression();
        if (!Body)
            return nullptr;
        return std::make_unique<LetExprAST>(IdName, Type, std::move(Init), std::move(Body));
    }

    return std::make_unique<LetExprAST>(IdName, Type, std::move(Init), nullptr);
}

std::unique_ptr<ExprAST> Parser::ParseLambdaExpr()
{
    getNextToken();
    if (CurTok != '(') {
        ReportError("expected '(' in lambda");
        return nullptr;
    }
    getNextToken();
    std::vector<std::string> Args;
    if (CurTok != ')') {
        while (true) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
                CurTok != static_cast<int>(TokenType::tok_state) &&
                CurTok != static_cast<int>(TokenType::tok_ic) &&
                CurTok != static_cast<int>(TokenType::tok_dt_var) &&
                CurTok != static_cast<int>(TokenType::tok_analysis)) {
                ReportError("expected identifier in lambda args");
                return nullptr;
            }
            Args.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ')')
                break;
            if (CurTok != ',') {
                ReportError("expected ')' or ',' in lambda args");
                return nullptr;
            }
            getNextToken();
        }
    }
    getNextToken();
    auto Body = ParseExpression();
    if (!Body)
        return nullptr;
    return std::make_unique<LambdaExprAST>(std::move(Args), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseUnaryExpr()
{
    bool isUnary = false;
    if (CurTok == '-' || CurTok == '+' || CurTok == static_cast<int>(TokenType::tok_logical_not) ||
        CurTok == static_cast<int>(TokenType::tok_bitwise_not)) {
        isUnary = true;
    }

    if (!isUnary || CurTok == '(' || CurTok == '[' || CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        return ParsePrimary();
    }

    int Opc = CurTok;
    getNextToken();
    if (auto Operand = ParseUnaryExpr())
        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    return nullptr;
}

std::unique_ptr<ExprAST> Parser::ParseMatrixExpr()
{
    getNextToken();
    std::vector<std::vector<std::unique_ptr<ExprAST>>> Rows;
    if (CurTok == ']') {
        getNextToken();
        return std::make_unique<MatrixExprAST>(std::move(Rows), 0, 0);
    }
    int maxCols = 0;
    while (true) {
        std::vector<std::unique_ptr<ExprAST>> CurrentRow;
        while (true) {
            auto Elem = ParseExpression();
            if (!Elem)
                return nullptr;
            CurrentRow.push_back(std::move(Elem));
            if (CurTok == ']' || CurTok == static_cast<int>(TokenType::tok_semicolon))
                break;
            if (CurTok == ',')
                getNextToken();
            else {
                ReportError("expected ',', ';', or ']' in matrix");
                return nullptr;
            }
        }
        if ((int)CurrentRow.size() > maxCols)
            maxCols = CurrentRow.size();
        Rows.push_back(std::move(CurrentRow));
        if (CurTok == ']') {
            getNextToken();
            break;
        }
        getNextToken();
    }
    for (auto& row : Rows)
        while (row.size() < (size_t)maxCols)
            row.push_back(std::make_unique<NumberExprAST>(0.0));
    return std::make_unique<MatrixExprAST>(std::move(Rows), Rows.size(), maxCols);
}

std::unique_ptr<ExprAST> Parser::ParseRangeExpr()
{
    auto Start = ParseExpression();
    if (!Start)
        return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_colon))
        return Start;
    getNextToken();
    auto End = ParseExpression();
    if (!End)
        return nullptr;
    return std::make_unique<RangeExprAST>(std::move(Start), std::move(End));
}

std::unique_ptr<ExprAST> Parser::ParseBlockExpr()
{
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> Stmts;
    while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (auto Expr = ParseExpression()) {
            Stmts.push_back(std::move(Expr));
            clearError(); // Clear any transient parse errors — we recovered
        } else {
            SkipToSynchronizationPoint();
            clearError();
        }
        while (CurTok == static_cast<int>(TokenType::tok_semicolon))
            getNextToken();
    }
    if (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
        ReportError("expected '}'");
        return nullptr;
    }
    getNextToken();
    return std::make_unique<BlockExprAST>(std::move(Stmts));
}

std::unique_ptr<ExprAST> Parser::ParsePrimary()
{
    std::unique_ptr<ExprAST> Res;
    switch (CurTok) {
    case static_cast<int>(TokenType::tok_identifier):
        Res = ParseIdentifierExpr();
        break;
    case static_cast<int>(TokenType::tok_number):
    case static_cast<int>(TokenType::tok_integer):
        Res = ParseNumberExpr();
        break;
    case static_cast<int>(TokenType::tok_true):
    case static_cast<int>(TokenType::tok_false):
        Res = ParseBoolExpr();
        break;
    case static_cast<int>(TokenType::tok_fixed):
        Res = ParseFixedExpr();
        break;
    case static_cast<int>(TokenType::tok_imaginary):
        Res = ParseImaginaryExpr();
        break;
    case static_cast<int>(TokenType::tok_string):
        Res = ParseStringExpr();
        break;
    case '(':
        Res = ParseParenExpr();
        break;
    case '[':
        Res = ParseMatrixExpr();
        break;
    case static_cast<int>(TokenType::tok_lbrace):
        Res = ParseBlockExpr();
        break;
    case static_cast<int>(TokenType::tok_if):
        // Peek ahead: if '(' follows, it's statement-based; otherwise expression-based
        if (m_lexer.peekToken() == '(') {
            Res = ParseIfStmt();
        } else {
            Res = ParseIfExpr();
        }
        break;
    case static_cast<int>(TokenType::tok_for):
        if (m_lexer.peekToken() == '(') {
            Res = ParseForStmt();
        } else {
            Res = ParseForExpr();
        }
        break;
    case static_cast<int>(TokenType::tok_while):
        if (m_lexer.peekToken() == '(') {
            Res = ParseWhileStmt();
        } else {
            Res = ParseWhileExpr();
        }
        break;
    case static_cast<int>(TokenType::tok_let):
    case static_cast<int>(TokenType::tok_var):
        Res = ParseLetExpr();
        break;
    case static_cast<int>(TokenType::tok_type_matrix):
    case static_cast<int>(TokenType::tok_type_vector):
        getNextToken(); // eat keyword
        if (CurTok != '(') {
            ReportError("expected '(' after matrix/vector");
            return nullptr;
        }
        getNextToken(); // eat (
        Res = ParseExpression();
        if (CurTok != ')') {
            ReportError("expected ')' after matrix/vector constructor");
            return nullptr;
        }
        getNextToken(); // eat )
        break;
    case static_cast<int>(TokenType::tok_fn):
        Res = ParseLambdaExpr();
        break;
    case static_cast<int>(TokenType::tok_import):
        Res = ParseImport();
        break;
    case static_cast<int>(TokenType::tok_from):
        Res = ParseFromImport();
        break;
    case static_cast<int>(TokenType::tok_debug):
        Res = ParseDebugStmt();
        break;
    case static_cast<int>(TokenType::tok_sensitivity):
        Res = ParseSensitivityStmt();
        break;
    case static_cast<int>(TokenType::tok_ask):
        Res = ParseAskExpr();
        break;
    case static_cast<int>(TokenType::tok_explain):
        Res = ParseExplainExpr();
        break;
    case static_cast<int>(TokenType::tok_inputs):
        Res = ParseInputsExpr();
        break;
    case static_cast<int>(TokenType::tok_outputs):
        Res = ParseOutputsExpr();
        break;
    case static_cast<int>(TokenType::tok_return):
        getNextToken(); // eat return
        Res = std::make_unique<ReturnExprAST>(ParseExpression());
        break;

    // Advanced control flow
    case static_cast<int>(TokenType::tok_switch):
        Res = ParseSwitchExpr();
        break;
    case static_cast<int>(TokenType::tok_break):
        Res = ParseBreakExpr();
        break;
    case static_cast<int>(TokenType::tok_continue):
        Res = ParseContinueExpr();
        break;

    // Additional keywords
    case static_cast<int>(TokenType::tok_try):
        Res = ParseTryCatchExpr();
        break;
    case static_cast<int>(TokenType::tok_throw):
        Res = ParseThrowExpr();
        break;
    case static_cast<int>(TokenType::tok_assert):
        Res = ParseAssertExpr();
        break;
    case static_cast<int>(TokenType::tok_analog):
        Res = ParseAnalogBlock();
        break;

    // Symbolic math
    case static_cast<int>(TokenType::tok_sym):
        Res = ParseSymDecl();
        break;
    case static_cast<int>(TokenType::tok_simplify):
        Res = ParseSimplifyExpr();
        break;
    case static_cast<int>(TokenType::tok_differentiate):
        Res = ParseDifferentiateExpr();
        break;
    case static_cast<int>(TokenType::tok_substitute):
        Res = ParseSubstituteExpr();
        break;
    case static_cast<int>(TokenType::tok_evaluate):
        Res = ParseEvaluateExpr();
        break;
    case static_cast<int>(TokenType::tok_jacobian):
        Res = ParseJacobianExpr();
        break;
    case static_cast<int>(TokenType::tok_pde):
        Res = ParsePDEExpr();
        break;
    case static_cast<int>(TokenType::tok_partial_diff):
        Res = ParsePartialDiffExpr();
        break;
    case static_cast<int>(TokenType::tok_thermal):
        Res = ParseThermalBlock();
        break;
    case static_cast<int>(TokenType::tok_goal):
        Res = ParseGoalExpr();
        break;
    case static_cast<int>(TokenType::tok_optimize):
        Res = ParseOptimizeExpr();
        break;
    case static_cast<int>(TokenType::tok_train):
        Res = ParseTrainExpr();
        break;
    case static_cast<int>(TokenType::tok_predict):
        Res = ParsePredictExpr();
        break;

    // Analysis and Measurements
    case static_cast<int>(TokenType::tok_measure):
        Res = ParseMeasure();
        break;
    case static_cast<int>(TokenType::tok_model):
        Res = ParseModel();
        break;
    case static_cast<int>(TokenType::tok_subckt):
        Res = ParseSubckt();
        break;
    case static_cast<int>(TokenType::tok_montecarlo):
        Res = ParseMonteCarlo();
        break;
    case static_cast<int>(TokenType::tok_virtual_probe):
        Res = ParseVirtualProbe();
        break;
    case static_cast<int>(TokenType::tok_hot_swap):
        Res = ParseHotSwap();
        break;

    case static_cast<int>(TokenType::tok_settle):
        Res = ParseSettleDecl();
        break;
    case static_cast<int>(TokenType::tok_golden):
        Res = ParseGoldenDecl();
        break;
    case static_cast<int>(TokenType::tok_compare):
        Res = ParseCompareDecl();
        break;
    case static_cast<int>(TokenType::tok_converge):
        Res = ParseConvergeDecl();
        break;
    case static_cast<int>(TokenType::tok_discontinuity):
        Res = ParseDiscontinuityDecl();
        break;
    case static_cast<int>(TokenType::tok_verify):
        Res = ParseVerifyBlock();
        break;
    case static_cast<int>(TokenType::tok_yield):
        Res = ParseYieldExpr();
        break;
    case static_cast<int>(TokenType::tok_corner):
        Res = ParseCornerExpr();
        break;

    // Explicit SPICE-like keywords
    case static_cast<int>(TokenType::tok_tran):
    case static_cast<int>(TokenType::tok_dc):
    case static_cast<int>(TokenType::tok_ac):
    case static_cast<int>(TokenType::tok_max):
    case static_cast<int>(TokenType::tok_min):
    case static_cast<int>(TokenType::tok_dt_var):
    case static_cast<int>(TokenType::tok_state):
    case static_cast<int>(TokenType::tok_ic):
    case static_cast<int>(TokenType::tok_analysis):
        Res = ParseIdentifierExpr();
        break;

    case static_cast<int>(TokenType::tok_match):
        Res = ParseMatchExpr();
        break;
    case static_cast<int>(TokenType::tok_foreach):
        Res = ParseForeachExpr();
        break;
    case static_cast<int>(TokenType::tok_repeat):
        Res = ParseRepeatUntil();
        break;
    case static_cast<int>(TokenType::tok_do):
        Res = ParseDoWhile();
        break;
    case static_cast<int>(TokenType::tok_parallel):
        Res = ParseParallelForExpr();
        break;

    // Schematic generation
    case static_cast<int>(TokenType::tok_schematic):
        Res = ParseSchematicExpr();
        break;
    case static_cast<int>(TokenType::tok_export):
        Res = ParseExportSchematic();
        break;

    // SPICE Time-Domain Simulation
    case static_cast<int>(TokenType::tok_time):
    case static_cast<int>(TokenType::tok_temp):
        Res = ParseBuiltinVar();
        break;
    case static_cast<int>(TokenType::tok_update):
        Res = ParseUpdateFunc();
        break;

    // SPICE Behavioral Sources
    case static_cast<int>(TokenType::tok_bsource):
        Res = ParseBSource();
        break;
    case static_cast<int>(TokenType::tok_esource):
        Res = ParseESource();
        break;
    case static_cast<int>(TokenType::tok_fsource):
        Res = ParseFSource();
        break;
    case static_cast<int>(TokenType::tok_gsource):
        Res = ParseGSource();
        break;
    case static_cast<int>(TokenType::tok_hsource):
        Res = ParseHSource();
        break;

    // SPICE Analysis Control
    case static_cast<int>(TokenType::tok_probe):
        Res = ParseProbe();
        break;
    case static_cast<int>(TokenType::tok_save):
        Res = ParseSave();
        break;
    case static_cast<int>(TokenType::tok_param):
        Res = ParseParam();
        break;

    /* Section 7.2: Mixed-Signal & Modeling Extensions */
    case static_cast<int>(TokenType::tok_above):
        Res = ParseAboveExpr();
        break;
    case static_cast<int>(TokenType::tok_timer):
        Res = ParseTimerExpr();
        break;
    case static_cast<int>(TokenType::tok_fsm):
        Res = ParseFSMExpr();
        break;
    case static_cast<int>(TokenType::tok_edge):
        Res = ParseEdgeExpr();
        break;
    case static_cast<int>(TokenType::tok_triggered):
        Res = ParseTriggeredExpr();
        break;
    case static_cast<int>(TokenType::tok_noise_fn):
        Res = ParseNoiseExpr();
        break;
    case static_cast<int>(TokenType::tok_white_noise):
        Res = ParseWhiteNoiseExpr();
        break;
    case static_cast<int>(TokenType::tok_flicker_noise):
        Res = ParseFlickerNoiseExpr();
        break;
    case static_cast<int>(TokenType::tok_thermal_noise):
        Res = ParseThermalNoiseExpr();
        break;
    case static_cast<int>(TokenType::tok_piecewise):
        Res = ParsePiecewiseExpr();
        break;
    case static_cast<int>(TokenType::tok_table):
        Res = ParseTableExpr();
        break;
    case static_cast<int>(TokenType::tok_csv_import):
        Res = ParseCsvImportExpr();
        break;
    case static_cast<int>(TokenType::tok_unit):
        Res = ParseUnitExpr();
        break;
    case static_cast<int>(TokenType::tok_dimension):
        Res = ParseDimensionExpr();
        break;
    case static_cast<int>(TokenType::tok_convert):
        Res = ParseConvertExpr();
        break;
    case static_cast<int>(TokenType::tok_has_unit):
        Res = ParseHasUnitExpr();
        break;

    default: {
        std::string msg = "unknown token in expression: ";
        if (CurTok > 0 && CurTok < 128)
            msg += (char)CurTok;
        else
            msg += std::to_string(CurTok);
        ReportError(msg);
        return nullptr;
    }
    }
    if (!Res)
        return nullptr;
    while (true) {
        if (CurTok == static_cast<int>(TokenType::tok_transpose)) {
            Res = std::make_unique<TransposeExprAST>(std::move(Res));
            getNextToken();
        } else if (CurTok == static_cast<int>(TokenType::tok_dot)) {
            getNextToken(); // eat .
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected identifier after '.'");
                return nullptr;
            }
            std::string MemberName = m_lexer.IdentifierStr;
            getNextToken();
            Res = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
        } else {
            break;
        }
    }
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS)
{
    while (true) {
        int TokPrec = GetTokPrecedence();
        if (TokPrec < ExprPrec)
            return LHS;
        int BinOp = CurTok;
        getNextToken();
        auto RHS = ParseUnaryExpr();
        if (!RHS)
            return nullptr;
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }
        if (BinOp == '=') {
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS));
        } else if (BinOp == static_cast<int>(TokenType::tok_plus_equal)) {
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS), '+');
        } else if (BinOp == static_cast<int>(TokenType::tok_minus_equal)) {
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS), '-');
        } else if (BinOp == static_cast<int>(TokenType::tok_star_equal)) {
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS), '*');
        } else if (BinOp == static_cast<int>(TokenType::tok_slash_equal)) {
            LHS = std::make_unique<AssignExprAST>(std::move(LHS), std::move(RHS), '/');
        } else if (BinOp == static_cast<int>(TokenType::tok_colon)) {
            // start:end or start:step:end range expression
            // RHS holds the value after the first colon
            if (CurTok == static_cast<int>(TokenType::tok_colon)) {
                // start:step:end form
                getNextToken();
                auto thirdExpr = ParseUnaryExpr();
                if (!thirdExpr)
                    return nullptr;
                int NextPrec3 = GetTokPrecedence();
                if (TokPrec < NextPrec3) {
                    thirdExpr = ParseBinOpRHS(TokPrec + 1, std::move(thirdExpr));
                    if (!thirdExpr)
                        return nullptr;
                }
                LHS = std::make_unique<RangeExprAST>(std::move(LHS), std::move(RHS), std::move(thirdExpr));
            } else {
                // start:end form — RHS is already the end value
                LHS = std::make_unique<RangeExprAST>(std::move(LHS), std::move(RHS));
            }
        } else if (BinOp == '[') {
            // A[i, j] — matrix indexing; A[i:j, k:l] — slicing
            auto rowIdx = std::move(RHS);
            if (CurTok == static_cast<int>(TokenType::tok_colon)) {
                getNextToken();
                auto end = ParseExpression();
                if (!end)
                    return nullptr;
                rowIdx = std::make_unique<RangeExprAST>(std::move(rowIdx), std::move(end));
            }
            std::unique_ptr<ExprAST> colIdx;
            if (CurTok == ',') {
                getNextToken();
                auto colStart = ParseUnaryExpr();
                if (!colStart)
                    return nullptr;
                if (CurTok == static_cast<int>(TokenType::tok_colon)) {
                    getNextToken();
                    auto colEnd = ParseUnaryExpr();
                    if (!colEnd)
                        return nullptr;
                    colIdx = std::make_unique<RangeExprAST>(std::move(colStart), std::move(colEnd));
                } else {
                    // Parse the rest of the column expression (operators after the start)
                    colIdx = ParseBinOpRHS(0, std::move(colStart));
                    if (!colIdx)
                        return nullptr;
                }
            }
            if (CurTok != ']') {
                ReportError("expected ']' after index");
                return nullptr;
            }
            getNextToken();
            LHS = std::make_unique<IndexExprAST>(std::move(LHS), std::move(rowIdx), std::move(colIdx));
        } else {
            LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
        }
    }
}

std::unique_ptr<ExprAST> Parser::ParseExpression()
{
    auto LHS = ParseUnaryExpr();
    if (!LHS)
        return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<PrototypeAST> Parser::ParsePrototype()
{
    if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
        CurTok != static_cast<int>(TokenType::tok_update) &&
        CurTok != static_cast<int>(TokenType::tok_state) &&
        CurTok != static_cast<int>(TokenType::tok_ic) &&
        CurTok != static_cast<int>(TokenType::tok_dt_var))
        return nullptr;

    std::string FnName = (CurTok == static_cast<int>(TokenType::tok_update)) ? "update" : m_lexer.IdentifierStr;
    getNextToken();
    if (CurTok != '(')
        return nullptr;
    getNextToken();
    std::vector<std::pair<std::string, FluxType>> Args;
    while (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_inputs) ||
           CurTok == static_cast<int>(TokenType::tok_outputs) || CurTok == static_cast<int>(TokenType::tok_dt_var) ||
           CurTok == static_cast<int>(TokenType::tok_state) || CurTok == static_cast<int>(TokenType::tok_ic) ||
           CurTok == static_cast<int>(TokenType::tok_analysis) ||
           CurTok == static_cast<int>(TokenType::tok_ddt) || CurTok == static_cast<int>(TokenType::tok_idt)) {
        std::string Name;
        if (CurTok == static_cast<int>(TokenType::tok_identifier))
            Name = m_lexer.IdentifierStr;
        else if (CurTok == static_cast<int>(TokenType::tok_inputs))
            Name = "inputs";
        else if (CurTok == static_cast<int>(TokenType::tok_outputs))
            Name = "outputs";
        else
            Name = m_lexer.IdentifierStr;

        getNextToken();
        FluxType Type(TypeKind::Double);
        if (CurTok == static_cast<int>(TokenType::tok_colon)) {
            getNextToken(); // eat :
            Type = FluxType::fromToken(CurTok);
            // Check for unit type names (e.g., Voltage, Current)
            if (Type.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_identifier)) {
                FluxType unitType = FluxType::fromUnitName(m_lexer.IdentifierStr);
                if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
                    unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
                    unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
                    Type = unitType;
                }
            }
            getNextToken(); // eat type keyword
        }
        Args.push_back({Name, Type});
        if (CurTok == ')')
            break;
        if (CurTok != ',')
            return nullptr;
        getNextToken();
    }
    if (CurTok != ')')
        return nullptr;
    getNextToken();
    FluxType RetType(TypeKind::Double);
    if (CurTok == static_cast<int>(TokenType::tok_arrow)) {
        getNextToken();
        RetType = FluxType::fromToken(CurTok);
        // Check for unit type names in return type annotation
        if (RetType.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_identifier)) {
            FluxType unitType = FluxType::fromUnitName(m_lexer.IdentifierStr);
            if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
                unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
                unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
                RetType = unitType;
            }
        }
        getNextToken();
    }
    return std::make_unique<PrototypeAST>(FnName, std::move(Args), RetType);
}

std::unique_ptr<PrototypeAST> Parser::ParseExtern()
{
    getNextToken();
    return ParsePrototype();
}

std::unique_ptr<FunctionAST> Parser::ParseDefinition()
{
    getNextToken();
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    if (auto Body = ParseExpression())
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
    return nullptr;
}

// Parse debug statement: debug circuit "file.flux" { symptom: "clipping" }
std::unique_ptr<DebugStmtAST> Parser::ParseDebugStmt()
{
    getNextToken(); // eat debug

    std::string circuitFile;
    std::string symptom;
    std::map<std::string, std::string> options;

    // Parse circuit file
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        circuitFile = m_lexer.StringVal;
        getNextToken();
    }

    // Parse options block
    if (CurTok == '{') {
        getNextToken();
        while (CurTok != '}' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string key = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == ':') {
                    getNextToken();
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        options[key] = m_lexer.StringVal;
                        if (key == "symptom")
                            symptom = m_lexer.StringVal;
                        getNextToken();
                    }
                }
            }
            if (CurTok == ',')
                getNextToken();
            else if (CurTok != '}')
                break;
        }
        if (CurTok == '}')
            getNextToken();
    }

    return std::make_unique<DebugStmtAST>(circuitFile, symptom, options);
}

// Parse sensitivity statement: sensitivity(output="Vout", params=["R1", "C1"])
std::unique_ptr<SensitivityStmtAST> Parser::ParseSensitivityStmt()
{
    getNextToken(); // eat sensitivity

    std::string output;
    std::vector<std::string> params;
    std::string circuit;

    if (CurTok == '(') {
        getNextToken();
        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string key = m_lexer.IdentifierStr;
                getNextToken();
                if (CurTok == '=') {
                    getNextToken();
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        if (key == "output")
                            output = m_lexer.StringVal;
                        else if (key == "circuit")
                            circuit = m_lexer.StringVal;
                        getNextToken();
                    } else if (CurTok == '[') {
                        // Parse array
                        getNextToken();
                        while (CurTok != ']') {
                            if (CurTok == static_cast<int>(TokenType::tok_string)) {
                                params.push_back(m_lexer.StringVal);
                                getNextToken();
                            }
                            if (CurTok == ',')
                                getNextToken();
                            else if (CurTok != ']')
                                break;
                        }
                        if (CurTok == ']')
                            getNextToken();
                    }
                }
            }
            if (CurTok == ',')
                getNextToken();
            else if (CurTok != ')')
                break;
        }
        if (CurTok == ')')
            getNextToken();
    }

    return std::make_unique<SensitivityStmtAST>(output, params, circuit);
}

// Parse ask expression: ask "Why is gain low?"
std::unique_ptr<AskExprAST> Parser::ParseAskExpr()
{
    getNextToken(); // eat ask

    std::string question;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        question = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<AskExprAST>(question);
}

// Parse explain expression: explain circuit "file.flux"
std::unique_ptr<ExplainExprAST> Parser::ParseExplainExpr()
{
    getNextToken(); // eat explain

    std::string circuit;
    std::string aspect;

    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "circuit") {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            circuit = m_lexer.StringVal;
            getNextToken();
        }
    } else if (CurTok == static_cast<int>(TokenType::tok_string)) {
        aspect = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<ExplainExprAST>(circuit, aspect);
}

// Parse substitute statement: substitute "RC0603FR-0710KL"
std::unique_ptr<SubstituteStmtAST> Parser::ParseSubstituteStmt()
{
    getNextToken(); // eat substitute

    std::string partNumber;
    std::vector<std::string> options;

    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        partNumber = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<SubstituteStmtAST>(partNumber, options);
}

std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr()
{
    if (auto Body = ParseExpression()) {
        FluxType RetType(TypeKind::Double);
        if (dynamic_cast<ComplexExprAST*>(Body.get())) {
            RetType = FluxType(TypeKind::Complex);
        } else if (dynamic_cast<MatrixExprAST*>(Body.get())) {
            RetType = FluxType(TypeKind::Matrix);
        }
        return std::make_unique<FunctionAST>(
            std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::pair<std::string, FluxType>>(), RetType),
            std::move(Body));
    }
    return nullptr;
}

// ============================================================================
// SPICE Parser Implementations
// ============================================================================

// Parse built-in variable: time, dt, temp
std::unique_ptr<ExprAST> Parser::ParseBuiltinVar()
{
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken(); // eat variable name
    return std::make_unique<BuiltinVarExprAST>(VarName);
}

// Parse update function: update(t, inputs) or update(t, inputs, outputs, state) { ... }
std::unique_ptr<ExprAST> Parser::ParseUpdateFunc()
{
    getNextToken(); // eat update

    if (CurTok != '(') {
        ReportError("expected '(' after update");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse time variable name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected time variable name");
        return nullptr;
    }
    std::string TimeVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' in update function");
        return nullptr;
    }
    getNextToken(); // eat ,

    // Parse inputs variable name (also accept keyword tok_inputs as a variable name)
    if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_inputs)) {
        ReportError("expected inputs variable name");
        return nullptr;
    }
    std::string InputsVar = (CurTok == static_cast<int>(TokenType::tok_inputs)) ? "inputs" : m_lexer.IdentifierStr;
    getNextToken();

    // Check for optional outputs and state parameters
    std::string OutputsVar, StateVar;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
            CurTok != static_cast<int>(TokenType::tok_outputs)) {
            ReportError("expected outputs variable name");
            return nullptr;
        }
        OutputsVar = (CurTok == static_cast<int>(TokenType::tok_outputs)) ? "outputs" : m_lexer.IdentifierStr;
        getNextToken();

        if (CurTok == ',') {
            getNextToken(); // eat ,
            if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
                CurTok != static_cast<int>(TokenType::tok_state)) {
                ReportError("expected state variable name");
                return nullptr;
            }
            StateVar = (CurTok == static_cast<int>(TokenType::tok_state)) ? "state" : m_lexer.IdentifierStr;
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after update parameters");
        return nullptr;
    }
    getNextToken(); // eat )

    // Parse body
    auto Body = ParseBlockExpr();
    if (!Body) {
        ReportError("expected body for update function");
        return nullptr;
    }

    if (!OutputsVar.empty() && !StateVar.empty()) {
        return std::make_unique<UpdateFuncAST>(TimeVar, InputsVar, OutputsVar, StateVar, std::move(Body));
    }
    return std::make_unique<UpdateFuncAST>(TimeVar, InputsVar, std::move(Body));
}

// Parse B-source: bsource name V(node+, node-) { expression }
std::unique_ptr<ExprAST> Parser::ParseBSource()
{
    getNextToken(); // eat bsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected B-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse V or I prefix
    bool IsCurrent = false;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        std::string Prefix = m_lexer.IdentifierStr;
        if (Prefix == "I")
            IsCurrent = true;
        else if (Prefix != "V") {
            ReportError("expected V or I prefix for B-source");
            return nullptr;
        }
        getNextToken();
    }

    // Parse V(node+, node-)
    if (CurTok != '(') {
        ReportError("expected '(' for B-source nodes");
        return nullptr;
    }
    getNextToken(); // eat (

    auto isNodeToken = [](int tok) {
        return tok == static_cast<int>(TokenType::tok_identifier) || tok == static_cast<int>(TokenType::tok_number) ||
               tok == static_cast<int>(TokenType::tok_integer);
    };
    auto nodeName = [&](int tok) -> std::string {
        if (tok == static_cast<int>(TokenType::tok_identifier))
            return m_lexer.IdentifierStr;
        if (tok == static_cast<int>(TokenType::tok_number))
            return std::to_string(m_lexer.NumVal);
        return std::to_string(static_cast<int>(m_lexer.IntVal));
    };
    if (!isNodeToken(CurTok)) {
        ReportError("expected positive node name");
        return nullptr;
    }
    std::string PosNode = nodeName(CurTok);
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between nodes");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (!isNodeToken(CurTok)) {
        ReportError("expected negative node name");
        return nullptr;
    }
    std::string NegNode = nodeName(CurTok);
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after nodes");
        return nullptr;
    }
    getNextToken(); // eat )

    // Parse expression body
    auto Expr = ParseBlockExpr();
    if (!Expr) {
        ReportError("expected expression body for B-source");
        return nullptr;
    }

    return std::make_unique<BSourceExprAST>(Name, PosNode, NegNode, std::move(Expr), IsCurrent);
}

// Parse E-source (VCVS): esource name V(node+, node-) V(ctrl+, ctrl-) { gain }
std::unique_ptr<ExprAST> Parser::ParseESource()
{
    getNextToken(); // eat esource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected E-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes V(node+, node-)
    if (CurTok != '(') {
        ReportError("expected '(' for E-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string PosNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string NegNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse control nodes V(ctrl+, ctrl-)
    if (CurTok != '(') {
        ReportError("expected '(' for E-source control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlPosNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string CtrlNegNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse gain expression
    auto Gain = ParseBlockExpr();
    if (!Gain)
        Gain = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<ESourceExprAST>(Name, PosNode, NegNode, CtrlPosNode, CtrlNegNode, std::move(Gain));
}

// Parse F-source (CCCS): fsource name V(node+, node-) Vname { gain }
std::unique_ptr<ExprAST> Parser::ParseFSource()
{
    getNextToken(); // eat fsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected F-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse voltage source name for current sensing
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected voltage source name for current sensing");
        return nullptr;
    }
    std::string VSourceName = m_lexer.IdentifierStr;
    getNextToken();

    // Parse gain
    auto Gain = ParseBlockExpr();
    if (!Gain)
        Gain = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<FSourceExprAST>(Name, PosNode, NegNode, VSourceName, std::move(Gain));
}

// Parse G-source (VCCS): gsource name V(node+, node-) V(ctrl+, ctrl-) { transconductance }
std::unique_ptr<ExprAST> Parser::ParseGSource()
{
    getNextToken(); // eat gsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected G-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse control nodes
    if (CurTok != '(') {
        ReportError("expected '(' for control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlPosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                  ? m_lexer.IdentifierStr
                                  : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string CtrlNegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                  ? m_lexer.IdentifierStr
                                  : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse transconductance
    auto Transcond = ParseBlockExpr();
    if (!Transcond)
        Transcond = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<GSourceExprAST>(Name, PosNode, NegNode, CtrlPosNode, CtrlNegNode, std::move(Transcond));
}

// Parse H-source (CCVS): hsource name V(node+, node-) Vname { transresistance }
std::unique_ptr<ExprAST> Parser::ParseHSource()
{
    getNextToken(); // eat hsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected H-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '('");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ','");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')'");
        return nullptr;
    }
    getNextToken();

    // Parse voltage source name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected voltage source name");
        return nullptr;
    }
    std::string VSourceName = m_lexer.IdentifierStr;
    getNextToken();

    // Parse transresistance
    auto Transres = ParseBlockExpr();
    if (!Transres)
        Transres = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<HSourceExprAST>(Name, PosNode, NegNode, VSourceName, std::move(Transres));
}
// Parse analysis directive: analysis tran { tstop, tstart, tstep }
std::unique_ptr<AnalysisExprAST> Parser::ParseAnalysis()
{
    getNextToken(); // eat analysis

    std::string typeStr = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : Lexer::tokenSpelling(CurTok);
    AnalysisType AType;

    if (typeStr == "tran")
        AType = AnalysisType::TRAN;
    else if (typeStr == "dc")
        AType = AnalysisType::DC;
    else if (typeStr == "ac")
        AType = AnalysisType::AC;
    else if (typeStr == "noise")
        AType = AnalysisType::NOISE;
    else if (typeStr == "op")
        AType = AnalysisType::OP;
    else if (typeStr == "tf")
        AType = AnalysisType::TF;
    else if (typeStr == "sens")
        AType = AnalysisType::SENS;
    else if (typeStr == "fourier")
        AType = AnalysisType::FOURIER;
    else {
        ReportError("expected analysis type, found: " + typeStr);
        return nullptr;
    }

    getNextToken(); // eat type

    auto Analysis = std::make_unique<AnalysisExprAST>(AType);

    // Parse parameters in braces
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok < 0) {
                std::string ParamName = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                            ? m_lexer.IdentifierStr
                                            : Lexer::tokenSpelling(CurTok);
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Analysis->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Analysis;
}
// Parse measure directive: measure name MAX { expression, params }
std::unique_ptr<MeasureExprAST> Parser::ParseMeasure()
{
    getNextToken(); // eat measure

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected measurement name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // CurTok now points to the measurement type (MAX, MIN, etc.)
    // Parse measurement type
    std::string typeStr = Lexer::tokenSpelling(CurTok);
    MeasureType MType;
    if (typeStr == "MAX")
        MType = MeasureType::MAX;
    else if (typeStr == "MIN")
        MType = MeasureType::MIN;
    else if (typeStr == "AVG")
        MType = MeasureType::AVG;
    else if (typeStr == "RMS")
        MType = MeasureType::RMS;
    else if (typeStr == "TRIG")
        MType = MeasureType::TRIG;
    else if (typeStr == "TARG")
        MType = MeasureType::TARG;
    else if (typeStr == "WHEN")
        MType = MeasureType::WHEN;
    else if (typeStr == "FIND")
        MType = MeasureType::FIND;
    else if (typeStr == "DERIV")
        MType = MeasureType::DERIV;
    else if (typeStr == "INTEG")
        MType = MeasureType::INTEG;
    else {
        ReportError("expected measurement type, found: " + typeStr);
        return nullptr;
    }

    getNextToken(); // eat type

    // Parse expression
    auto Expr = ParseExpression();
    if (!Expr) {
        ReportError("expected expression for measure");
        return nullptr;
    }

    auto Measure = std::make_unique<MeasureExprAST>(Name, MType, std::move(Expr));

    // Parse optional parameters
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string ParamName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Measure->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Measure;
}

// Parse probe directive: probe varname [as "outputname"]
std::unique_ptr<ExprAST> Parser::ParseProbe()
{
    getNextToken(); // eat probe

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after probe");
        return nullptr;
    }
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();

    std::string OutputName;
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "as") {
        getNextToken(); // eat as
        if (CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected string after 'as'");
            return nullptr;
        }
        OutputName = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<ProbeExprAST>(VarName, OutputName);
}

// Parse save directive: save varname
std::unique_ptr<ExprAST> Parser::ParseSave()
{
    getNextToken(); // eat save

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after save");
        return nullptr;
    }
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();

    return std::make_unique<SaveExprAST>(VarName);
}

// Parse subckt definition: subckt name (pin1, pin2, ...) { params... body... }
std::unique_ptr<SubcktAST> Parser::ParseSubckt()
{
    getNextToken(); // eat subckt

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected subcircuit name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse pin list
    std::vector<std::string> Pins;
    if (CurTok == '(') {
        getNextToken(); // eat (

        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok >= 0) {
                ReportError("expected pin name");
                return nullptr;
            }
            Pins.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == ')')
            getNextToken(); // eat )
    }

    auto Subckt = std::make_unique<SubcktExprAST>(Name, std::move(Pins));

    // Parse body
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_def)) {
                if (auto Func = ParseDefinition()) {
                    Subckt->addFunction(std::move(Func));
                } else {
                    return nullptr;
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_param) ||
                       CurTok == static_cast<int>(TokenType::tok_params)) {
                if (auto Param = ParseParam()) {
                    Subckt->addStatement(std::move(Param));
                } else {
                    return nullptr;
                }
            } else if (auto Stmt = ParseExpression()) {
                Subckt->addStatement(std::move(Stmt));
            } else {
                ReportError("invalid statement in subcircuit");
                return nullptr;
            }
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Subckt;
}

// Parse model declaration: model name type { params }
std::unique_ptr<ModelAST> Parser::ParseModel()
{
    getNextToken(); // eat model

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model type (D, NPN, PNP, NMOS, PMOS, R, L, C, etc.)");
        return nullptr;
    }
    std::string ModelType = m_lexer.IdentifierStr;
    getNextToken();

    auto Model = std::make_unique<ModelExprAST>(Name, ModelType);

    // Parse parameters
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string ParamName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Model->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Model;
}

// Parse param declaration: param name = value
std::unique_ptr<ExprAST> Parser::ParseParam()
{
    getNextToken(); // eat param

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected parameter name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    std::unique_ptr<ExprAST> Value = std::make_unique<NumberExprAST>(0.0);

    if (CurTok == '=') {
        getNextToken(); // eat =
        Value = ParseExpression();
        if (!Value)
            return nullptr;
    }

    return std::make_unique<ParamExprAST>(Name, std::move(Value));
}

// Parse initial condition: ic V(node) = value
std::unique_ptr<ExprAST> Parser::ParseIC()
{
    getNextToken(); // eat ic

    // Parse V(node)
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || m_lexer.IdentifierStr != "V") {
        ReportError("expected V(node) for initial condition");
        return nullptr;
    }
    getNextToken(); // eat V

    if (CurTok != '(') {
        ReportError("expected '(' after V");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected node name");
        return nullptr;
    }
    std::string NodeName = (CurTok == static_cast<int>(TokenType::tok_identifier))
                               ? m_lexer.IdentifierStr
                               : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after node name");
        return nullptr;
    }
    getNextToken(); // eat )

    if (CurTok != '=') {
        ReportError("expected '=' after node specification");
        return nullptr;
    }
    getNextToken(); // eat =

    auto Value = ParseExpression();
    if (!Value)
        return nullptr;

    return std::make_unique<ICExprAST>(NodeName, std::move(Value));
}

/* ========================================================================
 * Section 7.2: Mixed-Signal & Modeling Extensions - Parser
 * ======================================================================== */

// Parse above(expr, threshold)
std::unique_ptr<ExprAST> Parser::ParseAboveExpr()
{
    getNextToken(); // eat above

    if (CurTok != '(') {
        ReportError("expected '(' after 'above'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in above expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Threshold = ParseExpression();
    if (!Threshold)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after above expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<AboveExprAST>(std::move(Expr), std::move(Threshold));
}

// Parse timer()
std::unique_ptr<ExprAST> Parser::ParseTimerExpr()
{
    getNextToken(); // eat timer

    if (CurTok != '(') {
        ReportError("expected '(' after 'timer'");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != ')') {
        ReportError("expected ')' after timer");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<TimerExprAST>();
}

// Parse fsm(initial_state, transitions, "output_fn")
std::unique_ptr<ExprAST> Parser::ParseFSMExpr()
{
    getNextToken(); // eat fsm

    if (CurTok != '(') {
        ReportError("expected '(' after 'fsm'");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse initial state
    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected numeric initial state");
        return nullptr;
    }
    int InitialState = static_cast<int>(m_lexer.NumVal);
    getNextToken();

    auto FSM = std::make_unique<FSMExprAST>(InitialState, "moore");

    // Parse transitions array
    if (CurTok != ',') {
        ReportError("expected ',' after initial state");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != '[') {
        ReportError("expected '[' for transitions array");
        return nullptr;
    }
    getNextToken(); // eat [

    while (CurTok != ']' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        // Parse transition: { cur_state, next_state, condition, output }
        if (CurTok != '{') {
            ReportError("expected '{' for transition");
            return nullptr;
        }
        getNextToken(); // eat {

        // Current state
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected numeric current state");
            return nullptr;
        }
        int CurState = static_cast<int>(m_lexer.NumVal);
        getNextToken();

        if (CurTok != ',') {
            ReportError("expected ','");
            return nullptr;
        }
        getNextToken();

        // Next state
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected numeric next state");
            return nullptr;
        }
        int NextState = static_cast<int>(m_lexer.NumVal);
        getNextToken();

        if (CurTok != ',') {
            ReportError("expected ','");
            return nullptr;
        }
        getNextToken();

        // Condition
        auto Cond = ParseExpression();
        if (!Cond)
            return nullptr;

        if (CurTok != ',') {
            ReportError("expected ','");
            return nullptr;
        }
        getNextToken();

        // Output
        auto Out = ParseExpression();
        if (!Out)
            return nullptr;

        FSM->addTransition(CurState, NextState, std::move(Cond), std::move(Out));

        if (CurTok == '}')
            getNextToken(); // eat }
        if (CurTok == ',')
            getNextToken(); // eat , between transitions
    }

    if (CurTok != ']') {
        ReportError("expected ']' after transitions");
        return nullptr;
    }
    getNextToken(); // eat ]

    // Optional output function type
    if (CurTok == ',') {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            std::string fn = m_lexer.StringVal;
            if (fn == "mealy" || fn == "moore") {
                // We need to recreate the FSM with the right output fn
                // For now, we'll store it - in a full impl we'd rebuild
            }
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after fsm");
        return nullptr;
    }
    getNextToken(); // eat )

    return FSM;
}

void FSMExprAST::addTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out)
{
    Transitions.emplace_back(cur, next, std::move(cond), std::move(out));
}

// Parse edge(expr) or posedge(expr) / negedge(expr)
std::unique_ptr<ExprAST> Parser::ParseEdgeExpr()
{
    // Save the keyword before consuming the next token
    std::string edgeStr = m_lexer.IdentifierStr;
    getNextToken(); // eat edge/posedge/negedge keyword

    int edgeType = 0; // any
    if (edgeStr == "posedge")
        edgeType = 1;
    else if (edgeStr == "negedge")
        edgeType = -1;
    // If it was generic 'edge', edgeType stays 0 (will be set from args)

    if (CurTok != '(') {
        ReportError("expected '(' after edge keyword");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    // If generic 'edge', check for type argument
    if (edgeType == 0 && CurTok == ',') {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            std::string typeStr = m_lexer.IdentifierStr;
            if (typeStr == "rising" || typeStr == "posedge")
                edgeType = 1;
            else if (typeStr == "falling" || typeStr == "negedge")
                edgeType = -1;
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after edge expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<EdgeExprAST>(std::move(Expr), edgeType, edgeStr);
}

// Parse triggered(event, body)
std::unique_ptr<ExprAST> Parser::ParseTriggeredExpr()
{
    getNextToken(); // eat triggered

    if (CurTok != '(') {
        ReportError("expected '(' after 'triggered'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Event = ParseExpression();
    if (!Event)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in triggered expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after triggered expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<TriggeredExprAST>(std::move(Event), std::move(Body));
}

// Parse noise(type, amplitude, [freq])
std::unique_ptr<ExprAST> Parser::ParseNoiseExpr()
{
    getNextToken(); // eat noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse noise type string
    if (CurTok != static_cast<int>(TokenType::tok_string) && CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected noise type string");
        return nullptr;
    }
    std::string noiseType;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        noiseType = m_lexer.StringVal;
    } else {
        noiseType = m_lexer.IdentifierStr;
    }
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' after noise type");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Amplitude = ParseExpression();
    if (!Amplitude)
        return nullptr;

    std::unique_ptr<ExprAST> Freq = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        Freq = ParseExpression();
        if (!Freq)
            return nullptr;
    }

    if (CurTok != ')') {
        ReportError("expected ')' after noise expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<NoiseExprAST>(std::move(noiseType), std::move(Amplitude), std::move(Freq));
}

// Parse white_noise(amplitude)
std::unique_ptr<ExprAST> Parser::ParseWhiteNoiseExpr()
{
    getNextToken(); // eat white_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'white_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Amp = ParseExpression();
    if (!Amp)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after white_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<WhiteNoiseExprAST>(std::move(Amp));
}

// Parse flicker_noise(amplitude, corner_freq)
std::unique_ptr<ExprAST> Parser::ParseFlickerNoiseExpr()
{
    getNextToken(); // eat flicker_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'flicker_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Amp = ParseExpression();
    if (!Amp)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in flicker_noise");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto CF = ParseExpression();
    if (!CF)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after flicker_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<FlickerNoiseExprAST>(std::move(Amp), std::move(CF));
}

// Parse thermal_noise(resistance, [temperature])
std::unique_ptr<ExprAST> Parser::ParseThermalNoiseExpr()
{
    getNextToken(); // eat thermal_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'thermal_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Res = ParseExpression();
    if (!Res)
        return nullptr;

    std::unique_ptr<ExprAST> Temp = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        Temp = ParseExpression();
        if (!Temp)
            return nullptr;
    }

    if (CurTok != ')') {
        ReportError("expected ')' after thermal_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<ThermalNoiseExprAST>(std::move(Res), std::move(Temp));
}

// Parse piecewise(x1, y1, x2, y2, ..., [interpolate="linear"], x)
std::unique_ptr<ExprAST> Parser::ParsePiecewiseExpr()
{
    getNextToken(); // eat piecewise

    if (CurTok != '(') {
        ReportError("expected '(' after 'piecewise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto PW = std::make_unique<PiecewiseExprAST>("linear");

    // Parse pairs of (x, y)
    while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        auto X = ParseExpression();
        if (!X)
            return nullptr;

        if (CurTok != ',') {
            ReportError("expected ',' in piecewise");
            return nullptr;
        }
        getNextToken(); // eat ,

        auto Y = ParseExpression();
        if (!Y)
            return nullptr;

        // Check if next is another pair or query x
        if (CurTok == ',') {
            getNextToken(); // peek
            // If we see interpolate= or a standalone x (query), handle accordingly
            if (CurTok == static_cast<int>(TokenType::tok_identifier) &&
                (m_lexer.IdentifierStr == "interpolate" || m_lexer.IdentifierStr == "query")) {
                // This comma was for the next arg, put back conceptually
                // Actually we already consumed it, so X,Y is a point
                PW->addPoint(std::move(X), std::move(Y));
                // Continue loop to parse interpolate or query
                auto KeyId = m_lexer.IdentifierStr;
                getNextToken(); // eat identifier
                if (CurTok != '=') {
                    // It was query x, not interpolate
                    PW->setQueryX(std::move(X)); // X was actually the query
                    // Y should not have been parsed yet... let me reconsider
                    // Actually if it's `piecewise(x1,y1, x2,y2, x_query)`
                    // the last comma leads to an expression that has no comma after
                    // Let me simplify: just treat last expression as query
                    break;
                }
                getNextToken(); // eat =
                if (KeyId == "interpolate") {
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        // We'd need to rebuild PW with new interpolation
                        getNextToken();
                    }
                }
                continue;
            } else {
                // It was another pair
                PW->addPoint(std::move(X), std::move(Y));
                continue;
            }
        } else {
            // End of pairs
            PW->addPoint(std::move(X), std::move(Y));
            break;
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after piecewise");
        return nullptr;
    }
    getNextToken(); // eat )

    return PW;
}

void PiecewiseExprAST::addPoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y)
{
    Points.emplace_back(std::move(x), std::move(y));
}

// Parse table(k1, v1, k2, v2, ..., [default=v], key)
std::unique_ptr<ExprAST> Parser::ParseTableExpr()
{
    getNextToken(); // eat table

    if (CurTok != '(') {
        ReportError("expected '(' after 'table'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Table = std::make_unique<TableExprAST>();

    while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        auto Key = ParseExpression();
        if (!Key)
            return nullptr;

        if (CurTok == ',') {
            getNextToken();
            // Check if this was the query key (last item)
            if (CurTok == ')') {
                Table->setQueryKey(std::move(Key));
                break;
            }
        } else {
            // No comma - this should be the query key
            Table->setQueryKey(std::move(Key));
            break;
        }

        auto Val = ParseExpression();
        if (!Val)
            return nullptr;

        Table->addEntry(std::move(Key), std::move(Val));

        if (CurTok == ',')
            getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after table");
        return nullptr;
    }
    getNextToken(); // eat )

    return Table;
}

void TableExprAST::addEntry(std::unique_ptr<ExprAST> key, std::unique_ptr<ExprAST> val)
{
    Entries.emplace_back(std::move(key), std::move(val));
}

// Parse csv_import("file.csv", [options])
std::unique_ptr<ExprAST> Parser::ParseCsvImportExpr()
{
    getNextToken(); // eat csv_import

    if (CurTok != '(') {
        ReportError("expected '(' after 'csv_import'");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected filename string");
        return nullptr;
    }
    std::string filename = m_lexer.StringVal;
    getNextToken();

    std::map<std::string, std::string> options;
    if (CurTok == ',') {
        getNextToken();
        // Parse options as key=value pairs or just a dict
        if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
            getNextToken(); // eat {
            while (CurTok != '}' && CurTok != static_cast<int>(TokenType::tok_eof)) {
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string k = m_lexer.IdentifierStr;
                    getNextToken();
                    if (CurTok == '=') {
                        getNextToken();
                        if (CurTok == static_cast<int>(TokenType::tok_string)) {
                            options[k] = m_lexer.StringVal;
                            getNextToken();
                        }
                    }
                }
                if (CurTok == ',')
                    getNextToken();
            }
            if (CurTok == '}')
                getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after csv_import");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<CsvImportExprAST>(std::move(filename), std::move(options));
}

// Parse unit(value, "unit_string")
std::unique_ptr<ExprAST> Parser::ParseUnitExpr()
{
    getNextToken(); // eat unit

    if (CurTok != '(') {
        ReportError("expected '(' after 'unit'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in unit expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected unit string");
        return nullptr;
    }
    std::string unitStr = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after unit");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<UnitExprAST>(std::move(Val), std::move(unitStr));
}

// Parse dimension(expr)
std::unique_ptr<ExprAST> Parser::ParseDimensionExpr()
{
    getNextToken(); // eat dimension

    if (CurTok != '(') {
        ReportError("expected '(' after 'dimension'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after dimension");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<DimensionExprAST>(std::move(Expr));
}

// Parse convert(value, "from_unit", "to_unit")
std::unique_ptr<ExprAST> Parser::ParseConvertExpr()
{
    getNextToken(); // eat convert

    if (CurTok != '(') {
        ReportError("expected '(' after 'convert'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in convert");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected from_unit string");
        return nullptr;
    }
    std::string fromUnit = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' in convert");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected to_unit string");
        return nullptr;
    }
    std::string toUnit = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after convert");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<ConvertExprAST>(std::move(Val), std::move(fromUnit), std::move(toUnit));
}

// Parse has_unit(value, "unit_string")
std::unique_ptr<ExprAST> Parser::ParseHasUnitExpr()
{
    getNextToken(); // eat has_unit

    if (CurTok != '(') {
        ReportError("expected '(' after 'has_unit'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in has_unit");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected unit string");
        return nullptr;
    }
    std::string unitStr = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after has_unit");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<HasUnitExprAST>(std::move(Val), std::move(unitStr));
}

std::unique_ptr<ExprAST> Parser::ParseTrainExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat train

    if (CurTok != '(') {
        ReportError("expected '(' after train");
        return nullptr;
    }
    getNextToken(); // eat (

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after model in train");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto inputs = ParseExpression();
    if (!inputs)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after inputs in train");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto outputs = ParseExpression();
    if (!outputs)
        return nullptr;

    int epochs = 100;
    if (CurTok == ',') {
        getNextToken();
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected number of epochs");
            return nullptr;
        }
        epochs = (int)m_lexer.NumVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after train arguments");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<TrainExprAST>(std::move(model), std::move(inputs), std::move(outputs), epochs);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParsePredictExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat predict

    if (CurTok != '(') {
        ReportError("expected '(' after predict");
        return nullptr;
    }
    getNextToken(); // eat (

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after model in predict");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto input = ParseExpression();
    if (!input)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after predict arguments");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<PredictExprAST>(std::move(model), std::move(input));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseGoalExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat goal

    if (CurTok != '(') {
        ReportError("expected '(' after goal");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in goal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto target = ParseExpression();
    if (!target)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after goal");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<GoalExprAST>(std::move(expr), std::move(target));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseOptimizeExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat optimize

    if (CurTok != '(') {
        ReportError("expected '(' after optimize");
        return nullptr;
    }
    getNextToken(); // eat (

    // optimize() has no arguments for now, just runs the global optimizer
    if (CurTok != ')') {
        ReportError("expected ')' after optimize");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<CallExprAST>("optimize", std::vector<std::unique_ptr<ExprAST>>());
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseThermalBlock()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat thermal

    if (CurTok != '(') {
        ReportError("expected '(' after thermal");
        return nullptr;
    }
    getNextToken(); // eat (

    auto power = ParseExpression();
    if (!power)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after power in thermal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto res = ParseExpression();
    if (!res)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after resistance in thermal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto cap = ParseExpression();
    if (!cap)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after thermal arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<ThermalBlockAST>(std::move(power), std::move(res), std::move(cap));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseMonteCarlo()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat montecarlo

    if (CurTok != '(') {
        ReportError("expected '(' after montecarlo");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    std::vector<std::string> params;
    if (CurTok != ',') {
        ReportError("expected ',' after expression in montecarlo");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok == '[') {
        getNextToken(); // eat [
        while (CurTok != ']') {
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected parameter name in montecarlo list");
                return nullptr;
            }
            params.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ',')
                getNextToken();
        }
        getNextToken(); // eat ]
    } else {
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected parameter name or list in montecarlo");
            return nullptr;
        }
        params.push_back(m_lexer.IdentifierStr);
        getNextToken();
    }

    int iters = 100;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected number of iterations in montecarlo");
            return nullptr;
        }
        iters = (int)m_lexer.NumVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after montecarlo arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<MonteCarloExprAST>(std::move(expr), std::move(params), iters);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseVirtualProbe()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat virtual_probe

    if (CurTok != '(') {
        ReportError("expected '(' after virtual_probe");
        return nullptr;
    }
    getNextToken(); // eat (

    auto sig = ParseExpression();
    if (!sig)
        return nullptr;

    std::string name = "scope1";
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected probe name string in virtual_probe");
            return nullptr;
        }
        name = m_lexer.StringVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after virtual_probe arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<VirtualProbeExprAST>(std::move(sig), name);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseHotSwap()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat hot_swap

    if (CurTok != '(') {
        ReportError("expected '(' after hot_swap");
        return nullptr;
    }
    getNextToken(); // eat (

    auto subckt = ParseExpression();
    if (!subckt)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after subcircuit name in hot_swap");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after hot_swap arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<HotSwapExprAST>(std::move(subckt), std::move(model));
    Res->setLocation(line, col);
    return Res;
}
} // namespace Flux
