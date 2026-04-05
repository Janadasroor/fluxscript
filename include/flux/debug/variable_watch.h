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

#ifndef FLUX_VARIABLE_WATCH_H
#define FLUX_VARIABLE_WATCH_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QTreeView>
#include <QStandardItemModel>

namespace Flux {

/**
 * @brief Variable information for debugging
 */
struct VariableInfo {
    QString name;
    QString type;
    QString value;
    QString displayValue;  // Formatted for display
    bool isExpanded;       // Can be expanded (e.g., struct, array)
    int childCount;        // Number of child variables
    bool isModified;       // Changed since last stop
    QString address;       // Memory address (optional)
    
    VariableInfo() : isExpanded(false), childCount(0), isModified(false) {}
};

/**
 * @brief Watch expression for debugging
 */
struct WatchExpression {
    int id;
    QString expression;
    QString value;
    QString type;
    bool isValid;
    QString error;  // If evaluation failed
    
    WatchExpression() : id(-1), isValid(true) {}
};

/**
 * @brief Variable watch widget for debugging sessions
 * 
 * Features:
 * - Local variables display
 * - Watch expressions
 * - Value editing
 * - Type visualization
 * - Change highlighting
 */
class VariableWatch : public QTreeView {
    Q_OBJECT
    
public:
    enum class WatchMode {
        Locals,      // Local variables in current scope
        Watch,       // User-defined watch expressions
        Globals,     // Global variables
        Registers,   // CPU registers (advanced)
        CallStack    // Call stack
    };
    
    explicit VariableWatch(QWidget* parent = nullptr);
    explicit VariableWatch(WatchMode mode, QWidget* parent = nullptr);
    ~VariableWatch() override;

    // ========================================================================
    // Mode Management
    // ========================================================================
    WatchMode mode() const { return m_mode; }
    void setMode(WatchMode mode);
    
    // ========================================================================
    // Variable Display
    // ========================================================================
    
    /**
     * @brief Set variables to display
     * @param variables Map of variable name to info
     */
    void setVariables(const QMap<QString, VariableInfo>& variables);
    
    /**
     * @brief Add or update a single variable
     * @param name Variable name
     * @param info Variable information
     */
    void setVariable(const QString& name, const VariableInfo& info);
    
    /**
     * @brief Remove a variable
     * @param name Variable name
     */
    void removeVariable(const QString& name);
    
    /**
     * @brief Clear all variables
     */
    void clearVariables();
    
    /**
     * @brief Get variable info by name
     * @param name Variable name
     * @return Variable info, or empty optional if not found
     */
    std::optional<VariableInfo> variable(const QString& name) const;
    
    /**
     * @brief Get all variables
     * @return Map of variable name to info
     */
    QMap<QString, VariableInfo> allVariables() const;

    // ========================================================================
    // Watch Expressions
    // ========================================================================
    
    /**
     * @brief Add a watch expression
     * @param expression Expression to watch
     * @return Watch ID
     */
    int addWatch(const QString& expression);
    
    /**
     * @brief Remove a watch expression
     * @param id Watch ID
     */
    void removeWatch(int id);
    
    /**
     * @brief Update watch expression value
     * @param id Watch ID
     * @param value New value
     */
    void updateWatchValue(int id, const QString& value);
    
    /**
     * @brief Get all watch expressions
     * @return List of watch expressions
     */
    QList<WatchExpression> watches() const;
    
    /**
     * @brief Clear all watch expressions
     */
    void clearWatches();

    // ========================================================================
    // Value Editing
    // ========================================================================
    
    /**
     * @brief Check if a variable's value can be edited
     * @param name Variable name
     * @return true if editable
     */
    bool canEditVariable(const QString& name) const;
    
    /**
     * @brief Set a variable's value
     * @param name Variable name
     * @param value New value
     * @return true if successful
     */
    bool setVariableValue(const QString& name, const QString& value);

    // ========================================================================
    // Display Options
    // ========================================================================
    
    void showTypes(bool show);
    void showAddresses(bool show);
    void showModifiedHighlight(bool show);
    void setMaxStringLength(int length);
    void setExpandedDepth(int depth);

signals:
    // ========================================================================
    // User Actions
    // ========================================================================
    void valueEdited(const QString& name, const QString& newValue);
    void expressionAdded(const QString& expression);
    void expressionRemoved(int id);
    void variableExpanded(const QString& name);
    void variableCollapsed(const QString& name);
    
    // ========================================================================
    // Context Menu
    // ========================================================================
    void contextMenuRequested(const QPoint& pos, const QString& name);

public slots:
    void refresh();
    void expandAll();
    void collapseAll();

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onValueChanged(const QModelIndex& index, const QVariant& value);
    void onItemExpanded(const QModelIndex& index);
    void onItemCollapsed(const QModelIndex& index);

private:
    // ========================================================================
    // Initialization
    // ========================================================================
    void setupModel();
    void setupHeader();
    void setupStyle();
    
    // ========================================================================
    // Model Population
    // ========================================================================
    void populateFromVariables();
    void populateFromWatches();
    QStandardItem* createVariableItem(const QString& name, const VariableInfo& info);
    QStandardItem* createWatchItem(const WatchExpression& watch);
    
    // ========================================================================
    // Helpers
    // ========================================================================
    QColor colorForType(const QString& type) const;
    QIcon iconForType(const QString& type) const;
    QString formatValue(const VariableInfo& info) const;
    
    // ========================================================================
    // Members
    // ========================================================================
    WatchMode m_mode;
    QStandardItemModel* m_model;
    
    // Data storage
    QMap<QString, VariableInfo> m_variables;
    QList<WatchExpression> m_watches;
    
    // Display options
    bool m_showTypes;
    bool m_showAddresses;
    bool m_showModifiedHighlight;
    int m_maxStringLength;
    
    // Modified tracking
    QMap<QString, QString> m_previousValues;
};

/**
 * @brief Call stack widget for debugging
 */
class CallStackWidget : public QTreeView {
    Q_OBJECT
    
public:
    struct StackFrame {
        int frameIndex;
        QString function;
        QString file;
        int line;
        int column;
        QString module;
        QString address;
        
        StackFrame() : frameIndex(-1), line(-1), column(-1) {}
    };
    
    explicit CallStackWidget(QWidget* parent = nullptr);
    
    void setStackFrames(const QList<StackFrame>& frames);
    void setCurrentFrame(int frameIndex);
    QList<StackFrame> stackFrames() const;
    
signals:
    void frameSelected(int frameIndex);

private:
    void setupModel();
    void updateDisplay();
    
    QList<StackFrame> m_frames;
    int m_currentFrame;
    QStandardItemModel* m_model;
};

} // namespace Flux

#endif // FLUX_VARIABLE_WATCH_H
