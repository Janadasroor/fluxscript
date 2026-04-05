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

#include "flux/compiler/preprocessor.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <iostream>

namespace Flux {

Preprocessor::Preprocessor() {
    // Define some default macros
    m_macros["FLUX"] = "1";
    m_macros["FLUX_VERSION"] = "1.0.0";
    m_macros["__FLUX__"] = "1";
}

Preprocessor::~Preprocessor() = default;

void Preprocessor::define(const std::string& name, const std::string& value) {
    m_macros[name] = value;
}

void Preprocessor::undefine(const std::string& name) {
    m_macros.erase(name);
}

bool Preprocessor::isDefined(const std::string& name) const {
    return m_macros.find(name) != m_macros.end();
}

std::map<std::string, std::string> Preprocessor::getDefinitions() const {
    return m_macros;
}

void Preprocessor::setFeatures(const std::map<std::string, bool>& features) {
    for (const auto& [name, value] : features) {
        if (value) {
            m_macros[name] = "1";
        } else {
            m_macros.erase(name);
        }
    }
}

void Preprocessor::setConstants(const std::map<std::string, std::string>& constants) {
    for (const auto& [name, value] : constants) {
        m_macros[name] = value;
    }
}

void Preprocessor::addIncludePath(const std::string& path) {
    m_includePaths.push_back(path);
}

std::string Preprocessor::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

DirectiveType Preprocessor::parseDirective(const std::string& line, std::string& argument) {
    std::string trimmed = trim(line);
    
    if (trimmed.empty() || trimmed[0] != '#') {
        return DirectiveType::None;
    }
    
    // Remove the # and any whitespace
    std::string directive = trim(trimmed.substr(1));
    
    // Extract the directive name and argument
    size_t spacePos = directive.find_first_of(" \t");
    std::string name = (spacePos == std::string::npos) ? directive : directive.substr(0, spacePos);
    argument = (spacePos == std::string::npos) ? "" : trim(directive.substr(spacePos + 1));
    
    // Convert to lowercase for comparison
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    
    if (name == "ifdef") return DirectiveType::Ifdef;
    if (name == "ifndef") return DirectiveType::Ifndef;
    if (name == "if") return DirectiveType::If;
    if (name == "elif") return DirectiveType::Elif;
    if (name == "else") return DirectiveType::Else;
    if (name == "endif") return DirectiveType::Endif;
    if (name == "define") return DirectiveType::Define;
    if (name == "undef") return DirectiveType::Undef;
    if (name == "warning") return DirectiveType::Warning;
    if (name == "error") return DirectiveType::Error;
    if (name == "include") return DirectiveType::Include;
    if (name == "import") return DirectiveType::Import;
    if (name == "pragma") return DirectiveType::Pragma;
    
    return DirectiveType::None;
}

bool Preprocessor::evaluateCondition(const std::string& expr) {
    // Handle defined() function
    std::regex definedRegex(R"(defined\s*\(\s*(\w+)\s*\))");
    std::smatch match;
    
    std::string processed = expr;
    while (std::regex_search(processed, match, definedRegex)) {
        std::string macroName = match[1];
        bool isDef = isDefined(macroName);
        processed = processed.substr(0, match.position()) + 
                    (isDef ? "1" : "0") +
                    processed.substr(match.position() + match.length());
    }
    
    // Also handle defined MACRO (without parentheses)
    std::regex definedSimpleRegex(R"(defined\s+(\w+))");
    while (std::regex_search(processed, match, definedSimpleRegex)) {
        std::string macroName = match[1];
        bool isDef = isDefined(macroName);
        processed = processed.substr(0, match.position()) + 
                    (isDef ? "1" : "0") +
                    processed.substr(match.position() + match.length());
    }
    
    // Expand macros
    processed = expandMacros(processed);
    
    // Simple expression evaluation (supports &&, ||, !, ==, !=)
    // This is a simplified evaluator - a full implementation would use a proper expression parser
    
    // Replace && with & for bitwise evaluation
    std::replace(processed.begin(), processed.end(), '&', ' ');
    
    // Try to evaluate as a boolean expression
    // For now, just check if the result is non-zero
    try {
        // Simple check: if it contains any non-zero digit, it's true
        for (char c : processed) {
            if (c >= '1' && c <= '9') return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

std::string Preprocessor::expandMacros(const std::string& text) {
    std::string result = text;
    
    // Simple macro expansion (could be enhanced with proper tokenization)
    for (const auto& [name, value] : m_macros) {
        std::regex wordRegex("\\b" + name + "\\b");
        result = std::regex_replace(result, wordRegex, value);
    }
    
    return result;
}

void Preprocessor::handleDefine(const std::string& args) {
    std::istringstream iss(args);
    std::string name, value;
    
    iss >> name;
    
    // Get the rest as the value
    std::getline(iss >> std::ws, value);
    
    if (name.empty()) return;
    
    // Handle function-like macros (name(args))
    size_t parenPos = name.find('(');
    if (parenPos != std::string::npos) {
        // Function-like macro - for now, just store as-is
        m_macros[name] = value;
    } else {
        // Object-like macro
        if (value.empty()) {
            m_macros[name] = "1";
        } else {
            m_macros[name] = trim(value);
        }
    }
}

void Preprocessor::handleUndef(const std::string& args) {
    std::string name = trim(args);
    if (!name.empty()) {
        m_macros.erase(name);
    }
}

void Preprocessor::handleIfdef(const std::string& args) {
    std::string name = trim(args);
    bool isDef = isDefined(name);
    
    ConditionalState state;
    state.conditionMet = isDef;
    state.hasBeenTrue = isDef;
    state.inActiveBranch = isDef && !m_skipMode;
    
    m_conditionalStack.push(state);
    m_skipMode = m_skipMode || !isDef;
}

void Preprocessor::handleIfndef(const std::string& args) {
    std::string name = trim(args);
    bool isNotDef = !isDefined(name);
    
    ConditionalState state;
    state.conditionMet = isNotDef;
    state.hasBeenTrue = isNotDef;
    state.inActiveBranch = isNotDef && !m_skipMode;
    
    m_conditionalStack.push(state);
    m_skipMode = m_skipMode || !isNotDef;
}

void Preprocessor::handleIf(const std::string& args) {
    bool conditionTrue = evaluateCondition(args);
    
    ConditionalState state;
    state.conditionMet = conditionTrue;
    state.hasBeenTrue = conditionTrue;
    state.inActiveBranch = conditionTrue && !m_skipMode;
    
    m_conditionalStack.push(state);
    m_skipMode = m_skipMode || !conditionTrue;
}

void Preprocessor::handleElif(const std::string& args) {
    if (m_conditionalStack.empty()) {
        return;  // Error: #elif without #if
    }
    
    ConditionalState& top = m_conditionalStack.top();
    
    // Can only evaluate elif if no previous branch was true
    if (top.hasBeenTrue) {
        top.inActiveBranch = false;
        m_skipMode = true;
    } else {
        bool conditionTrue = evaluateCondition(args);
        top.conditionMet = conditionTrue;
        top.hasBeenTrue = conditionTrue;
        top.inActiveBranch = conditionTrue && !m_skipMode;
        m_skipMode = !conditionTrue;
    }
}

void Preprocessor::handleElse() {
    if (m_conditionalStack.empty()) {
        return;  // Error: #else without #if
    }
    
    ConditionalState& top = m_conditionalStack.top();
    
    // Else is active if no previous branch was true
    top.inActiveBranch = !top.hasBeenTrue && !m_skipMode;
    top.hasBeenTrue = true;
    m_skipMode = top.hasBeenTrue;
}

void Preprocessor::handleEndif() {
    if (m_conditionalStack.empty()) {
        return;  // Error: #endif without #if
    }

    m_conditionalStack.pop();

    // Recalculate skip mode
    m_skipMode = false;
    // Note: std::stack doesn't support iteration, so we need to check differently
    // For now, just check if stack is empty (simplified behavior)
    if (!m_conditionalStack.empty()) {
        m_skipMode = !m_conditionalStack.top().inActiveBranch;
    }
}

void Preprocessor::handleWarning(const std::string& args) {
    std::cout << "[Preprocessor Warning] " << expandMacros(args) << std::endl;
}

void Preprocessor::handleError(const std::string& args) {
    std::cerr << "[Preprocessor Error] " << expandMacros(args) << std::endl;
}

std::string Preprocessor::handleInclude(const std::string& args, PreprocessResult& result) {
    // Extract filename from quotes or angle brackets
    std::regex includeRegex(R"(["<]([^">]+)[">])");
    std::smatch match;
    
    if (!std::regex_search(args, match, includeRegex)) {
        result.errors.push_back("Invalid include directive: " + args);
        return "";
    }
    
    std::string filename = match[1];
    std::string content;
    
    // Try to find the file in include paths
    for (const auto& path : m_includePaths) {
        std::string fullPath = path + "/" + filename;
        std::ifstream file(fullPath);
        if (file.is_open()) {
            content = std::string((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
            break;
        }
    }
    
    if (content.empty()) {
        // Try current directory
        std::ifstream file(filename);
        if (file.is_open()) {
            content = std::string((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        } else {
            result.errors.push_back("Include file not found: " + filename);
            return "";
        }
    }
    
    // Recursively process the included file
    PreprocessResult includedResult = process(content, filename);
    
    if (!includedResult.success) {
        result.errors.insert(result.errors.end(), 
                             includedResult.errors.begin(), 
                             includedResult.errors.end());
    }
    
    return includedResult.output;
}

PreprocessResult Preprocessor::process(const std::string& source, const std::string& filename) {
    PreprocessResult result;
    std::istringstream inputStream(source);
    std::ostringstream outputStream;
    
    std::string line;
    m_currentLine = 1;
    m_outputLine = 1;
    m_skipMode = false;
    
    while (std::getline(inputStream, line)) {
        std::string directiveArg;
        DirectiveType directive = parseDirective(line, directiveArg);
        
        if (directive != DirectiveType::None) {
            // Handle preprocessor directive
            switch (directive) {
                case DirectiveType::Define:
                    if (!m_skipMode) handleDefine(directiveArg);
                    break;
                case DirectiveType::Undef:
                    if (!m_skipMode) handleUndef(directiveArg);
                    break;
                case DirectiveType::Ifdef:
                    handleIfdef(directiveArg);
                    break;
                case DirectiveType::Ifndef:
                    handleIfndef(directiveArg);
                    break;
                case DirectiveType::If:
                    handleIf(directiveArg);
                    break;
                case DirectiveType::Elif:
                    handleElif(directiveArg);
                    break;
                case DirectiveType::Else:
                    handleElse();
                    break;
                case DirectiveType::Endif:
                    handleEndif();
                    break;
                case DirectiveType::Warning:
                    if (!m_skipMode) handleWarning(directiveArg);
                    result.warnings.push_back(directiveArg);
                    break;
                case DirectiveType::Error:
                    if (!m_skipMode) handleError(directiveArg);
                    result.errors.push_back(directiveArg);
                    result.success = false;
                    break;
                case DirectiveType::Include:
                    if (!m_skipMode) {
                        std::string included = handleInclude(directiveArg, result);
                        if (!included.empty()) {
                            outputStream << included << "\n";
                            m_outputLine += std::count(included.begin(), included.end(), '\n');
                        }
                    }
                    break;
                default:
                    break;
            }
            
            m_lineNumberMap[m_outputLine] = m_currentLine;
            m_currentLine++;
            continue;
        }
        
        // Output the line if not skipping
        if (!m_skipMode) {
            // Expand macros in the line
            std::string expandedLine = expandMacros(line);
            outputStream << expandedLine << "\n";
            m_lineNumberMap[m_outputLine] = m_currentLine;
            m_outputLine++;
        }
        
        m_currentLine++;
    }
    
    // Check for unclosed conditionals
    if (!m_conditionalStack.empty()) {
        result.errors.push_back("Unclosed preprocessor conditional");
        result.success = false;
    }
    
    result.output = outputStream.str();
    result.definedMacros.clear();
    for (const auto& [name, value] : m_macros) {
        result.definedMacros.insert(name);
    }
    
    return result;
}

} // namespace Flux
