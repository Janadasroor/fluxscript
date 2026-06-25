/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/compiler/ast.h"
#include "flux/compiler/lexer.h"
#include "flux/compiler/parser.h"
#include "flux/tooling/lsp_server.h"
#include <algorithm>
#include <fstream>
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
        {"elif", CompletionItem::Keyword, "else-if branch", "", "elif ${1:cond} then ${2:result}", 2},
        {"in", CompletionItem::Keyword, "binding", "", "in", 1},
        {"do", CompletionItem::Keyword, "loop body", "", "do", 1},
        {"return", CompletionItem::Keyword, "return statement", "", "return ${1:value}", 2},
        {"case", CompletionItem::Keyword, "case label", "", "case", 1},
        {"default", CompletionItem::Keyword, "default case", "", "default:", 1},
        {"break", CompletionItem::Keyword, "break loop", "", "break", 1},
        {"continue", CompletionItem::Keyword, "continue loop", "", "continue", 1},
        {"catch", CompletionItem::Keyword, "catch block", "", "catch", 1},
        {"throw", CompletionItem::Keyword, "throw exception", "", "throw ${1:expr}", 2},
        {"match", CompletionItem::Keyword, "pattern match", "Pattern matching",
         "match ${1:expr} {\n  ${2:pattern} => ${3:result}\n}", 2},
        {"parallel", CompletionItem::Keyword, "parallel loop", "Parallel for loop",
         "parallel for ${1:i} in ${2:start}, ${3:end} do ${4:body}", 2},
        {"class", CompletionItem::Keyword, "class definition", "Define a class",
         "class ${1:Name} {\n  ${2:// fields}\n}", 2},
        {"struct", CompletionItem::Keyword, "struct definition", "Define a struct",
         "struct ${1:Name} {\n  ${2:field}: ${3:type}\n}", 2},
        {"enum", CompletionItem::Keyword, "enum definition", "Define an enum",
         "enum ${1:Name} {\n  ${2:Variant1}\n  ${3:Variant2}(type)\n}", 2},
        {"trait", CompletionItem::Keyword, "trait definition", "Define a trait/interface",
         "trait ${1:Name} {\n  ${2:def method(self)}\n}", 2},
        {"impl", CompletionItem::Keyword, "impl block", "Implement a trait or methods",
         "impl ${1:Name} {\n  ${2:def method(self) ...}\n}", 2},
        {"async", CompletionItem::Keyword, "async function", "Define an async function",
         "async def ${1:name}(${2:args}) ${3:body}", 2},
        {"await", CompletionItem::Keyword, "await expression", "Wait for async result",
         "await ${1:expr}", 2},
        {"import", CompletionItem::Keyword, "import module", "Import a module",
         "import ${1:module}", 2},
        {"from", CompletionItem::Keyword, "from import", "Import specific symbols",
         "from ${1:module} import ${2:symbol}", 2},
        {"pub", CompletionItem::Keyword, "public visibility", "Mark as public", "pub ", 1},
        {"mut", CompletionItem::Keyword, "mutable binding", "Mark variable as mutable", "mut ", 1},
        {"true", CompletionItem::Keyword, "boolean true", "Boolean literal", "true", 1},
        {"false", CompletionItem::Keyword, "boolean false", "Boolean literal", "false", 1},
        {"null", CompletionItem::Keyword, "null value", "Null literal", "null", 1},
        {"self", CompletionItem::Keyword, "self reference", "Reference to current instance", "self", 1},
        {"fn", CompletionItem::Keyword, "lambda expression", "Anonymous function",
         "fn(${1:args}) -> ${2:body}", 2},
    };
}

std::vector<CompletionItem> LspServer::getBuiltinFunctionCompletions()
{
    return {
        // Core math
        {"sin", CompletionItem::Function, "(x) -> double", "Sine function", "sin(${1:x})", 2},
        {"cos", CompletionItem::Function, "(x) -> double", "Cosine function", "cos(${1:x})", 2},
        {"tan", CompletionItem::Function, "(x) -> double", "Tangent function", "tan(${1:x})", 2},
        {"asin", CompletionItem::Function, "(x) -> double", "Inverse sine", "asin(${1:x})", 2},
        {"acos", CompletionItem::Function, "(x) -> double", "Inverse cosine", "acos(${1:x})", 2},
        {"atan", CompletionItem::Function, "(x) -> double", "Inverse tangent", "atan(${1:x})", 2},
        {"atan2", CompletionItem::Function, "(y, x) -> double", "Two-argument arctangent", "atan2(${1:y}, ${2:x})", 2},
        {"sqrt", CompletionItem::Function, "(x) -> double", "Square root", "sqrt(${1:x})", 2},
        {"exp", CompletionItem::Function, "(x) -> double", "Exponential function", "exp(${1:x})", 2},
        {"log", CompletionItem::Function, "(x) -> double", "Natural logarithm", "log(${1:x})", 2},
        {"log10", CompletionItem::Function, "(x) -> double", "Base-10 logarithm", "log10(${1:x})", 2},
        {"log2", CompletionItem::Function, "(x) -> double", "Base-2 logarithm", "log2(${1:x})", 2},
        {"abs", CompletionItem::Function, "(x) -> double", "Absolute value", "abs(${1:x})", 2},
        {"floor", CompletionItem::Function, "(x) -> double", "Floor function", "floor(${1:x})", 2},
        {"ceil", CompletionItem::Function, "(x) -> double", "Ceiling function", "ceil(${1:x})", 2},
        {"round", CompletionItem::Function, "(x) -> double", "Round to nearest integer", "round(${1:x})", 2},
        {"pow", CompletionItem::Function, "(x, y) -> double", "Power function", "pow(${1:x}, ${2:y})", 2},
        {"pi", CompletionItem::Function, "() -> double", "Pi constant (3.14159...)", "pi()", 2},
        // Extended math
        {"sec", CompletionItem::Function, "(x) -> double", "Secant", "sec(${1:x})", 2},
        {"csc", CompletionItem::Function, "(x) -> double", "Cosecant", "csc(${1:x})", 2},
        {"cot", CompletionItem::Function, "(x) -> double", "Cotangent", "cot(${1:x})", 2},
        {"sinc", CompletionItem::Function, "(x) -> double", "Sinc function", "sinc(${1:x})", 2},
        {"sech", CompletionItem::Function, "(x) -> double", "Hyperbolic secant", "sech(${1:x})", 2},
        {"csch", CompletionItem::Function, "(x) -> double", "Hyperbolic cosecant", "csch(${1:x})", 2},
        {"coth", CompletionItem::Function, "(x) -> double", "Hyperbolic cotangent", "coth(${1:x})", 2},
        {"sigmoid", CompletionItem::Function, "(x) -> double", "Sigmoid activation", "sigmoid(${1:x})", 2},
        {"relu", CompletionItem::Function, "(x) -> double", "ReLU activation", "relu(${1:x})", 2},
        {"clamp", CompletionItem::Function, "(x, lo, hi) -> double", "Clamp value between lo and hi",
         "clamp(${1:x}, ${2:0.0}, ${3:1.0})", 2},
        {"lerp", CompletionItem::Function, "(a, b, t) -> double", "Linear interpolation", "lerp(${1:a}, ${2:b}, ${3:t})", 2},
        {"smoothstep", CompletionItem::Function, "(x) -> double", "Smooth step function", "smoothstep(${1:x})", 2},
        {"step", CompletionItem::Function, "(x) -> double", "Step function (0 or 1)", "step(${1:x})", 2},
        {"frac", CompletionItem::Function, "(x) -> double", "Fractional part", "frac(${1:x})", 2},
        {"sign", CompletionItem::Function, "(x) -> double", "Sign of number (-1, 0, or 1)", "sign(${1:x})", 2},
        {"factorial", CompletionItem::Function, "(n) -> double", "Factorial", "factorial(${1:n})", 2},
        {"gcd", CompletionItem::Function, "(a, b) -> double", "Greatest common divisor", "gcd(${1:a}, ${2:b})", 2},
        {"lcm", CompletionItem::Function, "(a, b) -> double", "Least common multiple", "lcm(${1:a}, ${2:b})", 2},
        {"rad2deg", CompletionItem::Function, "(r) -> double", "Radians to degrees", "rad2deg(${1:r})", 2},
        {"deg2rad", CompletionItem::Function, "(d) -> double", "Degrees to radians", "deg2rad(${1:d})", 2},
        {"is_close", CompletionItem::Function, "(a, b) -> double", "Approximate equality check",
         "is_close(${1:a}, ${2:b})", 2},
        // Simulation / SPICE
        {"cross", CompletionItem::Function, "(expr, [rise_fall]) -> double", "Zero-crossing detection",
         "cross(${1:expr})", 2},
        {"above", CompletionItem::Function, "(expr, threshold) -> double", "Threshold detection",
         "above(${1:expr}, ${2:threshold})", 2},
        {"timer", CompletionItem::Function, "() -> double", "Elapsed simulation time", "timer()", 2},
        {"posedge", CompletionItem::Function, "(signal) -> double", "Positive edge detection",
         "posedge(${1:signal})", 2},
        {"negedge", CompletionItem::Function, "(signal) -> double", "Negative edge detection",
         "negedge(${1:signal})", 2},
        // Noise
        {"white_noise", CompletionItem::Function, "(amplitude) -> double", "Gaussian white noise",
         "white_noise(${1:amp})", 2},
        {"flicker_noise", CompletionItem::Function, "(amplitude, corner_freq) -> double", "1/f noise",
         "flicker_noise(${1:amp}, ${2:freq})", 2},
        {"thermal_noise", CompletionItem::Function, "(resistance, [temperature]) -> double",
         "Johnson-Nyquist thermal noise", "thermal_noise(${1:R}, ${2:300.15})", 2},
        // Units
        {"unit", CompletionItem::Function, "(value, unit_str)", "Annotate value with unit",
         "unit(${1:val}, \"${2:V}\")", 2},
        {"convert", CompletionItem::Function, "(value, from, to) -> double", "Unit conversion",
         "convert(${1:val}, \"${2:V}\", \"${3:mV}\")", 2},
        {"dimension", CompletionItem::Function, "(value) -> string", "Dimensional analysis",
         "dimension(${1:val})", 2},
        {"has_unit", CompletionItem::Function, "(value, unit) -> double", "Check unit compatibility",
         "has_unit(${1:val}, \"${2:V}\")", 2},
        // Array / Matrix
        {"linspace", CompletionItem::Function, "(start, stop, n) -> vector", "Evenly spaced values",
         "linspace(${1:0}, ${2:1}, ${3:100})", 2},
        {"logspace", CompletionItem::Function, "(start, stop, n) -> vector", "Logarithmically spaced values",
         "logspace(${1:0}, ${2:3}, ${3:100})", 2},
        {"arange", CompletionItem::Function, "(start, stop, step) -> vector", "Range with step",
         "arange(${1:0}, ${2:10}, ${3:0.1})", 2},
        {"eye", CompletionItem::Function, "(n) -> matrix", "Identity matrix", "eye(${1:n})", 2},
        {"ones", CompletionItem::Function, "(r, c) -> matrix", "Matrix of ones", "ones(${1:r}, ${2:c})", 2},
        {"zeros", CompletionItem::Function, "(r, c) -> matrix", "Matrix of zeros", "zeros(${1:r}, ${2:c})", 2},
        {"sum", CompletionItem::Function, "(m) -> double", "Sum of matrix elements", "sum(${1:m})", 2},
        {"mean", CompletionItem::Function, "(m) -> double", "Mean of matrix elements", "mean(${1:m})", 2},
        {"flatten", CompletionItem::Function, "(m) -> vector", "Flatten matrix to vector", "flatten(${1:m})", 2},
        {"reshape", CompletionItem::Function, "(m, r, c) -> matrix", "Reshape matrix",
         "reshape(${1:m}, ${2:r}, ${3:c})", 2},
        {"diff", CompletionItem::Function, "(m) -> vector", "Finite differences", "diff(${1:m})", 2},
        {"cumsum", CompletionItem::Function, "(m) -> vector", "Cumulative sum", "cumsum(${1:m})", 2},
        {"meshgrid", CompletionItem::Function, "(x, y) -> matrix", "Create coordinate grid",
         "meshgrid(${1:x}, ${2:y})", 2},
        // Stats
        {"variance", CompletionItem::Function, "(m) -> double", "Variance", "variance(${1:m})", 2},
        {"std", CompletionItem::Function, "(m) -> double", "Standard deviation", "std(${1:m})", 2},
        {"median", CompletionItem::Function, "(m) -> double", "Median value", "median(${1:m})", 2},
        {"rms", CompletionItem::Function, "(m) -> double", "Root mean square", "rms(${1:m})", 2},
        {"entropy", CompletionItem::Function, "(p) -> double", "Shannon entropy", "entropy(${1:p})", 2},
        {"normalize_minmax", CompletionItem::Function, "(m) -> matrix", "Min-max normalization",
         "normalize_minmax(${1:m})", 2},
        // Signal processing
        {"hann", CompletionItem::Function, "(n) -> vector", "Hann window", "hann(${1:n})", 2},
        {"hamming", CompletionItem::Function, "(n) -> vector", "Hamming window", "hamming(${1:n})", 2},
        {"blackman", CompletionItem::Function, "(n) -> vector", "Blackman window", "blackman(${1:n})", 2},
        {"square", CompletionItem::Function, "(t, duty) -> double", "Square wave", "square(${1:t}, ${2:0.5})", 2},
        {"sawtooth", CompletionItem::Function, "(t) -> double", "Sawtooth wave", "sawtooth(${1:t})", 2},
        {"triangle", CompletionItem::Function, "(t) -> double", "Triangle wave", "triangle(${1:t})", 2},
        {"chirp", CompletionItem::Function, "(t, f0, f1, t1) -> double", "Chirp signal",
         "chirp(${1:t}, ${2:f0}, ${3:f1}, ${4:t1})", 2},
        {"unwrap", CompletionItem::Function, "(phase) -> vector", "Unwrap phase discontinuities",
         "unwrap(${1:phase})", 2},
        // Piecewise / Table
        {"piecewise", CompletionItem::Function, "(x1,y1, x2,y2, ..., x) -> double", "Piecewise linear interpolation",
         "piecewise(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:x})", 2},
        {"table", CompletionItem::Function, "(k1,v1, k2,v2, ..., key) -> double", "Lookup table",
         "table(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:key})", 2},
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
        {"async-func", CompletionItem::Snippet, "Async function", "async def name(args) body",
         "async def ${1:name}(${2:args}) ${3:body}", 2},
        {"forloop", CompletionItem::Snippet, "For loop", "for i in start, end do body",
         "for ${1:i} in ${2:1}, ${3:10} do ${4:body}", 2},
        {"whileloop", CompletionItem::Snippet, "While loop", "while cond do body",
         "while ${1:cond} do ${2:body}", 2},
        {"ifelse", CompletionItem::Snippet, "If-else", "if cond then a else b",
         "if ${1:cond} then ${2:a} else ${3:b}", 2},
        {"ifdo", CompletionItem::Snippet, "If-do block", "if cond do body",
         "if ${1:cond} do ${2:body}", 2},
        {"match", CompletionItem::Snippet, "Match expression", "match expr with cases",
         "match ${1:expr} {\n  ${2:pattern} => ${3:result}\n}", 2},
        {"trycatch", CompletionItem::Snippet, "Try-catch", "try body catch handler",
         "try ${1:body} catch ${2:handler}", 2},
        {"class", CompletionItem::Snippet, "Class definition", "class Name { fields; methods }",
         "class ${1:Name} {\n  ${2:field}: ${3:type}\n\n  def ${4:method}(self) {\n    ${5:// body}\n  }\n}", 2},
        {"struct", CompletionItem::Snippet, "Struct definition", "struct Name { fields }",
         "struct ${1:Name} {\n  ${2:field}: ${3:type}\n}", 2},
        {"enum", CompletionItem::Snippet, "Enum definition", "enum Name { Variants }",
         "enum ${1:Name} {\n  ${2:Variant1}\n  ${3:Variant2}(type)\n}", 2},
        {"trait", CompletionItem::Snippet, "Trait definition", "trait Name { methods }",
         "trait ${1:Name} {\n  def ${2:method}(self) ${3:-> ReturnType}\n}", 2},
        {"impl", CompletionItem::Snippet, "Impl block", "impl Name { methods }",
         "impl ${1:Name} {\n  def ${2:method}(self) {\n    ${3:// body}\n  }\n}", 2},
        {"impl-trait", CompletionItem::Snippet, "Impl trait for type", "impl Trait for Type { ... }",
         "impl ${1:Trait} for ${2:Type} {\n  ${3:// methods}\n}", 2},
        {"lambda", CompletionItem::Snippet, "Lambda expression", "fn(args) -> body",
         "fn(${1:args}) -> ${2:body}", 2},
        {"parallel", CompletionItem::Snippet, "Parallel for loop", "parallel for i in range do body",
         "parallel for ${1:i} in ${2:start}, ${3:end} do ${4:body}", 2},
        {"bsource", CompletionItem::Snippet, "Behavioral source", "bsource B name n+ n- V={expr}",
         "bsource ${1:B1} ${2:out} ${3:0} V={${4:expression}}", 2},
        {"montecarlo", CompletionItem::Snippet, "Monte Carlo analysis", "montecarlo output iterations",
         "montecarlo ${1:output} ${2:1000}", 2},
        {"piecewise", CompletionItem::Snippet, "Piecewise function", "piecewise(x1,y1, x2,y2, ...)",
         "piecewise(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:x})", 2},
        {"table", CompletionItem::Snippet, "Lookup table", "table(k1,v1, k2,v2, key)",
         "table(${1:0.0}, ${2:0.0}, ${3:1.0}, ${4:1.0}, ${5:key})", 2},
        {"import", CompletionItem::Snippet, "Import module", "import module",
         "import ${1:math}", 2},
        {"from-import", CompletionItem::Snippet, "From-import", "from module import symbol",
         "from ${1:math} import ${2:pi}", 2},
        {"extern", CompletionItem::Snippet, "External declaration", "extern name(args)",
         "extern ${1:name}(${2:args})", 2},
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

    // Check if cursor is on an import statement: "import <word>"
    // Or on a module prefix in "module::function" syntax
    {
        std::string lineText = doc->getLine(pos.line);

        // Helper to resolve a module name to a file URI
                auto resolveModule = [&](const std::string& moduleName) -> bool {
                    std::vector<std::string> searchPaths = {".", "modules", "lib", "stdlib"};
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
                    std::string docDir;
                    size_t lastSlash = uri.rfind('/');
                    if (lastSlash != std::string::npos)
                        docDir = uri.substr(7, lastSlash - 7);
                    if (!docDir.empty())
                        searchPaths.insert(searchPaths.begin(), docDir);

                    for (auto& sp : searchPaths) {
                        std::string filePath = sp + "/" + moduleName + ".flux";
                        std::ifstream ifs(filePath);
                        if (ifs.good()) {
                            loc.uri = "file://" + filePath;
                            loc.range.start = {0, 0};
                            loc.range.end = {0, 0};
                            return true;
                        }
                    }
                    return false;
                };

        // Check for "import <word>" on this line
        size_t importPos = lineText.find("import ");
        if (importPos != std::string::npos) {
            size_t nameStart = importPos + 7;
            while (nameStart < lineText.size() && (lineText[nameStart] == ' ' || lineText[nameStart] == '\t'))
                nameStart++;
            size_t nameEnd = nameStart;
            while (nameEnd < lineText.size() && (isalnum(lineText[nameEnd]) || lineText[nameEnd] == '_'))
                nameEnd++;
            if (nameEnd > nameStart && pos.character >= nameStart && pos.character <= nameEnd) {
                std::string moduleName = lineText.substr(nameStart, nameEnd - nameStart);
                if (resolveModule(moduleName))
                    return loc;
            }
        }

        // Check for "module::submodule::" — cursor on any part of a qualified name
        {
            // Find the last :: on this line
            size_t lastNsSep = std::string::npos;
            size_t searchPos = 0;
            while (searchPos < lineText.size()) {
                size_t nsSep = lineText.find("::", searchPos);
                if (nsSep == std::string::npos)
                    break;
                lastNsSep = nsSep;
                searchPos = nsSep + 2;
            }
            if (lastNsSep != std::string::npos) {
                // Extract the full qualified module path before the last ::
                size_t qualEnd = lastNsSep;
                size_t qualStart = qualEnd;
                while (qualStart > 0 && (isalnum(lineText[qualStart - 1]) || lineText[qualStart - 1] == '_'))
                    qualStart--;
                while (qualStart >= 2 && lineText[qualStart - 1] == ':' && lineText[qualStart - 2] == ':') {
                    qualStart -= 2;
                    while (qualStart > 0 && (isalnum(lineText[qualStart - 1]) || lineText[qualStart - 1] == '_'))
                        qualStart--;
                }
                // Check if cursor is anywhere within the qualified name (module path + :: + function name)
                size_t funcStart = lastNsSep + 2;
                size_t funcEnd = funcStart;
                while (funcEnd < lineText.size() && (isalnum(lineText[funcEnd]) || lineText[funcEnd] == '_'))
                    funcEnd++;
                if (qualStart < funcEnd && pos.character >= qualStart && pos.character <= funcEnd) {
                    std::string modulePath = lineText.substr(qualStart, qualEnd - qualStart);
                    std::string slashPath = modulePath;
                    size_t pos2 = 0;
                    while ((pos2 = slashPath.find("::", pos2)) != std::string::npos)
                        slashPath.replace(pos2, 2, "/");

                    if (resolveModule(slashPath))
                        return loc;
                    size_t lastColon = modulePath.rfind("::");
                    if (lastColon != std::string::npos) {
                        std::string lastSeg = modulePath.substr(lastColon + 2);
                        if (resolveModule(lastSeg))
                            return loc;
                    }
                }
            }
        }
    }

    auto symbols = m_symbolTables.count(uri) ? m_symbolTables[uri] : buildSymbolTable(uri);
    // Prefer imported symbols (different URI) over local usages for go-to-definition
    for (auto& sym : symbols) {
        if (sym.name == word && sym.uri != uri && !sym.uri.empty()) {
            loc.uri = sym.uri;
            loc.range = sym.range;
            return loc;
        }
    }
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
