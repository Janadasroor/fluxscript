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
#include "flux/compiler/model_quality_ast.h"
#include "flux/compiler/symbolic_ast.h"
#include <iostream>

namespace Flux {

Parser::Parser(const std::string& input) : m_lexer(input), m_hasError(false), m_parsingMatchPattern(false)
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
    m_binopPrecedence[static_cast<int>(TokenType::tok_pipe)] = 1;
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
    m_binopPrecedence['%'] = 40;
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
    case TokenType::tok_end:
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
            CurTok = m_lexer.CurTok; // Sync parser's CurTok with main lexer
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
        ReportError("expected ')' after expression");
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
        if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
            CurTok != static_cast<int>(TokenType::tok_end)) {
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

    std::vector<FluxType> GenericTypeArgs;
    if (CurTok == '[') {
        bool isKnownTypeName = m_knownStructTypeNames.count(IdName) || m_knownEnumTypeNames.count(IdName);
        if (isKnownTypeName) {
            GenericTypeArgs = ParseGenericTypeArgs();
            if (hasError()) return nullptr;
        } else {
            // Not a known type name — could be array indexing (var[idx]) or generic function call (fn[T](x)).
            // Peek at the first token inside []. If it looks like a type (uppercase id, keyword, &, dyn),
            // try generic type args and check if '(' follows. Otherwise, treat as array indexing.
            int peek = m_lexer.peekToken();
            bool looksLikeType = (peek == static_cast<int>(TokenType::tok_identifier) &&
                                  !m_lexer.IdentifierStr.empty() &&
                                  std::isupper(static_cast<unsigned char>(m_lexer.IdentifierStr[0]))) ||
                                 peek == static_cast<int>(TokenType::tok_type_double) ||
                                 peek == static_cast<int>(TokenType::tok_type_int) ||
                                 peek == static_cast<int>(TokenType::tok_type_bool) ||
                                 peek == static_cast<int>(TokenType::tok_type_string) ||
                                 peek == static_cast<int>(TokenType::tok_type_matrix) ||
                                 peek == static_cast<int>(TokenType::tok_type_vector) ||
                                 peek == static_cast<int>(TokenType::tok_type_float) ||
                                 peek == static_cast<int>(TokenType::tok_type_complex) ||
                                 peek == static_cast<int>(TokenType::tok_type_void) ||
                                 peek == static_cast<int>(TokenType::tok_bitwise_and) ||
                                 peek == static_cast<int>(TokenType::tok_lifetime);
            if (looksLikeType) {
                auto savedState = m_lexer.saveState();
                int savedCurTok = CurTok;
                GenericTypeArgs = ParseGenericTypeArgs();
                if (hasError() || CurTok != '(') {
                    m_lexer.restoreState(savedState);
                    CurTok = savedCurTok;
                    m_hasError = false;
                    GenericTypeArgs.clear();
                }
            }
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
        auto Result = std::make_unique<CallExprAST>(IdName, std::move(Args), std::move(GenericTypeArgs));
        Result->setLocation(line, col);
        return Result;
    }
    // Struct construction: TypeName { name: expr, ... }
    // Only if IdName is a known struct or enum type, or we have explicit generic type args,
    // to avoid clashing with match body `match expr { ... }`.
    if (CurTok == static_cast<int>(TokenType::tok_lbrace) &&
        (m_knownStructTypeNames.count(IdName) || m_knownEnumTypeNames.count(IdName) || !GenericTypeArgs.empty())) {
        return ParseStructConstructExpr(IdName, GenericTypeArgs);
    }
    auto Result = std::make_unique<VariableExprAST>(IdName, std::move(GenericTypeArgs));
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
        ReportError("expected 'then' after if condition");
        return nullptr;
    }
    getNextToken();
    auto Then = ParseExpression();
    if (!Then)
        return nullptr;
    if (CurTok != static_cast<int>(TokenType::tok_else)) {
        ReportError("expected 'else' after if-then");
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

    // Optional else/elif block
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
    } else if (CurTok == static_cast<int>(TokenType::tok_elif)) {
        getNextToken(); // eat elif
        auto ElifStmt = ParseElifStmt();
        if (!ElifStmt)
            return nullptr;
        ElseBody.push_back(std::move(ElifStmt));
    }

    return std::make_unique<IfStmtAST>(std::move(Cond), std::move(ThenBody), std::move(ElseBody));
}

std::unique_ptr<ExprAST> Parser::ParseElifStmt()
{
    // Expect parentheses: elif (cond) { ... }
    if (CurTok != '(') {
        ReportError("expected '(' after elif");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after elif condition");
        return nullptr;
    }
    getNextToken(); // eat )

    auto ThenBody = ParseStmtBlock();

    // Optional chained elif/else
    std::vector<std::unique_ptr<ExprAST>> ElseBody;
    if (CurTok == static_cast<int>(TokenType::tok_elif)) {
        getNextToken(); // eat elif
        auto ElifStmt = ParseElifStmt();
        if (!ElifStmt)
            return nullptr;
        ElseBody.push_back(std::move(ElifStmt));
    } else if (CurTok == static_cast<int>(TokenType::tok_else)) {
        getNextToken(); // eat else
        if (CurTok == static_cast<int>(TokenType::tok_if)) {
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
        CurTok != static_cast<int>(TokenType::tok_end) &&
        CurTok != static_cast<int>(TokenType::tok_state) &&
        CurTok != static_cast<int>(TokenType::tok_ic) &&
        CurTok != static_cast<int>(TokenType::tok_dt_var) &&
        CurTok != static_cast<int>(TokenType::tok_analysis) &&
        CurTok != static_cast<int>(TokenType::tok_dc)) {
        ReportError("expected identifier after let/var");
        return nullptr;
    }
    std::string IdName = m_lexer.IdentifierStr;
    getNextToken();
    FluxType Type(TypeKind::Auto);
    if (CurTok == static_cast<int>(TokenType::tok_colon)) {
        getNextToken(); // eat :
        // Check for complex type expressions: dyn TraitName, &T, etc.
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "dyn") {
            Type = parseTypeName({}, {});
        } else if (CurTok == static_cast<int>(TokenType::tok_bitwise_and)) {
            Type = parseTypeName({}, {});
        } else {
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
    // Handle reference operators & and &mut
    if (CurTok == static_cast<int>(TokenType::tok_bitwise_and)) {
        getNextToken();
        bool isMut = false;
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "mut") {
            isMut = true;
            getNextToken();
        }
        auto Operand = ParseUnaryExpr();
        if (!Operand) return nullptr;
        // Use distinct op codes: & = tok_bitwise_and, &mut = tok_bitwise_and + offset
        return std::make_unique<UnaryExprAST>(
            isMut ? static_cast<int>(TokenType::tok_bitwise_and) + 2600
                  : static_cast<int>(TokenType::tok_bitwise_and),
            std::move(Operand));
    }

    // Handle dereference operator *
    bool isUnary = false;
    if (CurTok == '-' || CurTok == '+' || CurTok == '*' ||
        CurTok == static_cast<int>(TokenType::tok_logical_not) ||
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
    bool hasSemicolon = false;
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
        hasSemicolon = true;
        getNextToken();
    }
    if (!hasSemicolon && Rows.size() == 1) {
        std::vector<std::unique_ptr<ExprAST>> Elems;
        for (auto& elem : Rows[0])
            Elems.push_back(std::move(elem));
        return std::make_unique<VectorExprAST>(std::move(Elems));
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
        ReportError("expected '}' to close statement block");
        return nullptr;
    }
    getNextToken();
    return std::make_unique<BlockExprAST>(std::move(Stmts));
}

// Parse: spawn f(args)
std::unique_ptr<ExprAST> Parser::ParseSpawnExpr()
{
    getNextToken(); // consume 'spawn'
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected function name after 'spawn'");
        return nullptr;
    }
    std::string callee = m_lexer.IdentifierStr;
    getNextToken();
    if (CurTok != '(') {
        ReportError("expected '(' after spawn function name");
        return nullptr;
    }
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;
    if (CurTok != ')') {
        while (true) {
            if (auto arg = ParseExpression())
                args.push_back(std::move(arg));
            else
                return nullptr;
            if (CurTok == ')')
                break;
            if (CurTok != ',') {
                ReportError("expected ')' or ',' in spawn argument list");
                return nullptr;
            }
            getNextToken();
        }
    }
    getNextToken(); // consume ')'
    return std::make_unique<SpawnExprAST>(callee, std::move(args));
}

// Parse: join(expr)
std::unique_ptr<ExprAST> Parser::ParseJoinExpr()
{
    getNextToken(); // consume 'join'
    if (CurTok != '(') {
        ReportError("expected '(' after 'join'");
        return nullptr;
    }
    getNextToken();
    auto handle = ParseExpression();
    if (!handle)
        return nullptr;
    if (CurTok != ')') {
        ReportError("expected ')' after join expression");
        return nullptr;
    }
    getNextToken(); // consume ')'
    return std::make_unique<JoinExprAST>(std::move(handle));
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
        if (CurTok == static_cast<int>(TokenType::tok_rbrace) ||
            CurTok == static_cast<int>(TokenType::tok_eof) ||
            CurTok == static_cast<int>(TokenType::tok_else) ||
            CurTok == static_cast<int>(TokenType::tok_elif)) {
            Res = std::make_unique<ReturnExprAST>(nullptr);
        } else {
            Res = std::make_unique<ReturnExprAST>(ParseExpression());
        }
        break;

    // Enum declarations inside function bodies
    case static_cast<int>(TokenType::tok_enum):
    {
        std::vector<std::unique_ptr<StructDeclAST>> anonStructs;
        if (auto Enum = ParseEnumDecl(&anonStructs)) {
            m_localEnumDecls.push_back(std::move(Enum));
            for (auto& S : anonStructs)
                m_localAnonStructs.push_back(std::move(S));
        }
        Res = std::make_unique<NumberExprAST>(0.0);
        break;
    }

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
    case static_cast<int>(TokenType::tok_ddt):
        Res = ParseDdtExpr();
        break;
    case static_cast<int>(TokenType::tok_idt):
        Res = ParseIdtExpr();
        break;
    case static_cast<int>(TokenType::tok_cross):
        Res = ParseCrossExpr();
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
    case static_cast<int>(TokenType::tok_await):
        Res = ParseAwaitExpr();
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
    case static_cast<int>(TokenType::tok_spawn):
        Res = ParseSpawnExpr();
        break;
    case static_cast<int>(TokenType::tok_join):
        Res = ParseJoinExpr();
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

    // SPICE model quality directives
    case static_cast<int>(TokenType::tok_diagnostic):
        Res = ParseDiagnosticDecl();
        break;
    case static_cast<int>(TokenType::tok_tolerance):
        Res = ParseToleranceDecl();
        break;

    // Symbolic math: find poles/zeros of transfer function
    case static_cast<int>(TokenType::tok_poles):
        Res = ParsePolesExpr();
        break;
    case static_cast<int>(TokenType::tok_zeros):
        Res = ParseZerosExpr();
        break;

    case static_cast<int>(TokenType::tok_end):
        // Treat `end` as an identifier (variable/function name) when followed by
        // tokens that indicate expression usage: (), [], ., ::, =, ==, !=, etc.
        // Otherwise it is a block terminator — consume and return nullptr.
        {
            int peek = m_lexer.peekToken();
            if (peek == '(' || peek == '[' || peek == static_cast<int>(TokenType::tok_dot) ||
                peek == static_cast<int>(TokenType::tok_namespace_sep) ||
                peek == '=' || peek == static_cast<int>(TokenType::tok_equal) ||
                peek == static_cast<int>(TokenType::tok_not_equal) ||
                peek == '+' || peek == '-' || peek == '*' || peek == '/' ||
                peek == '>' || peek == '<' ||
                peek == static_cast<int>(TokenType::tok_less_equal) ||
                peek == static_cast<int>(TokenType::tok_greater_equal) ||
                peek == static_cast<int>(TokenType::tok_pipe) ||
                peek == static_cast<int>(TokenType::tok_question)) {
                Res = ParseIdentifierExpr();
            } else {
                getNextToken();
                return nullptr;
            }
        }
        break;
    case static_cast<int>(TokenType::tok_elif):
    case static_cast<int>(TokenType::tok_else):
        // elif/else are if-chain continuation keywords, not standalone expressions.
        // Silently consume to avoid spurious errors from pre-parse passes.
        getNextToken();
        return nullptr;
    default: {
        ReportError("unexpected token in expression: " + Lexer::tokenSpelling(CurTok));
        return nullptr;
    }
    }
    if (!Res)
        return nullptr;
    while (true) {
        if (CurTok == static_cast<int>(TokenType::tok_transpose)) {
            Res = std::make_unique<TransposeExprAST>(std::move(Res));
            getNextToken();
        } else if (CurTok == static_cast<int>(TokenType::tok_question)) {
            // expr? — early-return on Err/None (Rust-style propagation)
            getNextToken(); // eat ?
            Res = std::make_unique<TryPropagateExprAST>(std::move(Res));
        } else if (CurTok == static_cast<int>(TokenType::tok_dot)) {
            getNextToken(); // eat .
            if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
                CurTok != static_cast<int>(TokenType::tok_end)) {
                ReportError("expected identifier after '.'");
                return nullptr;
            }
            std::string MemberName = m_lexer.IdentifierStr;
            getNextToken();

            // Method call: obj.method(args)
            if (CurTok == '(') {
                getNextToken(); // eat (
                std::vector<std::unique_ptr<ExprAST>> CallArgs;
                if (CurTok != ')') {
                    while (true) {
                        if (auto Arg = ParseExpression())
                            CallArgs.push_back(std::move(Arg));
                        else
                            return nullptr;
                        if (CurTok == ')')
                            break;
                        if (CurTok != ',') {
                            ReportError("expected ')' or ',' in method argument list");
                            return nullptr;
                        }
                        getNextToken();
                    }
                }
                getNextToken(); // eat )
                auto member = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
                Res = std::make_unique<CallExprAST>(std::move(member), std::move(CallArgs));
            } else if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
                // Check if this is an enum variant construction with named fields: EnumName.VariantName { fields }
                bool isEnumVariant = false;
                std::string enumName;
                if (!m_parsingMatchPattern) {
                    if (auto* varExpr = dynamic_cast<VariableExprAST*>(Res.get())) {
                        enumName = varExpr->getName();
                        if (m_knownEnumTypeNames.count(enumName)) {
                            isEnumVariant = true;
                        }
                    }
                }
                if (isEnumVariant) {
                    std::string anonName = "__enum_" + enumName + "_" + MemberName + "_Fields";
                    auto structCtor = ParseStructConstructExpr(anonName);
                    if (!structCtor) return nullptr;

                    auto member = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
                    std::vector<std::unique_ptr<ExprAST>> CallArgs;
                    CallArgs.push_back(std::move(structCtor));
                    Res = std::make_unique<CallExprAST>(std::move(member), std::move(CallArgs));
                } else {
                    Res = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
                }
            } else {
                Res = std::make_unique<MemberExprAST>(std::move(Res), MemberName);
            }
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
            if (colIdx)
                LHS = std::make_unique<IndexExprAST>(std::move(LHS), std::move(rowIdx), std::move(colIdx));
            else
                LHS = std::make_unique<IndexExprAST>(std::move(LHS), std::move(rowIdx));
        } else if (BinOp == static_cast<int>(TokenType::tok_pipe)) {
                // x |> f          → f(x)
                // x |> f(y)       → f(x, y)
                // x |> obj.meth   → obj.meth(x)
                // x |> obj.m(y)   → obj.m(x, y)
                auto* call = dynamic_cast<CallExprAST*>(RHS.get());
                if (call) {
                    call->prependArg(std::move(LHS));
                    LHS = std::move(RHS);
                } else if (dynamic_cast<VariableExprAST*>(RHS.get())) {
                    auto calleeName = static_cast<VariableExprAST*>(RHS.get())->getName();
                    std::vector<std::unique_ptr<ExprAST>> args;
                    args.push_back(std::move(LHS));
                    LHS = std::make_unique<CallExprAST>(calleeName, std::move(args));
                } else if (dynamic_cast<MemberExprAST*>(RHS.get())) {
                    std::vector<std::unique_ptr<ExprAST>> args;
                    args.push_back(std::move(LHS));
                    LHS = std::make_unique<CallExprAST>(std::move(RHS), std::move(args));
                } else {
                    ReportError("expected function call after |>");
                    return nullptr;
                }
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
    // Optional generic params: def foo[T](x: T) -> T  or def foo[T: Display](x: T) -> T
    std::vector<std::string> GenericParams;
    std::map<std::string, std::vector<std::string>> GenericParamBounds;
    if (CurTok == '[') {
        GenericParams = ParseGenericParams(&GenericParamBounds);
        if (hasError()) return nullptr;
        m_activeGenericParams = GenericParams;
    }

    // Optional lifetime params: def foo<'a, 'b: 'a>(x: &'a T) -> &'b T
    std::vector<LifetimeParam> LifetimeParamsList;
    if (CurTok == '<') {
        getNextToken();
        while (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
            std::string lt = m_lexer.IdentifierStr;
            if (!lt.empty() && lt[0] == '\'')
                lt = lt.substr(1);
            std::vector<std::string> outlives;
            getNextToken();
            while (CurTok == static_cast<int>(TokenType::tok_colon)) {
                getNextToken(); // eat :
                if (CurTok == static_cast<int>(TokenType::tok_lifetime)) {
                    std::string bound = m_lexer.IdentifierStr;
                    if (!bound.empty() && bound[0] == '\'')
                        bound = bound.substr(1);
                    outlives.push_back(bound);
                    getNextToken();
                } else {
                    break;
                }
            }
            LifetimeParamsList.emplace_back(lt, outlives);
            if (CurTok == ',')
                getNextToken();
            else
                break;
        }
        if (CurTok != '>') {
            ReportError("expected '>' to close lifetime parameters in function prototype");
            return nullptr;
        }
        getNextToken(); // eat >
    }
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
            if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "dyn") {
                Type = parseTypeName(m_activeGenericParams);
                // parseTypeName already consumed the type name, so don't eat again
            } else if (CurTok == static_cast<int>(TokenType::tok_bitwise_and)) {
                Type = parseTypeName(m_activeGenericParams, LifetimeParamsList);
            } else {
                Type = FluxType::fromToken(CurTok);
                // Check for unit type names (e.g., Voltage, Current)
                if (Type.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string typeName = m_lexer.IdentifierStr;
                    // Check if it matches an active generic parameter
                    bool isGeneric = false;
                    for (auto& gp : m_activeGenericParams) {
                        if (gp == typeName) {
                            Type = FluxType::generic(typeName);
                            isGeneric = true;
                            break;
                        }
                    }
                    if (!isGeneric) {
                        // Check user-defined struct types first
                        if (m_knownStructTypeNames.count(typeName)) {
                            Type = FluxType(TypeKind::UserStruct);
                            Type.StructTypeId = -1;
                            Type.StructLLVMType = nullptr;
                            Type.GenericName = typeName;
                        } else if (m_knownEnumTypeNames.count(typeName)) {
                            Type = FluxType(TypeKind::UserEnum);
                            Type.EnumTypeId = -1;
                            Type.EnumLLVMType = nullptr;
                            Type.GenericName = typeName;
                        } else if (typeName == "Double" || typeName == "double") {
                            Type = FluxType(TypeKind::Double);
                        } else if (typeName == "Float" || typeName == "float") {
                            Type = FluxType(TypeKind::Float);
                        } else if (typeName == "Int" || typeName == "int") {
                            Type = FluxType(TypeKind::Int);
                        } else if (typeName == "Bool" || typeName == "bool") {
                            Type = FluxType(TypeKind::Bool);
                        } else if (typeName == "Void" || typeName == "void") {
                            Type = FluxType(TypeKind::Void);
                        } else if (typeName == "Complex" || typeName == "complex") {
                            Type = FluxType(TypeKind::Complex);
                        } else if (typeName == "String" || typeName == "string") {
                            Type = FluxType(TypeKind::String);
                        } else {
                            FluxType unitType = FluxType::fromUnitName(typeName);
                            if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
                                unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
                                unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
                                Type = unitType;
                            }
                        }
                    }
                    getNextToken(); // eat type keyword (identifier)
                    if (CurTok == '[' &&
                        (Type.Kind == TypeKind::UserStruct || Type.Kind == TypeKind::UserEnum))
                        Type.GenericArgs = ParseGenericTypeArgs();
                } else if (Type.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_type_double)) {
                    // Explicit "Double" keyword — keep as Double
                    getNextToken(); // eat type keyword
                } else {
                    getNextToken(); // eat type keyword
                }
            }
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
        if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "dyn") {
            RetType = parseTypeName(m_activeGenericParams, LifetimeParamsList);
        } else if (CurTok == static_cast<int>(TokenType::tok_bitwise_and)) {
            RetType = parseTypeName(m_activeGenericParams, LifetimeParamsList);
        } else {
            RetType = FluxType::fromToken(CurTok);
            // Check for unit type names or generic type params in return type annotation
            if (RetType.Kind == TypeKind::Double && CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string typeName = m_lexer.IdentifierStr;
                // Check if it matches an active generic parameter
                bool isGeneric = false;
                for (auto& gp : m_activeGenericParams) {
                    if (gp == typeName) {
                        RetType = FluxType::generic(typeName);
                        isGeneric = true;
                        break;
                    }
                }
                if (!isGeneric) {
                    // Check user-defined struct types first
                    if (m_knownStructTypeNames.count(typeName)) {
                        RetType = FluxType(TypeKind::UserStruct);
                        RetType.StructTypeId = -1;
                        RetType.StructLLVMType = nullptr;
                        RetType.GenericName = typeName;
                    } else if (m_knownEnumTypeNames.count(typeName)) {
                        RetType = FluxType(TypeKind::UserEnum);
                        RetType.EnumTypeId = -1;
                        RetType.EnumLLVMType = nullptr;
                        RetType.GenericName = typeName;
                    } else if (typeName == "Double" || typeName == "double") {
                        RetType = FluxType(TypeKind::Double);
                    } else if (typeName == "Float" || typeName == "float") {
                        RetType = FluxType(TypeKind::Float);
                    } else if (typeName == "Int" || typeName == "int") {
                        RetType = FluxType(TypeKind::Int);
                    } else if (typeName == "Bool" || typeName == "bool") {
                        RetType = FluxType(TypeKind::Bool);
                    } else if (typeName == "Void" || typeName == "void") {
                        RetType = FluxType(TypeKind::Void);
                    } else if (typeName == "Complex" || typeName == "complex") {
                        RetType = FluxType(TypeKind::Complex);
                    } else if (typeName == "String" || typeName == "string") {
                        RetType = FluxType(TypeKind::String);
                    } else {
                        FluxType unitType = FluxType::fromUnitName(typeName);
                        if (unitType.Dimensions.mass != 0 || unitType.Dimensions.length != 0 || unitType.Dimensions.time != 0 ||
                            unitType.Dimensions.current != 0 || unitType.Dimensions.temperature != 0 ||
                            unitType.Dimensions.amount != 0 || unitType.Dimensions.luminous != 0) {
                            RetType = unitType;
                        }
                    }
                }
            }
            getNextToken();
            if (CurTok == '[' &&
                (RetType.Kind == TypeKind::UserStruct || RetType.Kind == TypeKind::UserEnum))
                RetType.GenericArgs = ParseGenericTypeArgs();
        }
    }
    auto proto = std::make_unique<PrototypeAST>(FnName, std::move(Args), RetType);
    if (!GenericParams.empty()) {
        proto->setGenericParams(GenericParams);
        for (auto& [paramName, bounds] : GenericParamBounds) {
            for (auto& traitName : bounds) {
                proto->addGenericParamBound(paramName, traitName);
            }
        }
    }
    if (!LifetimeParamsList.empty())
        proto->setLifetimeParams(std::move(LifetimeParamsList));
    return proto;
}

std::unique_ptr<PrototypeAST> Parser::ParseExtern()
{
    getNextToken();
    if (CurTok == static_cast<int>(TokenType::tok_def))
        getNextToken();
    return ParsePrototype();
}

std::unique_ptr<FunctionAST> Parser::ParseDefinition()
{
    getNextToken();
    clearLocalDecls();
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    if (auto Body = ParseExpression()) {
        if (dynamic_cast<ComplexExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Complex));
        else if (dynamic_cast<MatrixExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Matrix));
        else if (dynamic_cast<VectorExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Vector));
        else if (auto* structCtor = dynamic_cast<StructConstructExprAST*>(Body.get())) {
            FluxType retType(TypeKind::UserStruct);
            retType.StructTypeId = -1;
            retType.StructLLVMType = nullptr;
            retType.GenericName = structCtor->getStructName();
            Proto->setReturnType(retType);
        }
        auto Func = std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
        Func->LocalEnums = takeLocalEnumDecls();
        Func->LocalAnonStructs = takeLocalAnonStructs();
        return Func;
    }
    return nullptr;
}

std::unique_ptr<FunctionAST> Parser::ParseAsyncDef()
{
    getNextToken(); // eat async
    if (CurTok != static_cast<int>(TokenType::tok_def)) {
        ReportError("expected 'def' after 'async'");
        return nullptr;
    }
    getNextToken(); // eat def
    clearLocalDecls();
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    Proto->setAsync(true);
    if (auto Body = ParseExpression()) {
        if (dynamic_cast<ComplexExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Complex));
        else if (dynamic_cast<MatrixExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Matrix));
        else if (dynamic_cast<VectorExprAST*>(Body.get()))
            Proto->setReturnType(FluxType(TypeKind::Vector));
        else if (auto* structCtor = dynamic_cast<StructConstructExprAST*>(Body.get())) {
            FluxType retType(TypeKind::UserStruct);
            retType.StructTypeId = -1;
            retType.StructLLVMType = nullptr;
            retType.GenericName = structCtor->getStructName();
            Proto->setReturnType(retType);
        }
        auto Func = std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
        Func->LocalEnums = takeLocalEnumDecls();
        Func->LocalAnonStructs = takeLocalAnonStructs();
        return Func;
    }
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
        } else if (dynamic_cast<VectorExprAST*>(Body.get())) {
            RetType = FluxType(TypeKind::Vector);
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
/// ParseGenericParams — parse [T, U, V] after a generic declaration name.
/// Expects CurTok == '['. Returns type parameter names.
std::vector<std::string> Parser::ParseGenericParams(
    std::map<std::string, std::vector<std::string>>* outBounds)
{
    getNextToken(); // eat [
    std::vector<std::string> params;
    while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        std::string ParamName = m_lexer.IdentifierStr;
        getNextToken();

        // Check for trait bound: T: Display
        std::vector<std::string> bounds;
        if (CurTok == static_cast<int>(TokenType::tok_colon)) {
            getNextToken(); // eat :
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected trait name in generic parameter bound");
                return {};
            }
            bounds.push_back(m_lexer.IdentifierStr);
            getNextToken();
            // Support T: Trait1 + Trait2 syntax
            while (CurTok == '+') {
                getNextToken();
                if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                    ReportError("expected trait name after '+'");
                    return {};
                }
                bounds.push_back(m_lexer.IdentifierStr);
                getNextToken();
            }
        }

        params.push_back(ParamName);
        if (outBounds && !bounds.empty())
            (*outBounds)[ParamName] = std::move(bounds);

        if (CurTok == ']')
            break;
        if (CurTok != ',') {
            ReportError("expected ',' or ']' in generic parameter list");
            return {};
        }
        getNextToken();
    }
    if (CurTok != ']') {
        ReportError("expected ']' to close generic parameter list");
        return {};
    }
    getNextToken(); // eat ]
    return params;
}

/// ParseGenericTypeArgs — parse [Double, Int, T] in generic call/construction.
/// Expects CurTok == '['. Returns resolved FluxTypes.
std::vector<FluxType> Parser::ParseGenericTypeArgs()
{
    getNextToken(); // eat [
    std::vector<FluxType> args;
    while (true) {
        FluxType type = parseTypeName(m_activeGenericParams);
        args.push_back(type);
        if (CurTok == ']')
            break;
        if (CurTok != ',') {
            ReportError("expected ',' or ']' in generic type arguments");
            return {};
        }
        getNextToken();
    }
    if (CurTok != ']') {
        ReportError("expected ']' to close generic type arguments");
        return {};
    }
    getNextToken(); // eat ]
    return args;
}

/// ParseStructDecl — parse:
///   struct Name
///       field: Type
///       field: Type
///   end
/// or:
///   struct Name { field: Type, field: Type }

} // namespace Flux
