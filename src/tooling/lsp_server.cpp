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

#include "flux/tooling/lsp_server.h"
#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"
#include "flux/jit/flux_jit.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

namespace Flux {
namespace Tooling {

// ============================================================================
// TextDocument Utilities
// ============================================================================

Position TextDocument::offsetToPosition(size_t offset) const
{
    Position pos;
    size_t current = 0;
    while (current < offset && current < text.size()) {
        if (text[current] == '\n') {
            pos.line++;
            pos.character = 0;
        } else {
            pos.character++;
        }
        current++;
    }
    return pos;
}

size_t TextDocument::positionToOffset(Position pos) const
{
    size_t offset = 0;
    int currentLine = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (currentLine == pos.line) {
            offset = i;
            break;
        }
        if (text[i] == '\n')
            currentLine++;
        if (i == text.size() - 1)
            offset = text.size();
    }
    // Move to character position on the line
    size_t lineStart = 0;
    int l = 0;
    for (size_t i = 0; i < text.size() && l < pos.line; ++i) {
        if (text[i] == '\n') {
            l++;
            lineStart = i + 1;
        }
    }
    return lineStart + static_cast<size_t>(pos.character);
}

std::string TextDocument::getLine(size_t lineNum) const
{
    size_t start = 0;
    int currentLine = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (currentLine == static_cast<int>(lineNum)) {
            start = i;
            break;
        }
        if (text[i] == '\n')
            currentLine++;
    }
    size_t end = text.find('\n', start);
    if (end == std::string::npos)
        end = text.size();
    return text.substr(start, end - start);
}

std::string TextDocument::getWordAtPosition(Position pos) const
{
    size_t offset = positionToOffset(pos);
    if (offset >= text.size())
        return "";

    // Find word boundaries
    size_t start = offset;
    while (start > 0 && (std::isalnum(text[start - 1]) || text[start - 1] == '_'))
        start--;
    size_t end = offset;
    while (end < text.size() && (std::isalnum(text[end]) || text[end] == '_'))
        end++;

    return text.substr(start, end - start);
}

// ============================================================================
// LSP Server Implementation
// ============================================================================

LspServer::LspServer() = default;
LspServer::~LspServer() = default;

std::string LspServer::processRequest(const std::string& jsonRequest)
{
    std::string method = jsonGet(jsonRequest, "method");
    int id = jsonGetInt(jsonRequest, "id", -1);
    std::string params = jsonRequest; // Full request for nested extraction

    if (method == "initialize") {
        return makeResponse(id, handleInitialize(params));
    } else if (method == "shutdown") {
        return makeResponse(id, handleShutdown(params));
    } else if (method == "exit") {
        return ""; // No response needed
    } else if (method == "textDocument/didOpen") {
        handleTextDocumentDidOpen(params);
        return ""; // Notification, no response
    } else if (method == "textDocument/didChange") {
        handleTextDocumentDidChange(params);
        return "";
    } else if (method == "textDocument/didClose") {
        handleTextDocumentDidClose(params);
        return "";
    } else if (method == "textDocument/completion") {
        return makeResponse(id, handleTextDocumentCompletion(params));
    } else if (method == "textDocument/hover") {
        return makeResponse(id, handleTextDocumentHover(params));
    } else if (method == "textDocument/definition") {
        return makeResponse(id, handleTextDocumentDefinition(params));
    } else if (method == "textDocument/references") {
        return makeResponse(id, handleTextDocumentReferences(params));
    } else if (method == "textDocument/implementation") {
        return makeResponse(id, handleTextDocumentImplementation(params));
    } else if (method == "textDocument/typeDefinition") {
        return makeResponse(id, handleTextDocumentTypeDefinition(params));
    } else if (method == "textDocument/codeAction") {
        return makeResponse(id, handleTextDocumentCodeAction(params));
    } else if (method == "textDocument/formatting") {
        return makeResponse(id, handleTextDocumentFormatting(params));
    } else if (method == "textDocument/signatureHelp") {
        return makeResponse(id, handleTextDocumentSignatureHelp(params));
    } else if (method == "textDocument/documentSymbol") {
        return makeResponse(id, handleTextDocumentDocumentSymbol(params));
    } else if (method == "textDocument/prepareRename") {
        return makeResponse(id, handleTextDocumentPrepareRename(params));
    } else if (method == "textDocument/codeLens") {
        return makeResponse(id, handleTextDocumentCodeLens(params));
    } else if (method == "textDocument/prepareCallHierarchy") {
        return makeResponse(id, handleTextDocumentPrepareCallHierarchy(params));
    } else if (method == "callHierarchy/incomingCalls") {
        return makeResponse(id, handleCallHierarchyIncomingCalls(params));
    } else if (method == "callHierarchy/outgoingCalls") {
        return makeResponse(id, handleCallHierarchyOutgoingCalls(params));
    } else if (method == "textDocument/prepareTypeHierarchy") {
        return makeResponse(id, handleTextDocumentPrepareTypeHierarchy(params));
    } else if (method == "typeHierarchy/supertypes") {
        return makeResponse(id, handleTypeHierarchySupertypes(params));
    } else if (method == "typeHierarchy/subtypes") {
        return makeResponse(id, handleTypeHierarchySubtypes(params));
    } else if (method == "textDocument/linkedEditingRange") {
        return makeResponse(id, handleTextDocumentLinkedEditingRange(params));
    } else if (method == "workspace/symbol") {
        return makeResponse(id, handleWorkspaceSymbol(params));
    } else if (method == "$/cancelRequest") {
        return "";
    }

    if (id >= 0) {
        return makeErrorResponse(id, -32601, "Method not found: " + method);
    }
    return "";
}

int LspServer::run()
{
    std::string line;
    int contentLength = 0;

    while (std::getline(std::cin, line)) {
        // Strip CR if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty()) {
            // End of headers, read content
            if (contentLength <= 0)
                continue;

            std::string content(contentLength, '\0');
            std::cin.read(&content[0], contentLength);
            // Consume the trailing CRLF if present
            char buf[2];
            std::cin.read(buf, 2);

            std::string response = processRequest(content);
            if (!response.empty()) {
                std::cout << "Content-Length: " << response.size() << "\r\n\r\n" << response;
                std::cout.flush();
            }
            contentLength = 0;
        } else if (line.find("Content-Length:") == 0) {
            contentLength = std::stoi(line.substr(15));
        }
    }
    return 0;
}

void LspServer::openDocument(const std::string& uri, const std::string& languageId, int version,
                             const std::string& text)
{
    TextDocument doc;
    doc.uri = uri;
    doc.languageId = languageId;
    doc.version = version;
    doc.text = text;
    m_documents[uri] = doc;

    // Analyze and publish diagnostics
    auto diags = analyzeDocument(uri);
    publishDiagnostics(uri, diags);
}

void LspServer::changeDocument(const std::string& uri, int version, const std::string& text)
{
    auto it = m_documents.find(uri);
    if (it != m_documents.end()) {
        it->second.version = version;
        it->second.text = text;

        // Re-analyze and publish diagnostics
        auto diags = analyzeDocument(uri);
        publishDiagnostics(uri, diags);
    }
}

void LspServer::closeDocument(const std::string& uri)
{
    m_documents.erase(uri);
    m_symbolTables.erase(uri);
}

TextDocument* LspServer::getDocument(const std::string& uri)
{
    auto it = m_documents.find(uri);
    return (it != m_documents.end()) ? &it->second : nullptr;
}

// ============================================================================
// Semantic Analysis
// ============================================================================

std::vector<Diagnostic> LspServer::analyzeDocument(const std::string& uri)
{
    std::vector<Diagnostic> diags;
    auto* doc = getDocument(uri);
    if (!doc)
        return diags;

    // Parse the full script (handles def, extern, and top-level expressions)
    {
        Flux::Parser parser(doc->text);

        // Parse all top-level constructs in a loop
        while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof)) {
            bool parsed = true;
            if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_def)) {
                parsed = parser.ParseDefinition() != nullptr;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_extern)) {
                parsed = parser.ParseExtern() != nullptr;
            } else {
                parsed = parser.ParseTopLevelExpr() != nullptr;
            }
            if (!parsed) {
                parser.getNextToken();
                if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_eof))
                    break;
            }
        }

        for (auto& err : parser.getErrors()) {
            Diagnostic d;
            d.severity = Diagnostic::Error;
            d.message = err.message;
            d.source = "fluxscript-compiler";
            d.range.start = {err.line - 1, err.column - 1};
            d.range.end = {err.line - 1, err.column};
            diags.push_back(d);
        }
    }

    // Build symbol table for completions
    m_symbolTables[uri] = buildSymbolTable(uri);

    return diags;
}

std::vector<SymbolEntry> LspServer::buildSymbolTable(const std::string& uri)
{
    std::vector<SymbolEntry> symbols;
    auto* doc = getDocument(uri);
    if (!doc)
        return symbols;

    Flux::Lexer lexer(doc->text);

    while (lexer.getNextToken() != static_cast<int>(Flux::TokenType::tok_eof)) {
        int token = lexer.CurTok;

        // Skip keywords that aren't symbol names
        if (token == static_cast<int>(Flux::TokenType::tok_identifier)) {
            SymbolEntry entry;
            entry.name = lexer.IdentifierStr;
            entry.uri = uri;
            entry.range.start = {lexer.getCurrentLine() - 1, lexer.getCurrentColumn() - 1};
            entry.range.end = {entry.range.start.line,
                               entry.range.start.character + static_cast<int>(entry.name.size())};

            // Peek ahead to determine kind
            if (lexer.peekToken() == '(') {
                entry.kind = SymbolEntry::Function;
            } else {
                entry.kind = SymbolEntry::Variable;
            }

            symbols.push_back(entry);
        }
    }

    return symbols;
}

// ============================================================================
// LSP Handlers
// ============================================================================

std::string LspServer::handleInitialize(const std::string& params)
{
    return R"({
        "capabilities": {
            "textDocumentSync": {
                "openClose": true,
                "change": 2
            },
            "completionProvider": {
                "resolveProvider": false,
                "triggerCharacters": [".", "(", ","]
            },
            "hoverProvider": true,
            "definitionProvider": true,
            "referencesProvider": true,
            "implementationProvider": true,
            "typeDefinitionProvider": true,
            "codeActionProvider": true,
            "documentFormattingProvider": true,
            "signatureHelpProvider": {
                "triggerCharacters": ["(", ","]
            },
            "documentSymbolProvider": true,
            "renameProvider": true,
            "codeLensProvider": {
                "resolveProvider": false
            },
            "callHierarchyProvider": true,
            "typeHierarchyProvider": true,
            "linkedEditingRangeProvider": true,
            "workspaceSymbolProvider": true
        },
        "serverInfo": {
            "name": "fluxscript-lsp",
            "version": "1.0.0"
        }
    })";
}

std::string LspServer::handleShutdown(const std::string& params)
{
    return "null";
}

std::string LspServer::handleTextDocumentDidOpen(const std::string& params)
{
    std::string uri = jsonGetNested(params, "textDocument", "uri");
    std::string langId = jsonGetNested(params, "textDocument", "languageId");
    int version = jsonGetInt(params, "textDocument.version");
    std::string text = jsonGetNested(params, "textDocument", "text");
    openDocument(uri, langId, version, text);
    return "";
}

std::string LspServer::handleTextDocumentDidChange(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int version = jsonGetInt(params, "textDocument.version");

    auto it = m_documents.find(uri);
    if (it == m_documents.end())
        return "";

    // Check for incremental changes (mode 2) vs full text (mode 1)
    size_t arrStart = params.find("\"contentChanges\"");
    if (arrStart == std::string::npos)
        return "";

    // Try to find a "range" key — if present, it's incremental
    size_t rangePos = params.find("\"range\"", arrStart);
    if (rangePos != std::string::npos && rangePos < params.find("\"text\"", arrStart)) {
        // Incremental change: extract the range and replacement text
        size_t textStart = params.find("\"text\"", arrStart);

        // Extract text
        std::string replacement;
        size_t colonPos = params.find(':', textStart);
        size_t quoteStart = params.find('"', colonPos);
        if (quoteStart != std::string::npos) {
            size_t quoteEnd = quoteStart + 1;
            while (quoteEnd < params.size()) {
                if (params[quoteEnd] == '"' && params[quoteEnd - 1] != '\\')
                    break;
                replacement += params[quoteEnd];
                quoteEnd++;
            }
        }

        // Extract range start line/character
        size_t startLinePos = params.find("\"line\"", rangePos);
        int startLine = 0, startChar = 0, endLine = 0, endChar = 0;
        if (startLinePos != std::string::npos) {
            size_t colon = params.find(':', startLinePos);
            size_t numStart = colon + 1;
            while (numStart < params.size() && params[numStart] == ' ')
                numStart++;
            startLine = std::stoi(params.substr(numStart));
        }

        size_t startCharPos = params.find("\"character\"", rangePos);
        if (startCharPos != std::string::npos) {
            size_t colon = params.find(':', startCharPos);
            size_t numStart = colon + 1;
            while (numStart < params.size() && params[numStart] == ' ')
                numStart++;
            startChar = std::stoi(params.substr(numStart));
        }

        // Find end range
        size_t endRangePos = params.find("\"end\"", rangePos + 10);
        if (endRangePos != std::string::npos) {
            size_t endLinePos = params.find("\"line\"", endRangePos);
            if (endLinePos != std::string::npos) {
                size_t colon = params.find(':', endLinePos);
                size_t numStart = colon + 1;
                while (numStart < params.size() && params[numStart] == ' ')
                    numStart++;
                endLine = std::stoi(params.substr(numStart));
            }
            size_t endCharPos = params.find("\"character\"", endRangePos);
            if (endCharPos != std::string::npos) {
                size_t colon = params.find(':', endCharPos);
                size_t numStart = colon + 1;
                while (numStart < params.size() && params[numStart] == ' ')
                    numStart++;
                endChar = std::stoi(params.substr(numStart));
            }
        }

        // Apply incremental change
        size_t startOffset = it->second.positionToOffset({startLine, startChar});
        size_t endOffset = it->second.positionToOffset({endLine, endChar});
        it->second.text.replace(startOffset, endOffset - startOffset, replacement);
        it->second.version = version;

        // Re-analyze and publish diagnostics
        auto diags = analyzeDocument(uri);
        publishDiagnostics(uri, diags);
    } else {
        // Full text replacement (mode 1 fallback)
        size_t textStart = params.find("\"text\"", arrStart);
        if (textStart != std::string::npos) {
            size_t colonPos = params.find(':', textStart);
            size_t quoteStart = params.find('"', colonPos);
            size_t quoteEnd = quoteStart + 1;
            std::string text;
            while (quoteEnd < params.size()) {
                if (params[quoteEnd] == '"' && params[quoteEnd - 1] != '\\')
                    break;
                text += params[quoteEnd];
                quoteEnd++;
            }
            changeDocument(uri, version, text);
        }
    }
    return "";
}

std::string LspServer::handleTextDocumentDidClose(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    closeDocument(uri);
    return "";
}

std::string LspServer::handleTextDocumentCompletion(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto items = getCompletions(uri, {line, col});

    std::ostringstream oss;
    oss << R"({"isIncomplete":false,"items":[)";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "{";
        oss << R"("label":")" << jsonEscape(items[i].label) << "\",";
        oss << R"("kind":)" << items[i].kind << ",";
        if (!items[i].detail.empty()) {
            oss << R"("detail":")" << jsonEscape(items[i].detail) << "\",";
        }
        if (!items[i].documentation.empty()) {
            oss << R"("documentation":")" << jsonEscape(items[i].documentation) << "\",";
        }
        if (!items[i].insertText.empty()) {
            oss << R"("insertText":")" << jsonEscape(items[i].insertText) << "\",";
            oss << R"("insertTextFormat":)" << items[i].insertTextFormat << ",";
        }
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string LspServer::handleTextDocumentHover(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto hover = getHover(uri, {line, col});

    if (hover.contents.empty())
        return "null";

    std::ostringstream oss;
    oss << R"({"contents":{"kind":"markdown","value":")" << jsonEscape(hover.contents) << "\"";
    oss << R"(,"range":{"start":{"line":)" << hover.range.start.line << ",";
    oss << R"("character":)" << hover.range.start.character << "},";
    oss << R"("end":{"line":)" << hover.range.end.line << ",";
    oss << R"("character":)" << hover.range.end.character << "}}}";
    return oss.str();
}

std::string LspServer::handleTextDocumentDefinition(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto loc = getDefinition(uri, {line, col});
    if (loc.uri.empty())
        return "null";

    std::ostringstream oss;
    oss << R"({"uri":")" << jsonEscape(loc.uri) << "\",";
    oss << R"("range":{"start":{"line":)" << loc.range.start.line << ",";
    oss << R"("character":)" << loc.range.start.character << "},";
    oss << R"("end":{"line":)" << loc.range.end.line << ",";
    oss << R"("character":)" << loc.range.end.character << "}}";
    return oss.str();
}

std::string LspServer::handleTextDocumentReferences(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto refs = getReferences(uri, {line, col});

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < refs.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"uri":")" << jsonEscape(refs[i].uri) << "\",";
        oss << R"("range":{"start":{"line":)" << refs[i].range.start.line << ",";
        oss << R"("character":)" << refs[i].range.start.character << "},";
        oss << R"("end":{"line":)" << refs[i].range.end.line << ",";
        oss << R"("character":)" << refs[i].range.end.character << "}}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentImplementation(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto locs = getImplementation(uri, {line, col});

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < locs.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"uri":")" << jsonEscape(locs[i].uri) << "\",";
        oss << R"("range":{"start":{"line":)" << locs[i].range.start.line << ",";
        oss << R"("character":)" << locs[i].range.start.character << "},";
        oss << R"("end":{"line":)" << locs[i].range.end.line << ",";
        oss << R"("character":)" << locs[i].range.end.character << "}}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentTypeDefinition(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto locs = getTypeDefinition(uri, {line, col});

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < locs.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"uri":")" << jsonEscape(locs[i].uri) << "\",";
        oss << R"("range":{"start":{"line":)" << locs[i].range.start.line << ",";
        oss << R"("character":)" << locs[i].range.start.character << "},";
        oss << R"("end":{"line":)" << locs[i].range.end.line << ",";
        oss << R"("character":)" << locs[i].range.end.character << "}}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentCodeAction(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    // Get diagnostics at this position
    auto diags = analyzeDocument(uri);

    int line = jsonGetInt(params, "range.start.line");
    int col = jsonGetInt(params, "range.start.character");
    Position pos{line, col};

    auto actions = getCodeActions(uri, pos, diags);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < actions.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "{";
        oss << R"("title":")" << jsonEscape(actions[i].title) << "\",";
        oss << R"("kind":")" << jsonEscape(actions[i].kind) << "\"";
        if (!actions[i].edit.empty()) {
            oss << R"(,"edit":)" << actions[i].edit;
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentFormatting(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    // Determine tab size from options
    std::string tabSizeStr = jsonGet(params, "options.tabSize");
    std::string tabStr = "    "; // default 4 spaces
    if (!tabSizeStr.empty()) {
        try {
            int tabSize = std::stoi(tabSizeStr);
            tabStr = std::string(static_cast<size_t>(tabSize), ' ');
        } catch (...) {
        }
    }

    auto edits = computeTextEdits(uri, tabStr);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < edits.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "{";
        oss << R"("range":{"start":{"line":)" << edits[i].first.line << ",";
        oss << R"("character":)" << edits[i].first.character << "},";
        oss << R"("end":{"line":)" << edits[i].first.line << ",";
        oss << R"("character":0}},)"
            << R"("newText":")" << jsonEscape(edits[i].second) << "\"}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentSignatureHelp(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto sigHelp = getSignatureHelp(uri, {line, col});

    std::ostringstream oss;
    oss << R"({"signatures":[)";
    for (size_t i = 0; i < sigHelp.signatures.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& sig = sigHelp.signatures[i];
        oss << "{";
        oss << R"("label":")" << jsonEscape(sig.label) << "\",";
        oss << R"("documentation":")" << jsonEscape(sig.documentation) << "\",";
        oss << R"("parameters":[)";
        for (size_t j = 0; j < sig.parameters.size(); ++j) {
            if (j > 0)
                oss << ",";
            oss << R"({"label":")" << jsonEscape(sig.parameters[j].label) << "\"}";
        }
        oss << "]}";
    }
    oss << R"(],"activeSignature":)" << sigHelp.activeSignature << ",";
    oss << R"("activeParameter":)" << sigHelp.activeParameter << "}";
    return oss.str();
}

std::string LspServer::handleTextDocumentDocumentSymbol(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);

    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto& sym : symbols) {
        if (sym.kind != SymbolEntry::Function && sym.kind != SymbolEntry::Variable)
            continue;
        if (!first)
            oss << ",";
        first = false;
        oss << "{";
        oss << R"("name":")" << jsonEscape(sym.name) << "\",";
        oss << R"("kind":)" << (sym.kind == SymbolEntry::Function ? "12" : "13") << ",";
        oss << R"("range":{"start":{"line":)" << sym.range.start.line << ",";
        oss << R"("character":)" << sym.range.start.character << "},";
        oss << R"("end":{"line":)" << sym.range.end.line << ",";
        oss << R"("character":)" << sym.range.end.character << "}}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleWorkspaceSymbol(const std::string& params)
{
    std::string query = jsonGet(params, "query");
    std::vector<SymbolEntry> allSymbols;
    for (auto& [uri, symbols] : m_symbolTables) {
        allSymbols.insert(allSymbols.end(), symbols.begin(), symbols.end());
    }

    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto& sym : allSymbols) {
        if (!query.empty() && sym.name.find(query) == std::string::npos)
            continue;
        if (!first)
            oss << ",";
        first = false;
        oss << "{";
        oss << R"("name":")" << jsonEscape(sym.name) << "\",";
        oss << R"("kind":)" << (sym.kind == SymbolEntry::Function ? "12" : "13") << ",";
        oss << R"("location":{"uri":")" << jsonEscape(sym.uri) << "\"}}";
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// Completion Engine
// ============================================================================

void LspServer::buildCompletions()
{
    if (m_completionsBuilt)
        return;
    m_completionsBuilt = true;

    auto keywords = getKeywordCompletions();
    auto builtins = getBuiltinFunctionCompletions();
    auto types = getTypeCompletions();
    auto snippets = getSnippetCompletions();

    m_allCompletions.insert(m_allCompletions.end(), keywords.begin(), keywords.end());
    m_allCompletions.insert(m_allCompletions.end(), builtins.begin(), builtins.end());
    m_allCompletions.insert(m_allCompletions.end(), types.begin(), types.end());
    m_allCompletions.insert(m_allCompletions.end(), snippets.begin(), snippets.end());
}

std::vector<CompletionItem> LspServer::getKeywordCompletions()
{
    return {
        {"def", CompletionItem::Keyword, "function definition", "Define a function",
         "def ${1:name}(${2:args}) ${3:body}", 2},
        {"extern", CompletionItem::Keyword, "external declaration", "Declare an external function",
         "extern ${1:name}(${2:args})", 2},
        {"let", CompletionItem::Keyword, "typed variable", "Declare a typed variable",
         "let ${1:name} : ${2:type} = ${3:value}", 2},
        {"var", CompletionItem::Keyword, "type-inferred variable", "Declare a variable with inferred type",
         "var ${1:name} = ${2:value}", 2},
        {"if", CompletionItem::Keyword, "conditional", "Conditional expression",
         "if ${1:cond} then ${2:then} else ${3:else}", 2},
        {"then", CompletionItem::Keyword, "then branch", "", "then", 1},
        {"else", CompletionItem::Keyword, "else branch", "", "else", 1},
        {"in", CompletionItem::Keyword, "binding", "", "in", 1},
        {"do", CompletionItem::Keyword, "loop body", "", "do", 1},
        {"return", CompletionItem::Keyword, "return statement", "", "return", 1},
        {"case", CompletionItem::Keyword, "case label", "", "case", 1},
        {"break", CompletionItem::Keyword, "break loop", "", "break", 1},
        {"continue", CompletionItem::Keyword, "continue loop", "", "continue", 1},
        {"catch", CompletionItem::Keyword, "catch block", "", "catch", 1},
        {"throw", CompletionItem::Keyword, "throw exception", "", "throw", 1},
        {"match", CompletionItem::Keyword, "pattern match", "Pattern matching",
         "match ${1:expr} { ${2:pattern} => ${3:result} }", 2},
        {"parallel", CompletionItem::Keyword, "parallel loop", "Parallel for loop",
         "parallel for ${1:i} in ${2:start}, ${3:end} do ${4:body}", 2},
    };
}

std::vector<CompletionItem> LspServer::getBuiltinFunctionCompletions()
{
    return {
        {"sin", CompletionItem::Function, "(x) -> double", "Sine function", "sin(${1:x})", 2},
        {"cos", CompletionItem::Function, "(x) -> double", "Cosine function", "cos(${1:x})", 2},
        {"tan", CompletionItem::Function, "(x) -> double", "Tangent function", "tan(${1:x})", 2},
        {"sqrt", CompletionItem::Function, "(x) -> double", "Square root", "sqrt(${1:x})", 2},
        {"exp", CompletionItem::Function, "(x) -> double", "Exponential function", "exp(${1:x})", 2},
        {"log", CompletionItem::Function, "(x) -> double", "Natural logarithm", "log(${1:x})", 2},
        {"log10", CompletionItem::Function, "(x) -> double", "Base-10 logarithm", "log10(${1:x})", 2},
        {"abs", CompletionItem::Function, "(x) -> double", "Absolute value", "abs(${1:x})", 2},
        {"floor", CompletionItem::Function, "(x) -> double", "Floor function", "floor(${1:x})", 2},
        {"ceil", CompletionItem::Function, "(x) -> double", "Ceiling function", "ceil(${1:x})", 2},
        {"pow", CompletionItem::Function, "(x, y) -> double", "Power function", "pow(${1:x}, ${2:y})", 2},
        {"atan2", CompletionItem::Function, "(y, x) -> double", "Two-argument arctangent", "atan2(${1:y}, ${2:x})", 2},
        {"pi", CompletionItem::Function, "() -> double", "Pi constant", "pi()", 2},
        {"cross", CompletionItem::Function, "(expr, [rise_fall]) -> double", "Zero-crossing detection",
         "cross(${1:expr})", 2},
        {"above", CompletionItem::Function, "(expr, threshold) -> double", "Threshold detection",
         "above(${1:expr}, ${2:threshold})", 2},
        {"timer", CompletionItem::Function, "() -> double", "Elapsed simulation time", "timer()", 2},
        {"posedge", CompletionItem::Function, "(signal) -> double", "Positive edge detection", "posedge(${1:signal})",
         2},
        {"negedge", CompletionItem::Function, "(signal) -> double", "Negative edge detection", "negedge(${1:signal})",
         2},
        {"white_noise", CompletionItem::Function, "(amplitude) -> double", "Gaussian white noise",
         "white_noise(${1:amp})", 2},
        {"flicker_noise", CompletionItem::Function, "(amplitude, corner_freq) -> double", "1/f noise",
         "flicker_noise(${1:amp}, ${2:freq})", 2},
        {"thermal_noise", CompletionItem::Function, "(resistance, [temperature]) -> double",
         "Johnson-Nyquist thermal noise", "thermal_noise(${1:R}, ${2:300.15})", 2},
        {"unit", CompletionItem::Function, "(value, unit_str)", "Annotate value with unit",
         "unit(${1:val}, \"${2:V}\")", 2},
        {"convert", CompletionItem::Function, "(value, from, to) -> double", "Unit conversion",
         "convert(${1:val}, \"${2:V}\", \"${3:mV}\")", 2},
        {"dimension", CompletionItem::Function, "(value) -> string", "Dimensional analysis", "dimension(${1:val})", 2},
        {"has_unit", CompletionItem::Function, "(value, unit) -> double", "Check unit compatibility",
         "has_unit(${1:val}, \"${2:V}\")", 2},
    };
}

std::vector<CompletionItem> LspServer::getTypeCompletions()
{
    return {
        {"double", CompletionItem::Type, "type", "Double precision float", "double", 1},
        {"float", CompletionItem::Type, "type", "Single precision float", "float", 1},
        {"int", CompletionItem::Type, "type", "32-bit integer", "int", 1},
        {"void", CompletionItem::Type, "type", "Void type", "void", 1},
        {"complex", CompletionItem::Type, "type", "Complex number", "complex", 1},
        {"string", CompletionItem::Type, "type", "String type", "string", 1},
        {"vector", CompletionItem::Type, "type", "Dynamic vector", "vector", 1},
        {"matrix", CompletionItem::Type, "type", "2D matrix", "matrix", 1},
    };
}

std::vector<CompletionItem> LspServer::getSnippetCompletions()
{
    return {
        {"func", CompletionItem::Snippet, "Function template", "def name(args) body",
         "def ${1:name}(${2:args}) ${3:body}", 2},
        {"forloop", CompletionItem::Snippet, "For loop", "for i in start, end do body",
         "for ${1:i} in ${2:1}, ${3:10} do ${4:body}", 2},
        {"whileloop", CompletionItem::Snippet, "While loop", "while cond do body", "while ${1:cond} do ${2:body}", 2},
        {"ifelse", CompletionItem::Snippet, "If-else", "if cond then a else b", "if ${1:cond} then ${2:a} else ${3:b}",
         2},
        {"bsource", CompletionItem::Snippet, "Behavioral source", "bsource B name n+ n- V={expr}",
         "bsource ${1:B1} ${2:out} ${3:0} V={${4:expression}}", 2},
        {"montecarlo", CompletionItem::Snippet, "Monte Carlo analysis", "montecarlo output iterations",
         "montecarlo ${1:output} ${2:1000}", 2},
        {"piecewise", CompletionItem::Snippet, "Piecewise function", "piecewise(x1,y1, x2,y2, ...)",
         "piecewise(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:x})", 2},
        {"table", CompletionItem::Snippet, "Lookup table", "table(k1,v1, k2,v2, key)",
         "table(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:key})", 2},
    };
}

std::vector<CompletionItem> LspServer::getCompletions(const std::string& uri, Position pos)
{
    buildCompletions();

    auto* doc = getDocument(uri);
    if (!doc)
        return m_allCompletions;

    std::string prefix = doc->getWordAtPosition(pos);

    std::vector<CompletionItem> filtered;
    for (auto& item : m_allCompletions) {
        if (prefix.empty() || item.label.find(prefix) == 0) {
            filtered.push_back(item);
        }
    }

    // Also add symbols from current document
    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);
    for (auto& sym : symbols) {
        if (prefix.empty() || sym.name.find(prefix) == 0) {
            CompletionItem ci;
            ci.label = sym.name;
            ci.kind = (sym.kind == SymbolEntry::Function) ? CompletionItem::Function : CompletionItem::Variable;
            ci.detail = sym.typeStr;
            filtered.push_back(ci);
        }
    }

    return filtered;
}

// ============================================================================
// Hover Engine
// ============================================================================

HoverContent LspServer::getHover(const std::string& uri, Position pos)
{
    HoverContent result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Check built-in functions
    auto builtins = getBuiltinFunctionCompletions();
    for (auto& b : builtins) {
        if (b.label == word) {
            result.contents = "```fluxscript\n" + b.detail + "\n```\n\n" + b.documentation;
            result.range.start = pos;
            result.range.end = {pos.line, pos.character + static_cast<int>(word.size())};
            return result;
        }
    }

    // Check types
    auto types = getTypeCompletions();
    for (auto& t : types) {
        if (t.label == word) {
            result.contents = "```fluxscript\n" + t.label + " (type)\n```\n\n" + t.documentation;
            result.range.start = pos;
            result.range.end = {pos.line, pos.character + static_cast<int>(word.size())};
            return result;
        }
    }

    // Check keywords
    auto keywords = getKeywordCompletions();
    for (auto& k : keywords) {
        if (k.label == word) {
            result.contents = "**" + k.label + "** (keyword)\n\n" + k.documentation;
            result.range.start = pos;
            result.range.end = {pos.line, pos.character + static_cast<int>(word.size())};
            return result;
        }
    }

    // Check local symbols
    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);
    for (auto& sym : symbols) {
        if (sym.name == word) {
            std::string kindStr = (sym.kind == SymbolEntry::Function) ? "function" : "variable";
            result.contents = "```fluxscript\n" + kindStr + " " + sym.name;
            if (!sym.typeStr.empty())
                result.contents += ": " + sym.typeStr;
            result.contents += "\n```";
            result.range = sym.range;
            return result;
        }
    }

    return result;
}

// ============================================================================
// Go-to-Definition
// ============================================================================

Location LspServer::getDefinition(const std::string& uri, Position pos)
{
    Location loc;
    auto* doc = getDocument(uri);
    if (!doc)
        return loc;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return loc;

    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);
    for (auto& sym : symbols) {
        if (sym.name == word) {
            loc.uri = sym.uri;
            loc.range = sym.range;
            return loc;
        }
    }

    return loc;
}

std::vector<Location> LspServer::getImplementation(const std::string& uri, Position pos)
{
    std::vector<Location> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Scan for `impl <word> for` patterns (trait implementations)
    std::string searchStr = "impl " + word + " for";
    size_t searchPos = 0;
    while ((searchPos = doc->text.find(searchStr, searchPos)) != std::string::npos) {
        int implLine = 0;
        for (size_t k = 0; k < searchPos; ++k) {
            if (doc->text[k] == '\n')
                implLine++;
        }

        size_t lineStart = searchPos;
        while (lineStart > 0 && doc->text[lineStart - 1] != '\n')
            lineStart--;
        size_t lineEnd = doc->text.find('\n', searchPos);
        if (lineEnd == std::string::npos)
            lineEnd = doc->text.size();
        int colEnd = static_cast<int>(lineEnd - lineStart);
        if (colEnd > 0 && doc->text[lineEnd - 1] == '\r')
            colEnd--;

        Location loc;
        loc.uri = uri;
        loc.range.start = {implLine, 0};
        loc.range.end = {implLine, colEnd};
        result.push_back(loc);

        searchPos = lineEnd;
    }

    return result;
}

std::vector<Location> LspServer::getTypeDefinition(const std::string& uri, Position pos)
{
    std::vector<Location> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Try direct keyword search first (word is already a type name).
    // If that fails, check if word is a variable with `: TypeName` annotation
    // and redirect the search to that type.
    size_t wordOffset = doc->positionToOffset(pos);
    bool foundDirect = false;

    // Phase 1: search for `struct <word>`, `enum <word>`, `trait <word>`, `class <word>`
    std::vector<std::string> keywords = {"struct ", "enum ", "trait ", "class "};
    for (auto& kw : keywords) {
        std::string searchStr = kw + word;
        size_t searchPos = 0;
        while ((searchPos = doc->text.find(searchStr, searchPos)) != std::string::npos) {
            int declLine = 0;
            for (size_t k = 0; k < searchPos; ++k) {
                if (doc->text[k] == '\n')
                    declLine++;
            }

            size_t lineStart = searchPos;
            while (lineStart > 0 && doc->text[lineStart - 1] != '\n')
                lineStart--;
            size_t lineEnd = doc->text.find('\n', searchPos);
            if (lineEnd == std::string::npos)
                lineEnd = doc->text.size();
            int colEnd = static_cast<int>(lineEnd - lineStart);
            if (colEnd > 0 && doc->text[lineEnd - 1] == '\r')
                colEnd--;

            Location loc;
            loc.uri = uri;
            loc.range.start = {declLine, 0};
            loc.range.end = {declLine, colEnd};
            result.push_back(loc);
            break; // One result per keyword
        }
        if (!result.empty())
            break;
    }

    // Phase 2: if direct search failed, check if word is a variable with `: TypeName`
    if (result.empty() && wordOffset < doc->text.size()) {
        size_t ls = wordOffset;
        while (ls > 0 && doc->text[ls - 1] != '\n')
            ls--;
        size_t le = doc->text.find('\n', ls);
        if (le == std::string::npos)
            le = doc->text.size();
        std::string line = doc->text.substr(ls, le - ls);

        size_t colonPos = line.find(':');
        while (colonPos != std::string::npos) {
            size_t ts = colonPos + 1;
            while (ts < line.size() && line[ts] == ' ')
                ts++;
            std::string tn;
            while (ts < line.size() && (isalnum(line[ts]) || line[ts] == '_'))
                tn += line[ts++];
            if (!tn.empty()) {
                size_t wp = line.rfind(word, colonPos);
                if (wp != std::string::npos && colonPos > wp &&
                    (wp == 0 || !isalnum(line[wp - 1]))) {
                    // word is a variable, tn is its type — redirect search
                    word = tn;
                    for (auto& kw : keywords) {
                        std::string ss = kw + word;
                        size_t sp = 0;
                        while ((sp = doc->text.find(ss, sp)) != std::string::npos) {
                            int dl = 0;
                            for (size_t k = 0; k < sp; ++k)
                                if (doc->text[k] == '\n') dl++;
                            size_t lls = sp;
                            while (lls > 0 && doc->text[lls - 1] != '\n') lls--;
                            size_t lle = doc->text.find('\n', sp);
                            if (lle == std::string::npos) lle = doc->text.size();
                            int ce = static_cast<int>(lle - lls);
                            if (ce > 0 && doc->text[lle - 1] == '\r') ce--;
                            Location loc;
                            loc.uri = uri;
                            loc.range.start = {dl, 0};
                            loc.range.end = {dl, ce};
                            result.push_back(loc);
                            break;
                        }
                        if (!result.empty()) break;
                    }
                    break;
                }
            }
            colonPos = line.find(':', colonPos + 1);
        }
    }

    return result;
}

// ============================================================================
// Prepare Rename — validates rename is possible at cursor position
// ============================================================================
LspServer::PrepareRenameResult LspServer::getPrepareRename(const std::string& uri, Position pos)
{
    PrepareRenameResult result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Find the range of the word at position
    size_t offset = doc->positionToOffset(pos);
    if (offset >= doc->text.size())
        return result;

    // Expand outward to capture full word boundaries
    size_t wordStart = offset;
    while (wordStart > 0 && (isalnum(doc->text[wordStart - 1]) || doc->text[wordStart - 1] == '_'))
        wordStart--;
    size_t wordEnd = offset;
    while (wordEnd < doc->text.size() && (isalnum(doc->text[wordEnd]) || doc->text[wordEnd] == '_'))
        wordEnd++;

    Position start = doc->offsetToPosition(wordStart);
    Position end = doc->offsetToPosition(wordEnd);

    result.range = {start, end};
    result.placeholder = word;
    result.valid = true;
    return result;
}

std::string LspServer::handleTextDocumentPrepareRename(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto result = getPrepareRename(uri, {line, col});
    if (!result.valid)
        return "null";

    std::ostringstream oss;
    oss << R"({"range":{"start":{"line":)" << result.range.start.line;
    oss << R"(,"character":)" << result.range.start.character << "},";
    oss << R"("end":{"line":)" << result.range.end.line;
    oss << R"(,"character":)" << result.range.end.character << "},";
    oss << R"("placeholder":")" << jsonEscape(result.placeholder) << "\"}";
    return oss.str();
}

// ============================================================================
// Code Lens — reference count badges above declarations
// ============================================================================
std::vector<LspServer::CodeLensItem> LspServer::getCodeLenses(const std::string& uri)
{
    std::vector<CodeLensItem> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);

    for (auto& sym : symbols) {
        if (sym.kind != SymbolEntry::Function)
            continue;
        if (sym.name.empty())
            continue;

        // Count whole-word occurrences in document
        int count = 0;
        size_t pos = 0;
        while ((pos = doc->text.find(sym.name, pos)) != std::string::npos) {
            // Check word boundaries
            bool leftOk = pos == 0 || (!isalnum(doc->text[pos - 1]) && doc->text[pos - 1] != '_');
            bool rightOk = (pos + sym.name.size() >= doc->text.size()) ||
                           (!isalnum(doc->text[pos + sym.name.size()]) && doc->text[pos + sym.name.size()] != '_');
            if (leftOk && rightOk)
                count++;
            pos += sym.name.size();
        }

        // Subtract 1 for the declaration itself
        int refCount = count - 1;
        if (refCount < 0)
            refCount = 0;

        std::string title = std::to_string(refCount) + " reference";
        if (refCount != 1)
            title += "s";

        CodeLensItem item;
        item.range = sym.range;
        item.title = title;
        item.command = "";
        result.push_back(item);
    }

    return result;
}

std::string LspServer::handleTextDocumentCodeLens(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    auto lenses = getCodeLenses(uri);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < lenses.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"range":{"start":{"line":)" << lenses[i].range.start.line;
        oss << R"(,"character":)" << lenses[i].range.start.character << "},";
        oss << R"("end":{"line":)" << lenses[i].range.end.line;
        oss << R"(,"character":)" << lenses[i].range.end.character << "},";
        oss << R"("command":{"title":")" << jsonEscape(lenses[i].title) << "\",";
        oss << R"("command":")" << jsonEscape(lenses[i].command) << "\"";
        oss << "}}";
    }
    oss << "]";
    return oss.str();
}

// ============================================================================
// Call Hierarchy
// ============================================================================

std::string LspServer::handleTextDocumentPrepareCallHierarchy(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto item = getPrepareCallHierarchy(uri, {line, col});
    if (item.name.empty())
        return "null";

    std::ostringstream oss;
    oss << R"({"name":")" << jsonEscape(item.name) << "\",";
    oss << R"("kind":)" << item.kind << ",";
    oss << R"("detail":")" << jsonEscape(item.detail) << "\",";
    oss << R"("uri":")" << jsonEscape(item.uri) << "\",";
    oss << R"("range":{"start":{"line":)" << item.range.start.line << ",";
    oss << R"("character":)" << item.range.start.character << "},";
    oss << R"("end":{"line":)" << item.range.end.line << ",";
    oss << R"("character":)" << item.range.end.character << "},";
    oss << R"("selectionRange":{"start":{"line":)" << item.selectionRange.start.line << ",";
    oss << R"("character":)" << item.selectionRange.start.character << "},";
    oss << R"("end":{"line":)" << item.selectionRange.end.line << ",";
    oss << R"("character":)" << item.selectionRange.end.character << "}})";
    return oss.str();
}

std::string LspServer::handleCallHierarchyIncomingCalls(const std::string& params)
{
    // Parse the item from params (sent by client in the request)
    std::string name = jsonGet(params, "item.name");
    std::string uri = jsonGet(params, "item.uri");
    std::string detail = jsonGet(params, "item.detail");
    int kind = jsonGetInt(params, "item.kind", 12);

    // Parse range
    int rangeStartLine = jsonGetInt(params, "item.range.start.line");
    int rangeStartChar = jsonGetInt(params, "item.range.start.character");
    int rangeEndLine = jsonGetInt(params, "item.range.end.line");
    int rangeEndChar = jsonGetInt(params, "item.range.end.character");

    // Parse selectionRange
    int selStartLine = jsonGetInt(params, "item.selectionRange.start.line");
    int selStartChar = jsonGetInt(params, "item.selectionRange.start.character");
    int selEndLine = jsonGetInt(params, "item.selectionRange.end.line");
    int selEndChar = jsonGetInt(params, "item.selectionRange.end.character");

    CallHierarchyItem item;
    item.name = name;
    item.kind = kind;
    item.detail = detail;
    item.uri = uri;
    item.range = {{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};
    item.selectionRange = {{selStartLine, selStartChar}, {selEndLine, selEndChar}};

    auto calls = getCallHierarchyIncomingCalls(item);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < calls.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& call = calls[i];
        oss << R"({"from":{"name":")" << jsonEscape(call.from.name) << "\",";
        oss << R"("kind":)" << call.from.kind << ",";
        oss << R"("detail":")" << jsonEscape(call.from.detail) << "\",";
        oss << R"("uri":")" << jsonEscape(call.from.uri) << "\",";
        oss << R"("range":{"start":{"line":)" << call.from.range.start.line << ",";
        oss << R"("character":)" << call.from.range.start.character << "},";
        oss << R"("end":{"line":)" << call.from.range.end.line << ",";
        oss << R"("character":)" << call.from.range.end.character << "},";
        oss << R"("selectionRange":{"start":{"line":)" << call.from.selectionRange.start.line << ",";
        oss << R"("character":)" << call.from.selectionRange.start.character << "},";
        oss << R"("end":{"line":)" << call.from.selectionRange.end.line << ",";
        oss << R"("character":)" << call.from.selectionRange.end.character << "}},";
        oss << R"("fromRanges":[)";
        for (size_t j = 0; j < call.fromRanges.size(); ++j) {
            if (j > 0)
                oss << ",";
            oss << R"({"start":{"line":)" << call.fromRanges[j].start.line;
            oss << R"(,"character":)" << call.fromRanges[j].start.character << "},";
            oss << R"("end":{"line":)" << call.fromRanges[j].end.line;
            oss << R"(,"character":)" << call.fromRanges[j].end.character << "}}";
        }
        oss << "]}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleCallHierarchyOutgoingCalls(const std::string& params)
{
    // Parse the item from params
    std::string name = jsonGet(params, "item.name");
    std::string uri = jsonGet(params, "item.uri");
    std::string detail = jsonGet(params, "item.detail");
    int kind = jsonGetInt(params, "item.kind", 12);

    int rangeStartLine = jsonGetInt(params, "item.range.start.line");
    int rangeStartChar = jsonGetInt(params, "item.range.start.character");
    int rangeEndLine = jsonGetInt(params, "item.range.end.line");
    int rangeEndChar = jsonGetInt(params, "item.range.end.character");

    int selStartLine = jsonGetInt(params, "item.selectionRange.start.line");
    int selStartChar = jsonGetInt(params, "item.selectionRange.start.character");
    int selEndLine = jsonGetInt(params, "item.selectionRange.end.line");
    int selEndChar = jsonGetInt(params, "item.selectionRange.end.character");

    CallHierarchyItem item;
    item.name = name;
    item.kind = kind;
    item.detail = detail;
    item.uri = uri;
    item.range = {{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};
    item.selectionRange = {{selStartLine, selStartChar}, {selEndLine, selEndChar}};

    auto calls = getCallHierarchyOutgoingCalls(item);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < calls.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& call = calls[i];
        oss << R"({"to":{"name":")" << jsonEscape(call.to.name) << "\",";
        oss << R"("kind":)" << call.to.kind << ",";
        oss << R"("detail":")" << jsonEscape(call.to.detail) << "\",";
        oss << R"("uri":")" << jsonEscape(call.to.uri) << "\",";
        oss << R"("range":{"start":{"line":)" << call.to.range.start.line << ",";
        oss << R"("character":)" << call.to.range.start.character << "},";
        oss << R"("end":{"line":)" << call.to.range.end.line << ",";
        oss << R"("character":)" << call.to.range.end.character << "},";
        oss << R"("selectionRange":{"start":{"line":)" << call.to.selectionRange.start.line << ",";
        oss << R"("character":)" << call.to.selectionRange.start.character << "},";
        oss << R"("end":{"line":)" << call.to.selectionRange.end.line << ",";
        oss << R"("character":)" << call.to.selectionRange.end.character << "}},";
        oss << R"("fromRanges":[)";
        for (size_t j = 0; j < call.fromRanges.size(); ++j) {
            if (j > 0)
                oss << ",";
            oss << R"({"start":{"line":)" << call.fromRanges[j].start.line;
            oss << R"(,"character":)" << call.fromRanges[j].start.character << "},";
            oss << R"("end":{"line":)" << call.fromRanges[j].end.line;
            oss << R"(,"character":)" << call.fromRanges[j].end.character << "}}";
        }
        oss << "]}";
    }
    oss << "]";
    return oss.str();
}

LspServer::CallHierarchyItem LspServer::getPrepareCallHierarchy(const std::string& uri, Position pos)
{
    CallHierarchyItem result;
    auto* doc = getDocument(uri);
    if (!doc) return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty()) return result;

    // Check if word is a function definition by scanning for `def <word>` pattern
    std::string searchStr = "def " + word;
    size_t searchPos = 0;
    while ((searchPos = doc->text.find(searchStr, searchPos)) != std::string::npos) {
        // Check word boundary after `def `
        size_t nameStart = searchPos + 4;
        size_t nameEnd = nameStart + word.size();
        if (nameEnd <= doc->text.size()) {
            std::string candidate = doc->text.substr(nameStart, word.size());
            if (candidate == word) {
                // Check that it's a whole word (followed by '(' or whitespace)
                if (nameEnd < doc->text.size() &&
                    (doc->text[nameEnd] == '(' || doc->text[nameEnd] == ' ' || doc->text[nameEnd] == '\n' ||
                     doc->text[nameEnd] == '\t' || doc->text[nameEnd] == '\r' || doc->text[nameEnd] == '{')) {
                    // Found the definition
                    Position defStart = doc->offsetToPosition(searchPos);
                    Position defEnd = doc->offsetToPosition(nameEnd);
                    result.name = word;
                    result.kind = 12; // Function
                    result.detail = "function";
                    result.uri = uri;
                    result.range = {defStart, defEnd};
                    result.selectionRange = {defStart, defEnd};
                    return result;
                }
            }
        }
        searchPos += searchStr.size();
    }

    return result;
}

std::vector<LspServer::CallHierarchyIncomingCall> LspServer::getCallHierarchyIncomingCalls(
    const CallHierarchyItem& item)
{
    std::vector<CallHierarchyIncomingCall> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty()) return result;

    if (item.name.empty()) return result;

    // Build a map of function name -> function definition location
    // (for the "from" field of incoming calls)
    std::map<std::string, CallHierarchyItem> funcDefs;
    size_t pos = 0;
    while ((pos = doc->text.find("def ", pos)) != std::string::npos) {
        size_t nameStart = pos + 4;
        size_t nameEnd = nameStart;
        while (nameEnd < doc->text.size() && (isalnum(doc->text[nameEnd]) || doc->text[nameEnd] == '_'))
            nameEnd++;

        std::string funcName = doc->text.substr(nameStart, nameEnd - nameStart);
        if (!funcName.empty() && funcName != item.name) {
            CallHierarchyItem ci;
            ci.name = funcName;
            ci.kind = 12;
            ci.detail = "function";
            ci.uri = item.uri;
            Position start = doc->offsetToPosition(pos);
            Position end = doc->offsetToPosition(nameEnd);
            ci.range = {start, end};
            ci.selectionRange = {start, end};
            funcDefs[funcName] = ci;
        }
        pos = nameEnd;
    }

    // Find all calls to item.name in the document
    // Skip the definition itself
    std::string defPattern = "def " + item.name;
    size_t defPos = doc->text.find(defPattern);
    size_t defEnd = defPos != std::string::npos ? defPos + defPattern.size() : 0;

    std::map<std::string, std::vector<Range>> callers; // caller func name -> ranges

    size_t searchPos = 0;
    while ((searchPos = doc->text.find(item.name, searchPos)) != std::string::npos) {
        // Skip the definition line
        if (defPos != std::string::npos && searchPos >= defPos - 4 &&
            searchPos < defEnd)
        {
            searchPos += item.name.size();
            continue;
        }

        // Check word boundaries
        bool leftOk = searchPos == 0 ||
                      (!isalnum(doc->text[searchPos - 1]) && doc->text[searchPos - 1] != '_');
        bool rightOk = (searchPos + item.name.size() >= doc->text.size()) ||
                       (!isalnum(doc->text[searchPos + item.name.size()]) &&
                        doc->text[searchPos + item.name.size()] != '_');
        if (leftOk && rightOk) {
            Range callRange = {doc->offsetToPosition(searchPos),
                               doc->offsetToPosition(searchPos + item.name.size())};

            // Find which function this call is inside
            // Scan backward to find the nearest `def ... (` before this position
            int callLine = callRange.start.line;
            // Simple approach: scan backward to find the nearest `def` before this line
            size_t backtrack = searchPos;
            std::string callerName;
            while (backtrack > 0) {
                // Find `def ` going backward
                size_t prevDef = doc->text.rfind("def ", backtrack);
                if (prevDef == std::string::npos || prevDef < 10) break;
                size_t cnStart = prevDef + 4;
                size_t cnEnd = cnStart;
                while (cnEnd < doc->text.size() &&
                       (isalnum(doc->text[cnEnd]) || doc->text[cnEnd] == '_'))
                    cnEnd++;
                callerName = doc->text.substr(cnStart, cnEnd - cnStart);
                if (!callerName.empty()) {
                    // Check this def isn't the target itself
                    if (callerName != item.name) {
                        break;
                    }
                }
                if (backtrack < 10) break;
                backtrack = prevDef - 1;
            }

            if (!callerName.empty() && callerName != item.name) {
                callers[callerName].push_back(callRange);
            }
        }
        searchPos += item.name.size();
    }

    // Build result
    for (auto& [callerName, ranges] : callers) {
        CallHierarchyIncomingCall call;
        if (funcDefs.count(callerName)) {
            call.from = funcDefs[callerName];
        } else {
            call.from.name = callerName;
            call.from.kind = 12;
            call.from.detail = "function";
            call.from.uri = item.uri;
        }
        call.fromRanges = ranges;
        result.push_back(call);
    }

    return result;
}

std::vector<LspServer::CallHierarchyOutgoingCall> LspServer::getCallHierarchyOutgoingCalls(
    const CallHierarchyItem& item)
{
    std::vector<CallHierarchyOutgoingCall> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty()) return result;

    // Find the definition range of this function
    std::string defPattern = "def " + item.name;
    size_t defPos = doc->text.find(defPattern);
    if (defPos == std::string::npos) return result;

    // Find the body start -- scan for `{` after `def name`
    size_t bodyStart = defPos + defPattern.size();
    size_t bracePos = doc->text.find('{', bodyStart);
    if (bracePos == std::string::npos) return result;

    // Find matching closing brace (simple depth count)
    size_t bodyEnd = bracePos + 1;
    int depth = 1;
    while (bodyEnd < doc->text.size() && depth > 0) {
        if (doc->text[bodyEnd] == '{') depth++;
        else if (doc->text[bodyEnd] == '}') depth--;
        bodyEnd++;
    }

    // Build a map of function name -> definition item for all functions
    std::map<std::string, CallHierarchyItem> funcDefs;
    // Also track def positions to exclude them from call detection
    std::map<std::string, Position> defPositions;

    size_t scanPos = 0;
    while ((scanPos = doc->text.find("def ", scanPos)) != std::string::npos) {
        size_t ns = scanPos + 4;
        size_t ne = ns;
        while (ne < doc->text.size() && (isalnum(doc->text[ne]) || doc->text[ne] == '_'))
            ne++;
        std::string fn = doc->text.substr(ns, ne - ns);
        if (!fn.empty()) {
            CallHierarchyItem ci;
            ci.name = fn;
            ci.kind = 12;
            ci.detail = "function";
            ci.uri = item.uri;
            Position s = doc->offsetToPosition(scanPos);
            Position e = doc->offsetToPosition(ne);
            ci.range = {s, e};
            ci.selectionRange = {s, e};
            funcDefs[fn] = ci;
            defPositions[fn] = s;
        }
        scanPos = ne;
    }

    // Scan the body for function calls (identifier followed by '(')
    std::map<std::string, std::vector<Range>> callees;

    size_t i = bracePos;
    while (i < bodyEnd) {
        // Skip string literals
        if (doc->text[i] == '"') {
            i++;
            while (i < doc->text.size() && doc->text[i] != '"') {
                if (doc->text[i] == '\\') i++;
                i++;
            }
            i++;
            continue;
        }

        // Check for identifier pattern: word followed by '('
        if (isalpha(doc->text[i]) || doc->text[i] == '_') {
            size_t ws = i;
            while (i < doc->text.size() && (isalnum(doc->text[i]) || doc->text[i] == '_'))
                i++;
            std::string callee = doc->text.substr(ws, i - ws);

            // After identifier, check for '(' — indicates a function call
            size_t j = i;
            while (j < doc->text.size() && (doc->text[j] == ' ' || doc->text[j] == '\t'))
                j++;
            if (j < doc->text.size() && doc->text[j] == '(') {
                // It's a function call
                if (!callee.empty() && callee != item.name && funcDefs.count(callee)) {
                    Range callRange = {doc->offsetToPosition(ws), doc->offsetToPosition(i)};
                    callees[callee].push_back(callRange);
                }
            }
        } else {
            i++;
        }
    }

    // Build result
    for (auto& [calleeName, ranges] : callees) {
        CallHierarchyOutgoingCall call;
        if (funcDefs.count(calleeName)) {
            call.to = funcDefs[calleeName];
        } else {
            call.to.name = calleeName;
            call.to.kind = 12;
            call.to.detail = "function";
            call.to.uri = item.uri;
        }
        call.fromRanges = ranges;
        result.push_back(call);
    }

    return result;
}

// ============================================================================
// Type Hierarchy
// ============================================================================

std::string LspServer::handleTextDocumentPrepareTypeHierarchy(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto item = getPrepareTypeHierarchy(uri, {line, col});
    if (item.name.empty())
        return "null";

    std::ostringstream oss;
    oss << R"({"name":")" << jsonEscape(item.name) << "\",";
    oss << R"("kind":)" << item.kind << ",";
    oss << R"("detail":")" << jsonEscape(item.detail) << "\",";
    oss << R"("uri":")" << jsonEscape(item.uri) << "\",";
    oss << R"("range":{"start":{"line":)" << item.range.start.line << ",";
    oss << R"("character":)" << item.range.start.character << "},";
    oss << R"("end":{"line":)" << item.range.end.line << ",";
    oss << R"("character":)" << item.range.end.character << "},";
    oss << R"("selectionRange":{"start":{"line":)" << item.selectionRange.start.line << ",";
    oss << R"("character":)" << item.selectionRange.start.character << "},";
    oss << R"("end":{"line":)" << item.selectionRange.end.line << ",";
    oss << R"("character":)" << item.selectionRange.end.character << "}})";
    return oss.str();
}

std::string LspServer::handleTypeHierarchySupertypes(const std::string& params)
{
    std::string name = jsonGet(params, "item.name");
    std::string uri = jsonGet(params, "item.uri");
    std::string detail = jsonGet(params, "item.detail");
    int kind = jsonGetInt(params, "item.kind", 22);

    int rangeStartLine = jsonGetInt(params, "item.range.start.line");
    int rangeStartChar = jsonGetInt(params, "item.range.start.character");
    int rangeEndLine = jsonGetInt(params, "item.range.end.line");
    int rangeEndChar = jsonGetInt(params, "item.range.end.character");

    int selStartLine = jsonGetInt(params, "item.selectionRange.start.line");
    int selStartChar = jsonGetInt(params, "item.selectionRange.start.character");
    int selEndLine = jsonGetInt(params, "item.selectionRange.end.line");
    int selEndChar = jsonGetInt(params, "item.selectionRange.end.character");

    TypeHierarchyItem item;
    item.name = name;
    item.kind = kind;
    item.detail = detail;
    item.uri = uri;
    item.range = {{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};
    item.selectionRange = {{selStartLine, selStartChar}, {selEndLine, selEndChar}};

    auto supertypes = getTypeHierarchySupertypes(item);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < supertypes.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& t = supertypes[i];
        oss << R"({"name":")" << jsonEscape(t.name) << "\",";
        oss << R"("kind":)" << t.kind << ",";
        oss << R"("detail":")" << jsonEscape(t.detail) << "\",";
        oss << R"("uri":")" << jsonEscape(t.uri) << "\",";
        oss << R"("range":{"start":{"line":)" << t.range.start.line << ",";
        oss << R"("character":)" << t.range.start.character << "},";
        oss << R"("end":{"line":)" << t.range.end.line << ",";
        oss << R"("character":)" << t.range.end.character << "},";
        oss << R"("selectionRange":{"start":{"line":)" << t.selectionRange.start.line << ",";
        oss << R"("character":)" << t.selectionRange.start.character << "},";
        oss << R"("end":{"line":)" << t.selectionRange.end.line << ",";
        oss << R"("character":)" << t.selectionRange.end.character << "}})";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTypeHierarchySubtypes(const std::string& params)
{
    std::string name = jsonGet(params, "item.name");
    std::string uri = jsonGet(params, "item.uri");
    std::string detail = jsonGet(params, "item.detail");
    int kind = jsonGetInt(params, "item.kind", 22);

    int rangeStartLine = jsonGetInt(params, "item.range.start.line");
    int rangeStartChar = jsonGetInt(params, "item.range.start.character");
    int rangeEndLine = jsonGetInt(params, "item.range.end.line");
    int rangeEndChar = jsonGetInt(params, "item.range.end.character");

    int selStartLine = jsonGetInt(params, "item.selectionRange.start.line");
    int selStartChar = jsonGetInt(params, "item.selectionRange.start.character");
    int selEndLine = jsonGetInt(params, "item.selectionRange.end.line");
    int selEndChar = jsonGetInt(params, "item.selectionRange.end.character");

    TypeHierarchyItem item;
    item.name = name;
    item.kind = kind;
    item.detail = detail;
    item.uri = uri;
    item.range = {{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};
    item.selectionRange = {{selStartLine, selStartChar}, {selEndLine, selEndChar}};

    auto subtypes = getTypeHierarchySubtypes(item);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < subtypes.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& t = subtypes[i];
        oss << R"({"name":")" << jsonEscape(t.name) << "\",";
        oss << R"("kind":)" << t.kind << ",";
        oss << R"("detail":")" << jsonEscape(t.detail) << "\",";
        oss << R"("uri":")" << jsonEscape(t.uri) << "\",";
        oss << R"("range":{"start":{"line":)" << t.range.start.line << ",";
        oss << R"("character":)" << t.range.start.character << "},";
        oss << R"("end":{"line":)" << t.range.end.line << ",";
        oss << R"("character":)" << t.range.end.character << "},";
        oss << R"("selectionRange":{"start":{"line":)" << t.selectionRange.start.line << ",";
        oss << R"("character":)" << t.selectionRange.start.character << "},";
        oss << R"("end":{"line":)" << t.selectionRange.end.line << ",";
        oss << R"("character":)" << t.selectionRange.end.character << "}})";
    }
    oss << "]";
    return oss.str();
}

// Helper: scan document for type definitions and return items
static std::vector<LspServer::TypeHierarchyItem> scanTypeDefs(
    Flux::Tooling::LspServer* server,
    const std::string& uri)
{
    std::vector<LspServer::TypeHierarchyItem> result;
    auto* doc = server->getDocument(uri);
    if (!doc) return result;

    // Scan for `struct <name>`, `class <name>`, `enum <name>`, `trait <name>`
    struct KeywordInfo { std::string keyword; int kind; std::string detail; };
    KeywordInfo keywords[] = {
        {"struct ", 22, "struct"},
        {"class ", 5, "class"},
        {"enum ", 24, "enum"},
        {"trait ", 6, "trait"},
    };

    for (auto& ki : keywords) {
        size_t pos = 0;
        while ((pos = doc->text.find(ki.keyword, pos)) != std::string::npos) {
            size_t nameStart = pos + ki.keyword.size();
            size_t nameEnd = nameStart;
            while (nameEnd < doc->text.size() && (isalnum(doc->text[nameEnd]) || doc->text[nameEnd] == '_'))
                nameEnd++;
            std::string name = doc->text.substr(nameStart, nameEnd - nameStart);
            if (!name.empty()) {
                LspServer::TypeHierarchyItem item;
                item.name = name;
                item.kind = ki.kind;
                item.detail = ki.detail;
                item.uri = uri;
                Position s = doc->offsetToPosition(pos);
                Position e = doc->offsetToPosition(nameEnd);
                item.range = {s, e};
                item.selectionRange = {s, e};
                result.push_back(item);
            }
            pos = nameEnd;
        }
    }

    return result;
}

LspServer::TypeHierarchyItem LspServer::getPrepareTypeHierarchy(const std::string& uri, Position pos)
{
    TypeHierarchyItem result;
    auto* doc = getDocument(uri);
    if (!doc) return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty()) return result;

    // Check if word is a type definition
    auto types = scanTypeDefs(this, uri);
    for (auto& t : types) {
        if (t.name == word) {
            return t;
        }
    }

    return result;
}

std::vector<LspServer::TypeHierarchyItem> LspServer::getTypeHierarchySupertypes(
    const TypeHierarchyItem& item)
{
    std::vector<TypeHierarchyItem> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty()) return result;

    // Scan all type definitions to build a mapping
    auto allTypes = scanTypeDefs(this, item.uri);

    // 1. For a class: check if it has a parent class (class Child : Parent)
    if (item.kind == 5) { // Class
        std::string classPattern = "class " + item.name;
        size_t defPos = doc->text.find(classPattern);
        if (defPos != std::string::npos) {
            // Find colon after class name (before '{')
            size_t afterName = defPos + classPattern.size();
            size_t bracePos = doc->text.find('{', afterName);
            size_t colonPos = doc->text.find(':', afterName);
            if (colonPos != std::string::npos &&
                (bracePos == std::string::npos || colonPos < bracePos)) {
                // Extract parent name after ':'
                size_t ps = colonPos + 1;
                while (ps < doc->text.size() && doc->text[ps] == ' ') ps++;
                size_t pe = ps;
                while (pe < doc->text.size() && (isalnum(doc->text[pe]) || doc->text[pe] == '_'))
                    pe++;
                std::string parentName = doc->text.substr(ps, pe - ps);
                if (!parentName.empty()) {
                    for (auto& t : allTypes) {
                        if (t.name == parentName) {
                            result.push_back(t);
                            break;
                        }
                    }
                    // If not found as a known type, still return it as an item
                    if (result.empty()) {
                        TypeHierarchyItem parent;
                        parent.name = parentName;
                        parent.kind = 5;
                        parent.detail = "class";
                        parent.uri = item.uri;
                        result.push_back(parent);
                    }
                }
            }
        }
    }

    // 2. For any type: find traits it implements
    // Check for `impl TraitName for TypeName` patterns
    std::string implForPattern = " for " + item.name;
    size_t implPos = 0;
    while ((implPos = doc->text.find("impl ", implPos)) != std::string::npos) {
        size_t forPos = doc->text.find(" for ", implPos);
        if (forPos == std::string::npos) { implPos += 5; continue; }
        size_t typePos = forPos + 5;
        size_t typeEnd = typePos + item.name.size();
        if (typeEnd <= doc->text.size() && doc->text.substr(typePos, item.name.size()) == item.name) {
            // Check word boundary after type name
            if (typeEnd >= doc->text.size() || !isalnum(doc->text[typeEnd])) {
                // Extract trait name
                size_t traitStart = implPos + 5;
                std::string traitName;
                size_t ts = traitStart;
                while (ts < doc->text.size() && (isalnum(doc->text[ts]) || doc->text[ts] == '_')) {
                    traitName += doc->text[ts++];
                }
                if (!traitName.empty() && traitName != item.name) {
                    bool alreadyAdded = false;
                    for (auto& r : result) {
                        if (r.name == traitName) { alreadyAdded = true; break; }
                    }
                    if (!alreadyAdded) {
                        // Find the trait in allTypes
                        bool found = false;
                        for (auto& t : allTypes) {
                            if (t.name == traitName) {
                                result.push_back(t);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            TypeHierarchyItem trait;
                            trait.name = traitName;
                            trait.kind = 6;
                            trait.detail = "trait";
                            trait.uri = item.uri;
                            result.push_back(trait);
                        }
                    }
                }
            }
        }
        implPos = typeEnd;
    }

    return result;
}

std::vector<LspServer::TypeHierarchyItem> LspServer::getTypeHierarchySubtypes(
    const TypeHierarchyItem& item)
{
    std::vector<TypeHierarchyItem> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty()) return result;

    auto allTypes = scanTypeDefs(this, item.uri);

    // 1. For a class: find classes that extend it (class Child : ParentName)
    if (item.kind == 5) {
        // Scan for `class <name> : item.name` pattern
        size_t pos = 0;
        while ((pos = doc->text.find("class ", pos)) != std::string::npos) {
            size_t nameStart = pos + 6;
            size_t nameEnd = nameStart;
            while (nameEnd < doc->text.size() && (isalnum(doc->text[nameEnd]) || doc->text[nameEnd] == '_'))
                nameEnd++;
            std::string childName = doc->text.substr(nameStart, nameEnd - nameStart);
            if (!childName.empty() && childName != item.name) {
                // Find colon and parent name
                size_t bracePos = doc->text.find('{', nameEnd);
                size_t colonPos = doc->text.find(':', nameEnd);
                if (colonPos != std::string::npos &&
                    (bracePos == std::string::npos || colonPos < bracePos)) {
                    size_t ps = colonPos + 1;
                    while (ps < doc->text.size() && doc->text[ps] == ' ') ps++;
                    size_t pe = ps;
                    while (pe < doc->text.size() && (isalnum(doc->text[pe]) || doc->text[pe] == '_'))
                        pe++;
                    std::string parentName = doc->text.substr(ps, pe - ps);
                    if (parentName == item.name) {
                        bool found = false;
                        for (auto& t : allTypes) {
                            if (t.name == childName) {
                                result.push_back(t);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            TypeHierarchyItem child;
                            child.name = childName;
                            child.kind = 5;
                            child.detail = "class";
                            child.uri = item.uri;
                            result.push_back(child);
                        }
                    }
                }
            }
            pos = nameEnd;
        }
    }

    // 2. For a trait: find types that implement it (impl TraitName for TypeName)
    if (item.kind == 6) { // Trait/Interface
        std::string implPattern = "impl " + item.name + " for ";
        size_t pos = 0;
        while ((pos = doc->text.find(implPattern, pos)) != std::string::npos) {
            size_t typeStart = pos + implPattern.size();
            size_t typeEnd = typeStart;
            while (typeEnd < doc->text.size() && (isalnum(doc->text[typeEnd]) || doc->text[typeEnd] == '_'))
                typeEnd++;
            std::string typeName = doc->text.substr(typeStart, typeEnd - typeStart);
            if (!typeName.empty()) {
                bool alreadyAdded = false;
                for (auto& r : result) {
                    if (r.name == typeName) { alreadyAdded = true; break; }
                }
                if (!alreadyAdded) {
                    bool found = false;
                    for (auto& t : allTypes) {
                        if (t.name == typeName) {
                            result.push_back(t);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        TypeHierarchyItem subtype;
                        subtype.name = typeName;
                        subtype.kind = 22;
                        subtype.detail = "struct";
                        subtype.uri = item.uri;
                        result.push_back(subtype);
                    }
                }
            }
            pos = typeEnd;
        }
    }

    return result;
}

// ============================================================================
// Linked Editing Range
// ============================================================================

std::string LspServer::handleTextDocumentLinkedEditingRange(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    int line = jsonGetInt(params, "position.line");
    int col = jsonGetInt(params, "position.character");

    auto result = getLinkedEditingRanges(uri, {line, col});
    if (result.ranges.empty())
        return "null";

    std::ostringstream oss;
    oss << R"({"ranges":[)";
    for (size_t i = 0; i < result.ranges.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"start":{"line":)" << result.ranges[i].start.line;
        oss << R"(,"character":)" << result.ranges[i].start.character << "},";
        oss << R"("end":{"line":)" << result.ranges[i].end.line;
        oss << R"(,"character":)" << result.ranges[i].end.character << "}";
    }
    oss << R"(],"wordPattern":")" << jsonEscape(result.wordPattern) << "\"}";
    return oss.str();
}

LspServer::LinkedEditingRanges LspServer::getLinkedEditingRanges(const std::string& uri, Position pos)
{
    LinkedEditingRanges result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Find all whole-word occurrences of this identifier
    size_t searchPos = 0;
    while ((searchPos = doc->text.find(word, searchPos)) != std::string::npos) {
        // Check word boundaries
        bool leftOk = searchPos == 0 ||
                      (!isalnum(doc->text[searchPos - 1]) && doc->text[searchPos - 1] != '_');
        bool rightOk = (searchPos + word.size() >= doc->text.size()) ||
                       (!isalnum(doc->text[searchPos + word.size()]) &&
                        doc->text[searchPos + word.size()] != '_');
        if (leftOk && rightOk) {
            result.ranges.push_back({doc->offsetToPosition(searchPos),
                                     doc->offsetToPosition(searchPos + word.size())});
        }
        searchPos += word.size();
    }

    // Only return if there are multiple occurrences (linked editing makes sense with 2+)
    if (result.ranges.size() < 2) {
        result.ranges.clear();
        return result;
    }

    // Word pattern: match identifiers of this form
    result.wordPattern = "[a-zA-Z_][a-zA-Z0-9_]*";
    return result;
}

// ============================================================================
// Signature Help
// ============================================================================

LspServer::SignatureHelpResult LspServer::getSignatureHelp(const std::string& uri, Position pos)
{
    SignatureHelpResult result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    // Look backward from cursor to find the function being called
    size_t offset = doc->positionToOffset(pos);
    if (offset == 0 || offset > doc->text.size())
        return result;

    // Scan backward to find function name before '('
    size_t parenPos = doc->text.rfind('(', offset - 1);
    if (parenPos == std::string::npos || parenPos == 0)
        return result;

    // Extract function name
    size_t nameEnd = parenPos;
    while (nameEnd > 0 && (doc->text[nameEnd - 1] == ' ' || doc->text[nameEnd - 1] == '\t'))
        nameEnd--;
    size_t nameStart = nameEnd;
    while (nameStart > 0 && (std::isalnum(doc->text[nameStart - 1]) || doc->text[nameStart - 1] == '_'))
        nameStart--;

    std::string funcName = doc->text.substr(nameStart, nameEnd - nameStart);

    // Count arguments
    int argCount = 0;
    int parenDepth = 0;
    for (size_t i = nameEnd; i < offset; ++i) {
        if (doc->text[i] == '(')
            parenDepth++;
        else if (doc->text[i] == ')')
            parenDepth--;
        else if (doc->text[i] == ',' && parenDepth == 1)
            argCount++;
    }

    // Look up function signature
    auto builtins = getBuiltinFunctionCompletions();
    for (auto& b : builtins) {
        if (b.label == funcName) {
            SignatureInfo sig;
            sig.label = b.detail;
            sig.documentation = b.documentation;

            // Parse parameters from detail
            size_t parenStart = b.detail.find('(');
            size_t parenEnd = b.detail.find(')');
            if (parenStart != std::string::npos && parenEnd != std::string::npos) {
                std::string params = b.detail.substr(parenStart + 1, parenEnd - parenStart - 1);
                size_t start = 0;
                while (start < params.size()) {
                    size_t comma = params.find(',', start);
                    if (comma == std::string::npos)
                        comma = params.size();
                    std::string param = params.substr(start, comma - start);
                    // Trim
                    while (!param.empty() && param[0] == ' ')
                        param.erase(0, 1);
                    while (!param.empty() && param.back() == ' ')
                        param.pop_back();
                    if (!param.empty()) {
                        sig.parameters.push_back({param, ""});
                    }
                    start = comma + 1;
                }
            }

            result.signatures.push_back(sig);
            result.activeParameter = std::min(argCount, static_cast<int>(sig.parameters.size()) - 1);
            return result;
        }
    }

    return result;
}

// ============================================================================
// JSON Helpers
// ============================================================================

std::string LspServer::jsonGet(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.rfind(searchKey); // Use rfind for nested keys
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return "";

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && json[valueStart] == ' ')
        valueStart++;

    if (valueStart >= json.size())
        return "";

    if (json[valueStart] == '"') {
        size_t strStart = valueStart + 1;
        std::string result;
        for (size_t i = strStart; i < json.size(); ++i) {
            if (json[i] == '"' && json[i - 1] != '\\')
                break;
            result += json[i];
        }
        return result;
    } else if (json.substr(valueStart, 4) == "null") {
        return "";
    } else if (json.substr(valueStart, 4) == "true") {
        return "true";
    } else if (json.substr(valueStart, 5) == "false") {
        return "false";
    }

    // Try to extract nested object
    if (json[valueStart] == '{') {
        int depth = 0;
        size_t objStart = valueStart;
        for (size_t i = valueStart; i < json.size(); ++i) {
            if (json[i] == '{')
                depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0)
                    return json.substr(objStart, i - objStart + 1);
            }
        }
    }

    return "";
}

std::string LspServer::jsonGetNested(const std::string& json, const std::string& key1, const std::string& key2)
{
    // Find key1, then within that object find key2
    std::string searchKey = "\"" + key1 + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return "";

    size_t objStart = colonPos + 1;
    while (objStart < json.size() && json[objStart] == ' ')
        objStart++;

    if (objStart >= json.size() || json[objStart] != '{') {
        // Try direct key2 lookup (flat structure)
        return jsonGet(json, key2);
    }

    int depth = 0;
    size_t objEnd = objStart;
    for (size_t i = objStart; i < json.size(); ++i) {
        if (json[i] == '{')
            depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) {
                objEnd = i;
                break;
            }
        }
    }

    std::string inner = json.substr(objStart, objEnd - objStart + 1);
    return jsonGet(inner, key2);
}

int LspServer::jsonGetInt(const std::string& json, const std::string& key, int defaultVal)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return defaultVal;

    size_t colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return defaultVal;

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && json[valueStart] == ' ')
        valueStart++;

    try {
        return std::stoi(json.substr(valueStart));
    } catch (...) {
        return defaultVal;
    }
}

std::string LspServer::jsonEscape(const std::string& s)
{
    std::string result;
    for (char c : s) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
        }
    }
    return result;
}

std::string LspServer::makeResponse(int id, const std::string& result)
{
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

std::string LspServer::makeErrorResponse(int id, int code, const std::string& message)
{
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"error\":{\"code\":" + std::to_string(code) +
           ",\"message\":\"" + jsonEscape(message) + "\"}}";
}

std::string LspServer::makeNotification(const std::string& method, const std::string& params)
{
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params + "}";
}

void LspServer::publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics)
{
    std::ostringstream oss;
    oss << R"({"uri":")" << jsonEscape(uri) << R"(","diagnostics":[)";
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& d = diagnostics[i];
        oss << "{";
        oss << R"("range":{"start":{"line":)" << d.range.start.line << ",";
        oss << R"("character":)" << d.range.start.character << "},";
        oss << R"("end":{"line":)" << d.range.end.line << ",";
        oss << R"("character":)" << d.range.end.character << "},";
        oss << R"("severity":)" << static_cast<int>(d.severity) << ",";
        oss << R"("source":")" << jsonEscape(d.source) << "\",";
        oss << R"("message":")" << jsonEscape(d.message) << "\"}";
    }
    oss << "]}";
    std::string notification = makeNotification("textDocument/publishDiagnostics", oss.str());
    std::cout << "Content-Length: " << notification.size() << "\r\n\r\n" << notification;
    std::cout.flush();
}

// ============================================================================
// References Provider
// ============================================================================

std::vector<Location> LspServer::getReferences(const std::string& uri, Position pos)
{
    std::vector<Location> refs;
    auto* doc = getDocument(uri);
    if (!doc)
        return refs;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return refs;

    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);

    for (auto& sym : symbols) {
        if (sym.name == word) {
            Location loc;
            loc.uri = sym.uri;
            loc.range = sym.range;
            refs.push_back(loc);
        }
    }

    return refs;
}

// ============================================================================
// Code Actions Provider
// ============================================================================

std::vector<LspServer::CodeAction> LspServer::getCodeActions(const std::string& uri, Position pos,
                                                             const std::vector<Diagnostic>& diags)
{
    std::vector<CodeAction> actions;
    auto* doc = getDocument(uri);
    if (!doc)
        return actions;

    for (auto& d : diags) {
        // Only suggest actions for diagnostics at or near the requested position
        if (d.range.start.line != pos.line)
            continue;

        // Action: add missing semicolon
        if (d.message.find("expected ';'") != std::string::npos || d.message.find("expected ;") != std::string::npos) {
            CodeAction action;
            action.title = "Add missing ';'";
            action.kind = "quickfix";
            std::string line = doc->getLine(static_cast<size_t>(d.range.start.line));
            std::string newLine = line + ";";
            std::ostringstream oss;
            oss << R"({"changes":{")" << jsonEscape(uri) << R"(":[{)";
            oss << R"("range":{"start":{"line":)" << d.range.start.line << R"(,"character":0},)";
            oss << R"("end":{"line":)" << d.range.start.line << R"(,"character":)" << line.size() << R"(}},)";
            oss << R"("newText":")" << jsonEscape(newLine) << R"("}]}})";
            action.edit = oss.str();
            actions.push_back(action);
        }

        // Action: add missing closing paren
        if (d.message.find("expected ')'") != std::string::npos) {
            CodeAction action;
            action.title = "Add missing ')'";
            action.kind = "quickfix";
            std::string line = doc->getLine(static_cast<size_t>(d.range.start.line));
            std::ostringstream oss;
            oss << R"({"changes":{")" << jsonEscape(uri) << R"(":[{)";
            oss << R"("range":{"start":{"line":)" << d.range.start.line << R"(,"character":)" << line.size() << R"(},)";
            oss << R"("end":{"line":)" << d.range.start.line << R"(,"character":)" << line.size() << R"(}},)";
            oss << R"("newText":")" << ")" << R"("}]}})";
            action.edit = oss.str();
            actions.push_back(action);
        }
    }

    return actions;
}

// ============================================================================
// Document Formatting
// ============================================================================

std::vector<std::pair<Position, std::string>> LspServer::computeTextEdits(const std::string& uri,
                                                                          const std::string& tabStr)
{
    std::vector<std::pair<Position, std::string>> edits;
    auto* doc = getDocument(uri);
    if (!doc)
        return edits;

    size_t lineStart = 0;
    int lineNum = 0;
    int baseIndent = 0;

    for (size_t i = 0; i <= doc->text.size(); ++i) {
        if (i == doc->text.size() || doc->text[i] == '\n') {
            std::string originalLine = doc->text.substr(lineStart, i - lineStart);

            // Trim whitespace
            size_t firstNonSpace = originalLine.find_first_not_of(" \t");
            std::string trimmed = (firstNonSpace == std::string::npos) ? "" : originalLine.substr(firstNonSpace);

            // Count brackets in trimmed content to adjust indent for NEXT line
            int openBrackets = 0;
            int closeBrackets = 0;
            for (char c : trimmed) {
                if (c == '{' || c == '(' || c == '[')
                    openBrackets++;
                if (c == '}' || c == ')' || c == ']')
                    closeBrackets++;
            }

            // The indent level for this line is baseIndent
            int effectiveIndent = baseIndent - closeBrackets;
            if (effectiveIndent < 0)
                effectiveIndent = 0;

            // Build properly indented line
            std::string newLine;
            if (!trimmed.empty()) {
                for (int j = 0; j < effectiveIndent; j++)
                    newLine += tabStr;
                newLine += trimmed;
            }

            if (newLine != originalLine) {
                edits.push_back({{lineNum, 0}, newLine});
            }

            // Update baseIndent for next line
            baseIndent = effectiveIndent + openBrackets;
            if (baseIndent < 0)
                baseIndent = 0;

            lineStart = i + 1;
            lineNum++;
        }
    }

    return edits;
}

} // namespace Tooling
} // namespace Flux
