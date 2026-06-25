/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"
#include "flux/tooling/lsp_server.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace Flux {
namespace Tooling {

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
    oss << R"("character":)" << item.range.end.character << "}},";
    oss << R"("selectionRange":{"start":{"line":)" << item.selectionRange.start.line << ",";
    oss << R"("character":)" << item.selectionRange.start.character << "},";
    oss << R"("end":{"line":)" << item.selectionRange.end.line << ",";
    oss << R"("character":)" << item.selectionRange.end.character << "}}}";
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
        oss << R"("character":)" << call.from.range.end.character << "}},";
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
        oss << R"("character":)" << call.to.range.end.character << "}},";
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
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

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

std::vector<LspServer::CallHierarchyIncomingCall>
LspServer::getCallHierarchyIncomingCalls(const CallHierarchyItem& item)
{
    std::vector<CallHierarchyIncomingCall> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty())
        return result;

    if (item.name.empty())
        return result;

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
        if (defPos != std::string::npos && searchPos >= defPos - 4 && searchPos < defEnd) {
            searchPos += item.name.size();
            continue;
        }

        // Check word boundaries
        bool leftOk = searchPos == 0 || (!isalnum(doc->text[searchPos - 1]) && doc->text[searchPos - 1] != '_');
        bool rightOk =
            (searchPos + item.name.size() >= doc->text.size()) ||
            (!isalnum(doc->text[searchPos + item.name.size()]) && doc->text[searchPos + item.name.size()] != '_');
        if (leftOk && rightOk) {
            Range callRange = {doc->offsetToPosition(searchPos), doc->offsetToPosition(searchPos + item.name.size())};

            // Find which function this call is inside
            // Scan backward to find the nearest `def ... (` before this position
            int callLine = callRange.start.line;
            // Simple approach: scan backward to find the nearest `def` before this line
            size_t backtrack = searchPos;
            std::string callerName;
            while (backtrack > 0) {
                // Find `def ` going backward
                size_t prevDef = doc->text.rfind("def ", backtrack);
                if (prevDef == std::string::npos || prevDef < 10)
                    break;
                size_t cnStart = prevDef + 4;
                size_t cnEnd = cnStart;
                while (cnEnd < doc->text.size() && (isalnum(doc->text[cnEnd]) || doc->text[cnEnd] == '_'))
                    cnEnd++;
                callerName = doc->text.substr(cnStart, cnEnd - cnStart);
                if (!callerName.empty()) {
                    // Check this def isn't the target itself
                    if (callerName != item.name) {
                        break;
                    }
                }
                if (backtrack < 10)
                    break;
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

std::vector<LspServer::CallHierarchyOutgoingCall>
LspServer::getCallHierarchyOutgoingCalls(const CallHierarchyItem& item)
{
    std::vector<CallHierarchyOutgoingCall> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty())
        return result;

    // Find the definition range of this function
    std::string defPattern = "def " + item.name;
    size_t defPos = doc->text.find(defPattern);
    if (defPos == std::string::npos)
        return result;

    // Find the body start -- scan for `{` after `def name`
    size_t bodyStart = defPos + defPattern.size();
    size_t bracePos = doc->text.find('{', bodyStart);
    if (bracePos == std::string::npos)
        return result;

    // Find matching closing brace (simple depth count)
    size_t bodyEnd = bracePos + 1;
    int depth = 1;
    while (bodyEnd < doc->text.size() && depth > 0) {
        if (doc->text[bodyEnd] == '{')
            depth++;
        else if (doc->text[bodyEnd] == '}')
            depth--;
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
                if (doc->text[i] == '\\')
                    i++;
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
    oss << R"("character":)" << item.range.end.character << "}},";
    oss << R"("selectionRange":{"start":{"line":)" << item.selectionRange.start.line << ",";
    oss << R"("character":)" << item.selectionRange.start.character << "},";
    oss << R"("end":{"line":)" << item.selectionRange.end.line << ",";
    oss << R"("character":)" << item.selectionRange.end.character << "}}}";
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
        oss << R"("character":)" << t.range.end.character << "}},";
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
        oss << R"("character":)" << t.range.end.character << "}},";
        oss << R"("selectionRange":{"start":{"line":)" << t.selectionRange.start.line << ",";
        oss << R"("character":)" << t.selectionRange.start.character << "},";
        oss << R"("end":{"line":)" << t.selectionRange.end.line << ",";
        oss << R"("character":)" << t.selectionRange.end.character << "}})";
    }
    oss << "]";
    return oss.str();
}

// Helper: scan document for type definitions and return items
static std::vector<LspServer::TypeHierarchyItem> scanTypeDefs(Flux::Tooling::LspServer* server, const std::string& uri)
{
    std::vector<LspServer::TypeHierarchyItem> result;
    auto* doc = server->getDocument(uri);
    if (!doc)
        return result;

    // Scan for `struct <name>`, `class <name>`, `enum <name>`, `trait <name>`
    struct KeywordInfo
    {
        std::string keyword;
        int kind;
        std::string detail;
    };
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
    if (!doc)
        return result;

    std::string word = doc->getWordAtPosition(pos);
    if (word.empty())
        return result;

    // Check if word is a type definition
    auto types = scanTypeDefs(this, uri);
    for (auto& t : types) {
        if (t.name == word) {
            return t;
        }
    }

    return result;
}

std::vector<LspServer::TypeHierarchyItem> LspServer::getTypeHierarchySupertypes(const TypeHierarchyItem& item)
{
    std::vector<TypeHierarchyItem> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty())
        return result;

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
            if (colonPos != std::string::npos && (bracePos == std::string::npos || colonPos < bracePos)) {
                // Extract parent name after ':'
                size_t ps = colonPos + 1;
                while (ps < doc->text.size() && doc->text[ps] == ' ')
                    ps++;
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
        if (forPos == std::string::npos) {
            implPos += 5;
            continue;
        }
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
                        if (r.name == traitName) {
                            alreadyAdded = true;
                            break;
                        }
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

std::vector<LspServer::TypeHierarchyItem> LspServer::getTypeHierarchySubtypes(const TypeHierarchyItem& item)
{
    std::vector<TypeHierarchyItem> result;
    auto* doc = getDocument(item.uri);
    if (!doc || item.name.empty())
        return result;

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
                if (colonPos != std::string::npos && (bracePos == std::string::npos || colonPos < bracePos)) {
                    size_t ps = colonPos + 1;
                    while (ps < doc->text.size() && doc->text[ps] == ' ')
                        ps++;
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
                    if (r.name == typeName) {
                        alreadyAdded = true;
                        break;
                    }
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

} // namespace Tooling
} // namespace Flux
