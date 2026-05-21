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
// Parses doc comments (/// and /** */) and generates API documentation
// ============================================================================

#ifndef FLUX_DOC_GENERATOR_H
#define FLUX_DOC_GENERATOR_H

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Flux {

// Documentation tags
struct DocTag
{
    std::string name; // @param, @return, @example, @see, etc.
    std::string value;
};

// Documented item
struct DocItem
{
    std::string name;                              // Function/variable name
    std::string kind;                              // "function", "variable", "class", "module"
    std::string summary;                           // Brief description
    std::string description;                       // Full description
    std::vector<DocTag> tags;                      // @param, @return, etc.
    std::vector<std::string> examples;             // Code examples
    std::string file;                              // Source file
    int line_number;                               // Line in source
    std::vector<std::string> signatures;           // Function signatures
    std::string return_type;                       // Return type (for functions)
    std::map<std::string, std::string> parameters; // param name -> description
};

// Documentation output
struct DocOutput
{
    std::vector<DocItem> items;
    std::string title;
    std::string version;
    std::string generated_date;
};

// Source comment documentation generator
class DocGenerator
{
public:
    static DocGenerator& instance();

    // Initialize/finalize
    void initialize();
    void finalize();

    // Parse documentation from source file
    DocOutput parseSourceFile(const std::string& source_code, const std::string& filename = "");

    // Parse multiple files
    DocOutput parseSourceFiles(const std::vector<std::pair<std::string, std::string>>& files);

    // Generate documentation in various formats
    std::string generateMarkdown(const DocOutput& doc);
    std::string generateHTML(const DocOutput& doc);
    std::string generateJSON(const DocOutput& doc);
    std::string generateText(const DocOutput& doc);
    std::string generateRST(const DocOutput& doc); // reStructuredText

    // Generate from specific constructs
    std::string generateFunctionDocs(const DocItem& item, const std::string& format);
    std::string generateModuleDocs(const DocOutput& doc, const std::string& format);

    // Export to file
    bool exportToFile(const DocOutput& doc, const std::string& filename, const std::string& format);

    // Utilities
    std::vector<DocItem> searchDocs(const DocOutput& doc, const std::string& query);
    DocItem* findItem(DocOutput& doc, const std::string& name);

private:
    DocGenerator() = default;
    ~DocGenerator() = default;

    // Parsing helpers
    std::vector<DocItem> extractDocItems(const std::string& source, const std::string& filename);
    std::string extractDocComment(const std::string& source, size_t pos, int& line);
    DocItem parseDocComment(const std::string& comment, const std::string& filename, int line);
    std::string extractIdentifier(const std::string& source, size_t pos);
    std::string extractSignature(const std::string& source, size_t pos);
    std::string determineKind(const std::string& source, size_t pos);
};

} // namespace Flux

#endif // FLUX_DOC_GENERATOR_H
