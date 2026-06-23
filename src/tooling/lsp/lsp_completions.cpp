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


} // namespace Tooling
} // namespace Flux
