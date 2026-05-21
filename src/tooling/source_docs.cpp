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

#include "flux/tooling/source_docs.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace Flux {
namespace Tooling {

SourceDocGenerator::SourceDocGenerator()
{
    m_apiDoc.projectName = "FluxScript";
    m_apiDoc.version = "1.0.0";
}

bool SourceDocGenerator::parseSource(const std::string& sourceCode, const std::string& filename)
{
    m_currentFile = filename;
    m_pendingDocComments.clear();

    extractDocComments(sourceCode);
    extractFunctions(sourceCode);
    extractTypes(sourceCode);
    extractExamples(sourceCode);

    return true;
}

bool SourceDocGenerator::parseFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
        return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return parseSource(content, filename);
}

// ============================================================================
// Doc Comment Extraction
// ============================================================================

void SourceDocGenerator::extractDocComments(const std::string& source)
{
    std::istringstream stream(source);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        // Match /// doc comment
        if (line.find("///") == 0 || (line.size() > 1 && line[0] == '/' && line[1] == '*' && line.find("/**") == 0)) {
            DocComment comment;
            comment.line = lineNum;

            // Strip comment markers
            std::string text = line;
            if (text.find("///") == 0) {
                text = text.substr(3);
            } else if (text.find("/**") == 0) {
                text = text.substr(3);
                // Handle multi-line block comments
                if (text.find("*/") != std::string::npos) {
                    text = text.substr(0, text.find("*/"));
                }
            }

            // Trim leading/trailing whitespace
            size_t start = text.find_first_not_of(" \t");
            if (start != std::string::npos) {
                text = text.substr(start);
                size_t end = text.find_last_not_of(" \t\r\n");
                if (end != std::string::npos)
                    text = text.substr(0, end + 1);
            }

            comment.text = text;
            m_pendingDocComments[lineNum] = comment;
        }
    }
}

// ============================================================================
// Function Extraction
// ============================================================================

void SourceDocGenerator::extractFunctions(const std::string& source)
{
    std::istringstream stream(source);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        // Match function definition: def name(args) body
        // or: extern name(args)
        size_t defPos = line.find("def ");
        size_t externPos = line.find("extern ");

        size_t keywordPos =
            (defPos != std::string::npos) ? defPos : ((externPos != std::string::npos) ? externPos : std::string::npos);

        if (keywordPos == std::string::npos)
            continue;

        std::string keyword = (defPos != std::string::npos) ? "def" : "extern";

        // Extract function name
        size_t nameStart = keywordPos + keyword.size() + 1;
        while (nameStart < line.size() && (line[nameStart] == ' ' || line[nameStart] == '\t'))
            nameStart++;

        size_t nameEnd = line.find('(', nameStart);
        if (nameEnd == std::string::npos)
            continue;

        std::string funcName = line.substr(nameStart, nameEnd - nameStart);

        // Extract parameters
        size_t parenEnd = line.find(')', nameEnd);
        std::string paramStr;
        if (parenEnd != std::string::npos) {
            paramStr = line.substr(nameEnd + 1, parenEnd - nameEnd - 1);
        }

        std::vector<std::string> params;
        if (!paramStr.empty()) {
            std::istringstream paramStream(paramStr);
            std::string param;
            while (std::getline(paramStream, param, ',')) {
                // Trim
                size_t start = param.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    params.push_back(param.substr(start));
                }
            }
        }

        // Find return type if present (-> type)
        std::string returnType;
        size_t arrowPos = line.find("->");
        if (arrowPos != std::string::npos) {
            returnType = line.substr(arrowPos + 2);
            size_t end = returnType.find_first_of(" \t{;");
            if (end != std::string::npos)
                returnType = returnType.substr(0, end);
            // Trim
            size_t start = returnType.find_first_not_of(" \t");
            if (start != std::string::npos)
                returnType = returnType.substr(start);
        } else {
            returnType = "double"; // Default
        }

        // Build signature
        std::string signature = keyword + " " + funcName + "(" + paramStr + ")";
        if (!returnType.empty())
            signature += " -> " + returnType;

        // Build function doc
        FunctionDoc funcDoc;
        funcDoc.name = funcName;
        funcDoc.signature = signature;
        funcDoc.parameters = params;
        funcDoc.returnType = returnType;

        // Look for doc comment on previous line(s)
        for (int i = lineNum - 1; i >= 1; --i) {
            auto it = m_pendingDocComments.find(i);
            if (it != m_pendingDocComments.end()) {
                if (funcDoc.docComment.text.empty()) {
                    funcDoc.docComment = it->second;
                } else {
                    funcDoc.docComment.text = it->second.text + "\n" + funcDoc.docComment.text;
                }
            } else {
                break; // Stop at first non-doc line
            }
        }

        // Check for @param, @return, @example tags
        std::istringstream docStream(funcDoc.docComment.text);
        std::string docLine;
        while (std::getline(docStream, docLine)) {
            if (docLine.find("@example") == 0 || docLine.find("@code") == 0) {
                funcDoc.examples.push_back(docLine.substr(7));
            } else if (docLine.find("@deprecated") == 0) {
                funcDoc.deprecated = docLine.substr(12);
            }
        }

        m_apiDoc.topLevelFunctions.push_back(funcDoc);
    }
}

// ============================================================================
// Type Extraction
// ============================================================================

void SourceDocGenerator::extractTypes(const std::string& source)
{
    std::istringstream stream(source);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        // Match type declarations: struct Name, enum Name, alias Name = ...
        if (line.find("struct ") == 0 || line.find("enum ") == 0) {
            std::string kind = (line.find("struct ") == 0) ? "struct" : "enum";
            size_t nameStart = line.find(' ') + 1;
            size_t nameEnd = line.find_first_of("{ \t:;", nameStart);

            std::string typeName = line.substr(nameStart, nameEnd - nameStart);

            TypeDoc typeDoc;
            typeDoc.name = typeName;
            typeDoc.kind = kind;

            // Look for doc comment
            for (int i = lineNum - 1; i >= 1; --i) {
                auto it = m_pendingDocComments.find(i);
                if (it != m_pendingDocComments.end()) {
                    typeDoc.docComment = it->second;
                    break;
                } else {
                    break;
                }
            }

            m_apiDoc.topLevelTypes.push_back(typeDoc);
        }
    }
}

// ============================================================================
// Example Extraction
// ============================================================================

void SourceDocGenerator::extractExamples(const std::string& source)
{
    // Extract @example blocks from doc comments
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line)) {
        size_t examplePos = line.find("@example");
        if (examplePos == std::string::npos)
            continue;

        // This is handled in function extraction
    }
}

// ============================================================================
// Query Functions
// ============================================================================

std::vector<FunctionDoc> SourceDocGenerator::getFunctions() const
{
    return m_apiDoc.topLevelFunctions;
}

std::vector<TypeDoc> SourceDocGenerator::getTypes() const
{
    return m_apiDoc.topLevelTypes;
}

const FunctionDoc* SourceDocGenerator::findFunction(const std::string& name) const
{
    for (auto& func : m_apiDoc.topLevelFunctions) {
        if (func.name == name)
            return &func;
    }
    return nullptr;
}

const TypeDoc* SourceDocGenerator::findType(const std::string& name) const
{
    for (auto& type : m_apiDoc.topLevelTypes) {
        if (type.name == name)
            return &type;
    }
    return nullptr;
}

// ============================================================================
// Output Generation
// ============================================================================

std::string SourceDocGenerator::generateMarkdown() const
{
    std::ostringstream oss;

    oss << "# " << m_apiDoc.projectName << " API Reference\n\n";
    oss << "**Version:** " << m_apiDoc.version << "\n\n";

    // Table of contents
    oss << "## Table of Contents\n\n";
    oss << "- [Functions](#functions)\n";
    oss << "- [Types](#types)\n\n";

    // Functions
    oss << "## Functions\n\n";
    for (auto& func : m_apiDoc.topLevelFunctions) {
        oss << generateFunctionDoc(func, "md") << "\n";
    }

    // Types
    oss << "## Types\n\n";
    for (auto& type : m_apiDoc.topLevelTypes) {
        oss << generateTypeDoc(type, "md") << "\n";
    }

    return oss.str();
}

std::string SourceDocGenerator::generateHtml() const
{
    std::ostringstream oss;

    oss << "<!DOCTYPE html>\n<html>\n<head>\n";
    oss << "<title>" << m_apiDoc.projectName << " API Reference</title>\n";
    oss << "<style>\n";
    oss << "body { font-family: -apple-system, sans-serif; max-width: 900px; margin: 0 auto; padding: 20px; }\n";
    oss << "h1 { border-bottom: 2px solid #333; }\n";
    oss << "h2 { color: #333; }\n";
    oss << "h3 { color: #666; }\n";
    oss << "code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }\n";
    oss << "pre { background: #f4f4f4; padding: 15px; border-radius: 5px; overflow-x: auto; }\n";
    oss << ".func-sig { background: #e8e8e8; padding: 10px; border-left: 3px solid #0066cc; }\n";
    oss << ".deprecated { color: #999; text-decoration: line-through; }\n";
    oss << "table { border-collapse: collapse; width: 100%; }\n";
    oss << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    oss << "th { background: #f4f4f4; }\n";
    oss << "</style>\n</head>\n<body>\n";

    oss << "<h1>" << m_apiDoc.projectName << " API Reference</h1>\n";
    oss << "<p><strong>Version:</strong> " << m_apiDoc.version << "</p>\n\n";

    oss << "<h2>Functions</h2>\n";
    for (auto& func : m_apiDoc.topLevelFunctions) {
        oss << "<h3 id=\"func-" << func.name << "\">" << func.name << "</h3>\n";
        oss << "<div class=\"func-sig\"><code>" << func.signature << "</code></div>\n";
        if (!func.docComment.text.empty()) {
            oss << "<p>" << func.docComment.text << "</p>\n";
        }
        if (!func.parameters.empty()) {
            oss << "<h4>Parameters</h4>\n<ul>\n";
            for (auto& param : func.parameters) {
                oss << "<li><code>" << param << "</code></li>\n";
            }
            oss << "</ul>\n";
        }
        oss << "<p><strong>Returns:</strong> <code>" << func.returnType << "</code></p>\n";
        if (!func.deprecated.empty()) {
            oss << "<p class=\"deprecated\">Deprecated: " << func.deprecated << "</p>\n";
        }
        oss << "<hr>\n";
    }

    oss << "<h2>Types</h2>\n";
    for (auto& type : m_apiDoc.topLevelTypes) {
        oss << "<h3 id=\"type-" << type.name << "\">" << type.name << "</h3>\n";
        oss << "<p><code>" << type.kind << "</code></p>\n";
        if (!type.docComment.text.empty()) {
            oss << "<p>" << type.docComment.text << "</p>\n";
        }
        oss << "<hr>\n";
    }

    oss << "</body>\n</html>\n";

    return oss.str();
}

std::string SourceDocGenerator::generateRst() const
{
    std::ostringstream oss;

    oss << m_apiDoc.projectName << " API Reference\n";
    oss << std::string(m_apiDoc.projectName.size() + 16, '=') << "\n\n";
    oss << "**Version:** " << m_apiDoc.version << "\n\n";

    oss << "Functions\n";
    oss << "---------\n\n";

    for (auto& func : m_apiDoc.topLevelFunctions) {
        oss << ".. function:: " << func.signature << "\n\n";
        if (!func.docComment.text.empty()) {
            oss << "   " << func.docComment.text << "\n\n";
        }
        if (!func.parameters.empty()) {
            oss << "   :param " << func.parameters[0] << "\n";
        }
        oss << "   :returns: " << func.returnType << "\n\n";
    }

    oss << "Types\n";
    oss << "-----\n\n";

    for (auto& type : m_apiDoc.topLevelTypes) {
        oss << ".. " << type.kind << ":: " << type.name << "\n\n";
        if (!type.docComment.text.empty()) {
            oss << "   " << type.docComment.text << "\n\n";
        }
    }

    return oss.str();
}

std::string SourceDocGenerator::generatePlainText() const
{
    std::ostringstream oss;

    oss << m_apiDoc.projectName << " API Reference (v" << m_apiDoc.version << ")\n";
    oss << "==========================================================\n\n";

    oss << "FUNCTIONS\n";
    oss << "---------\n\n";

    for (auto& func : m_apiDoc.topLevelFunctions) {
        oss << func.name << "\n";
        oss << "  Signature: " << func.signature << "\n";
        if (!func.docComment.text.empty()) {
            oss << "  Description: " << func.docComment.text << "\n";
        }
        if (!func.parameters.empty()) {
            oss << "  Parameters: ";
            for (size_t i = 0; i < func.parameters.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << func.parameters[i];
            }
            oss << "\n";
        }
        oss << "  Returns: " << func.returnType << "\n";
        if (!func.deprecated.empty()) {
            oss << "  DEPRECATED: " << func.deprecated << "\n";
        }
        oss << "\n";
    }

    oss << "TYPES\n";
    oss << "-----\n\n";

    for (auto& type : m_apiDoc.topLevelTypes) {
        oss << type.name << " (" << type.kind << ")\n";
        if (!type.docComment.text.empty()) {
            oss << "  Description: " << type.docComment.text << "\n";
        }
        oss << "\n";
    }

    return oss.str();
}

std::string SourceDocGenerator::generateJson() const
{
    std::ostringstream oss;

    oss << "{\n";
    oss << "  \"project\": \"" << m_apiDoc.projectName << "\",\n";
    oss << "  \"version\": \"" << m_apiDoc.version << "\",\n";
    oss << "  \"functions\": [\n";

    for (size_t i = 0; i < m_apiDoc.topLevelFunctions.size(); ++i) {
        auto& func = m_apiDoc.topLevelFunctions[i];
        if (i > 0)
            oss << ",\n";
        oss << "    {\n";
        oss << "      \"name\": \"" << func.name << "\",\n";
        oss << "      \"signature\": \"" << func.signature << "\",\n";
        oss << "      \"description\": \"" << func.docComment.text << "\",\n";
        oss << "      \"parameters\": [";
        for (size_t j = 0; j < func.parameters.size(); ++j) {
            if (j > 0)
                oss << ", ";
            oss << "\"" << func.parameters[j] << "\"";
        }
        oss << "],\n";
        oss << "      \"returnType\": \"" << func.returnType << "\"\n";
        oss << "    }";
    }

    oss << "\n  ]\n";
    oss << "}\n";

    return oss.str();
}

std::string SourceDocGenerator::generateFunctionDoc(const FunctionDoc& func, const std::string& format) const
{
    if (format == "md") {
        std::ostringstream oss;
        oss << "### `" << func.name << "`\n\n";
        oss << "```fluxscript\n" << func.signature << "\n```\n\n";
        if (!func.docComment.text.empty()) {
            oss << func.docComment.text << "\n\n";
        }
        if (!func.parameters.empty()) {
            oss << "**Parameters:**\n";
            for (auto& param : func.parameters) {
                oss << "- `" << param << "`\n";
            }
            oss << "\n";
        }
        oss << "**Returns:** `" << func.returnType << "`\n";
        if (!func.deprecated.empty()) {
            oss << "\n>  **Deprecated:** " << func.deprecated << "\n";
        }
        return oss.str();
    }
    return func.signature;
}

std::string SourceDocGenerator::generateTypeDoc(const TypeDoc& type, const std::string& format) const
{
    if (format == "md") {
        std::ostringstream oss;
        oss << "### `" << type.name << "`\n\n";
        oss << "`" << type.kind << "`\n\n";
        if (!type.docComment.text.empty()) {
            oss << type.docComment.text << "\n\n";
        }
        return oss.str();
    }
    return type.name + " (" + type.kind + ")";
}

} // namespace Tooling
} // namespace Flux
