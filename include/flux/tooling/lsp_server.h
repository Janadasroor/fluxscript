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

#ifndef FLUX_TOOLING_LSP_SERVER_H
#define FLUX_TOOLING_LSP_SERVER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

// Forward declarations
class Lexer;
class Parser;

namespace Tooling {

// ============================================================================
// LSP Types (subset of LSP 3.17 spec)
// ============================================================================

struct Position
{
    int line = 0;      // 0-based
    int character = 0; // 0-based UTF-16 code units
    bool operator==(const Position& other) const { return line == other.line && character == other.character; }
    bool operator!=(const Position& other) const { return !(*this == other); }
    bool operator<(const Position& other) const
    {
        return line < other.line || (line == other.line && character < other.character);
    }
};

struct Range
{
    Position start;
    Position end;
};

struct Location
{
    std::string uri;
    Range range;
};

struct Diagnostic
{
    enum Severity
    {
        Error = 1,
        Warning = 2,
        Info = 3,
        Hint = 4
    };
    Range range;
    Severity severity = Error;
    std::string message;
    std::string source = "fluxscript";
};

struct CompletionItem
{
    enum Kind
    {
        Text = 1,
        Method = 2,
        Function = 3,
        Constructor = 4,
        Field = 5,
        Variable = 6,
        Class = 7,
        Interface = 8,
        Module = 9,
        Property = 10,
        Unit = 12,
        Value = 13,
        Enum = 14,
        Keyword = 15,
        Snippet = 16,
        Color = 16,
        File = 17,
        Reference = 18,
        Folder = 19,
        EnumMember = 20,
        Constant = 21,
        Struct = 22,
        Event = 23,
        Operator = 24,
        TypeParameter = 25,
        Type = 26
    };
    std::string label;
    Kind kind = Text;
    std::string detail; // e.g., "(x: double) -> double"
    std::string documentation;
    std::string insertText;
    int insertTextFormat = 1; // 1=plaintext, 2=snippet
};

struct ParameterInfo
{
    std::string label;
    std::string documentation;
};

struct SignatureInfo
{
    std::string label; // e.g., "sin(x: double)"
    std::string documentation;
    std::vector<ParameterInfo> parameters;
};

struct HoverContent
{
    std::string contents; // Markdown
    Range range;
};

// ============================================================================
// Document State
// ============================================================================

struct TextDocument
{
    std::string uri;
    std::string languageId;
    int version = 0;
    std::string text;

    Position offsetToPosition(size_t offset) const;
    size_t positionToOffset(Position pos) const;
    std::string getLine(size_t line) const;
    std::string getWordAtPosition(Position pos) const;
};

// ============================================================================
// Symbol Table Entry
// ============================================================================

struct SymbolEntry
{
    enum Kind
    {
        Variable,
        Function,
        Parameter,
        Builtin,
        Keyword,
        Type
    };
    std::string name;
    Kind kind = Variable;
    std::string typeStr; // e.g., "double", "(double) -> double"
    std::string documentation;
    Range range;
    std::string uri;
};

// ============================================================================
// LSP Server
// ============================================================================

class LspServer
{
public:
    LspServer();
    ~LspServer();

    // Main entry point: process one JSON-RPC request from stdin
    // Returns the JSON-RPC response string
    std::string processRequest(const std::string& jsonRequest);

    // Run the server loop (reads from stdin, writes to stdout)
    int run();

    // Document management
    void openDocument(const std::string& uri, const std::string& languageId, int version, const std::string& text);
    void changeDocument(const std::string& uri, int version, const std::string& text);
    void closeDocument(const std::string& uri);
    TextDocument* getDocument(const std::string& uri);

    // Semantic analysis
    std::vector<Diagnostic> analyzeDocument(const std::string& uri);
    std::vector<SymbolEntry> buildSymbolTable(const std::string& uri);

    // LSP handlers
    std::string handleInitialize(const std::string& params);
    std::string handleShutdown(const std::string& params);
    std::string handleTextDocumentDidOpen(const std::string& params);
    std::string handleTextDocumentDidChange(const std::string& params);
    std::string handleTextDocumentDidClose(const std::string& params);
    std::string handleTextDocumentDidSave(const std::string& params);
    std::string handleTextDocumentCompletion(const std::string& params);
    std::string handleTextDocumentHover(const std::string& params);
    std::string handleTextDocumentDefinition(const std::string& params);
    std::string handleTextDocumentReferences(const std::string& params);
    std::string handleTextDocumentImplementation(const std::string& params);
    std::string handleTextDocumentTypeDefinition(const std::string& params);
    std::string handleTextDocumentCodeAction(const std::string& params);
    std::string handleTextDocumentFormatting(const std::string& params);
    std::string handleTextDocumentSignatureHelp(const std::string& params);
    std::string handleTextDocumentDocumentSymbol(const std::string& params);
    std::string handleTextDocumentPrepareRename(const std::string& params);
    std::string handleTextDocumentCodeLens(const std::string& params);
    std::string handleTextDocumentPrepareCallHierarchy(const std::string& params);
    std::string handleCallHierarchyIncomingCalls(const std::string& params);
    std::string handleCallHierarchyOutgoingCalls(const std::string& params);
    std::string handleTextDocumentPrepareTypeHierarchy(const std::string& params);
    std::string handleTypeHierarchySupertypes(const std::string& params);
    std::string handleTypeHierarchySubtypes(const std::string& params);
    std::string handleTextDocumentLinkedEditingRange(const std::string& params);
    std::string handleTextDocumentFoldingRange(const std::string& params);
    std::string handleTextDocumentDocumentLink(const std::string& params);
    std::string handleTextDocumentSelectionRange(const std::string& params);
    std::string handleTextDocumentDocumentColor(const std::string& params);
    std::string handleTextDocumentColorPresentation(const std::string& params);
    std::string handleTextDocumentSemanticTokensFull(const std::string& params);
    std::string handleTextDocumentSemanticTokensRange(const std::string& params);
    std::string handleTextDocumentInlayHint(const std::string& params);
    std::string handleTextDocumentMoniker(const std::string& params);
    std::string handleWorkspaceSymbol(const std::string& params);

    // Completion engine
    std::vector<CompletionItem> getCompletions(const std::string& uri, Position pos);
    std::vector<CompletionItem> getKeywordCompletions();
    std::vector<CompletionItem> getBuiltinFunctionCompletions();
    std::vector<CompletionItem> getTypeCompletions();
    std::vector<CompletionItem> getSnippetCompletions();

    // Hover engine
    HoverContent getHover(const std::string& uri, Position pos);

    // Definition provider
    Location getDefinition(const std::string& uri, Position pos);

    // References provider
    std::vector<Location> getReferences(const std::string& uri, Position pos);

    // Implementation provider (go to impl for traits/interfaces)
    std::vector<Location> getImplementation(const std::string& uri, Position pos);

    // Type definition provider (go to definition of the type)
    std::vector<Location> getTypeDefinition(const std::string& uri, Position pos);

    // Code actions
    struct CodeAction
    {
        std::string title;
        std::string kind; // "quickfix", "refactor", etc.
        std::string edit; // WorkspaceEdit JSON
    };
    std::vector<CodeAction> getCodeActions(const std::string& uri, Position pos, const std::vector<Diagnostic>& diags);

    // Document formatting
    std::vector<std::pair<Position, std::string>> computeTextEdits(const std::string& uri, const std::string& tabSize);

    // Signature help
    struct SignatureHelpResult
    {
        std::vector<SignatureInfo> signatures;
        int activeSignature = 0;
        int activeParameter = 0;
    };
    SignatureHelpResult getSignatureHelp(const std::string& uri, Position pos);

    // Prepare rename (validates rename is possible at position)
    struct PrepareRenameResult
    {
        Range range;
        std::string placeholder;
        bool valid = false;
    };
    PrepareRenameResult getPrepareRename(const std::string& uri, Position pos);

    // Code Lens
    struct CodeLensItem
    {
        Range range;
        std::string title;
        std::string command; // command identifier
    };
    std::vector<CodeLensItem> getCodeLenses(const std::string& uri);

    // Call Hierarchy
    struct CallHierarchyItem
    {
        std::string name;
        int kind = 12; // 12 = Function
        std::string detail;
        std::string uri;
        Range range;
        Range selectionRange;
    };

    struct CallHierarchyIncomingCall
    {
        CallHierarchyItem from;
        std::vector<Range> fromRanges;
    };

    struct CallHierarchyOutgoingCall
    {
        CallHierarchyItem to;
        std::vector<Range> fromRanges;
    };

    CallHierarchyItem getPrepareCallHierarchy(const std::string& uri, Position pos);
    std::vector<CallHierarchyIncomingCall> getCallHierarchyIncomingCalls(const CallHierarchyItem& item);
    std::vector<CallHierarchyOutgoingCall> getCallHierarchyOutgoingCalls(const CallHierarchyItem& item);

    // Type Hierarchy
    struct TypeHierarchyItem
    {
        std::string name;
        int kind = 22; // 22 = Struct
        std::string detail;
        std::string uri;
        Range range;
        Range selectionRange;
    };

    TypeHierarchyItem getPrepareTypeHierarchy(const std::string& uri, Position pos);
    std::vector<TypeHierarchyItem> getTypeHierarchySupertypes(const TypeHierarchyItem& item);
    std::vector<TypeHierarchyItem> getTypeHierarchySubtypes(const TypeHierarchyItem& item);

    // Linked Editing Range
    struct LinkedEditingRanges
    {
        std::vector<Range> ranges;
        std::string wordPattern; // optional regex pattern
    };
    LinkedEditingRanges getLinkedEditingRanges(const std::string& uri, Position pos);

    // Folding Range
    struct FoldingRange
    {
        int startLine;
        int startCharacter = 0;
        int endLine;
        int endCharacter = 0;
        std::string kind; // "comment", "imports", "region"
    };
    std::vector<FoldingRange> getFoldingRanges(const std::string& uri);

    // Document Link
    struct DocumentLink
    {
        Range range;
        std::string target;
        std::string tooltip;
    };
    std::vector<DocumentLink> getDocumentLinks(const std::string& uri);

    // Selection Range
    struct SelectionRangeItem
    {
        Range range;
        int parentIndex = -1;
    };
    std::vector<SelectionRangeItem> getSelectionRanges(const std::string& uri, const std::vector<Position>& positions);

    // Document Color
    struct Color
    {
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
        double alpha = 1.0;
    };
    struct ColorInformation
    {
        Range range;
        Color color;
    };
    struct ColorPresentation
    {
        std::string label;
        std::string textEdit; // JSON TextEdit if replacing
    };
    std::vector<ColorInformation> getDocumentColors(const std::string& uri);
    std::vector<ColorPresentation> getColorPresentations(const std::string& uri, const Color& color,
                                                         const Range& range);

    // Semantic Tokens
    struct SemanticTokensResult
    {
        std::vector<unsigned int> data;
    };
    SemanticTokensResult getSemanticTokens(const std::string& uri);
    SemanticTokensResult getSemanticTokensRange(const std::string& uri, Range range);
    std::string getSemanticTokensLegend(); // returns JSON legend

    // Inlay Hint
    struct InlayHint
    {
        Position position;
        std::string label;
        int kind = 0; // 1=Type, 2=Parameter
        std::string tooltip;
    };
    std::vector<InlayHint> getInlayHints(const std::string& uri, Range range);

    // Moniker
    struct Moniker
    {
        std::string scheme;     // e.g. "flux"
        std::string identifier; // unique stable identifier
        int kind = 0;           // 1=Import, 2=Export, 3=Local
    };
    std::vector<Moniker> getMonikers(const std::string& uri, Position pos);

private:
    // JSON helpers
    std::string jsonGet(const std::string& json, const std::string& key);
    int jsonGetInt(const std::string& json, const std::string& key, int defaultVal = 0);
    std::string jsonGetNested(const std::string& json, const std::string& key1, const std::string& key2);
    std::string jsonEscape(const std::string& s);

    // Response builder
    std::string makeResponse(int id, const std::string& result);
    std::string makeErrorResponse(int id, int code, const std::string& message);
    std::string makeNotification(const std::string& method, const std::string& params);

    // Publish diagnostics to client
    void publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics);

    std::map<std::string, TextDocument> m_documents;
    std::map<std::string, std::vector<SymbolEntry>> m_symbolTables;
    int m_requestId = 0;

    // Completion cache
    std::vector<CompletionItem> m_allCompletions;
    bool m_completionsBuilt = false;

    void buildCompletions();
};

} // namespace Tooling
} // namespace Flux

#endif // FLUX_TOOLING_LSP_SERVER_H
