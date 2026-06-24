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

#include "flux/tooling/repl_impl.h"
#include <cstdlib>
#include <ctime>
// #include <readline/readline.h>
// #include <readline/history.h>

namespace Flux {
namespace REPL {

REPL::REPL() : m_running(false), m_scope(0)
{
    // Initialize keywords
    m_keywords = {"let",    "fun",    "if",     "else", "for",   "in", "return",
                  "import", "module", "export", "true", "false", "pi", "e"};

    // Initialize built-in functions
    m_functions = {"sin", "cos", "tan", "exp",  "log",  "log10", "sqrt", "abs",   "floor",  "ceil", "round",
                   "min", "max", "sum", "mean", "plot", "print", "len",  "range", "append", "help", "type"};

    // Initialize documentation
    m_documentation = {
        {"sin", "sin(x) - Sine function (radians)"},    {"cos", "cos(x) - Cosine function (radians)"},
        {"tan", "tan(x) - Tangent function (radians)"}, {"exp", "exp(x) - Exponential function e^x"},
        {"log", "log(x) - Natural logarithm"},          {"sqrt", "sqrt(x) - Square root"},
        {"plot", "plot(x, y) - Create a plot"},         {"print", "print(x) - Print value to console"},
        {"let", "let x = value - Define a variable"},   {"fun", "fun name(args) { body } - Define a function"}};
}

REPL::~REPL()
{
    if (m_config.autoSave) {
        saveHistory();
    }
}

REPL& globalREPL()
{
    static REPL repl;
    return repl;
}

void REPL::initialize()
{
    loadHistory();
    m_running = true;
}

void REPL::run()
{
    initialize();

    std::cout << "FluxScript REPL v1.0\n";
    std::cout << "Type 'help' for available commands, 'quit' to exit.\n\n";

    std::string buffer;
    bool continuing = false;

    while (m_running) {
        std::string prompt = continuing ? m_config.continuationPrompt : m_config.prompt;

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "\n";
            break;
        }

        if (input.empty())
            continue;

        // Add to readline history
        // add_history;

        // Check for multi-line
        if (m_config.multiLine && !continuing) {
            int openParens = std::count(input.begin(), input.end(), '(') - std::count(input.begin(), input.end(), ')');
            int openBraces = std::count(input.begin(), input.end(), '{') - std::count(input.begin(), input.end(), '}');

            if (openParens > 0 || openBraces > 0) {
                buffer = input;
                continuing = true;
                continue;
            }
        }

        if (continuing) {
            buffer += "\n" + input;
            int openParens =
                std::count(buffer.begin(), buffer.end(), '(') - std::count(buffer.begin(), buffer.end(), ')');
            int openBraces =
                std::count(buffer.begin(), buffer.end(), '{') - std::count(buffer.begin(), buffer.end(), '}');

            if (openParens > 0 || openBraces > 0) {
                continue;
            }
            input = buffer;
            buffer.clear();
            continuing = false;
        }

        CommandResult result = execute(input);

        if (!result.output.empty()) {
            std::cout << result.output << "\n";
        }
        if (!result.error.empty()) {
            std::cerr << Colors::ERROR << result.error << Colors::RESET << "\n";
        }
    }

    if (m_config.autoSave) {
        saveHistory();
    }
}

CommandResult REPL::execute(const std::string& input)
{
    // Trim whitespace
    std::string trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return CommandResult();
    }

    // Check for commands
    if (trimmed[0] == ':') {
        return processCommand(trimmed.substr(1));
    }

    // Check for shell command
    if (trimmed[0] == '!') {
        return handleShell(trimmed.substr(1));
    }

    // Evaluate as expression
    return evaluate(trimmed);
}

CommandResult REPL::processCommand(const std::string& cmd)
{
    std::istringstream iss(cmd);
    std::string command, args;
    iss >> command;
    std::getline(iss, args);

    // Trim args
    args.erase(0, args.find_first_not_of(" \t"));
    args.erase(args.find_last_not_of(" \t") + 1);

    if (command == "help" || command == "h" || command == "?") {
        return handleHelp(args);
    } else if (command == "clear" || command == "cls") {
        if (system("clear")) {
        }
        return CommandResult();
    } else if (command == "history" || command == "hist") {
        return handleHistory(args);
    } else if (command == "save") {
        return handleSave(args);
    } else if (command == "load") {
        return handleLoad(args);
    } else if (command == "list" || command == "ls") {
        return handleList(args);
    } else if (command == "type") {
        return handleType(args);
    } else if (command == "doc" || command == "d") {
        return handleDoc(args);
    } else if (command == "edit" || command == "e") {
        return handleEdit(args);
    } else if (command == "quit" || command == "exit" || command == "q") {
        m_running = false;
        return CommandResult("Goodbye!");
    } else {
        return CommandResult(false, "Unknown command: " + command);
    }
}

CommandResult REPL::evaluate(const std::string& expr)
{
    // Check for let binding
    if (expr.substr(0, 4) == "let ") {
        return handleLet(expr.substr(4));
    }

    // Check for function definition
    if (expr.substr(0, 4) == "fun ") {
        return handleFun(expr.substr(4));
    }

    // Use callback if available
    if (m_evalCallback) {
        try {
            std::string result = m_evalCallback(expr);

            std::string type;
            if (m_typeCallback) {
                type = m_typeCallback(expr);
            }

            if (m_config.showTypes && !type.empty()) {
                return CommandResult(result + " : " + type);
            }
            return CommandResult(result);
        } catch (const std::exception& e) {
            return CommandResult(false, e.what());
        }
    }

    // Fallback: try to evaluate simple expressions
    // This is a placeholder - real implementation would use the FluxScript parser
    return CommandResult("[Expression evaluated: " + expr + "]");
}

CommandResult REPL::handleLet(const std::string& args)
{
    // Parse: name = value
    size_t eqPos = args.find('=');
    if (eqPos == std::string::npos) {
        return CommandResult(false, "Invalid let syntax: use 'let name = value'");
    }

    std::string name = args.substr(0, eqPos);
    std::string value = args.substr(eqPos + 1);

    // Trim
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    VariableInfo info;
    info.name = name;
    info.value = value;
    info.type = "inferred";
    info.isFunction = false;
    info.scope = m_scope;

    m_variables[name] = info;

    return CommandResult(name + " = " + value);
}

CommandResult REPL::handleFun(const std::string& args)
{
    // Parse: name(args) { body }
    size_t parenPos = args.find('(');
    if (parenPos == std::string::npos) {
        return CommandResult(false, "Invalid fun syntax: use 'fun name(args) { body }'");
    }

    std::string name = args.substr(0, parenPos);
    name.erase(0, name.find_first_not_of(" \t"));
    name.erase(name.find_last_not_of(" \t") + 1);

    VariableInfo info;
    info.name = name;
    info.value = args;
    info.type = "function";
    info.isFunction = true;
    info.scope = m_scope;

    m_variables[name] = info;
    m_functions.insert(name);

    return CommandResult("Function '" + name + "' defined");
}

CommandResult REPL::handleHelp(const std::string& topic)
{
    if (topic.empty()) {
        std::ostringstream oss;
        oss << "FluxScript REPL Commands:\n\n";
        oss << "  :help [topic]     - Show this help or help for topic\n";
        oss << "  :clear            - Clear screen\n";
        oss << "  :history [n]      - Show last n commands (default: 10)\n";
        oss << "  :save [file]      - Save session to file\n";
        oss << "  :load [file]      - Load file\n";
        oss << "  :list             - List all variables\n";
        oss << "  :type <expr>      - Show type of expression\n";
        oss << "  :doc <topic>      - Show documentation\n";
        oss << "  :edit <var>       - Edit variable\n";
        oss << "  :quit             - Exit REPL\n\n";
        oss << "Other:\n";
        oss << "  !<command>        - Run shell command\n";
        oss << "  let x = value     - Define variable\n";
        oss << "  fun f(x) { ... }  - Define function\n";

        return CommandResult(oss.str());
    }

    // Look up topic
    auto it = m_documentation.find(topic);
    if (it != m_documentation.end()) {
        return CommandResult(it->second);
    }

    if (m_functions.count(topic)) {
        return CommandResult(topic + " - Built-in function");
    }

    return CommandResult(false, "No help available for: " + topic);
}

CommandResult REPL::handleHistory(const std::string& args)
{
    int n = 10;
    if (!args.empty()) {
        try {
            n = std::stoi(args);
        } catch (...) {
            n = 10;
        }
    }

    std::ostringstream oss;
    size_t start = m_history.size() > n ? m_history.size() - n : 0;

    for (size_t i = start; i < m_history.size(); ++i) {
        oss << (i + 1) << ": " << m_history[i].input << "\n";
    }

    return CommandResult(oss.str());
}

CommandResult REPL::handleSave(const std::string& args)
{
    std::string filename = args.empty() ? "session.flux" : args;

    std::ofstream file(filename);
    if (!file.is_open()) {
        return CommandResult(false, "Cannot open file: " + filename);
    }

    for (const auto& entry : m_history) {
        file << entry.input << "\n";
    }

    file.close();
    return CommandResult("Session saved to: " + filename);
}

CommandResult REPL::handleLoad(const std::string& args)
{
    if (args.empty()) {
        return CommandResult(false, "Usage: :load <filename>");
    }

    std::ifstream file(args);
    if (!file.is_open()) {
        return CommandResult(false, "Cannot open file: " + args);
    }

    std::string line;
    int count = 0;

    while (std::getline(file, line)) {
        CommandResult result = execute(line);
        if (result.success) {
            count++;
        }
    }

    return CommandResult("Loaded " + std::to_string(count) + " commands from: " + args);
}

CommandResult REPL::handleList(const std::string& args)
{
    std::ostringstream oss;
    oss << "Variables:\n";

    for (const auto& pair : m_variables) {
        const auto& info = pair.second;
        oss << "  " << info.name;
        if (m_config.showTypes) {
            oss << ": " << info.type;
        }
        if (!info.isFunction) {
            oss << " = " << info.value;
        }
        oss << "\n";
    }

    return CommandResult(oss.str());
}

CommandResult REPL::handleType(const std::string& args)
{
    if (args.empty()) {
        return CommandResult(false, "Usage: :type <expression>");
    }

    if (m_typeCallback) {
        return CommandResult(m_typeCallback(args));
    }

    return CommandResult("Type checking not available");
}

CommandResult REPL::handleDoc(const std::string& topic)
{
    if (topic.empty()) {
        return CommandResult(false, "Usage: :doc <topic>");
    }

    auto it = m_documentation.find(topic);
    if (it != m_documentation.end()) {
        return CommandResult(it->second);
    }

    return CommandResult(false, "No documentation for: " + topic);
}

CommandResult REPL::handleEdit(const std::string& varName)
{
    if (varName.empty()) {
        return CommandResult(false, "Usage: :edit <variable>");
    }

    auto it = m_variables.find(varName);
    if (it == m_variables.end()) {
        return CommandResult(false, "Variable not found: " + varName);
    }

    // In a real implementation, this would open an editor
    return CommandResult("Editing " + varName + " (editor not implemented)");
}

CommandResult REPL::handleShell(const std::string& cmd)
{
    if (cmd.empty()) {
        return CommandResult(false, "Empty shell command");
    }

    int result = system(cmd.c_str());
    return CommandResult("Shell command exited with code: " + std::to_string(result));
}

void REPL::addToHistory(const std::string& input, const std::string& output, bool success)
{
    HistoryEntry entry;
    entry.input = input;
    entry.output = output;
    entry.timestamp = std::time(nullptr);
    entry.success = success;

    m_history.push_back(entry);

    // Trim history
    while (m_history.size() > m_config.maxHistory) {
        m_history.erase(m_history.begin());
    }
}

std::vector<Completion> REPL::getCompletions(const std::string& prefix, size_t position)
{
    std::vector<Completion> completions;

    // Complete keywords
    for (const auto& kw : m_keywords) {
        if (kw.find(prefix) == 0) {
            Completion c;
            c.text = kw;
            c.type = "keyword";
            c.description = "Keyword";
            completions.push_back(c);
        }
    }

    // Complete functions
    for (const auto& fn : m_functions) {
        if (fn.find(prefix) == 0) {
            Completion c;
            c.text = fn;
            c.type = "function";
            auto it = m_documentation.find(fn);
            c.description = (it != m_documentation.end()) ? it->second : "Function";
            completions.push_back(c);
        }
    }

    // Complete variables
    for (const auto& pair : m_variables) {
        if (pair.first.find(prefix) == 0) {
            Completion c;
            c.text = pair.first;
            c.type = "variable";
            c.description = pair.second.type;
            completions.push_back(c);
        }
    }

    return completions;
}

std::string REPL::highlightSyntax(const std::string& code) const
{
    std::string result = code;

    // This is a simplified implementation
    // A full implementation would use proper lexing

    // Highlight keywords
    for (const auto& kw : m_keywords) {
        std::string pattern = "\\b" + kw + "\\b";
        // In real implementation, would use regex
        size_t pos = result.find(kw);
        if (pos != std::string::npos) {
            result.replace(pos, kw.length(), Colors::KEYWORD + kw + Colors::RESET);
        }
    }

    return result;
}

void REPL::saveHistory()
{
    std::string filename = expandPath(m_config.historyFile);
    std::ofstream file(filename);

    for (const auto& entry : m_history) {
        file << entry.input << "\n";
    }
}

void REPL::loadHistory()
{
    std::string filename = expandPath(m_config.historyFile);
    std::ifstream file(filename);

    std::string line;
    while (std::getline(file, line)) {
        // add_history;
    }
}

void REPL::clearHistory()
{
    m_history.clear();
    // clear_history;
}

std::vector<VariableInfo> REPL::listVariables() const
{
    std::vector<VariableInfo> result;
    for (const auto& pair : m_variables) {
        result.push_back(pair.second);
    }
    return result;
}

VariableInfo REPL::getVariable(const std::string& name) const
{
    auto it = m_variables.find(name);
    if (it != m_variables.end()) {
        return it->second;
    }
    return VariableInfo();
}

void REPL::deleteVariable(const std::string& name)
{
    m_variables.erase(name);
}

std::string REPL::expandPath(const std::string& path)
{
    std::string result = path;

    // Expand ~ to home directory
    if (!result.empty() && result[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            result = std::string(home) + result.substr(1);
        }
    }

    return result;
}

std::string REPL::formatValue(const std::string& value, const std::string& type) const
{
    if (type == "string") {
        return "\"" + value + "\"";
    }
    return value;
}

void REPL::setConfig(const REPLConfig& config)
{
    m_config = config;
}

} // namespace REPL
} // namespace Flux
