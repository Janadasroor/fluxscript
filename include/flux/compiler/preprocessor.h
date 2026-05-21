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

#ifndef FLUX_COMPILER_PREPROCESSOR_H
#define FLUX_COMPILER_PREPROCESSOR_H

#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

namespace Flux {

// Preprocessor directive types
enum class DirectiveType
{
    None,
    Ifdef,
    Ifndef,
    If,
    Elif,
    Else,
    Endif,
    Define,
    Undef,
    Warning,
    Error,
    Include,
    Import,
    Pragma
};

// Preprocessor result
struct PreprocessResult
{
    std::string output;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::set<std::string> definedMacros;
    bool success = true;
};

// Preprocessor class for handling #ifdef, #define, etc.
class Preprocessor
{
public:
    Preprocessor();
    ~Preprocessor();

    // Set feature flags for conditional compilation
    void define(const std::string& name, const std::string& value = "1");
    void undefine(const std::string& name);
    bool isDefined(const std::string& name) const;

    // Get all defined macros
    std::map<std::string, std::string> getDefinitions() const;

    // Preprocess source code
    PreprocessResult process(const std::string& source, const std::string& filename = "<input>");

    // Set compile configuration
    void setFeatures(const std::map<std::string, bool>& features);
    void setConstants(const std::map<std::string, std::string>& constants);

    // Include path management
    void addIncludePath(const std::string& path);

    // Get line number mapping (for error reporting)
    std::map<int, int> getLineNumberMap() const { return m_lineNumberMap; }

private:
    struct ConditionalState
    {
        bool conditionMet;   // Was the condition true?
        bool hasBeenTrue;    // Has any branch been true?
        bool inActiveBranch; // Are we currently in an active branch?
    };

    std::map<std::string, std::string> m_macros;
    std::stack<ConditionalState> m_conditionalStack;
    std::vector<std::string> m_includePaths;
    std::map<int, int> m_lineNumberMap; // Output line -> Input line
    int m_currentLine = 1;
    int m_outputLine = 1;
    bool m_skipMode = false; // Are we skipping lines?

    DirectiveType parseDirective(const std::string& line, std::string& argument);
    bool evaluateCondition(const std::string& expr);
    std::string expandMacros(const std::string& text);
    std::string trim(const std::string& str);

    void handleDefine(const std::string& args);
    void handleUndef(const std::string& args);
    void handleIfdef(const std::string& args);
    void handleIfndef(const std::string& args);
    void handleIf(const std::string& args);
    void handleElif(const std::string& args);
    void handleElse();
    void handleEndif();
    void handleWarning(const std::string& args);
    void handleError(const std::string& args);
    std::string handleInclude(const std::string& args, PreprocessResult& result);
};

} // namespace Flux

#endif // FLUX_COMPILER_PREPROCESSOR_H
