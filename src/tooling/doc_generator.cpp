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

// ============================================================================
// FluxScript Documentation Generator from Source Comments
// Parses /// and /** */ doc comments and generates API documentation
// ============================================================================

#include "flux/tooling/doc_generator.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Flux {

// ============================================================================
// DocGenerator Singleton
// ============================================================================

DocGenerator& DocGenerator::instance()
{
    static DocGenerator instance;
    return instance;
}

void DocGenerator::initialize()
{
    std::cout << "[DocGenerator] Initialized" << std::endl;
}

void DocGenerator::finalize()
{
    std::cout << "[DocGenerator] Finalized" << std::endl;
}

// ============================================================================
// Source File Parsing
// ============================================================================

DocOutput DocGenerator::parseSourceFile(const std::string& source_code, const std::string& filename)
{
    DocOutput doc;
    doc.title = "FluxScript API Documentation";
    doc.version = "1.0";

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    doc.generated_date = ss.str();

    doc.items = extractDocItems(source_code, filename);

    std::cout << "[DocGenerator] Parsed " << doc.items.size() << " items from " << filename << std::endl;
    return doc;
}

DocOutput DocGenerator::parseSourceFiles(const std::vector<std::pair<std::string, std::string>>& files)
{
    DocOutput doc;
    doc.title = "FluxScript API Documentation";
    doc.version = "1.0";

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    doc.generated_date = ss.str();

    for (const auto& [source, filename] : files) {
        auto items = extractDocItems(source, filename);
        doc.items.insert(doc.items.end(), items.begin(), items.end());
    }

    std::cout << "[DocGenerator] Parsed " << doc.items.size() << " items from " << files.size() << " files"
              << std::endl;
    return doc;
}

std::vector<DocItem> DocGenerator::extractDocItems(const std::string& source, const std::string& filename)
{
    std::vector<DocItem> items;
    size_t pos = 0;

    while (pos < source.length()) {
        // Look for doc comments: /// or /**
        if (source.substr(pos, 3) == "///") {
            int line = 1;
            for (size_t i = 0; i < pos && i < source.length(); i++) {
                if (source[i] == '\n')
                    line++;
            }

            std::string comment = extractDocComment(source, pos, line);
            DocItem item = parseDocComment(comment, filename, line);

            // Extract the identifier after the comment
            size_t id_pos = pos + comment.length();
            while (id_pos < source.length() &&
                   (source[id_pos] == ' ' || source[id_pos] == '\t' || source[id_pos] == '\n')) {
                id_pos++;
            }

            if (id_pos < source.length()) {
                item.name = extractIdentifier(source, id_pos);
                item.kind = determineKind(source, id_pos);
                item.signatures.push_back(extractSignature(source, id_pos));
                items.push_back(item);
            }
        } else if (source.substr(pos, 2) == "/**") {
            int line = 1;
            for (size_t i = 0; i < pos && i < source.length(); i++) {
                if (source[i] == '\n')
                    line++;
            }

            size_t end = source.find("*/", pos + 2);
            if (end == std::string::npos)
                break;

            std::string comment = source.substr(pos, end - pos + 2);
            DocItem item = parseDocComment(comment, filename, line);

            // Extract identifier after comment
            size_t id_pos = end + 2;
            while (id_pos < source.length() &&
                   (source[id_pos] == ' ' || source[id_pos] == '\t' || source[id_pos] == '\n')) {
                id_pos++;
            }

            if (id_pos < source.length()) {
                item.name = extractIdentifier(source, id_pos);
                item.kind = determineKind(source, id_pos);
                item.signatures.push_back(extractSignature(source, id_pos));
                items.push_back(item);
            }
        }

        pos++;
    }

    return items;
}

std::string DocGenerator::extractDocComment(const std::string& source, size_t pos, int& line)
{
    if (source.substr(pos, 3) == "///") {
        // Single-line doc comment
        size_t end = source.find('\n', pos);
        if (end == std::string::npos)
            end = source.length();
        return source.substr(pos, end - pos);
    } else if (source.substr(pos, 2) == "/**") {
        // Multi-line doc comment
        size_t end = source.find("*/", pos + 2);
        if (end == std::string::npos)
            end = source.length();
        else
            end += 2;
        return source.substr(pos, end - pos);
    }
    return "";
}

DocItem DocGenerator::parseDocComment(const std::string& comment, const std::string& filename, int line)
{
    DocItem item;
    item.file = filename;
    item.line_number = line;

    std::istringstream stream(comment);
    std::string line_str;
    bool in_description = false;
    std::string description;

    while (std::getline(stream, line_str)) {
        // Strip leading comment markers
        size_t start = 0;
        if (line_str.find("///") == 0) {
            start = 3;
        } else if (line_str.find("/**") == 0) {
            start = 3;
        } else if (line_str.find("*/") != std::string::npos) {
            continue; // End marker
        }

        // Strip leading whitespace and *
        while (start < line_str.length() &&
               (line_str[start] == ' ' || line_str[start] == '*' || line_str[start] == '\t')) {
            start++;
        }

        std::string content = line_str.substr(start);

        if (content.empty())
            continue;

        // Check for tags (@param, @return, @example, @see, etc.)
        if (content[0] == '@') {
            in_description = false;
            size_t space_pos = content.find(' ');
            std::string tag_name = content.substr(0, space_pos);
            std::string tag_value = (space_pos != std::string::npos) ? content.substr(space_pos + 1) : "";

            if (tag_name == "@param" || tag_name == "@parameter") {
                size_t param_space = tag_value.find(' ');
                if (param_space != std::string::npos) {
                    std::string param_name = tag_value.substr(0, param_space);
                    std::string param_desc = tag_value.substr(param_space + 1);
                    item.parameters[param_name] = param_desc;
                }
            } else if (tag_name == "@return" || tag_name == "@returns") {
                item.return_type = tag_value;
            } else if (tag_name == "@example") {
                item.examples.push_back(tag_value);
            } else if (tag_name == "@see" || tag_name == "@sa") {
                DocTag tag;
                tag.name = "see";
                tag.value = tag_value;
                item.tags.push_back(tag);
            } else {
                DocTag tag;
                tag.name = tag_name;
                tag.value = tag_value;
                item.tags.push_back(tag);
            }
        } else if (!in_description && item.summary.empty()) {
            // First non-tag line is the summary
            item.summary = content;
            in_description = true;
        } else if (in_description) {
            if (!description.empty())
                description += "\n";
            description += content;
        }
    }

    item.description = description;
    return item;
}

std::string DocGenerator::extractIdentifier(const std::string& source, size_t pos)
{
    std::string ident;
    while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
        ident += source[pos];
        pos++;
    }
    return ident;
}

std::string DocGenerator::extractSignature(const std::string& source, size_t pos)
{
    // Skip identifier
    while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
        pos++;
    }

    // Skip whitespace
    while (pos < source.length() && (source[pos] == ' ' || source[pos] == '\t')) {
        pos++;
    }

    // Extract until { or ; or newline
    std::string sig;
    int paren_depth = 0;
    while (pos < source.length()) {
        char c = source[pos];
        if (c == '{' && paren_depth == 0)
            break;
        if (c == ';' && paren_depth == 0)
            break;
        if (c == '\n' && paren_depth == 0)
            break;
        if (c == '(')
            paren_depth++;
        if (c == ')')
            paren_depth--;
        sig += c;
        pos++;
    }

    return sig;
}

std::string DocGenerator::determineKind(const std::string& source, size_t pos)
{
    // Look backwards for keyword
    size_t search_start = (pos > 100) ? pos - 100 : 0;
    std::string context = source.substr(search_start, pos - search_start);

    if (context.find("def ") != std::string::npos || context.find("fn ") != std::string::npos) {
        return "function";
    } else if (context.find("let ") != std::string::npos || context.find("var ") != std::string::npos) {
        return "variable";
    } else if (context.find("class ") != std::string::npos) {
        return "class";
    } else if (context.find("module ") != std::string::npos || context.find("import ") != std::string::npos) {
        return "module";
    }

    return "unknown";
}

// ============================================================================
// Documentation Generation
// ============================================================================

std::string DocGenerator::generateMarkdown(const DocOutput& doc)
{
    std::ostringstream oss;

    oss << "# " << doc.title << "\n\n";
    oss << "**Version:** " << doc.version << "  \n";
    oss << "**Generated:** " << doc.generated_date << "\n\n";
    oss << "---\n\n";

    // Table of contents
    oss << "## Table of Contents\n\n";
    for (const auto& item : doc.items) {
        oss << "- [" << item.name << "](#" << item.name << ")";
        if (!item.summary.empty()) {
            oss << " - " << item.summary;
        }
        oss << "\n";
    }
    oss << "\n---\n\n";

    // Items
    for (const auto& item : doc.items) {
        oss << "## " << item.name << "\n\n";

        if (!item.summary.empty()) {
            oss << "**Summary:** " << item.summary << "\n\n";
        }

        if (!item.signatures.empty()) {
            oss << "### Signature\n\n";
            oss << "```fluxscript\n" << item.signatures[0] << "\n```\n\n";
        }

        if (!item.description.empty()) {
            oss << "### Description\n\n" << item.description << "\n\n";
        }

        if (!item.parameters.empty()) {
            oss << "### Parameters\n\n";
            for (const auto& [param, desc] : item.parameters) {
                oss << "- `" << param << "`: " << desc << "\n";
            }
            oss << "\n";
        }

        if (!item.return_type.empty()) {
            oss << "### Returns\n\n" << item.return_type << "\n\n";
        }

        if (!item.examples.empty()) {
            oss << "### Examples\n\n";
            for (const auto& example : item.examples) {
                oss << "```fluxscript\n" << example << "\n```\n\n";
            }
        }

        if (!item.tags.empty()) {
            oss << "### See Also\n\n";
            for (const auto& tag : item.tags) {
                if (tag.name == "see") {
                    oss << "- [" << tag.value << "](#" << tag.value << ")\n";
                }
            }
            oss << "\n";
        }

        oss << "---\n\n";
    }

    return oss.str();
}

std::string DocGenerator::generateHTML(const DocOutput& doc)
{
    std::ostringstream oss;

    oss << "<!DOCTYPE html>\n<html>\n<head>\n";
    oss << "<title>" << doc.title << "</title>\n";
    oss << "<style>\n";
    oss << "body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }\n";
    oss << "h1 { color: #333; }\n";
    oss << "h2 { color: #666; border-bottom: 1px solid #ddd; padding-bottom: 5px; }\n";
    oss << "h3 { color: #888; }\n";
    oss << "pre { background: #f5f5f5; padding: 10px; border-radius: 5px; overflow-x: auto; }\n";
    oss << "code { background: #f5f5f5; padding: 2px 5px; border-radius: 3px; }\n";
    oss << ".item { margin: 30px 0; }\n";
    oss << ".summary { color: #666; font-style: italic; }\n";
    oss << ".parameters { margin: 10px 0; }\n";
    oss << "</style>\n</head>\n<body>\n";

    oss << "<h1>" << doc.title << "</h1>\n";
    oss << "<p><strong>Version:</strong> " << doc.version << "<br>\n";
    oss << "<strong>Generated:</strong> " << doc.generated_date << "</p>\n";
    oss << "<hr>\n";

    // TOC
    oss << "<h2>Table of Contents</h2>\n<ul>\n";
    for (const auto& item : doc.items) {
        oss << "<li><a href=\"#" << item.name << "\">" << item.name << "</a>";
        if (!item.summary.empty()) {
            oss << " - <span class=\"summary\">" << item.summary << "</span>";
        }
        oss << "</li>\n";
    }
    oss << "</ul>\n<hr>\n";

    // Items
    for (const auto& item : doc.items) {
        oss << "<div class=\"item\">\n";
        oss << "<h2 id=\"" << item.name << "\">" << item.name << "</h2>\n";

        if (!item.summary.empty()) {
            oss << "<p class=\"summary\">" << item.summary << "</p>\n";
        }

        if (!item.signatures.empty()) {
            oss << "<h3>Signature</h3>\n";
            oss << "<pre>" << item.signatures[0] << "</pre>\n";
        }

        if (!item.description.empty()) {
            oss << "<h3>Description</h3>\n";
            oss << "<p>" << item.description << "</p>\n";
        }

        if (!item.parameters.empty()) {
            oss << "<h3>Parameters</h3>\n<div class=\"parameters\">\n<ul>\n";
            for (const auto& [param, desc] : item.parameters) {
                oss << "<li><code>" << param << "</code>: " << desc << "</li>\n";
            }
            oss << "</ul>\n</div>\n";
        }

        if (!item.return_type.empty()) {
            oss << "<h3>Returns</h3>\n<p>" << item.return_type << "</p>\n";
        }

        if (!item.examples.empty()) {
            oss << "<h3>Examples</h3>\n";
            for (const auto& example : item.examples) {
                oss << "<pre>" << example << "</pre>\n";
            }
        }

        oss << "</div>\n<hr>\n";
    }

    oss << "</body>\n</html>";
    return oss.str();
}

std::string DocGenerator::generateJSON(const DocOutput& doc)
{
    std::ostringstream oss;

    oss << "{\n";
    oss << "  \"title\": \"" << doc.title << "\",\n";
    oss << "  \"version\": \"" << doc.version << "\",\n";
    oss << "  \"generated_date\": \"" << doc.generated_date << "\",\n";
    oss << "  \"items\": [\n";

    for (size_t i = 0; i < doc.items.size(); i++) {
        const auto& item = doc.items[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << item.name << "\",\n";
        oss << "      \"kind\": \"" << item.kind << "\",\n";
        oss << "      \"summary\": \"" << item.summary << "\",\n";
        oss << "      \"signature\": \"" << item.signatures[0] << "\",\n";
        oss << "      \"file\": \"" << item.file << "\",\n";
        oss << "      \"line\": " << item.line_number << "\n";
        oss << "    }";
        if (i < doc.items.size() - 1)
            oss << ",";
        oss << "\n";
    }

    oss << "  ]\n";
    oss << "}\n";

    return oss.str();
}

std::string DocGenerator::generateText(const DocOutput& doc)
{
    std::ostringstream oss;

    oss << doc.title << "\n";
    oss << std::string(doc.title.length(), '=') << "\n\n";
    oss << "Version: " << doc.version << "  |  Generated: " << doc.generated_date << "\n\n";

    for (const auto& item : doc.items) {
        oss << item.name << " (" << item.kind << ")\n";
        oss << std::string(item.name.length(), '-') << "\n";

        if (!item.summary.empty()) {
            oss << "  " << item.summary << "\n";
        }

        if (!item.signatures.empty()) {
            oss << "  Signature: " << item.signatures[0] << "\n";
        }

        if (!item.parameters.empty()) {
            oss << "  Parameters:\n";
            for (const auto& [param, desc] : item.parameters) {
                oss << "    " << param << ": " << desc << "\n";
            }
        }

        oss << "\n";
    }

    return oss.str();
}

std::string DocGenerator::generateRST(const DocOutput& doc)
{
    std::ostringstream oss;

    oss << doc.title << "\n";
    oss << std::string(doc.title.length(), '=') << "\n\n";
    oss << "**Version:** " << doc.version << "\n\n";
    oss << "**Generated:** " << doc.generated_date << "\n\n";

    for (const auto& item : doc.items) {
        oss << item.name << "\n";
        oss << std::string(item.name.length(), '-') << "\n\n";

        if (!item.summary.empty()) {
            oss << item.summary << "\n\n";
        }

        if (!item.signatures.empty()) {
            oss << ".. code-block:: fluxscript\n\n";
            oss << "   " << item.signatures[0][0] << "\n\n";
        }

        if (!item.description.empty()) {
            oss << item.description << "\n\n";
        }

        if (!item.parameters.empty()) {
            oss << ":Parameters:\n";
            for (const auto& [param, desc] : item.parameters) {
                oss << "  - " << param << ": " << desc << "\n";
            }
            oss << "\n";
        }
    }

    return oss.str();
}

std::string DocGenerator::generateFunctionDocs(const DocItem& item, const std::string& format)
{
    DocOutput doc;
    doc.title = "FluxScript Function Documentation";
    doc.items.push_back(item);

    if (format == "markdown")
        return generateMarkdown(doc);
    if (format == "html")
        return generateHTML(doc);
    if (format == "json")
        return generateJSON(doc);
    if (format == "text")
        return generateText(doc);
    if (format == "rst")
        return generateRST(doc);

    return generateMarkdown(doc);
}

std::string DocGenerator::generateModuleDocs(const DocOutput& doc, const std::string& format)
{
    if (format == "markdown")
        return generateMarkdown(doc);
    if (format == "html")
        return generateHTML(doc);
    if (format == "json")
        return generateJSON(doc);
    if (format == "text")
        return generateText(doc);
    if (format == "rst")
        return generateRST(doc);

    return generateMarkdown(doc);
}

bool DocGenerator::exportToFile(const DocOutput& doc, const std::string& filename, const std::string& format)
{
    std::string content;

    if (format == "md" || format == "markdown") {
        content = generateMarkdown(doc);
    } else if (format == "html") {
        content = generateHTML(doc);
    } else if (format == "json") {
        content = generateJSON(doc);
    } else if (format == "txt" || format == "text") {
        content = generateText(doc);
    } else if (format == "rst") {
        content = generateRST(doc);
    } else {
        std::cerr << "[DocGenerator] Unsupported format: " << format << std::endl;
        return false;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[DocGenerator] Failed to open file: " << filename << std::endl;
        return false;
    }

    file << content;
    file.close();

    std::cout << "[DocGenerator] Exported documentation to: " << filename << std::endl;
    return true;
}

// ============================================================================
// Search and Utilities
// ============================================================================

std::vector<DocItem> DocGenerator::searchDocs(const DocOutput& doc, const std::string& query)
{
    std::vector<DocItem> results;

    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

    for (const auto& item : doc.items) {
        std::string name_lower = item.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        std::string summary_lower = item.summary;
        std::transform(summary_lower.begin(), summary_lower.end(), summary_lower.begin(), ::tolower);

        if (name_lower.find(query_lower) != std::string::npos || summary_lower.find(query_lower) != std::string::npos) {
            results.push_back(item);
        }
    }

    return results;
}

DocItem* DocGenerator::findItem(DocOutput& doc, const std::string& name)
{
    for (auto& item : doc.items) {
        if (item.name == name) {
            return &item;
        }
    }
    return nullptr;
}

} // namespace Flux
