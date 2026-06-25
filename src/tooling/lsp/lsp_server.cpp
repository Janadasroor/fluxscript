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
#include <fstream>

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
    if (l < pos.line)
        return text.size();
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
    } else if (method == "textDocument/didSave") {
        handleTextDocumentDidSave(params);
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
    } else if (method == "textDocument/foldingRange") {
        return makeResponse(id, handleTextDocumentFoldingRange(params));
    } else if (method == "textDocument/documentLink") {
        return makeResponse(id, handleTextDocumentDocumentLink(params));
    } else if (method == "textDocument/selectionRange") {
        return makeResponse(id, handleTextDocumentSelectionRange(params));
    } else if (method == "textDocument/documentColor") {
        return makeResponse(id, handleTextDocumentDocumentColor(params));
    } else if (method == "textDocument/colorPresentation") {
        return makeResponse(id, handleTextDocumentColorPresentation(params));
    } else if (method == "textDocument/semanticTokens/full") {
        return makeResponse(id, handleTextDocumentSemanticTokensFull(params));
    } else if (method == "textDocument/semanticTokens/range") {
        return makeResponse(id, handleTextDocumentSemanticTokensRange(params));
    } else if (method == "textDocument/inlayHint") {
        return makeResponse(id, handleTextDocumentInlayHint(params));
    } else if (method == "textDocument/moniker") {
        return makeResponse(id, handleTextDocumentMoniker(params));
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
    std::vector<SymbolEntry> importedSymbols;
    auto* doc = getDocument(uri);
    if (!doc)
        return diags;

    // Parse the full script (handles def, extern, class/struct/enum/trait/impl, and top-level expressions)
    {
        Flux::Parser parser(doc->text);

        // Pre-scan source for enum/struct/class/trait declarations so the parser
        // recognizes Enum.Variant { fields } and TypeName { fields } syntax.
        {
            const std::string& src = doc->text;
            size_t scanPos = 0;
            while (scanPos < src.size()) {
                // Skip to start of a word
                while (scanPos < src.size() && !isalpha(src[scanPos]) && src[scanPos] != '_')
                    scanPos++;
                if (scanPos >= src.size())
                    break;
                // Read the word
                size_t wordStart = scanPos;
                while (scanPos < src.size() && (isalnum(src[scanPos]) || src[scanPos] == '_'))
                    scanPos++;
                std::string word = src.substr(wordStart, scanPos - wordStart);

                if (word == "enum" || word == "struct" || word == "class" || word == "trait") {
                    // Skip whitespace to find the type name
                    while (scanPos < src.size() && (src[scanPos] == ' ' || src[scanPos] == '\t'))
                        scanPos++;
                    size_t nameStart = scanPos;
                    while (scanPos < src.size() && (isalnum(src[scanPos]) || src[scanPos] == '_'))
                        scanPos++;
                    if (scanPos > nameStart) {
                        std::string name = src.substr(nameStart, scanPos - nameStart);
                        if (word == "enum")
                            parser.getKnownEnumTypeNames().insert(name);
                        else
                            parser.getKnownStructTypeNames().insert(name);
                    }
                }
            }

            // Also scan for Name.Variant { patterns to infer enum types from usage
            // (handles cross-module enums like Component.R { ... })
            scanPos = 0;
            while (scanPos < src.size()) {
                // Look for: word.word {
                size_t w1Start = scanPos;
                while (scanPos < src.size() && (isalnum(src[scanPos]) || src[scanPos] == '_'))
                    scanPos++;
                if (scanPos >= src.size() || src[scanPos] != '.') {
                    scanPos = w1Start + 1;
                    continue;
                }
                size_t dotPos = scanPos;
                scanPos++; // skip .
                if (scanPos >= src.size() || !(isalpha(src[scanPos]) || src[scanPos] == '_')) {
                    scanPos = dotPos + 1;
                    continue;
                }
                size_t w2Start = scanPos;
                while (scanPos < src.size() && (isalnum(src[scanPos]) || src[scanPos] == '_'))
                    scanPos++;
                // Skip whitespace, then check for {
                size_t peek = scanPos;
                while (peek < src.size() && (src[peek] == ' ' || src[peek] == '\t'))
                    peek++;
                if (peek < src.size() && src[peek] == '{') {
                    std::string parent = src.substr(w1Start, dotPos - w1Start);
                    std::string variant = src.substr(w2Start, scanPos - w2Start);
                    // Heuristic: if variant starts with uppercase, it's likely an enum variant
                    if (!variant.empty() && std::isupper(static_cast<unsigned char>(variant[0]))) {
                        parser.getKnownEnumTypeNames().insert(parent);
                    }
                }
            }
        }

        // Resolve cross-file imports: scan imported files for type/function/variable declarations
        {
            const std::string& src = doc->text;
            size_t scanPos = 0;
            while (scanPos < src.size()) {
                // Find "import" keyword
                size_t impPos = src.find("import ", scanPos);
                if (impPos == std::string::npos)
                    break;
                // Make sure it's at start of line (after whitespace)
                bool atLineStart = (impPos == 0) || src[impPos - 1] == '\n' || src[impPos - 1] == '\r';
                if (!atLineStart) {
                    scanPos = impPos + 7;
                    continue;
                }
                size_t nameStart = impPos + 7;
                while (nameStart < src.size() && (src[nameStart] == ' ' || src[nameStart] == '\t'))
                    nameStart++;
                size_t nameEnd = nameStart;
                while (nameEnd < src.size() && (isalnum(src[nameEnd]) || src[nameEnd] == '_'))
                    nameEnd++;
                if (nameEnd > nameStart) {
                    std::string moduleName = src.substr(nameStart, nameEnd - nameStart);
                    // Resolve module path using search paths (CWD, stdlib/, modules/, etc.)
                    std::vector<std::string> searchPaths = {
                        ".",
                        "modules",
                        "lib",
                        "stdlib",
                    };
                    const char* home = std::getenv("HOME");
                    if (home) {
                        searchPaths.push_back(std::string(home) + "/.flux/modules");
                        searchPaths.push_back(std::string(home) + "/.flux/stdlib");
                    }
                    const char* modPath = std::getenv("FLUX_MODULE_PATH");
                    if (modPath) {
                        std::istringstream mps(modPath);
                        std::string p;
                        while (std::getline(mps, p, ':'))
                            if (!p.empty()) searchPaths.push_back(p);
                    }
                    // Also try relative to the document's directory
                    std::string docDir;
                    size_t lastSlash = uri.rfind('/');
                    if (lastSlash != std::string::npos)
                        docDir = uri.substr(7, lastSlash - 7); // strip "file://"
                    if (!docDir.empty())
                        searchPaths.insert(searchPaths.begin(), docDir);

                    for (auto& sp : searchPaths) {
                        std::string filePath = sp + "/" + moduleName + ".flux";
                        std::ifstream ifs(filePath);
                        if (!ifs.good())
                            continue;
                        std::string importedSrc((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                        // Scan for declarations: types, functions, variables
                        size_t p2 = 0;
                        while (p2 < importedSrc.size()) {
                            while (p2 < importedSrc.size() && !isalpha(importedSrc[p2]) && importedSrc[p2] != '_')
                                p2++;
                            if (p2 >= importedSrc.size()) break;
                            size_t ws = p2;
                            while (p2 < importedSrc.size() && (isalnum(importedSrc[p2]) || importedSrc[p2] == '_'))
                                p2++;
                            std::string word = importedSrc.substr(ws, p2 - ws);

                            if (word == "enum" || word == "struct" || word == "class" || word == "trait") {
                                while (p2 < importedSrc.size() && (importedSrc[p2] == ' ' || importedSrc[p2] == '\t'))
                                    p2++;
                                size_t ns = p2;
                                while (p2 < importedSrc.size() && (isalnum(importedSrc[p2]) || importedSrc[p2] == '_'))
                                    p2++;
                                if (p2 > ns) {
                                    std::string name = importedSrc.substr(ns, p2 - ns);
                                    if (word == "enum")
                                        parser.getKnownEnumTypeNames().insert(name);
                                    else
                                        parser.getKnownStructTypeNames().insert(name);
                                }
                            } else if (word == "def") {
                                while (p2 < importedSrc.size() && (importedSrc[p2] == ' ' || importedSrc[p2] == '\t'))
                                    p2++;
                                size_t ns = p2;
                                while (p2 < importedSrc.size() && (isalnum(importedSrc[p2]) || importedSrc[p2] == '_'))
                                    p2++;
                                if (p2 > ns) {
                                    std::string fnName = importedSrc.substr(ns, p2 - ns);
                                    // Extract signature: read until newline
                                    size_t sigStart = p2;
                                    size_t sigEnd = importedSrc.find('\n', sigStart);
                                    if (sigEnd == std::string::npos) sigEnd = importedSrc.size();
                                    std::string sig = importedSrc.substr(sigStart, sigEnd - sigStart);
                                    // Trim
                                    while (!sig.empty() && sig[0] == ' ') sig.erase(0, 1);

                                    SymbolEntry se;
                                    se.name = fnName;
                                    se.kind = SymbolEntry::Function;
                                    se.typeStr = sig;
                                    se.uri = "file://" + filePath;
                                    se.range.start = {0, 0};
                                    se.range.end = {0, static_cast<int>(fnName.size())};
                                    importedSymbols.push_back(se);
                                }
                            } else if (word == "var" || word == "let") {
                                while (p2 < importedSrc.size() && (importedSrc[p2] == ' ' || importedSrc[p2] == '\t'))
                                    p2++;
                                size_t ns = p2;
                                while (p2 < importedSrc.size() && (isalnum(importedSrc[p2]) || importedSrc[p2] == '_'))
                                    p2++;
                                if (p2 > ns) {
                                    std::string varName = importedSrc.substr(ns, p2 - ns);
                                    SymbolEntry se;
                                    se.name = varName;
                                    se.kind = SymbolEntry::Variable;
                                    se.uri = "file://" + filePath;
                                    se.range.start = {0, 0};
                                    se.range.end = {0, static_cast<int>(varName.size())};
                                    importedSymbols.push_back(se);
                                }
                            }
                        }
                        break; // found the file, stop searching
                    }
                }
                scanPos = nameEnd;
            }
        }

        while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof)) {
            bool parsed = true;
            if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_def)) {
                parsed = parser.ParseDefinition() != nullptr;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_extern)) {
                parsed = parser.ParseExtern() != nullptr;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_class) ||
                       parser.CurTok == static_cast<int>(Flux::TokenType::tok_struct) ||
                       parser.CurTok == static_cast<int>(Flux::TokenType::tok_enum) ||
                       parser.CurTok == static_cast<int>(Flux::TokenType::tok_trait) ||
                       parser.CurTok == static_cast<int>(Flux::TokenType::tok_impl)) {
                // Skip class/struct/enum/trait/impl body — consume tokens until matching '}'
                parser.getNextToken(); // consume keyword
                // Skip name, generics, colon, 'for', etc. until '{'
                while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof) &&
                       parser.CurTok != '{')
                    parser.getNextToken();
                if (parser.CurTok == '{') {
                    int depth = 1;
                    parser.getNextToken();
                    while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof) && depth > 0) {
                        if (parser.CurTok == '{')
                            depth++;
                        else if (parser.CurTok == '}')
                            depth--;
                        if (depth > 0)
                            parser.getNextToken();
                    }
                    if (parser.CurTok == '}')
                        parser.getNextToken();
                }
                parsed = true;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_semicolon) ||
                       parser.CurTok == ',') {
                // Semicolons and stray commas are statement separators — skip them
                parser.getNextToken();
                continue;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_import)) {
                // Skip import statements: import <name>
                parser.getNextToken(); // consume 'import'
                while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof) &&
                       parser.CurTok != '\n' && parser.CurTok != ';')
                    parser.getNextToken();
                parsed = true;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_from)) {
                // from <module> import <symbol> — skip entire line
                parser.getNextToken(); // consume 'from'
                while (parser.CurTok != static_cast<int>(Flux::TokenType::tok_eof) &&
                       parser.CurTok != '\n' && parser.CurTok != ';')
                    parser.getNextToken();
                parsed = true;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_async)) {
                // async def ...
                parsed = parser.ParseAsyncDef() != nullptr;
            } else if (parser.CurTok == static_cast<int>(Flux::TokenType::tok_update)) {
                // Skip update functions — they're program-specific
                parser.getNextToken();
            } else if (parser.CurTok == '}') {
                // Stray closing brace — skip
                parser.getNextToken();
                continue;
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
            // Sanitize message: strip control characters that break JSON
            std::string msg;
            for (char c : err.message) {
                if (c == '\n' || c == '\r')
                    continue; // strip newlines — location info is in range
                if (static_cast<unsigned char>(c) < 0x20 && c != '\t')
                    continue; // strip other control chars
                msg += c;
            }
            // Strip trailing backslash — VSCode merges it with source label
            while (!msg.empty() && msg.back() == '\\')
                msg.pop_back();
            d.message = msg;
            d.source = "fluxscript-compiler";
            d.range.start = {err.line - 1, err.column - 1};
            d.range.end = {err.line - 1, err.column};
            diags.push_back(d);
        }
    }

    // Build symbol table for completions
    m_symbolTables[uri] = buildSymbolTable(uri);

    // Add imported symbols from cross-file imports
    for (auto& sym : importedSymbols) {
        m_symbolTables[uri].push_back(sym);
    }

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
                "change": 2,
                "save": { "includeText": true }
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
            "foldingRangeProvider": true,
            "documentLinkProvider": {
                "resolveProvider": false
            },
            "selectionRangeProvider": true,
            "colorProvider": true,
            "inlayHintProvider": true,
            "monikerProvider": true,
            "semanticTokensProvider": {
                "legend": {
                    "tokenTypes": ["namespace","type","class","enum","interface","struct","typeParameter","parameter","variable","property","enumMember","function","method","keyword","modifier","comment","string","number","operator"],
                    "tokenModifiers": ["declaration","definition","readonly","static","deprecated","abstract","async","modification","documentation"]
                },
                "range": true,
                "full": {
                    "delta": false
                }
            },
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

std::string LspServer::handleTextDocumentDidSave(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");
    // Publish diagnostics on save
    auto diags = analyzeDocument(uri);
    publishDiagnostics(uri, diags);
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
        oss << R"("label":")" << jsonEscape(items[i].label) << "\"";
        oss << R"(,"kind":)" << items[i].kind;
        if (!items[i].detail.empty()) {
            oss << R"(,"detail":")" << jsonEscape(items[i].detail) << "\"";
        }
        if (!items[i].documentation.empty()) {
            oss << R"(,"documentation":")" << jsonEscape(items[i].documentation) << "\"";
        }
        if (!items[i].insertText.empty()) {
            oss << R"(,"insertText":")" << jsonEscape(items[i].insertText) << "\"";
            oss << R"(,"insertTextFormat":)" << items[i].insertTextFormat;
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
    oss << R"({"contents":{"kind":"markdown","value":")" << jsonEscape(hover.contents) << "\"}";
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
    oss << R"("character":)" << loc.range.end.character << "}}}";
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
        oss << R"("character":)" << refs[i].range.end.character << "}}}";
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
        oss << R"("character":)" << locs[i].range.end.character << "}}}";
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
        oss << R"("character":)" << locs[i].range.end.character << "}}}";
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
        oss << R"("location":{"uri":")" << jsonEscape(sym.uri) << "\",";
        oss << R"("range":{"start":{"line":)" << sym.range.start.line << ",";
        oss << R"("character":)" << sym.range.start.character << "},";
        oss << R"("end":{"line":)" << sym.range.end.line << ",";
        oss << R"("character":)" << sym.range.end.character << "}}}}";
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
                if (wp != std::string::npos && colonPos > wp && (wp == 0 || !isalnum(line[wp - 1]))) {
                    // word is a variable, tn is its type — redirect search
                    word = tn;
                    for (auto& kw : keywords) {
                        std::string ss = kw + word;
                        size_t sp = 0;
                        while ((sp = doc->text.find(ss, sp)) != std::string::npos) {
                            int dl = 0;
                            for (size_t k = 0; k < sp; ++k)
                                if (doc->text[k] == '\n')
                                    dl++;
                            size_t lls = sp;
                            while (lls > 0 && doc->text[lls - 1] != '\n')
                                lls--;
                            size_t lle = doc->text.find('\n', sp);
                            if (lle == std::string::npos)
                                lle = doc->text.size();
                            int ce = static_cast<int>(lle - lls);
                            if (ce > 0 && doc->text[lle - 1] == '\r')
                                ce--;
                            Location loc;
                            loc.uri = uri;
                            loc.range.start = {dl, 0};
                            loc.range.end = {dl, ce};
                            result.push_back(loc);
                            break;
                        }
                        if (!result.empty())
                            break;
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
        oss << R"(,"character":)" << lenses[i].range.end.character << "}},";
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
        oss << R"(,"character":)" << result.ranges[i].end.character << "}}";
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
        bool leftOk = searchPos == 0 || (!isalnum(doc->text[searchPos - 1]) && doc->text[searchPos - 1] != '_');
        bool rightOk = (searchPos + word.size() >= doc->text.size()) ||
                       (!isalnum(doc->text[searchPos + word.size()]) && doc->text[searchPos + word.size()] != '_');
        if (leftOk && rightOk) {
            result.ranges.push_back({doc->offsetToPosition(searchPos), doc->offsetToPosition(searchPos + word.size())});
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
// Folding Range — code folding regions
// ============================================================================

std::string LspServer::handleTextDocumentFoldingRange(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    auto ranges = getFoldingRanges(uri);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < ranges.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "{";
        oss << R"("startLine":)" << ranges[i].startLine << ",";
        oss << R"("endLine":)" << ranges[i].endLine;
        if (!ranges[i].kind.empty()) {
            oss << R"(,"kind":")" << jsonEscape(ranges[i].kind) << "\"";
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<LspServer::FoldingRange> LspServer::getFoldingRanges(const std::string& uri)
{
    std::vector<FoldingRange> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    const std::string& text = doc->text;

    // Scan for brace-delimited blocks: { ... }
    // Use a stack-based approach to find matching brace pairs
    std::vector<int> braceStack; // stores line numbers of opening braces

    for (size_t i = 0; i < text.size(); ++i) {
        // Skip string literals
        if (text[i] == '"') {
            i++;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\')
                    i++;
                i++;
            }
            continue;
        }

        // Skip single-line comments
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
            size_t commentStart = i;
            while (i < text.size() && text[i] != '\n')
                i++;
            // Check for region folding for comment blocks (3+ consecutive comment lines)
            // Handled below
            continue;
        }

        if (text[i] == '{') {
            int line = 0;
            for (size_t k = 0; k < i; ++k) {
                if (text[k] == '\n')
                    line++;
            }
            braceStack.push_back(line);
        } else if (text[i] == '}') {
            if (!braceStack.empty()) {
                int startLine = braceStack.back();
                braceStack.pop_back();
                int endLine = 0;
                for (size_t k = 0; k < i; ++k) {
                    if (text[k] == '\n')
                        endLine++;
                }
                // Only fold blocks that span at least 2 lines
                if (endLine - startLine >= 1) {
                    FoldingRange range;
                    range.startLine = startLine;
                    range.startCharacter = 0;
                    range.endLine = endLine;
                    range.endCharacter = 0;
                    result.push_back(range);
                }
            }
        }
    }

    // Scan for comment regions (3+ consecutive single-line comments)
    // Simplified: find runs of lines starting with optional whitespace followed by //
    size_t commentRunStart = std::string::npos;
    for (size_t i = 0; i < text.size();) {
        size_t lineStart = i;
        size_t lineEnd = text.find('\n', i);
        if (lineEnd == std::string::npos)
            lineEnd = text.size();

        std::string line = text.substr(lineStart, lineEnd - lineStart);
        // Trim leading whitespace
        size_t firstNonSpace = line.find_first_not_of(" \t\r");
        bool isComment = firstNonSpace != std::string::npos && line.size() >= firstNonSpace + 2 &&
                         line[firstNonSpace] == '/' && line[firstNonSpace + 1] == '/';

        int lineNum = 0;
        for (size_t k = 0; k < i; ++k) {
            if (text[k] == '\n')
                lineNum++;
        }

        if (isComment) {
            if (commentRunStart == std::string::npos)
                commentRunStart = lineNum;
        } else {
            if (commentRunStart != std::string::npos) {
                int runEnd = lineNum - 1;
                if (runEnd - static_cast<int>(commentRunStart) >= 2) {
                    FoldingRange range;
                    range.startLine = static_cast<int>(commentRunStart);
                    range.endLine = runEnd;
                    range.kind = "comment";
                    result.push_back(range);
                }
                commentRunStart = std::string::npos;
            }
        }

        i = lineEnd + 1;
    }
    // Handle trailing comment run
    if (commentRunStart != std::string::npos) {
        int lastLine = 0;
        for (size_t k = 0; k < text.size(); ++k) {
            if (text[k] == '\n')
                lastLine++;
        }
        if (lastLine - static_cast<int>(commentRunStart) >= 2) {
            FoldingRange range;
            range.startLine = static_cast<int>(commentRunStart);
            range.endLine = lastLine;
            range.kind = "comment";
            result.push_back(range);
        }
    }

    return result;
}

// ============================================================================
// Document Link — clickable URLs in comments
// ============================================================================

std::string LspServer::handleTextDocumentDocumentLink(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    auto links = getDocumentLinks(uri);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < links.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << "{";
        oss << R"("range":{"start":{"line":)" << links[i].range.start.line;
        oss << R"(,"character":)" << links[i].range.start.character << "},";
        oss << R"("end":{"line":)" << links[i].range.end.line;
        oss << R"(,"character":)" << links[i].range.end.character << "},";
        oss << R"("target":")" << jsonEscape(links[i].target) << "\"";
        if (!links[i].tooltip.empty()) {
            oss << R"(,"tooltip":")" << jsonEscape(links[i].tooltip) << "\"";
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<LspServer::DocumentLink> LspServer::getDocumentLinks(const std::string& uri)
{
    std::vector<DocumentLink> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    const std::string& text = doc->text;

    // Scan for URLs: http://, https://, ftp://, file://
    // Simple approach: find "://" and expand backward/forward to capture the full URL
    std::vector<std::string> protocols = {"http://", "https://", "ftp://", "file://"};

    size_t pos = 0;
    while (pos < text.size()) {
        // Find the next protocol prefix
        size_t nearestPos = std::string::npos;
        std::string foundProtocol;
        for (auto& proto : protocols) {
            size_t p = text.find(proto, pos);
            if (p != std::string::npos && (nearestPos == std::string::npos || p < nearestPos)) {
                nearestPos = p;
                foundProtocol = proto;
            }
        }

        if (nearestPos == std::string::npos)
            break;

        // Expand backward to find the start of the URL (handle markdown links)
        size_t urlStart = nearestPos;
        // If preceded by "(" (markdown), include it? No, just start from the protocol

        // Expand forward to find end of URL (stop at whitespace, closing paren/bracket/quote)
        size_t urlEnd = nearestPos + foundProtocol.size();
        while (urlEnd < text.size() && text[urlEnd] != ' ' && text[urlEnd] != '\t' && text[urlEnd] != '\n' &&
               text[urlEnd] != '\r' && text[urlEnd] != '"' && text[urlEnd] != '\'' && text[urlEnd] != '>' &&
               text[urlEnd] != ']' && text[urlEnd] != ')' && text[urlEnd] != '|') {
            urlEnd++;
        }

        std::string url = text.substr(urlStart, urlEnd - urlStart);

        if (!url.empty()) {
            DocumentLink link;
            link.range.start = doc->offsetToPosition(urlStart);
            link.range.end = doc->offsetToPosition(urlEnd);
            link.target = url;
            link.tooltip = "Open " + url;
            result.push_back(link);
        }

        pos = urlEnd;
    }

    return result;
}

// ============================================================================
// Selection Range — smart selection expansion
// ============================================================================

std::string LspServer::handleTextDocumentSelectionRange(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    // Parse positions array from params
    std::vector<Position> positions;
    size_t arrStart = params.find("\"positions\"");
    if (arrStart != std::string::npos) {
        size_t bracketPos = params.find('[', arrStart);
        if (bracketPos != std::string::npos) {
            size_t endBracket = params.find(']', bracketPos);
            std::string arrContent = params.substr(bracketPos + 1, endBracket - bracketPos - 1);

            size_t linePos = 0;
            while ((linePos = arrContent.find("\"line\"", linePos)) != std::string::npos) {
                size_t colon = arrContent.find(':', linePos);
                size_t numStart = colon + 1;
                while (numStart < arrContent.size() && arrContent[numStart] == ' ')
                    numStart++;
                int line = std::stoi(arrContent.substr(numStart));

                size_t charPos = arrContent.find("\"character\"", linePos);
                int ch = 0;
                if (charPos != std::string::npos) {
                    size_t charColon = arrContent.find(':', charPos);
                    size_t charNumStart = charColon + 1;
                    while (charNumStart < arrContent.size() && arrContent[charNumStart] == ' ')
                        charNumStart++;
                    ch = std::stoi(arrContent.substr(charNumStart));
                }
                positions.push_back({line, ch});
                linePos = charPos + 10;
            }
        }
    }

    if (positions.empty())
        return "null";

    auto result = getSelectionRanges(uri, positions);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < result.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"range":{"start":{"line":)" << result[i].range.start.line;
        oss << R"(,"character":)" << result[i].range.start.character << "},";
        oss << R"("end":{"line":)" << result[i].range.end.line;
        oss << R"(,"character":)" << result[i].range.end.character << "}},";
        oss << R"("parentIndex":)" << result[i].parentIndex << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<LspServer::SelectionRangeItem> LspServer::getSelectionRanges(const std::string& uri,
                                                                         const std::vector<Position>& positions)
{
    std::vector<SelectionRangeItem> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    for (auto& pos : positions) {
        // Build a hierarchy from smallest to largest
        std::vector<Range> hierarchy;

        std::string word = doc->getWordAtPosition(pos);
        if (word.empty()) {
            // No word — just use the cursor position as a single point range
            hierarchy.push_back({pos, pos});
        } else {
            // Level 1: Word range
            size_t offset = doc->positionToOffset(pos);
            size_t wordStart = offset;
            while (wordStart > 0 && (isalnum(doc->text[wordStart - 1]) || doc->text[wordStart - 1] == '_'))
                wordStart--;
            size_t wordEnd = offset;
            while (wordEnd < doc->text.size() && (isalnum(doc->text[wordEnd]) || doc->text[wordEnd] == '_'))
                wordEnd++;
            hierarchy.push_back({doc->offsetToPosition(wordStart), doc->offsetToPosition(wordEnd)});
        }

        // Level 2: Current line
        size_t lineStart = 0;
        int lineNum = 0;
        for (size_t k = 0; k < doc->text.size(); ++k) {
            if (lineNum == pos.line) {
                lineStart = k;
                break;
            }
            if (doc->text[k] == '\n')
                lineNum++;
        }
        size_t lineEnd = doc->text.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = doc->text.size();
        // Trim trailing whitespace
        size_t trimmedEnd = lineEnd;
        while (trimmedEnd > lineStart && (doc->text[trimmedEnd - 1] == ' ' || doc->text[trimmedEnd - 1] == '\t' ||
                                          doc->text[trimmedEnd - 1] == '\r'))
            trimmedEnd--;
        hierarchy.push_back({doc->offsetToPosition(lineStart), doc->offsetToPosition(trimmedEnd)});

        // Level 3: Nearest enclosing brace block
        // Scan backward for { and forward for matching }
        int braceStartLine = -1;
        int braceEndLine = -1;
        size_t offset = doc->positionToOffset(pos);
        size_t searchBack = offset;
        while (searchBack > 0) {
            searchBack--;
            if (doc->text[searchBack] == '{') {
                // Found opening brace — count lines
                int bl = 0;
                for (size_t k = 0; k < searchBack; ++k)
                    if (doc->text[k] == '\n')
                        bl++;
                braceStartLine = bl;
                break;
            }
            if (doc->text[searchBack] == '}')
                break; // Don't cross brace boundaries
        }

        if (braceStartLine >= 0) {
            // Find matching closing brace
            size_t searchForward = searchBack + 1;
            int depth = 1;
            while (searchForward < doc->text.size() && depth > 0) {
                if (doc->text[searchForward] == '{')
                    depth++;
                else if (doc->text[searchForward] == '}')
                    depth--;
                searchForward++;
            }
            if (depth == 0) {
                int el = 0;
                for (size_t k = 0; k < searchForward; ++k)
                    if (doc->text[k] == '\n')
                        el++;
                braceEndLine = el - 1; // end brace line
                if (braceEndLine > braceStartLine) {
                    hierarchy.push_back({{braceStartLine, 0}, {braceEndLine, 0}});
                }
            }
        }

        // Level 4: Whole document
        int lastLine = 0;
        for (size_t k = 0; k < doc->text.size(); ++k)
            if (doc->text[k] == '\n')
                lastLine++;
        hierarchy.push_back({{0, 0}, {lastLine, 0}});

        // Build SelectionRange items with parent links (parent = larger one, i.e. next in array)
        // The LSP spec: array where each element has range + parentIndex.
        // parentIndex points to the parent (larger range) in the array.
        // We'll add them smallest-first and link parent to the next.
        std::vector<SelectionRangeItem> items;
        for (size_t i = 0; i < hierarchy.size(); ++i) {
            SelectionRangeItem item;
            item.range = hierarchy[i];
            // Parent is the next (larger) range, unless this is the last
            if (i + 1 < hierarchy.size())
                item.parentIndex = static_cast<int>(items.size() + 1);
            else
                item.parentIndex = -1;
            items.push_back(item);
        }

        result.insert(result.end(), items.begin(), items.end());
    }

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
    size_t parenPos = doc->text.rfind('(', offset);
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

    // Look up user-defined function from symbol table
    if (m_symbolTables.count(uri)) {
        for (auto& sym : m_symbolTables[uri]) {
            if (sym.kind == SymbolEntry::Function && sym.name == funcName) {
                SignatureInfo sig;
                sig.label = funcName;
                sig.documentation = sym.documentation;

                // Find function definition in source text to extract params
                std::string defPattern = "def " + funcName + "(";
                size_t defPos = doc->text.find(defPattern);
                if (defPos == std::string::npos) {
                    defPattern = "extern " + funcName + "(";
                    defPos = doc->text.find(defPattern);
                }
                if (defPos != std::string::npos) {
                    size_t paramsStart = defPos + defPattern.size();
                    size_t paramsEnd = doc->text.find(')', paramsStart);
                    if (paramsEnd != std::string::npos) {
                        std::string params = doc->text.substr(paramsStart, paramsEnd - paramsStart);
                        size_t start = 0;
                        while (start < params.size()) {
                            size_t comma = params.find(',', start);
                            if (comma == std::string::npos)
                                comma = params.size();
                            std::string param = params.substr(start, comma - start);
                            while (!param.empty() && param[0] == ' ')
                                param.erase(0, 1);
                            while (!param.empty() && param.back() == ' ')
                                param.pop_back();
                            if (!param.empty()) {
                                sig.label += (sig.parameters.empty() ? "(" : ", ") + param;
                                sig.parameters.push_back({param, ""});
                            }
                            start = comma + 1;
                        }
                    }
                }
                sig.label += ")";

                result.signatures.push_back(sig);
                result.activeParameter = std::min(argCount, static_cast<int>(sig.parameters.size()) - 1);
                return result;
            }
        }
    }

    return result;
}

// ============================================================================
// Document Color — color picker for hex color literals
// ============================================================================

std::string LspServer::handleTextDocumentDocumentColor(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    auto colors = getDocumentColors(uri);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < colors.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"range":{"start":{"line":)" << colors[i].range.start.line;
        oss << R"(,"character":)" << colors[i].range.start.character << "},";
        oss << R"("end":{"line":)" << colors[i].range.end.line;
        oss << R"(,"character":)" << colors[i].range.end.character << "},";
        oss << R"("color":{"red":)" << colors[i].color.red;
        oss << R"(,"green":)" << colors[i].color.green;
        oss << R"(,"blue":)" << colors[i].color.blue;
        oss << R"(,"alpha":)" << colors[i].color.alpha << "}}";
    }
    oss << "]";
    return oss.str();
}

std::string LspServer::handleTextDocumentColorPresentation(const std::string& params)
{
    std::string uri = jsonGet(params, "textDocument.uri");

    double red = std::stod(jsonGet(params, "color.red"));
    double green = std::stod(jsonGet(params, "color.green"));
    double blue = std::stod(jsonGet(params, "color.blue"));
    double alpha = std::stod(jsonGet(params, "color.alpha"));

    int rangeStartLine = jsonGetInt(params, "range.start.line");
    int rangeStartChar = jsonGetInt(params, "range.start.character");
    int rangeEndLine = jsonGetInt(params, "range.end.line");
    int rangeEndChar = jsonGetInt(params, "range.end.character");

    Color color{red, green, blue, alpha};
    Range range{{rangeStartLine, rangeStartChar}, {rangeEndLine, rangeEndChar}};

    auto presentations = getColorPresentations(uri, color, range);

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < presentations.size(); ++i) {
        if (i > 0)
            oss << ",";
        oss << R"({"label":")" << jsonEscape(presentations[i].label) << "\"";
        if (!presentations[i].textEdit.empty()) {
            oss << R"(,"textEdit":)" << presentations[i].textEdit;
        }
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

static bool parseHexColor(const std::string& str, LspServer::Color& color)
{
    if (str.empty() || str[0] != '#')
        return false;
    std::string hex = str.substr(1);
    if (hex.size() != 6 && hex.size() != 8)
        return false;

    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };

    int r1 = hexVal(hex[0]), r2 = hexVal(hex[1]);
    int g1 = hexVal(hex[2]), g2 = hexVal(hex[3]);
    int b1 = hexVal(hex[4]), b2 = hexVal(hex[5]);
    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
        return false;

    color.red = (r1 * 16 + r2) / 255.0;
    color.green = (g1 * 16 + g2) / 255.0;
    color.blue = (b1 * 16 + b2) / 255.0;
    color.alpha = 1.0;

    if (hex.size() == 8) {
        int a1 = hexVal(hex[6]), a2 = hexVal(hex[7]);
        if (a1 >= 0 && a2 >= 0) {
            color.alpha = (a1 * 16 + a2) / 255.0;
        }
    }

    return true;
}

std::vector<LspServer::ColorInformation> LspServer::getDocumentColors(const std::string& uri)
{
    std::vector<ColorInformation> result;
    auto* doc = getDocument(uri);
    if (!doc)
        return result;

    const std::string& text = doc->text;

    size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] == '#' && pos + 7 <= text.size()) {
            std::string candidate = text.substr(pos, 7);
            Color c;
            if (parseHexColor(candidate, c)) {
                ColorInformation info;
                info.range.start = doc->offsetToPosition(pos);
                info.range.end = doc->offsetToPosition(pos + 7);
                info.color = c;
                result.push_back(info);
                pos += 7;
                continue;
            }
            if (pos + 9 <= text.size()) {
                candidate = text.substr(pos, 9);
                if (parseHexColor(candidate, c)) {
                    ColorInformation info;
                    info.range.start = doc->offsetToPosition(pos);
                    info.range.end = doc->offsetToPosition(pos + 9);
                    info.color = c;
                    result.push_back(info);
                    pos += 9;
                    continue;
                }
            }
        }
        pos++;
    }

    return result;
}

std::vector<LspServer::ColorPresentation> LspServer::getColorPresentations(const std::string& uri, const Color& color,
                                                                           const Range& range)
{
    std::vector<ColorPresentation> result;
    (void)uri;

    int r = static_cast<int>(color.red * 255.0 + 0.5);
    int g = static_cast<int>(color.green * 255.0 + 0.5);
    int b = static_cast<int>(color.blue * 255.0 + 0.5);
    int a = static_cast<int>(color.alpha * 255.0 + 0.5);

    char hexBuf[10];
    if (a < 255) {
        snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X%02X", r, g, b, a);
    } else {
        snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", r, g, b);
    }

    ColorPresentation hexPresentation;
    hexPresentation.label = std::string(hexBuf);
    std::ostringstream oss;
    oss << R"({"range":{"start":{"line":)" << range.start.line;
    oss << R"(,"character":)" << range.start.character << "},";
    oss << R"("end":{"line":)" << range.end.line;
    oss << R"(,"character":)" << range.end.character << "},";
    oss << R"("newText":")" << hexBuf << "\"}";
    hexPresentation.textEdit = oss.str();
    result.push_back(hexPresentation);

    char rgbBuf[64];
    if (a < 255) {
        snprintf(rgbBuf, sizeof(rgbBuf), "rgba(%d, %d, %d, %.2f)", r, g, b, color.alpha);
    } else {
        snprintf(rgbBuf, sizeof(rgbBuf), "rgb(%d, %d, %d)", r, g, b);
    }
    ColorPresentation rgbPresentation;
    rgbPresentation.label = std::string(rgbBuf);
    result.push_back(rgbPresentation);

    return result;
}

// ============================================================================
// Semantic Tokens — syntax highlighting via LSP
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
