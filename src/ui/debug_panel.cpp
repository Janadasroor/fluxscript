#include "flux/ui/debug_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QMessageBox>

namespace Flux {

DebugPanel::DebugPanel(QWidget* parent)
    : QWidget(parent)
    , m_variablesTree(nullptr)
    , m_watchTree(nullptr)
    , m_callStackTree(nullptr)
    , m_breakpointsTree(nullptr)
    , m_debugToolbar(nullptr)
    , m_debugProcess(nullptr)
    , m_isDebugging(false)
    , m_currentLine(-1)
{
    setupUI();
    setupConnections();
}

DebugPanel::~DebugPanel()
{
    if (m_debugProcess) {
        m_debugProcess->kill();
        m_debugProcess->waitForFinished(1000);
    }
}

void DebugPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Debug toolbar
    m_debugToolbar = new QToolBar();
    m_debugToolbar->setMovable(false);
    m_debugToolbar->setIconSize(QSize(16, 16));
    m_debugToolbar->setStyleSheet(
        "QToolBar { background-color: #2d2d2d; border: none; padding: 2px; spacing: 2px; }"
        "QToolButton { background-color: #3c3c3c; color: #cccccc; border: none; border-radius: 2px; padding: 4px 8px; }"
        "QToolButton:hover { background-color: #4c4c4c; }"
        "QToolButton:disabled { color: #666666; }"
    );
    
    auto* continueBtn = new QToolButton();
    continueBtn->setText("▶ Continue");
    continueBtn->setToolTip("Continue (F5)");
    continueBtn->setEnabled(false);
    
    auto* stopBtn = new QToolButton();
    stopBtn->setText("⏹ Stop");
    stopBtn->setToolTip("Stop Debugging");
    
    m_debugToolbar->addWidget(continueBtn);
    m_debugToolbar->addSeparator();
    
    auto* stepOverBtn = new QToolButton();
    stepOverBtn->setText("⏭ Over");
    stepOverBtn->setToolTip("Step Over (F10)");
    stepOverBtn->setEnabled(false);
    
    auto* stepIntoBtn = new QToolButton();
    stepIntoBtn->setText("⬇ Into");
    stepIntoBtn->setToolTip("Step Into (F11)");
    stepIntoBtn->setEnabled(false);
    
    auto* stepOutBtn = new QToolButton();
    stepOutBtn->setText("⬆ Out");
    stepOutBtn->setToolTip("Step Out (Shift+F11)");
    stepOutBtn->setEnabled(false);
    
    m_debugToolbar->addWidget(stepOverBtn);
    m_debugToolbar->addWidget(stepIntoBtn);
    m_debugToolbar->addWidget(stepOutBtn);
    
    layout->addWidget(m_debugToolbar);
    
    // Status label
    auto* statusLabel = new QLabel("Not debugging");
    statusLabel->setStyleSheet("color: #858585; padding: 4px 8px; background-color: #252526;");
    layout->addWidget(statusLabel);
    
    // Tab-like interface for different panels
    auto* panelsWidget = new QWidget();
    auto* panelsLayout = new QVBoxLayout(panelsWidget);
    panelsLayout->setContentsMargins(0, 0, 0, 0);
    panelsLayout->setSpacing(4);
    panelsWidget->setStyleSheet("background-color: #252526;");
    
    // Variables tree
    auto* variablesLabel = new QLabel("📦 Variables");
    variablesLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 4px; background-color: #2d2d2d;");
    panelsLayout->addWidget(variablesLabel);
    
    m_variablesTree = new QTreeWidget();
    m_variablesTree->setHeaderLabels(QStringList() << "Name" << "Value" << "Type");
    m_variablesTree->setColumnWidth(0, 120);
    m_variablesTree->setColumnWidth(1, 150);
    m_variablesTree->setColumnWidth(2, 80);
    m_variablesTree->setStyleSheet(
        "QTreeWidget { background-color: #252526; color: #cccccc; border: none; }"
        "QTreeWidget::item { padding: 2px; }"
        "QTreeWidget::item:hover { background-color: #2a2d2e; }"
        "QTreeWidget::item:selected { background-color: #37373d; }"
        "QHeaderView::section { background-color: #2d2d2d; color: #cccccc; border: none; padding: 4px; }"
    );
    m_variablesTree->setIndentation(16);
    panelsLayout->addWidget(m_variablesTree, 1);
    
    // Watch tree
    auto* watchLabel = new QLabel("👁 Watch");
    watchLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 4px; background-color: #2d2d2d;");
    panelsLayout->addWidget(watchLabel);
    
    m_watchTree = new QTreeWidget();
    m_watchTree->setHeaderLabels(QStringList() << "Expression" << "Value");
    m_watchTree->setColumnWidth(0, 150);
    m_watchTree->setColumnWidth(1, 150);
    m_watchTree->setStyleSheet(m_variablesTree->styleSheet());
    panelsLayout->addWidget(m_watchTree, 1);
    
    // Call stack
    auto* stackLabel = new QLabel("📚 Call Stack");
    stackLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 4px; background-color: #2d2d2d;");
    panelsLayout->addWidget(stackLabel);
    
    m_callStackTree = new QTreeWidget();
    m_callStackTree->setHeaderLabels(QStringList() << "Function" << "Location");
    m_callStackTree->setColumnWidth(0, 150);
    m_callStackTree->setColumnWidth(1, 150);
    m_callStackTree->setStyleSheet(m_variablesTree->styleSheet());
    panelsLayout->addWidget(m_callStackTree, 1);
    
    // Breakpoints
    auto* bpLabel = new QLabel("🚩 Breakpoints");
    bpLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 4px; background-color: #2d2d2d;");
    panelsLayout->addWidget(bpLabel);
    
    m_breakpointsTree = new QTreeWidget();
    m_breakpointsTree->setHeaderLabels(QStringList() << "File" << "Line");
    m_breakpointsTree->setColumnWidth(0, 200);
    m_breakpointsTree->setColumnWidth(1, 50);
    m_breakpointsTree->setStyleSheet(m_variablesTree->styleSheet());
    panelsLayout->addWidget(m_breakpointsTree, 1);
    
    layout->addWidget(panelsWidget, 1);
}

void DebugPanel::setupConnections()
{
    // Debug toolbar connections would go here
}

void DebugPanel::clear()
{
    m_variablesTree->clear();
    m_watchTree->clear();
    m_callStackTree->clear();
    m_breakpointsTree->clear();
    m_currentLine = -1;
    m_currentFile.clear();
}

void DebugPanel::setVariables(const QList<DebugItem>& variables)
{
    m_variablesTree->clear();
    
    for (const auto& var : variables) {
        auto* item = new QTreeWidgetItem(m_variablesTree);
        item->setText(0, var.name);
        item->setText(1, var.value);
        item->setText(2, var.type);
        
        for (const auto& child : var.children) {
            auto* childItem = new QTreeWidgetItem(item);
            childItem->setText(0, child.name);
            childItem->setText(1, child.value);
            childItem->setText(2, child.type);
        }
    }
    
    m_variablesTree->expandAll();
}

void DebugPanel::addVariable(const DebugItem& variable)
{
    auto* item = new QTreeWidgetItem(m_variablesTree);
    item->setText(0, variable.name);
    item->setText(1, variable.value);
    item->setText(2, variable.type);
}

void DebugPanel::addWatch(const QString& expression)
{
    auto* item = new QTreeWidgetItem(m_watchTree);
    item->setText(0, expression);
    item->setText(1, "N/A");
}

void DebugPanel::removeWatch(int index)
{
    if (QTreeWidgetItem* item = m_watchTree->topLevelItem(index)) {
        delete item;
    }
}

void DebugPanel::updateWatchValue(int index, const QString& value)
{
    if (QTreeWidgetItem* item = m_watchTree->topLevelItem(index)) {
        item->setText(1, value);
    }
}

void DebugPanel::setCallStack(const QList<StackFrame>& frames)
{
    m_callStackTree->clear();
    m_callStack = frames;
    
    for (const auto& frame : frames) {
        auto* item = new QTreeWidgetItem(m_callStackTree);
        item->setText(0, frame.functionName);
        item->setText(1, QString("%1:%2").arg(frame.fileName).arg(frame.lineNumber));
        
        if (frame.frameIndex == 0) {
            item->setForeground(0, QColor(255, 200, 100));
            item->setForeground(1, QColor(255, 200, 100));
        }
    }
}

void DebugPanel::setCurrentFrame(int frameIndex)
{
    for (int i = 0; i < m_callStackTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_callStackTree->topLevelItem(i);
        if (i == frameIndex) {
            item->setForeground(0, QColor(255, 200, 100));
            item->setForeground(1, QColor(255, 200, 100));
        } else {
            item->setForeground(0, QColor(204, 204, 204));
            item->setForeground(1, QColor(204, 204, 204));
        }
    }
}

void DebugPanel::setBreakpoints(const QList<QPair<QString, int>>& breakpoints)
{
    m_breakpointsTree->clear();
    m_breakpoints = breakpoints;
    
    for (const auto& bp : breakpoints) {
        auto* item = new QTreeWidgetItem(m_breakpointsTree);
        item->setText(0, bp.first);
        item->setText(1, QString::number(bp.second));
    }
}

void DebugPanel::addBreakpoint(const QString& file, int line)
{
    auto* item = new QTreeWidgetItem(m_breakpointsTree);
    item->setText(0, file);
    item->setText(1, QString::number(line));
}

void DebugPanel::removeBreakpoint(const QString& file, int line)
{
    for (int i = 0; i < m_breakpointsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_breakpointsTree->topLevelItem(i);
        if (item->text(0) == file && item->text(1).toInt() == line) {
            delete item;
            break;
        }
    }
}

void DebugPanel::setDebuggingState(bool debugging)
{
    m_isDebugging = debugging;
    updateToolbar();
}

void DebugPanel::setCurrentLine(const QString& file, int line)
{
    m_currentFile = file;
    m_currentLine = line;
    Q_EMIT breakpointHit(file, line);
}

void DebugPanel::clearCurrentLine()
{
    m_currentLine = -1;
    m_currentFile.clear();
}

void DebugPanel::startDebugging(const QString& scriptPath)
{
    m_isDebugging = true;
    updateToolbar();
    Q_EMIT debuggingStarted();
}

void DebugPanel::stopDebugging()
{
    if (m_debugProcess) {
        m_debugProcess->kill();
        m_debugProcess->waitForFinished(1000);
    }
    
    m_isDebugging = false;
    clear();
    updateToolbar();
    Q_EMIT debuggingStopped();
}

void DebugPanel::stepOver()
{
    if (m_isDebugging) {
        // Would send step-over command to debugger
        Q_EMIT stepCompleted();
    }
}

void DebugPanel::stepInto()
{
    if (m_isDebugging) {
        // Would send step-into command to debugger
        Q_EMIT stepCompleted();
    }
}

void DebugPanel::stepOut()
{
    if (m_isDebugging) {
        // Would send step-out command to debugger
        Q_EMIT stepCompleted();
    }
}

void DebugPanel::continueExecution()
{
    if (m_isDebugging) {
        // Would send continue command to debugger
        Q_EMIT executionContinued();
    }
}

void DebugPanel::updateToolbar()
{
    // Update toolbar button states based on debugging state
}

} // namespace Flux
