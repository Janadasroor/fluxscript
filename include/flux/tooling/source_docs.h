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

#ifndef FLUX_TOOLING_SOURCE_DOCS_H
#define FLUX_TOOLING_SOURCE_DOCS_H

#include <string>
#include <vector>
#include <map>

namespace Flux {
namespace Tooling {

// ============================================================================
// Source-level Documentation Types
// ============================================================================

struct DocComment {
    std::string text;           // The comment text (/// content)
    int line = 0;               // Line number in source
    std::string format = "md";  // "md" for markdown, "rst", "plain"
};

struct FunctionDoc {
    std::string name;
    std::string signature;      // e.g., "def sin(x: double) -> double"
    DocComment docComment;
    std::vector<std::string> parameters;
    std::string returnType;
    std::string body;           // Implementation snippet for reference
    std::vector<std::string> examples;
    std::string deprecated;     // Deprecation message if deprecated
};

struct TypeDoc {
    std::string name;
    DocComment docComment;
    std::string kind;           // "struct", "enum", "alias"
    std::vector<std::pair<std::string, std::string>> members;  // name: type
};

struct ModuleDoc {
    std::string name;
    DocComment docComment;
    std::vector<FunctionDoc> functions;
    std::vector<TypeDoc> types;
    std::string description;
};

struct ApiDocumentation {
    std::string projectName;
    std::string version;
    std::map<std::string, ModuleDoc> modules;
    std::vector<FunctionDoc> topLevelFunctions;
    std::vector<TypeDoc> topLevelTypes;
};

// ============================================================================
// Source Documentation Generator
// ============================================================================

class SourceDocGenerator {
public:
    SourceDocGenerator();

    // Parse source file and extract documentation
    bool parseSource(const std::string& sourceCode, const std::string& filename = "");
    bool parseFile(const std::string& filename);

    // Get extracted documentation
    ApiDocumentation getApiDoc() const { return m_apiDoc; }
    std::vector<FunctionDoc> getFunctions() const;
    std::vector<TypeDoc> getTypes() const;

    // Generate output in various formats
    std::string generateMarkdown() const;
    std::string generateHtml() const;
    std::string generateRst() const;
    std::string generatePlainText() const;
    std::string generateJson() const;

    // Generate documentation for a single function/type
    std::string generateFunctionDoc(const FunctionDoc& func, const std::string& format = "md") const;
    std::string generateTypeDoc(const TypeDoc& type, const std::string& format = "md") const;

    // Find function/type by name
    const FunctionDoc* findFunction(const std::string& name) const;
    const TypeDoc* findType(const std::string& name) const;

private:
    void extractDocComments(const std::string& source);
    void extractFunctions(const std::string& source);
    void extractTypes(const std::string& source);
    void extractExamples(const std::string& source);
    std::string formatFunctionSignature(const FunctionDoc& func) const;

    ApiDocumentation m_apiDoc;
    std::map<int, DocComment> m_pendingDocComments;  // Line -> comment
    std::string m_currentFile;
};

} // namespace Tooling
} // namespace Flux

#endif // FLUX_TOOLING_SOURCE_DOCS_H
