/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/tooling/lsp_server.h"
#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace Flux {
namespace Tooling {

std::string LspServer::handleTextDocumentSemanticTokensFull(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    auto tokens = getSemanticTokens(uri);

    std::ostringstream oss;
    oss << R"({"data":[)";
    for (size_t i = 0; i < tokens.data.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << tokens.data[i];
    }
    oss << "]}";
    return oss.str();
}

std::string LspServer::handleTextDocumentSemanticTokensRange(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    int startLine = jsonGetInt(params, "range.start.line");
    int startChar = jsonGetInt(params, "range.start.character");
    int endLine = jsonGetInt(params, "range.end.line");
    int endChar = jsonGetInt(params, "range.end.character");

    Range range{{startLine, startChar}, {endLine, endChar}};
    auto tokens = getSemanticTokensRange(uri, range);

    std::ostringstream oss;
    oss << R"({"data":[)";
    for (size_t i = 0; i < tokens.data.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << tokens.data[i];
    }
    oss << "]}";
    return oss.str();
}

std::string LspServer::getSemanticTokensLegend()
{
    return R"({"tokenTypes":["namespace","type","class","enum","interface","struct","typeParameter","parameter","variable","property","enumMember","function","method","keyword","modifier","comment","string","number","operator"],"tokenModifiers":["declaration","definition","readonly","static","deprecated","abstract","async","modification","documentation"]})";
}

// Token type indices matching the legend
namespace {
    const int TOK_TYPE = 1;
    const int TOK_CLASS = 2;
    const int TOK_ENUM = 3;
    const int TOK_INTERFACE = 4;
    const int TOK_STRUCT = 5;
    const int TOK_PARAMETER = 7;
    const int TOK_VARIABLE = 8;
    const int TOK_FUNCTION = 11;
    const int TOK_METHOD = 12;
    const int TOK_KEYWORD = 13;
    const int TOK_COMMENT = 16;
    const int TOK_STRING = 17;
    const int TOK_NUMBER = 18;
    const int TOK_OPERATOR = 19;

    const int MOD_DECLARATION = 1;
    const int MOD_DEFINITION = 2;
}

LspServer::SemanticTokensResult LspServer::getSemanticTokens(const std::string& uri)
{
    SemanticTokensResult result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::vector<unsigned int> data;
    int prevLine = 0;
    int prevCol = 0;

    // Use Lexer to tokenize
    Flux::Lexer lexer(doc->text);

    while (lexer.getNextToken() != static_cast<int>(Flux::TokenType::tok_eof)) {
        int token = lexer.CurTok;
        int line = lexer.getCurrentLine() - 1;
        int col = lexer.getCurrentColumn() - 1;
        int len = static_cast<int>(lexer.IdentifierStr.size());

        if (token == static_cast<int>(Flux::TokenType::tok_identifier)) {
            len = static_cast<int>(lexer.IdentifierStr.size());
        } else if (token == static_cast<int>(Flux::TokenType::tok_string)) {
            len = static_cast<int>(lexer.StringVal.size()) + 2;
        } else {
            // Compute length from source text for numbers and other tokens
            std::string lineText = lexer.getCurrentLineText();
            int startCol = lexer.getCurrentColumn() - 1;
            len = 1;
            if (startCol >= 0 && static_cast<size_t>(startCol) < lineText.size()) {
                size_t end = static_cast<size_t>(startCol);
                while (end < lineText.size() && !isspace(lineText[end]) &&
                       lineText[end] != ')' && lineText[end] != '}' && lineText[end] != ']' &&
                       lineText[end] != ';' && lineText[end] != ',' && lineText[end] != ':')
                    end++;
                len = static_cast<int>(end - startCol);
                if (len < 1) len = 1;
            }
        }

        int typeIdx = -1;
        int modBits = 0;

        // Classify token
        if (token == static_cast<int>(Flux::TokenType::tok_def) ||
            token == static_cast<int>(Flux::TokenType::tok_let) ||
            token == static_cast<int>(Flux::TokenType::tok_var) ||
            token == static_cast<int>(Flux::TokenType::tok_extern) ||
            token == static_cast<int>(Flux::TokenType::tok_if) ||
            token == static_cast<int>(Flux::TokenType::tok_then) ||
            token == static_cast<int>(Flux::TokenType::tok_else) ||
            token == static_cast<int>(Flux::TokenType::tok_for) ||
            token == static_cast<int>(Flux::TokenType::tok_while) ||
            token == static_cast<int>(Flux::TokenType::tok_do) ||
            token == static_cast<int>(Flux::TokenType::tok_in) ||
            token == static_cast<int>(Flux::TokenType::tok_return) ||
            token == static_cast<int>(Flux::TokenType::tok_match) ||
            token == static_cast<int>(Flux::TokenType::tok_case) ||
            token == static_cast<int>(Flux::TokenType::tok_break) ||
            token == static_cast<int>(Flux::TokenType::tok_continue) ||
            token == static_cast<int>(Flux::TokenType::tok_import) ||
            token == static_cast<int>(Flux::TokenType::tok_from) ||
            token == static_cast<int>(Flux::TokenType::tok_yield) ||
            token == static_cast<int>(Flux::TokenType::tok_async) ||
            token == static_cast<int>(Flux::TokenType::tok_await)) {
            typeIdx = TOK_KEYWORD;
        } else if (token == static_cast<int>(Flux::TokenType::tok_identifier)) {
            std::string id = lexer.IdentifierStr;
            // Check if it's a type keyword
            if (id == "struct") typeIdx = TOK_STRUCT;
            else if (id == "class") typeIdx = TOK_CLASS;
            else if (id == "enum") typeIdx = TOK_ENUM;
            else if (id == "trait") typeIdx = TOK_INTERFACE;
            else if (id == "impl") typeIdx = TOK_KEYWORD;
            else if (id == "fn") typeIdx = TOK_KEYWORD;
            else if (id == "true" || id == "false") typeIdx = TOK_KEYWORD;
            else if (id == "public" || id == "private" || id == "protected") {
                typeIdx = TOK_KEYWORD;
                modBits |= MOD_DECLARATION;
            } else {
                typeIdx = TOK_VARIABLE;
            }
        } else if (token == static_cast<int>(Flux::TokenType::tok_number)) {
            typeIdx = TOK_NUMBER;
        } else if (token == static_cast<int>(Flux::TokenType::tok_string)) {
            typeIdx = TOK_STRING;
        } else if (token == '+' || token == '-' || token == '*' || token == '/' ||
                   token == '%' || token == '=' || token == '<' || token == '>' ||
                   token == '!' || token == '&' || token == '|' || token == '^' ||
                   token == '~') {
            typeIdx = TOK_OPERATOR;
        }

        if (typeIdx >= 0) {
            // Encode as relative (delta) to previous
            int deltaLine = line - prevLine;
            int deltaCol = (deltaLine == 0) ? (col - prevCol) : col;
            data.push_back(static_cast<unsigned int>(deltaLine));
            data.push_back(static_cast<unsigned int>(deltaCol));
            data.push_back(static_cast<unsigned int>(len));
            data.push_back(static_cast<unsigned int>(typeIdx));
            data.push_back(static_cast<unsigned int>(modBits));
            prevLine = line;
            prevCol = col + len;
        }
    }

    result.data = data;
    return result;
}

LspServer::SemanticTokensResult LspServer::getSemanticTokensRange(const std::string& uri, Range range)
{
    // For simplicity, get full tokens and filter by range
    auto full = getSemanticTokens(uri);

    // We'd need to decode the delta-encoded data to filter by range.
    // For now, return full tokens (the client can ignore out-of-range if needed).
    (void)range;
    return full;
}

// ============================================================================
// Inlay Hint — inline type hints
// ============================================================================

std::string LspServer::handleTextDocumentInlayHint(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int rangeStartLine = jsonGetInt(params, "range.start.line");
    int rangeStartChar = jsonGetInt(params, "range.start.character");
    int rangeEndLine = jsonGetInt(params, "range.end.line");
    int rangeEndChar = jsonGetInt(params, "range.end.character");
    Range range{{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};
    auto hints = getInlayHints(uri, range);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < hints.size(); ++i) {
        if (i > 0) oss << ",";
        oss << R"({"position":{"line":)" << hints[i].position.line;
        oss << R"(,"character":)" << hints[i].position.character << "},";
        oss << R"("label":")" << jsonEscape(hints[i].label) << "\"";
        if (hints[i].kind > 0) {
            oss << R"(,"kind":)" << hints[i].kind;
        }
        if (!hints[i].tooltip.empty()) {
            oss << R"(,"tooltip":")" << jsonEscape(hints[i].tooltip) << "\"";
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<LspServer::InlayHint> LspServer::getInlayHints(const std::string& uri, Range range)
{
    std::vector<InlayHint> result;
    auto* doc = getDocument(uri);
    if (!doc) return result;

    const std::string& text = doc->text;
    int startLine = range.start.line;
    int endLine = range.end.line;
    if (startLine == 0 && endLine == 0 && range.start.character == 0 && range.end.character == 0) {
        endLine = 999999;
    }

    int currentLine = 0;
    size_t lineStart = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            if (currentLine >= startLine && currentLine <= endLine) {
                std::string line = text.substr(lineStart, i - lineStart);
                while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
                    line.pop_back();
                if (!line.empty()) {
                    size_t varPos = line.find("var ");
                    if (varPos != std::string::npos && varPos < 4) {
                        size_t eqPos = line.find(" = ", varPos);
                        if (eqPos != std::string::npos) {
                            std::string rhs = line.substr(eqPos + 3);
                            while (!rhs.empty() && rhs[0] == ' ') rhs.erase(0, 1);
                            std::string inferredType;
                            if (!rhs.empty() && (isdigit((unsigned char)rhs[0]) || rhs[0] == '.')) {
                                inferredType = "double";
                            } else if (rhs.size() >= 2 && rhs[0] == '"') {
                                inferredType = "string";
                            } else if (rhs.size() >= 4 && rhs.substr(0, 4) == "true") {
                                inferredType = "bool";
                            } else if (rhs.size() >= 5 && rhs.substr(0, 5) == "false") {
                                inferredType = "bool";
                            } else if ((rhs.size() >= 3 && rhs.substr(0, 3) == "sin") ||
                                       (rhs.size() >= 3 && rhs.substr(0, 3) == "cos") ||
                                       (rhs.size() >= 4 && rhs.substr(0, 4) == "sqrt")) {
                                inferredType = "double";
                            } else {
                                inferredType = "auto";
                            }
                            if (!inferredType.empty()) {
                                Position hintPos{currentLine, static_cast<int>(line.size())};
                                InlayHint hint;
                                hint.position = hintPos;
                                hint.label = ": " + inferredType;
                                hint.kind = 1;
                                hint.tooltip = "Inferred type: " + inferredType;
                                result.push_back(hint);
                            }
                        }
                    }
                }
            }
            currentLine++;
            lineStart = i + 1;
        }
    }
    return result;
}

// ============================================================================
// Moniker — stable identifiers
// ============================================================================

std::string LspServer::handleTextDocumentMoniker(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    Position pos;
    pos.line = jsonGetInt(params, "position.line");
    pos.character = jsonGetInt(params, "position.character");

    auto monikers = getMonikers(uri, pos);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < monikers.size(); ++i) {
        if (i > 0) oss << ",";
        oss << R"({"scheme":")" << jsonEscape(monikers[i].scheme) << "\"";
        oss << R"(,"identifier":")" << jsonEscape(monikers[i].identifier) << "\"";
        if (monikers[i].kind > 0) {
            oss << R"(,"kind":)" << monikers[i].kind;
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<LspServer::Moniker> LspServer::getMonikers(const std::string& uri, Position pos)
{
    std::vector<Moniker> result;
    auto* doc = getDocument(uri);
    if (!doc) return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty()) return result;

    auto& symbols = m_symbolTables[uri];
    if (symbols.empty()) symbols = buildSymbolTable(uri);

    for (const auto& sym : symbols) {
        if (sym.name != word) continue;
        if (pos.line < sym.range.start.line || pos.line > sym.range.end.line) continue;
        Moniker m;
        m.scheme = "flux";
        m.identifier = sym.name;
        m.kind = (sym.kind == SymbolEntry::Function || sym.kind == SymbolEntry::Type) ? 2 : 3;
        result.push_back(m);
        break;
    }
    return result;
}

// ============================================================================
// JSON Helpers
// ============================================================================


} // namespace Tooling
} // namespace Flux
