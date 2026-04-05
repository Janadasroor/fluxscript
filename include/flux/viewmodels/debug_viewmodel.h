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

#ifndef FLUX_VIEWMODELS_DEBUG_VIEWMODEL_H
#define FLUX_VIEWMODELS_DEBUG_VIEWMODEL_H

#include "flux/mvvm/viewmodel_base.h"
#include "flux/mvvm/observable_property.h"
#include "flux/mvvm/relay_command.h"
#include <memory>

namespace Flux::MVVM {

/**
 * @brief Debug session state
 */
enum class DebugState {
    Inactive,
    Starting,
    Running,
    Stopped,
    Stepping,
    Terminating,
    Terminated
};

/**
 * @brief Stack frame information
 */
struct StackFrameInfo {
    int frameIndex;
    QString function;
    QString file;
    int line;
    QString module;
    
    StackFrameInfo() : frameIndex(-1), line(-1) {}
};

/**
 * @brief Variable information for watch/locals
 */
struct VariableInfo {
    QString name;
    QString value;
    QString type;
    QString displayValue;
    bool isExpanded;
    bool isModified;
    int childCount;
    
    VariableInfo() : isExpanded(false), isModified(false), childCount(0) {}
};

/**
 * @brief Breakpoint information
 */
struct BreakpointInfo {
    int id;
    int line;
    QString file;
    bool enabled;
    QString condition;
    int hitCount;
    bool isHit;
    
    BreakpointInfo() : id(-1), line(-1), enabled(true), hitCount(0), isHit(false) {}
};

/**
 * @brief ViewModel for debugging functionality
 * 
 * Provides data binding for:
 * - Debug session state
 * - Breakpoint management
 * - Variable inspection (locals, watches, globals)
 * - Call stack navigation
 */
class DebugViewModel : public ViewModelBase {
    Q_OBJECT
    
public:
    explicit DebugViewModel(QObject* parent = nullptr);
    ~DebugViewModel() override;

    // ========================================================================
    // Debug Session State
    // ========================================================================
    
    /**
     * @brief Current debug state
     */
    ObservableProperty<DebugState> DebugState;
    ObservableProperty<bool> IsDebugging;
    ObservableProperty<bool> IsRunning;
    ObservableProperty<bool> IsStopped;
    
    /**
     * @brief Current execution location
     */
    ObservableProperty<QString> CurrentFile;
    ObservableProperty<int> CurrentLine;
    ObservableProperty<QString> CurrentFunction;
    
    /**
     * @brief Process information
     */
    ObservableProperty<int> ProcessId;
    ObservableProperty<QString> ProcessName;
    ObservableProperty<QString> DebugOutput;
    
    // ========================================================================
    // Variables (Locals, Watches, Globals)
    // ========================================================================
    
    /**
     * @brief Local variables
     */
    ObservableProperty<QList<VariableInfo>> LocalVariables;
    
    /**
     * @brief Watch expressions
     */
    ObservableProperty<QList<VariableInfo>> WatchExpressions;
    
    /**
     * @brief Global variables
     */
    ObservableProperty<QList<VariableInfo>> GlobalVariables;
    
    // ========================================================================
    // Call Stack
    // ========================================================================
    
    /**
     * @brief Stack frames
     */
    ObservableProperty<QList<StackFrameInfo>> StackFrames;
    ObservableProperty<int> CurrentFrameIndex;
    
    // ========================================================================
    // Breakpoints
    // ========================================================================
    
    /**
     * @brief All breakpoints
     */
    ObservableProperty<QList<BreakpointInfo>> Breakpoints;
    ObservableProperty<int> BreakpointHitCount;
    
    // ========================================================================
    // Commands
    // ========================================================================
    
    std::shared_ptr<RelayCommand> StartDebuggingCommand;
    std::shared_ptr<RelayCommand> StopDebuggingCommand;
    std::shared_ptr<RelayCommand> RestartDebuggingCommand;
    
    std::shared_ptr<RelayCommand> ContinueCommand;
    std::shared_ptr<RelayCommand> PauseCommand;
    
    std::shared_ptr<RelayCommand> StepOverCommand;
    std::shared_ptr<RelayCommand> StepIntoCommand;
    std::shared_ptr<RelayCommand> StepOutCommand;
    
    std::shared_ptr<RelayCommand> RunToCursorCommand;
    std::shared_ptr<RelayCommand> SetNextStatementCommand;
    
    std::shared_ptr<RelayCommand> ToggleBreakpointCommand;
    std::shared_ptr<RelayCommand> EnableBreakpointCommand;
    std::shared_ptr<RelayCommand> DisableBreakpointCommand;
    std::shared_ptr<RelayCommand> DeleteBreakpointCommand;
    std::shared_ptr<RelayCommand> DeleteAllBreakpointsCommand;
    
    std::shared_ptr<RelayCommand> AddWatchCommand;
    std::shared_ptr<RelayCommand> RemoveWatchCommand;
    std::shared_ptr<RelayCommand> EditWatchCommand;
    
    std::shared_ptr<RelayCommand> EvaluateExpressionCommand;
    std::shared_ptr<RelayCommand> SetVariableValueCommand;
    
    std::shared_ptr<RelayCommand> ClearDebugOutputCommand;

    // ========================================================================
    // Debug Control
    // ========================================================================
    
    /**
     * @brief Start debugging the current document
     * @param filePath File to debug
     * @return true if started successfully
     */
    Q_INVOKABLE bool startDebugging(const QString& filePath);
    
    /**
     * @brief Stop the current debug session
     */
    Q_INVOKABLE void stopDebugging();
    
    /**
     * @brief Restart the debug session
     */
    Q_INVOKABLE void restartDebugging();
    
    /**
     * @brief Continue execution
     */
    Q_INVOKABLE void continueExecution();
    
    /**
     * @brief Pause execution
     */
    Q_INVOKABLE void pauseExecution();
    
    /**
     * @brief Step over (execute current line, don't enter functions)
     */
    Q_INVOKABLE void stepOver();
    
    /**
     * @brief Step into (enter function calls)
     */
    Q_INVOKABLE void stepInto();
    
    /**
     * @brief Step out (complete current function)
     */
    Q_INVOKABLE void stepOut();
    
    /**
     * @brief Run to cursor position
     * @param line Line number to run to
     */
    Q_INVOKABLE void runToCursor(int line);
    
    /**
     * @brief Set next statement (jump to line)
     * @param line Line number
     * @return true if successful
     */
    Q_INVOKABLE bool setNextStatement(int line);

    // ========================================================================
    // Breakpoint Management
    // ========================================================================
    
    /**
     * @brief Toggle breakpoint at line
     * @param file File path
     * @param line Line number
     * @return Breakpoint ID, or -1 if failed
     */
    Q_INVOKABLE int toggleBreakpoint(const QString& file, int line);
    
    /**
     * @brief Enable breakpoint
     * @param id Breakpoint ID
     */
    Q_INVOKABLE void enableBreakpoint(int id);
    
    /**
     * @brief Disable breakpoint
     * @param id Breakpoint ID
     */
    Q_INVOKABLE void disableBreakpoint(int id);
    
    /**
     * @brief Delete breakpoint
     * @param id Breakpoint ID
     */
    Q_INVOKABLE void deleteBreakpoint(int id);
    
    /**
     * @brief Delete all breakpoints
     */
    Q_INVOKABLE void deleteAllBreakpoints();
    
    /**
     * @brief Set breakpoint condition
     * @param id Breakpoint ID
     * @param condition Condition expression
     */
    Q_INVOKABLE void setBreakpointCondition(int id, const QString& condition);
    
    /**
     * @brief Get breakpoint at location
     * @param file File path
     * @param line Line number
     * @return Breakpoint info, or empty optional
     */
    Q_INVOKABLE std::optional<BreakpointInfo> getBreakpointAt(const QString& file, int line) const;

    // ========================================================================
    // Variable Inspection
    // ========================================================================
    
    /**
     * @brief Add watch expression
     * @param expression Expression to watch
     * @return Watch ID
     */
    Q_INVOKABLE int addWatch(const QString& expression);
    
    /**
     * @brief Remove watch expression
     * @param id Watch ID
     */
    Q_INVOKABLE void removeWatch(int id);
    
    /**
     * @brief Edit watch expression
     * @param id Watch ID
     * @param expression New expression
     */
    Q_INVOKABLE void editWatch(int id, const QString& expression);
    
    /**
     * @brief Evaluate expression
     * @param expression Expression to evaluate
     * @return Evaluation result
     */
    Q_INVOKABLE QString evaluateExpression(const QString& expression);
    
    /**
     * @brief Set variable value
     * @param name Variable name
     * @param value New value
     * @return true if successful
     */
    Q_INVOKABLE bool setVariableValue(const QString& name, const QString& value);
    
    /**
     * @brief Get variable value
     * @param name Variable name
     * @return Variable value
     */
    Q_INVOKABLE QString getVariableValue(const QString& name) const;
    
    /**
     * @brief Refresh variable display
     */
    Q_INVOKABLE void refreshVariables();

    // ========================================================================
    // Call Stack
    // ========================================================================
    
    /**
     * @brief Get stack frame at index
     * @param index Frame index
     * @return Stack frame info
     */
    Q_INVOKABLE StackFrameInfo getStackFrame(int index) const;
    
    /**
     * @brief Navigate to stack frame
     * @param index Frame index
     */
    Q_INVOKABLE void navigateToFrame(int index);

    // ========================================================================
    // Output
    // ========================================================================
    
    /**
     * @brief Append output text
     * @param text Text to append
     */
    Q_INVOKABLE void appendOutput(const QString& text);
    
    /**
     * @brief Clear debug output
     */
    Q_INVOKABLE void clearOutput();

    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Enable/disable breaking on exceptions
     * @param enabled Enable state
     */
    Q_INVOKABLE void setBreakOnExceptions(bool enabled);
    
    /**
     * @brief Enable/disable stepping through system code
     * @param enabled Enable state
     */
    Q_INVOKABLE void setStepThroughSystemCode(bool enabled);

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    void onLoad() override;
    void onUnload() override;
    bool validate() override;

signals:
    // ========================================================================
    // Debug Events
    // ========================================================================
    
    void debugSessionStarted();
    void debugSessionEnded();
    void debugStateChanged(DebugState state);
    
    void executionResumed();
    void executionPaused();
    void executionStopped(int line, const QString& file);
    
    void breakpointHit(int breakpointId);
    void exceptionThrown(const QString& message);
    
    void variablesRefreshed();
    void stackFrameChanged(int frameIndex);
    void outputAppended(const QString& text);

protected:
    // ========================================================================
    // Protected Helpers
    // ========================================================================
    
    void setupCommands();
    void setupBindings();
    void updateCommandStates();
    void updateStateProperties();
    
    bool canStartDebugging() const;
    bool canStopDebugging() const;
    bool canContinue() const;
    bool canStep() const;

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    DebugState m_currentState = DebugState::Inactive;
    bool m_breakOnExceptions = true;
    bool m_stepThroughSystemCode = false;
    
    // Break on exception setting
    ObservableProperty<bool> BreakOnExceptions;
    ObservableProperty<bool> StepThroughSystemCode;
};

} // namespace Flux::MVVM

#endif // FLUX_VIEWMODELS_DEBUG_VIEWMODEL_H
