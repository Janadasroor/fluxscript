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

#ifndef FLUX_REPL_H
#define FLUX_REPL_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace Flux {
namespace REPL {

// Command types
enum class CommandType {
    Eval,           // Evaluate expression
    Let,            // Define variable
    Fun,            // Define function
    Help,           // Show help
    Clear,          // Clear screen
    History,        // Show history
    Save,           // Save session
    Load,           // Load file
    Quit,           // Exit REPL
    Edit,           // Edit variable
    List,           // List variables
    Type,           // Show type
    Doc,            // Show documentation
    Shell           // Shell command
};

// Command result
struct CommandResult {
    bool success;
    std::string output;
    std::string error;
    std::string type;  // For type display
    
    CommandResult() : success(true) {}
    CommandResult(const std::string& out) : success(true), output(out) {}
    CommandResult(bool s, const std::string& msg) : success(s), output(msg) {}
};

// History entry
struct HistoryEntry {
    std::string input;
    std::string output;
    double timestamp;
    bool success;
};

// Variable info
struct VariableInfo {
    std::string name;
    std::string type;
    std::string value;
    bool isFunction;
    size_t scope;
};

// Completion suggestion
struct Completion {
    std::string text;
    std::string type;  // "keyword", "function", "variable", "file"
    std::string description;
};

// REPL Configuration
struct REPLConfig {
    std::string prompt = ">>> ";
    std::string continuationPrompt = "... ";
    bool showTypes = true;
    bool autoSave = false;
    std::string historyFile = "~/.fluxscript_history";
    size_t maxHistory = 1000;
    bool syntaxHighlight = true;
    bool multiLine = true;
};

// Main REPL class
class REPL {
public:
    REPL();
    ~REPL();
    
    // Initialize REPL
    void initialize();
    
    // Run REPL interactively
    void run();
    
    // Run single command
    CommandResult execute(const std::string& input);
    
    // Evaluate expression
    CommandResult evaluate(const std::string& expr);
    
    // Get completions
    std::vector<Completion> getCompletions(const std::string& prefix, size_t position);
    
    // Get help for topic
    std::string getHelp(const std::string& topic = "");
    
    // Configuration
    void setConfig(const REPLConfig& config);
    const REPLConfig& config() const { return m_config; }
    
    // History
    std::vector<HistoryEntry> history() const { return m_history; }
    void saveHistory();
    void loadHistory();
    void clearHistory();
    
    // Variables
    std::vector<VariableInfo> listVariables() const;
    VariableInfo getVariable(const std::string& name) const;
    void deleteVariable(const std::string& name);
    
    // Callbacks for integration
    using EvalCallback = std::function<std::string(const std::string&)>;
    using TypeCallback = std::function<std::string(const std::string&)>;
    
    void setEvalCallback(EvalCallback cb) { m_evalCallback = cb; }
    void setTypeCallback(TypeCallback cb) { m_typeCallback = cb; }
    
    // Syntax highlighting
    std::string highlightSyntax(const std::string& code) const;
    
private:
    CommandResult processCommand(const std::string& input);
    CommandResult handleLet(const std::string& args);
    CommandResult handleFun(const std::string& args);
    CommandResult handleHelp(const std::string& args);
    CommandResult handleHistory(const std::string& args);
    CommandResult handleSave(const std::string& args);
    CommandResult handleLoad(const std::string& args);
    CommandResult handleList(const std::string& args);
    CommandResult handleType(const std::string& args);
    CommandResult handleDoc(const std::string& args);
    CommandResult handleShell(const std::string& args);
    CommandResult handleEdit(const std::string& args);
    
    void addToHistory(const std::string& input, const std::string& output, bool success);
    std::string expandPath(const std::string& path);
    std::string formatValue(const std::string& value, const std::string& type) const;
    
    REPLConfig m_config;
    std::vector<HistoryEntry> m_history;
    std::map<std::string, VariableInfo> m_variables;
    std::set<std::string> m_keywords;
    std::set<std::string> m_functions;
    std::map<std::string, std::string> m_documentation;
    
    EvalCallback m_evalCallback;
    TypeCallback m_typeCallback;
    
    bool m_running;
    size_t m_scope;
};

// ANSI color codes for syntax highlighting
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string KEYWORD = "\033[1;35m";    // Bold magenta
    const std::string FUNCTION = "\033[1;36m";   // Bold cyan
    const std::string STRING = "\033[32m";       // Green
    const std::string NUMBER = "\033[33m";       // Yellow
    const std::string COMMENT = "\033[90m";      // Gray
    const std::string TYPE = "\033[1;34m";       // Bold blue
    const std::string VARIABLE = "\033[37m";     // White
    const std::string ERROR = "\033[91m";        // Red
}

// Global REPL instance
REPL& globalREPL();

} // namespace REPL
} // namespace Flux

#endif // FLUX_REPL_H
