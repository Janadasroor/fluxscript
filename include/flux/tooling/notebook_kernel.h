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
// FluxScript Notebook Kernel - Jupyter-like interactive computing protocol
// Provides cell-based execution with rich output (text, plots, data)
// ============================================================================

#ifndef FLUX_NOTEBOOK_KERNEL_H
#define FLUX_NOTEBOOK_KERNEL_H

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "flux/compiler/ast.h"

namespace Flux {

// Output types for notebook cells
enum class OutputType
{
    Text,      // Plain text output
    HTML,      // HTML rendering
    SVG,       // SVG graphics (plots)
    PNG,       // PNG image data
    DataFrame, // Tabular data
    Error,     // Execution error
    Latex,     // LaTeX math
    JSON       // JSON data
};

// Single output from cell execution
struct CellOutput
{
    OutputType type;
    std::string data;      // Content (text, HTML, base64 PNG, etc.)
    std::string mime_type; // MIME type for rich display
    bool is_error = false;
    std::string error_name; // e.g., "SyntaxError", "RuntimeError"
    std::string error_message;
    std::vector<std::string> traceback; // Error traceback
};

// Notebook cell
struct NotebookCell
{
    std::string id;
    std::string source_code;
    std::vector<CellOutput> outputs;
    std::chrono::steady_clock::time_point last_executed;
    double execution_time_ms = 0.0;
    bool success = false;
    int execution_count = 0;
};

// Notebook document
struct NotebookDocument
{
    std::string id;
    std::string title;
    std::string language = "fluxscript";
    std::vector<NotebookCell> cells;
    std::map<std::string, std::string> metadata; // Author, date, etc.
};

// Kernel state
enum class KernelState
{
    Starting,
    Ready,
    Busy, // Executing code
    Idle,
    Error
};

// Notebook Kernel - executes cells and manages state
class NotebookKernel
{
public:
    static NotebookKernel& instance();

    // Initialize/finalize
    void initialize();
    void finalize();
    KernelState state() const { return m_state; }

    // Notebook management
    std::string createNotebook(const std::string& title = "Untitled");
    bool openNotebook(const std::string& notebook_id);
    bool saveNotebook(const std::string& notebook_id, const std::string& filename);
    bool loadNotebook(const std::string& filename);
    std::vector<std::string> getOpenNotebooks() const;

    // Cell management
    std::string addCell(const std::string& notebook_id, const std::string& source);
    bool executeCell(const std::string& notebook_id, const std::string& cell_id);
    bool executeAllCells(const std::string& notebook_id);
    bool deleteCell(const std::string& notebook_id, const std::string& cell_id);
    bool updateCellSource(const std::string& notebook_id, const std::string& cell_id, const std::string& source);

    // Cell execution with callbacks
    bool executeCellAsync(const std::string& notebook_id, const std::string& cell_id,
                          std::function<void(const std::string& cell_id, const CellOutput&)> output_callback);

    // Interrupt execution
    bool interruptExecution();

    // Variable inspection
    std::map<std::string, std::string> getVariables() const;
    std::string getVariableValue(const std::string& name) const;
    bool setVariable(const std::string& name, double value);

    // Cell output retrieval
    const std::vector<CellOutput>& getCellOutputs(const std::string& notebook_id, const std::string& cell_id) const;

    // Export
    std::string exportAsScript(const std::string& notebook_id) const;
    std::string exportAsHTML(const std::string& notebook_id) const;
    std::string exportAsMarkdown(const std::string& notebook_id) const;

private:
    NotebookKernel() = default;
    ~NotebookKernel() = default;

    std::vector<CellOutput> executeCode(const std::string& code);
    std::vector<CellOutput> executeExpression(const std::string& expr);
    std::vector<CellOutput> executeStatement(const std::string& stmt);

    std::map<std::string, NotebookDocument> m_notebooks;
    std::map<std::string, double> m_variables; // Shared variable store
    KernelState m_state = KernelState::Starting;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_interrupt_requested = false;
};

// Notebook file format serializer
namespace NotebookSerializer {
std::string serializeToJSON(const NotebookDocument& notebook);
NotebookDocument deserializeFromJSON(const std::string& json);
std::string serializeToHTML(const NotebookDocument& notebook);
std::string serializeToMarkdown(const NotebookDocument& notebook);
} // namespace NotebookSerializer

} // namespace Flux

#endif // FLUX_NOTEBOOK_KERNEL_H
