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

#ifndef FLUX_DEBUG_PANEL_H
#define FLUX_DEBUG_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QToolBar>
#include <QProcess>
#include <QMap>
#include <QList>

namespace Flux {

/**
 * @brief Debug information item (variables, stack frame, etc.)
 */
struct DebugItem {
    QString name;
    QString value;
    QString type;
    QList<DebugItem> children;
};

/**
 * @brief Stack frame information
 */
struct StackFrame {
    int frameIndex;
    QString functionName;
    QString fileName;
    int lineNumber;
    QString module;
};

/**
 * @brief Debug panel for displaying variables, watch expressions, and call stack
 */
class DebugPanel : public QWidget {
    Q_OBJECT

public:
    explicit DebugPanel(QWidget* parent = nullptr);
    ~DebugPanel() override;

    void clear();
    
    // Variables panel
    void setVariables(const QList<DebugItem>& variables);
    void addVariable(const DebugItem& variable);
    
    // Watch panel
    void addWatch(const QString& expression);
    void removeWatch(int index);
    void updateWatchValue(int index, const QString& value);
    
    // Call stack
    void setCallStack(const QList<StackFrame>& frames);
    void setCurrentFrame(int frameIndex);
    
    // Breakpoints
    void setBreakpoints(const QList<QPair<QString, int>>& breakpoints);
    void addBreakpoint(const QString& file, int line);
    void removeBreakpoint(const QString& file, int line);
    
    // State
    bool isDebugging() const { return m_isDebugging; }
    void setDebuggingState(bool debugging);
    
    void setCurrentLine(const QString& file, int line);
    void clearCurrentLine();

public Q_SLOTS:
    void startDebugging(const QString& scriptPath);
    void stopDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void continueExecution();

Q_SIGNALS:
    void debuggingStarted();
    void debuggingStopped();
    void breakpointHit(const QString& file, int line);
    void stepCompleted();
    void executionContinued();
    void variableChanged(const QString& name, const QString& newValue);

private:
    void setupUI();
    void setupConnections();
    void updateToolbar();
    
    QTreeWidget* m_variablesTree;
    QTreeWidget* m_watchTree;
    QTreeWidget* m_callStackTree;
    QTreeWidget* m_breakpointsTree;
    
    QToolBar* m_debugToolbar;
    
    QProcess* m_debugProcess;
    bool m_isDebugging;
    
    QString m_currentFile;
    int m_currentLine;
    QList<StackFrame> m_callStack;
    QList<DebugItem> m_variables;
    QList<QPair<QString, int>> m_breakpoints;
};

} // namespace Flux

#endif // FLUX_DEBUG_PANEL_H
