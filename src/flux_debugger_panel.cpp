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

#include "flux/flux_debugger_panel.h"

#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QTextEdit>
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>

namespace Flux {

// ============================================================================
// BreakpointTree Implementation
// ============================================================================

BreakpointTree::BreakpointTree(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderLabels(QStringList() << "File" << "Line" << "Condition" << "Hits" << "Enabled");
    setColumnWidth(0, 150);
    setColumnWidth(1, 60);
    setColumnWidth(2, 150);
    setColumnWidth(3, 50);
    setColumnWidth(4, 70);
    setAlternatingRowColors(true);
    setRootIsDecorated(false);

    connect(this, &QTreeWidget::itemChanged,
            this, &BreakpointTree::onItemChanged);
    connect(this, &QTreeWidget::itemDoubleClicked,
            this, &BreakpointTree::onItemDoubleClicked);
}

void BreakpointTree::addBreakpoint(const BreakpointInfo& info)
{
    m_breakpoints[info.line] = info;

    auto* item = new QTreeWidgetItem();
    item->setData(0, Qt::UserRole, info.line);
    item->setText(0, "script.flux");  // Would get from editor
    item->setText(1, QString::number(info.line));
    item->setText(2, info.condition);
    item->setText(3, QString::number(info.hitCount));
    item->setCheckState(4, info.enabled ? Qt::Checked : Qt::Unchecked);
    
    // Color code based on state
    if (!info.enabled) {
        for (int i = 0; i < columnCount(); ++i) {
            item->setForeground(i, QColor(150, 150, 150));
        }
    }

    addTopLevelItem(item);
}

void BreakpointTree::updateBreakpoint(int line, const BreakpointInfo& info)
{
    m_breakpoints[line] = info;
    auto* item = findBreakpointItem(line);
    if (item) {
        updateItem(item, info);
    }
}

void BreakpointTree::removeBreakpoint(int line)
{
    m_breakpoints.remove(line);
    auto* item = findBreakpointItem(line);
    if (item) {
        delete item;
    }
}

void BreakpointTree::clearBreakpoints()
{
    m_breakpoints.clear();
    clear();
}

QList<int> BreakpointTree::breakpointLines() const
{
    return m_breakpoints.keys();
}

BreakpointInfo BreakpointTree::breakpointAt(int line) const
{
    return m_breakpoints.value(line);
}

QMap<int, BreakpointInfo> BreakpointTree::allBreakpoints() const
{
    return m_breakpoints;
}

QTreeWidgetItem* BreakpointTree::findBreakpointItem(int line) const
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        auto* item = topLevelItem(i);
        if (item->data(0, Qt::UserRole).toInt() == line) {
            return item;
        }
    }
    return nullptr;
}

void BreakpointTree::updateItem(QTreeWidgetItem* item, const BreakpointInfo& info)
{
    item->setText(1, QString::number(info.line));
    item->setText(2, info.condition);
    item->setText(3, QString::number(info.hitCount));
    item->setCheckState(4, info.enabled ? Qt::Checked : Qt::Unchecked);

    if (!info.enabled) {
        for (int i = 0; i < columnCount(); ++i) {
            item->setForeground(i, QColor(150, 150, 150));
        }
    } else {
        for (int i = 0; i < columnCount(); ++i) {
            item->setForeground(i, palette().color(QPalette::Text));
        }
    }
}

void BreakpointTree::contextMenuEvent(QContextMenuEvent* event)
{
    auto* item = itemAt(event->pos());
    if (!item) {
        return;
    }

    int line = item->data(0, Qt::UserRole).toInt();

    QMenu menu(this);
    QAction* deleteAction = menu.addAction("Delete Breakpoint");
    QAction* disableAction = menu.addAction(m_breakpoints[line].enabled ? "Disable" : "Enable");
    QAction* editConditionAction = menu.addAction("Edit Condition...");

    connect(deleteAction, &QAction::triggered, this, [this, line]() {
        Q_EMIT breakpointDeleted(line);
    });

    connect(disableAction, &QAction::triggered, this, [this, line]() {
        bool currentlyEnabled = m_breakpoints[line].enabled;
        Q_EMIT breakpointToggled(line, !currentlyEnabled);
    });

    connect(editConditionAction, &QAction::triggered, this, [this, line]() {
        // Would open condition editor dialog
        QString condition = QInputDialog::getText(
            this, "Breakpoint Condition",
            "Enter condition (empty for unconditional):"
        );
        BreakpointInfo info = m_breakpoints[line];
        info.condition = condition;
        updateBreakpoint(line, info);
    });

    menu.exec(event->globalPos());
}

void BreakpointTree::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 4) {  // Enabled column
        int line = item->data(0, Qt::UserRole).toInt();
        bool enabled = (item->checkState(4) == Qt::Checked);
        Q_EMIT breakpointToggled(line, enabled);
    }
}

void BreakpointTree::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    int line = item->data(0, Qt::UserRole).toInt();
    Q_EMIT breakpointDoubleClicked(line);
}

// ============================================================================
// VariableTable Implementation
// ============================================================================

VariableTable::VariableTable(QWidget* parent)
    : QTableWidget(parent)
{
    setupColumns();
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);

    connect(this, &QTableWidget::cellChanged,
            this, &VariableTable::onCellChanged);
}

void VariableTable::setupColumns()
{
    setColumnCount(4);
    setHorizontalHeaderLabels(QStringList() << "Name" << "Value" << "Type" << "Scope");
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    setColumnWidth(0, 120);
    setColumnWidth(2, 100);
    setColumnWidth(3, 80);
}

void VariableTable::addVariable(const VariableInfo& var)
{
    m_variables[var.name] = var;

    int row = rowCount();
    insertRow(row);

    auto* nameItem = new QTableWidgetItem(var.name);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 0, nameItem);

    auto* valueItem = new QTableWidgetItem(var.value);
    if (var.changed) {
        valueItem->setBackground(QColor(255, 255, 200));
    }
    setItem(row, 1, valueItem);

    auto* typeItem = new QTableWidgetItem(var.type);
    typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 2, typeItem);

    auto* scopeItem = new QTableWidgetItem(var.scope == 0 ? "Local" : "Global");
    scopeItem->setFlags(scopeItem->flags() & ~Qt::ItemIsEditable);
    setItem(row, 3, scopeItem);
}

void VariableTable::updateVariable(const QString& name, const VariableInfo& var)
{
    m_variables[name] = var;
    int row = findVariableRow(name);
    if (row >= 0) {
        QTableWidgetItem* valueItem = item(row, 1);
        QString oldValue = valueItem ? valueItem->text() : "";
        
        if (valueItem) {
            valueItem->setText(var.value);
            if (var.changed) {
                valueItem->setBackground(QColor(255, 255, 200));
            } else {
                valueItem->setBackground(Qt::transparent);
            }
        }

        if (!oldValue.isEmpty() && oldValue != var.value) {
            Q_EMIT valueChanged(name, oldValue, var.value);
        }
    }
}

void VariableTable::removeVariable(const QString& name)
{
    m_variables.remove(name);
    int row = findVariableRow(name);
    if (row >= 0) {
        removeRow(row);
    }
}

void VariableTable::clearVariables()
{
    m_variables.clear();
    setRowCount(0);
}

void VariableTable::addWatchExpression(const QString& expression)
{
    if (!m_watchExpressions.contains(expression)) {
        m_watchExpressions.append(expression);
        Q_EMIT expressionAdded(expression);
    }
}

void VariableTable::removeWatchExpression(const QString& expression)
{
    m_watchExpressions.removeAll(expression);
    Q_EMIT expressionRemoved(expression);
}

QStringList VariableTable::watchExpressions() const
{
    return m_watchExpressions;
}

int VariableTable::findVariableRow(const QString& name) const
{
    for (int row = 0; row < rowCount(); ++row) {
        if (item(row, 0) && item(row, 0)->text() == name) {
            return row;
        }
    }
    return -1;
}

void VariableTable::onCellChanged(int row, int column)
{
    if (column == 1) {  // Value column
        // Allow manual value editing for simulation debugging
        QString name = item(row, 0)->text();
        QString newValue = item(row, 1)->text();
        Q_EMIT valueChanged(name, "", newValue);
    }
}

// ============================================================================
// CallStackTree Implementation
// ============================================================================

CallStackTree::CallStackTree(QWidget* parent)
    : QTreeWidget(parent)
    , m_currentFrame(-1)
{
    setHeaderLabels(QStringList() << "Function" << "Location");
    setColumnWidth(0, 200);
    setColumnWidth(1, 150);

    connect(this, &QTreeWidget::currentItemChanged,
            this, &CallStackTree::onCurrentItemChanged);
}

void CallStackTree::setStackFrames(const QList<StackFrame>& frames)
{
    m_frames = frames;
    clear();

    for (const auto& frame : frames) {
        auto* item = new QTreeWidgetItem();
        item->setText(0, frame.functionName);
        item->setText(1, QString("%1:%2").arg(frame.fileName).arg(frame.lineNumber));
        item->setData(0, Qt::UserRole, frame.frameIndex);
        addTopLevelItem(item);
    }

    if (!frames.isEmpty()) {
        setCurrentItem(topLevelItem(0));
    }
}

void CallStackTree::clearStack()
{
    m_frames.clear();
    m_currentFrame = -1;
    clear();
}

void CallStackTree::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (current) {
        m_currentFrame = current->data(0, Qt::UserRole).toInt();
        Q_EMIT frameSelected(m_currentFrame);
    }
}

// ============================================================================
// DebuggerPanel Implementation
// ============================================================================

DebuggerPanel::DebuggerPanel(QWidget* parent)
    : QDockWidget("Debugger", parent)
    , m_isDebugging(false)
    , m_currentLine(0)
{
    setObjectName("DebuggerPanel");
    setupUI();
    setupConnections();
}

DebuggerPanel::~DebuggerPanel()
{
}

void DebuggerPanel::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    setupToolbar();
    createTabWidget();

    m_centralWidget->setLayout(m_mainLayout);
    setWidget(m_centralWidget);
}

void DebuggerPanel::setupToolbar()
{
    m_toolbar = new QToolBar(m_centralWidget);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));

    // Continue button
    m_continueBtn = new QPushButton("▶ Continue");
    m_continueBtn->setToolTip("Continue execution (F5)");
    m_continueBtn->setStyleSheet(
        "QPushButton { background: #4CAF50; color: white; border: none; padding: 4px 8px; border-radius: 2px; } "
        "QPushButton:hover { background: #45a049; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    m_toolbar->addWidget(m_continueBtn);

    // Step Over button
    m_stepOverBtn = new QPushButton("⏭ Step Over");
    m_stepOverBtn->setToolTip("Step over (F10)");
    m_stepOverBtn->setStyleSheet(
        "QPushButton { background: #2196F3; color: white; border: none; padding: 4px 8px; border-radius: 2px; } "
        "QPushButton:hover { background: #1e88e5; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    m_toolbar->addWidget(m_stepOverBtn);

    // Step Into button
    m_stepIntoBtn = new QPushButton("⤵ Step Into");
    m_stepIntoBtn->setToolTip("Step into (F11)");
    m_stepIntoBtn->setStyleSheet(
        "QPushButton { background: #9C27B0; color: white; border: none; padding: 4px 8px; border-radius: 2px; } "
        "QPushButton:hover { background: #8e24aa; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    m_toolbar->addWidget(m_stepIntoBtn);

    // Step Out button
    m_stepOutBtn = new QPushButton("⤴ Step Out");
    m_stepOutBtn->setToolTip("Step out (Shift+F11)");
    m_stepOutBtn->setStyleSheet(
        "QPushButton { background: #FF9800; color: white; border: none; padding: 4px 8px; border-radius: 2px; } "
        "QPushButton:hover { background: #e68900; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    m_toolbar->addWidget(m_stepOutBtn);

    // Stop button
    m_stopBtn = new QPushButton("⏹ Stop");
    m_stopBtn->setToolTip("Stop debugging (Shift+F5)");
    m_stopBtn->setStyleSheet(
        "QPushButton { background: #f44336; color: white; border: none; padding: 4px 8px; border-radius: 2px; } "
        "QPushButton:hover { background: #e53935; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    m_toolbar->addWidget(m_stopBtn);

    m_mainLayout->addWidget(m_toolbar);
}

void DebuggerPanel::createTabWidget()
{
    m_tabWidget = new QTabWidget(m_centralWidget);

    // Breakpoints tab
    m_breakpointTree = new BreakpointTree(m_centralWidget);
    m_tabWidget->addTab(m_breakpointTree, "Breakpoints");

    // Variables tab
    m_variableTable = new VariableTable(m_centralWidget);
    m_tabWidget->addTab(m_variableTable, "Variables");

    // Call Stack tab
    m_callStackTree = new CallStackTree(m_centralWidget);
    m_tabWidget->addTab(m_callStackTree, "Call Stack");

    m_mainLayout->addWidget(m_tabWidget);
}

void DebuggerPanel::setupConnections()
{
    connect(m_continueBtn, &QPushButton::clicked,
            this, &DebuggerPanel::continueExecution);
    connect(m_stepOverBtn, &QPushButton::clicked,
            this, &DebuggerPanel::stepOver);
    connect(m_stepIntoBtn, &QPushButton::clicked,
            this, &DebuggerPanel::stepInto);
    connect(m_stepOutBtn, &QPushButton::clicked,
            this, &DebuggerPanel::stepOut);
    connect(m_stopBtn, &QPushButton::clicked,
            this, &DebuggerPanel::stopDebugging);

    connect(m_breakpointTree, &BreakpointTree::breakpointToggled,
            this, [this](int line, bool enabled) {
                enableBreakpoint(line, enabled);
            });
    connect(m_breakpointTree, &BreakpointTree::breakpointDeleted,
            this, [this](int line) {
                removeBreakpoint(line);
            });
    connect(m_breakpointTree, &BreakpointTree::breakpointDoubleClicked,
            this, [this](int line) {
                // Navigate to breakpoint line
                updateCurrentLine(line);
            });

    connect(m_callStackTree, &CallStackTree::frameSelected,
            this, &DebuggerPanel::setCurrentFrame);
}

void DebuggerPanel::startDebugging()
{
    m_isDebugging = true;
    m_continueBtn->setEnabled(true);
    m_stepOverBtn->setEnabled(true);
    m_stepIntoBtn->setEnabled(true);
    m_stepOutBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
    Q_EMIT debuggingStarted();
}

void DebuggerPanel::stopDebugging()
{
    m_isDebugging = false;
    m_currentLine = 0;
    m_continueBtn->setEnabled(false);
    m_stepOverBtn->setEnabled(false);
    m_stepIntoBtn->setEnabled(false);
    m_stepOutBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_callStackTree->clearStack();
    m_variableTable->clearVariables();
    Q_EMIT debuggingStopped();
}

void DebuggerPanel::pauseDebugging()
{
    Q_EMIT debuggingPaused();
}

void DebuggerPanel::stepOver()
{
    Q_EMIT stepOverRequested();
}

void DebuggerPanel::stepInto()
{
    Q_EMIT stepIntoRequested();
}

void DebuggerPanel::stepOut()
{
    Q_EMIT stepOutRequested();
}

void DebuggerPanel::continueExecution()
{
    Q_EMIT continueRequested();
}

void DebuggerPanel::addBreakpoint(int line, const QString& condition, bool temporary)
{
    BreakpointInfo info;
    info.line = line;
    info.enabled = true;
    info.condition = condition;
    info.hitCount = 0;
    info.temporary = temporary;

    m_breakpointTree->addBreakpoint(info);
    Q_EMIT breakpointAdded(line, condition);
}

void DebuggerPanel::removeBreakpoint(int line)
{
    m_breakpointTree->removeBreakpoint(line);
    Q_EMIT breakpointRemoved(line);
}

void DebuggerPanel::enableBreakpoint(int line, bool enabled)
{
    BreakpointInfo info = m_breakpointTree->breakpointAt(line);
    info.enabled = enabled;
    m_breakpointTree->updateBreakpoint(line, info);
    Q_EMIT breakpointEnabled(line, enabled);
}

void DebuggerPanel::clearAllBreakpoints()
{
    m_breakpointTree->clearBreakpoints();
}

QList<int> DebuggerPanel::breakpointLines() const
{
    return m_breakpointTree->breakpointLines();
}

bool DebuggerPanel::hasBreakpoint(int line) const
{
    return m_breakpointTree->breakpointLines().contains(line);
}

void DebuggerPanel::addWatch(const QString& expression)
{
    m_variableTable->addWatchExpression(expression);
}

void DebuggerPanel::removeWatch(const QString& expression)
{
    m_variableTable->removeWatchExpression(expression);
}

void DebuggerPanel::updateVariables(const QList<VariableInfo>& vars)
{
    m_variableTable->clearVariables();
    for (const auto& var : vars) {
        m_variableTable->addVariable(var);
    }
}

void DebuggerPanel::setCallStack(const QList<StackFrame>& frames)
{
    m_callStackTree->setStackFrames(frames);
}

void DebuggerPanel::setCurrentFrame(int frameIndex)
{
    m_callStackTree->setCurrentFrame(frameIndex);
}

void DebuggerPanel::onBreakpointHit(int line)
{
    updateCurrentLine(line);
    BreakpointInfo info = m_breakpointTree->breakpointAt(line);
    info.hitCount++;
    m_breakpointTree->updateBreakpoint(line, info);
}

void DebuggerPanel::onExecutionPaused()
{
    // Update UI to show paused state
}

void DebuggerPanel::onExecutionResumed()
{
    // Update UI to show running state
}

void DebuggerPanel::onExecutionStopped()
{
    stopDebugging();
}

void DebuggerPanel::updateCurrentLine(int line)
{
    m_currentLine = line;
    // Would highlight line in editor
}

// ============================================================================
// DebugOutputPanel Implementation
// ============================================================================

DebugOutputPanel::DebugOutputPanel(QWidget* parent)
    : QDockWidget("Debug Output", parent)
{
    setObjectName("DebugOutputPanel");
    setupUI();
}

void DebugOutputPanel::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar
    m_toolbar = new QToolBar(centralWidget);
    m_toolbar->setMovable(false);

    m_clearBtn = new QPushButton("Clear");
    m_clearBtn->setToolTip("Clear output");
    m_toolbar->addWidget(m_clearBtn);

    m_saveBtn = new QPushButton("Save");
    m_saveBtn->setToolTip("Save output to file");
    m_toolbar->addWidget(m_saveBtn);

    m_toolbar->addSeparator();

    auto* filterLabel = new QLabel("Filter:");
    m_toolbar->addWidget(filterLabel);

    auto* filterCombo = new QComboBox();
    filterCombo->addItem("All Messages");
    filterCombo->addItem("Errors Only");
    filterCombo->addItem("Warnings Only");
    m_toolbar->addWidget(filterCombo);

    layout->addWidget(m_toolbar);

    // Output edit
    m_outputEdit = new QTextEdit();
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setFont(QFont("Consolas", 9));
    m_outputEdit->setStyleSheet(
        "QTextEdit { background: #1E1E1E; color: #C8C8C8; border: none; }"
    );
    layout->addWidget(m_outputEdit);

    setWidget(centralWidget);

    connect(m_clearBtn, &QPushButton::clicked,
            this, &DebugOutputPanel::clearOutput);
    connect(m_saveBtn, &QPushButton::clicked,
            this, [this]() {
                QString fileName = QFileDialog::getSaveFileName(
                    this, "Save Debug Output", "", "Text Files (*.txt)"
                );
                if (!fileName.isEmpty()) {
                    QFile file(fileName);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(m_outputEdit->toPlainText().toUtf8());
                    }
                }
            });
}

void DebugOutputPanel::appendMessage(const QString& message, int level)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString indent(level * 2, ' ');
    m_outputEdit->append(QString("[%1] %2%3").arg(timestamp, indent, message));
}

void DebugOutputPanel::appendError(const QString& error)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_outputEdit->append(QString("<font color='#f44336'>[%1] ERROR: %2</font>").arg(timestamp, error));
}

void DebugOutputPanel::appendWarning(const QString& warning)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_outputEdit->append(QString("<font color='#FF9800'>[%1] WARNING: %2</font>").arg(timestamp, warning));
}

void DebugOutputPanel::clearOutput()
{
    m_outputEdit->clear();
}

void DebugOutputPanel::onDebugMessage(const QString& message)
{
    appendMessage(message);
}

void DebugOutputPanel::onDebugError(const QString& error)
{
    appendError(error);
}

} // namespace Flux
