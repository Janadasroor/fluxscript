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

#ifndef FLUX_DEBUGGER_PANEL_H
#define FLUX_DEBUGGER_PANEL_H

/**
 * @file flux_debugger_panel.h
 * @brief Debugger panel for FluxScript with VioSpice integration
 */

#include <QWidget>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QSet>
#include <QInputDialog>
#include <QContextMenuEvent>

namespace Flux {

/**
 * @brief Breakpoint information structure
 */
struct BreakpointInfo {
    int line;
    bool enabled;
    QString condition;
    int hitCount;
    bool temporary;
};

/**
 * @brief Variable information structure
 */
struct VariableInfo {
    QString name;
    QString value;
    QString type;
    int scope;  // 0=local, 1=global
    bool changed;
};

/**
 * @brief Call stack frame information
 */
struct StackFrame {
    int frameIndex;
    QString functionName;
    QString fileName;
    int lineNumber;
    QMap<QString, QString> locals;
};

/**
 * @brief Breakpoint tree widget
 */
class BreakpointTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit BreakpointTree(QWidget* parent = nullptr);

    // Breakpoint management
    void addBreakpoint(const BreakpointInfo& info);
    void updateBreakpoint(int line, const BreakpointInfo& info);
    void removeBreakpoint(int line);
    void clearBreakpoints();

    // Access
    QList<int> breakpointLines() const;
    BreakpointInfo breakpointAt(int line) const;
    QMap<int, BreakpointInfo> allBreakpoints() const;

Q_SIGNALS:
    void breakpointToggled(int line, bool enabled);
    void breakpointDeleted(int line);
    void breakpointDoubleClicked(int line);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private Q_SLOTS:
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    QTreeWidgetItem* findBreakpointItem(int line) const;
    void updateItem(QTreeWidgetItem* item, const BreakpointInfo& info);

    QMap<int, BreakpointInfo> m_breakpoints;
};

/**
 * @brief Variable watch table
 */
class VariableTable : public QTableWidget {
    Q_OBJECT

public:
    explicit VariableTable(QWidget* parent = nullptr);

    // Variable management
    void addVariable(const VariableInfo& var);
    void updateVariable(const QString& name, const VariableInfo& var);
    void removeVariable(const QString& name);
    void clearVariables();

    // Watch expressions
    void addWatchExpression(const QString& expression);
    void removeWatchExpression(const QString& expression);
    QStringList watchExpressions() const;

Q_SIGNALS:
    void valueChanged(const QString& name, const QString& oldValue, const QString& newValue);
    void expressionAdded(const QString& expression);
    void expressionRemoved(const QString& expression);

private Q_SLOTS:
    void onCellChanged(int row, int column);

private:
    void setupColumns();
    int findVariableRow(const QString& name) const;

    QMap<QString, VariableInfo> m_variables;
    QStringList m_watchExpressions;
};

/**
 * @brief Call stack tree widget
 */
class CallStackTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit CallStackTree(QWidget* parent = nullptr);

    // Stack management
    void setStackFrames(const QList<StackFrame>& frames);
    void clearStack();
    int currentFrame() const { return m_currentFrame; }
    void setCurrentFrame(int frameIndex) { m_currentFrame = frameIndex; }

Q_SIGNALS:
    void frameSelected(int frameIndex);

private Q_SLOTS:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

private:
    void setupUI();

    int m_currentFrame;
    QList<StackFrame> m_frames;
};

/**
 * @brief Main debugger panel
 *
 * Provides debugging controls, breakpoint management, variable watching,
 * and call stack navigation
 */
class DebuggerPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit DebuggerPanel(QWidget* parent = nullptr);
    ~DebuggerPanel() override;

    // Breakpoint management
    void addBreakpoint(int line, const QString& condition = QString(), bool temporary = false);
    void removeBreakpoint(int line);
    void enableBreakpoint(int line, bool enabled);
    void clearAllBreakpoints();
    QList<int> breakpointLines() const;
    bool hasBreakpoint(int line) const;

    // Variable watching
    void addWatch(const QString& expression);
    void removeWatch(const QString& expression);
    void updateVariables(const QList<VariableInfo>& vars);

    // Stack management
    void setCallStack(const QList<StackFrame>& frames);
    void setCurrentFrame(int frameIndex);

    // State
    bool isDebugging() const { return m_isDebugging; }
    int currentLine() const { return m_currentLine; }

public Q_SLOTS:
    void startDebugging();
    void stopDebugging();
    void pauseDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void continueExecution();

    void onBreakpointHit(int line);
    void onExecutionPaused();
    void onExecutionResumed();
    void onExecutionStopped();
    void updateCurrentLine(int line);

Q_SIGNALS:
    void debuggingStarted();
    void debuggingStopped();
    void debuggingPaused();
    void debuggingResumed();
    void stepOverRequested();
    void stepIntoRequested();
    void stepOutRequested();
    void continueRequested();

    void breakpointAdded(int line, const QString& condition);
    void breakpointRemoved(int line);
    void breakpointEnabled(int line, bool enabled);

private:
    void setupUI();
    void setupConnections();
    void setupToolbar();
    void createTabWidget();

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QTabWidget* m_tabWidget;

    // Toolbar buttons
    QPushButton* m_continueBtn;
    QPushButton* m_stepOverBtn;
    QPushButton* m_stepIntoBtn;
    QPushButton* m_stepOutBtn;
    QPushButton* m_stopBtn;

    // Panels
    BreakpointTree* m_breakpointTree;
    VariableTable* m_variableTable;
    CallStackTree* m_callStackTree;

    // State
    bool m_isDebugging;
    int m_currentLine;
    QString m_currentFile;
};

/**
 * @brief Debug output panel
 *
 * Shows debug messages, warnings, and errors
 */
class DebugOutputPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit DebugOutputPanel(QWidget* parent = nullptr);

    // Output methods
    void appendMessage(const QString& message, int level = 0);
    void appendError(const QString& error);
    void appendWarning(const QString& warning);
    void clearOutput();

public Q_SLOTS:
    void onDebugMessage(const QString& message);
    void onDebugError(const QString& error);

private:
    void setupUI();

    QTextEdit* m_outputEdit;
    QToolBar* m_toolbar;
    QPushButton* m_clearBtn;
    QPushButton* m_saveBtn;
};

} // namespace Flux

#endif // FLUX_DEBUGGER_PANEL_H
