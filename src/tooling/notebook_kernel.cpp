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
// FluxScript Notebook Kernel Implementation
// Jupyter-like cell-based execution with state persistence
// ============================================================================

#include "flux/tooling/notebook_kernel.h"
#include "flux/jit_engine.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Flux {

// Unique ID counter
static std::atomic<uint64_t> s_id_counter{0};

// ============================================================================
// NotebookKernel Singleton
// ============================================================================

NotebookKernel& NotebookKernel::instance()
{
    static NotebookKernel instance;
    return instance;
}

void NotebookKernel::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
        return;

    m_state = KernelState::Ready;
    m_initialized = true;

    std::cout << "[NotebookKernel] Initialized" << std::endl;
}

void NotebookKernel::finalize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_notebooks.clear();
    m_variables.clear();
    m_state = KernelState::Idle;
    m_initialized = false;

    std::cout << "[NotebookKernel] Finalized" << std::endl;
}

// ============================================================================
// Notebook Management
// ============================================================================

std::string NotebookKernel::createNotebook(const std::string& title)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string id = "nb_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    NotebookDocument nb;
    nb.id = id;
    nb.title = title;
    nb.language = "fluxscript";
    nb.metadata["created"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    nb.metadata["version"] = "1.0";

    // Add initial empty cell
    NotebookCell cell;
    cell.id = id + "_cell_1";
    cell.source_code = "// FluxScript Notebook\n// Type your code here\n";
    nb.cells.push_back(cell);

    m_notebooks[id] = nb;
    m_state = KernelState::Ready;

    std::cout << "[NotebookKernel] Created notebook: " << title << " (" << id << ")" << std::endl;
    return id;
}

bool NotebookKernel::openNotebook(const std::string& notebook_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_notebooks.count(notebook_id) > 0;
}

bool NotebookKernel::saveNotebook(const std::string& notebook_id, const std::string& filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return false;

    std::string json = NotebookSerializer::serializeToJSON(it->second);

    std::ofstream file(filename);
    if (!file.is_open())
        return false;
    file << json;
    file.close();

    std::cout << "[NotebookKernel] Saved notebook to: " << filename << std::endl;
    return true;
}

bool NotebookKernel::loadNotebook(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(filename);
    if (!file.is_open())
        return false;

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    try {
        NotebookDocument nb = NotebookSerializer::deserializeFromJSON(json);
        m_notebooks[nb.id] = nb;
        std::cout << "[NotebookKernel] Loaded notebook: " << nb.title << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[NotebookKernel] Failed to load notebook: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> NotebookKernel::getOpenNotebooks() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (const auto& [id, nb] : m_notebooks) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Cell Management
// ============================================================================

std::string NotebookKernel::addCell(const std::string& notebook_id, const std::string& source)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return "";

    std::string cell_id = notebook_id + "_cell_" + std::to_string(it->second.cells.size() + 1);

    NotebookCell cell;
    cell.id = cell_id;
    cell.source_code = source;
    it->second.cells.push_back(cell);

    return cell_id;
}

bool NotebookKernel::executeCell(const std::string& notebook_id, const std::string& cell_id)
{
    auto start_time = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    auto nb_it = m_notebooks.find(notebook_id);
    if (nb_it == m_notebooks.end())
        return false;

    auto& nb = nb_it->second;
    auto cell_it =
        std::find_if(nb.cells.begin(), nb.cells.end(), [&cell_id](const NotebookCell& c) { return c.id == cell_id; });
    if (cell_it == nb.cells.end())
        return false;

    // Set state to busy
    m_state = KernelState::Busy;

    // Execute code
    cell_it->outputs = executeCode(cell_it->source_code);

    // Update cell metadata
    auto end_time = std::chrono::steady_clock::now();
    cell_it->execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    cell_it->last_executed = end_time;
    cell_it->execution_count++;
    cell_it->success = true;

    for (const auto& out : cell_it->outputs) {
        if (out.is_error) {
            cell_it->success = false;
            break;
        }
    }

    m_state = KernelState::Ready;
    return cell_it->success;
}

bool NotebookKernel::executeAllCells(const std::string& notebook_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return false;

    bool all_success = true;
    for (auto& cell : it->second.cells) {
        // Unlock mutex for execution, then relock
        m_mutex.unlock();
        bool success = executeCell(notebook_id, cell.id);
        m_mutex.lock();

        if (!success)
            all_success = false;
        if (m_interrupt_requested) {
            m_interrupt_requested = false;
            break;
        }
    }

    return all_success;
}

bool NotebookKernel::deleteCell(const std::string& notebook_id, const std::string& cell_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return false;

    auto& cells = it->second.cells;
    auto cell_it =
        std::find_if(cells.begin(), cells.end(), [&cell_id](const NotebookCell& c) { return c.id == cell_id; });
    if (cell_it == cells.end())
        return false;

    cells.erase(cell_it);
    return true;
}

bool NotebookKernel::updateCellSource(const std::string& notebook_id, const std::string& cell_id,
                                      const std::string& source)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return false;

    for (auto& cell : it->second.cells) {
        if (cell.id == cell_id) {
            cell.source_code = source;
            return true;
        }
    }
    return false;
}

bool NotebookKernel::executeCellAsync(const std::string& notebook_id, const std::string& cell_id,
                                      std::function<void(const std::string&, const CellOutput&)> output_callback)
{
    // For now, execute synchronously and call callback for each output
    bool success = executeCell(notebook_id, cell_id);

    // Call callbacks for outputs
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto nb_it = m_notebooks.find(notebook_id);
        if (nb_it == m_notebooks.end())
            return false;

        for (const auto& cell : nb_it->second.cells) {
            if (cell.id == cell_id) {
                for (const auto& output : cell.outputs) {
                    output_callback(cell_id, output);
                }
                break;
            }
        }
    }

    return success;
}

bool NotebookKernel::interruptExecution()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupt_requested = true;
    std::cout << "[NotebookKernel] Interrupt requested" << std::endl;
    return true;
}

// ============================================================================
// Variable Inspection
// ============================================================================

std::map<std::string, std::string> NotebookKernel::getVariables() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, std::string> vars;

    for (const auto& [name, value] : m_variables) {
        vars[name] = std::to_string(value);
    }

    return vars;
}

std::string NotebookKernel::getVariableValue(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_variables.find(name);
    if (it == m_variables.end())
        return "<undefined>";
    return std::to_string(it->second);
}

bool NotebookKernel::setVariable(const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_variables[name] = value;
    return true;
}

const std::vector<CellOutput>& NotebookKernel::getCellOutputs(const std::string& notebook_id,
                                                              const std::string& cell_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    static const std::vector<CellOutput> empty_outputs;

    auto nb_it = m_notebooks.find(notebook_id);
    if (nb_it == m_notebooks.end())
        return empty_outputs;

    for (const auto& cell : nb_it->second.cells) {
        if (cell.id == cell_id) {
            return cell.outputs;
        }
    }

    return empty_outputs;
}

// ============================================================================
// Code Execution
// ============================================================================

std::vector<CellOutput> NotebookKernel::executeCode(const std::string& code)
{
    std::vector<CellOutput> outputs;

    if (code.empty()) {
        return outputs;
    }

    try {
        // Use JITEngine to compile and execute
        auto& jit_engine = JITEngine::instance();

        std::string error;
        bool success = jit_engine.compileScript(code, &error);

        if (!success) {
            CellOutput error_out;
            error_out.type = OutputType::Error;
            error_out.is_error = true;
            error_out.error_name = "CompilationError";
            error_out.error_message = error;
            outputs.push_back(error_out);
            return outputs;
        }

        // Execute and capture output
        auto result = jit_engine.callFunction("main", {});

        if (std::holds_alternative<double>(result)) {
            CellOutput out;
            out.type = OutputType::Text;
            out.mime_type = "text/plain";
            out.data = std::to_string(std::get<double>(result));
            outputs.push_back(out);
        } else if (std::holds_alternative<int>(result)) {
            CellOutput out;
            out.type = OutputType::Text;
            out.mime_type = "text/plain";
            out.data = std::to_string(std::get<int>(result));
            outputs.push_back(out);
        }

    } catch (const std::exception& e) {
        CellOutput error_out;
        error_out.type = OutputType::Error;
        error_out.is_error = true;
        error_out.error_name = "RuntimeError";
        error_out.error_message = e.what();
        outputs.push_back(error_out);
    }

    return outputs;
}

std::vector<CellOutput> NotebookKernel::executeExpression(const std::string& expr)
{
    return executeCode("return " + expr + ";");
}

std::vector<CellOutput> NotebookKernel::executeStatement(const std::string& stmt)
{
    return executeCode(stmt);
}

// ============================================================================
// Export
// ============================================================================

std::string NotebookKernel::exportAsScript(const std::string& notebook_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return "";

    std::ostringstream oss;
    oss << "// Exported from FluxScript Notebook: " << it->second.title << "\n\n";

    for (const auto& cell : it->second.cells) {
        oss << "// === Cell " << cell.id << " ===\n";
        oss << cell.source_code << "\n\n";
    }

    return oss.str();
}

std::string NotebookKernel::exportAsHTML(const std::string& notebook_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return "";

    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n<html>\n<head>\n";
    oss << "<title>" << it->second.title << "</title>\n";
    oss << "<style>\n";
    oss << "body { font-family: monospace; margin: 20px; }\n";
    oss << ".cell { border: 1px solid #ddd; margin: 10px 0; padding: 10px; }\n";
    oss << ".code { background: #f5f5f5; padding: 5px; }\n";
    oss << ".output { background: #e8f4e8; padding: 5px; margin-top: 5px; }\n";
    oss << ".error { background: #f8e8e8; color: red; padding: 5px; }\n";
    oss << "</style>\n</head>\n<body>\n";
    oss << "<h1>" << it->second.title << "</h1>\n";

    for (const auto& cell : it->second.cells) {
        oss << "<div class='cell'>\n";
        oss << "<div class='code'><pre>" << cell.source_code << "</pre></div>\n";

        for (const auto& output : cell.outputs) {
            if (output.is_error) {
                oss << "<div class='error'>" << output.error_name << ": " << output.error_message << "</div>\n";
            } else {
                oss << "<div class='output'>" << output.data << "</div>\n";
            }
        }

        oss << "</div>\n";
    }

    oss << "</body>\n</html>";
    return oss.str();
}

std::string NotebookKernel::exportAsMarkdown(const std::string& notebook_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_notebooks.find(notebook_id);
    if (it == m_notebooks.end())
        return "";

    std::ostringstream oss;
    oss << "# " << it->second.title << "\n\n";

    for (const auto& cell : it->second.cells) {
        oss << "## Cell: " << cell.id << "\n\n";
        oss << "```fluxscript\n" << cell.source_code << "\n```\n\n";

        for (const auto& output : cell.outputs) {
            if (output.is_error) {
                oss << "**Error:** " << output.error_name << " - " << output.error_message << "\n\n";
            } else {
                oss << "```\n" << output.data << "\n```\n\n";
            }
        }
    }

    return oss.str();
}

// ============================================================================
// NotebookSerializer Implementation
// ============================================================================

namespace NotebookSerializer {

std::string serializeToJSON(const NotebookDocument& notebook)
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"id\": \"" << notebook.id << "\",\n";
    oss << "  \"title\": \"" << notebook.title << "\",\n";
    oss << "  \"language\": \"" << notebook.language << "\",\n";
    oss << "  \"cells\": [\n";

    for (size_t i = 0; i < notebook.cells.size(); i++) {
        const auto& cell = notebook.cells[i];
        oss << "    {\n";
        oss << "      \"id\": \"" << cell.id << "\",\n";
        oss << "      \"source\": \"" << cell.source_code << "\",\n";
        oss << "      \"execution_count\": " << cell.execution_count << ",\n";
        oss << "      \"outputs\": []\n"; // Simplified
        oss << "    }";
        if (i < notebook.cells.size() - 1)
            oss << ",";
        oss << "\n";
    }

    oss << "  ]\n";
    oss << "}\n";

    return oss.str();
}

NotebookDocument deserializeFromJSON(const std::string& json)
{
    NotebookDocument nb;

    // Simple JSON parsing (for a full implementation, use a proper JSON library)
    size_t title_pos = json.find("\"title\":");
    if (title_pos != std::string::npos) {
        size_t start = json.find("\"", title_pos + 8) + 1;
        size_t end = json.find("\"", start);
        nb.title = json.substr(start, end - start);
    }

    size_t id_pos = json.find("\"id\":");
    if (id_pos != std::string::npos) {
        size_t start = json.find("\"", id_pos + 5) + 1;
        size_t end = json.find("\"", start);
        nb.id = json.substr(start, end - start);
    }

    // Parse cells (simplified)
    size_t cell_pos = json.find("\"source\":");
    while (cell_pos != std::string::npos) {
        size_t start = json.find("\"", cell_pos + 9) + 1;
        size_t end = json.find("\"", start);
        std::string source = json.substr(start, end - start);

        NotebookCell cell;
        cell.id = nb.id + "_cell_" + std::to_string(nb.cells.size() + 1);
        cell.source_code = source;
        nb.cells.push_back(cell);

        cell_pos = json.find("\"source\":", end);
    }

    return nb;
}

std::string serializeToHTML(const NotebookDocument& notebook)
{
    return NotebookKernel::instance().exportAsHTML(notebook.id);
}

std::string serializeToMarkdown(const NotebookDocument& notebook)
{
    return NotebookKernel::instance().exportAsMarkdown(notebook.id);
}

} // namespace NotebookSerializer

} // namespace Flux
