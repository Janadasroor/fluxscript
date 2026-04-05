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

#ifndef FLUX_BREAKPOINT_MANAGER_H
#define FLUX_BREAKPOINT_MANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QList>

namespace Flux {

/**
 * @brief Manages breakpoints for debugging sessions
 * 
 * Features:
 * - Add/remove/enable/disable breakpoints
 * - Conditional breakpoints
 * - Hit count tracking
 * - File-based organization
 * - Persistence
 */
class BreakpointManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Breakpoint information structure
     */
    struct Breakpoint {
        int id;
        int line;
        QString filePath;
        bool enabled;
        QString condition;      // Optional condition expression
        int hitCount;
        bool temporary;         // Auto-remove after hit
        QString logMessage;     // Optional log message (logpoint)
        
        Breakpoint() : id(-1), line(-1), enabled(true), hitCount(0), temporary(false) {}
        
        bool hasCondition() const { return !condition.isEmpty(); }
        bool isLogpoint() const { return !logMessage.isEmpty(); }
    };
    
    explicit BreakpointManager(QObject* parent = nullptr);
    ~BreakpointManager() override;

    // ========================================================================
    // Breakpoint Operations
    // ========================================================================
    
    /**
     * @brief Add a breakpoint at the specified line
     * @param filePath Source file path
     * @param line Line number (1-based)
     * @return Breakpoint ID, or -1 if failed
     */
    int addBreakpoint(const QString& filePath, int line);
    
    /**
     * @brief Remove a breakpoint by ID
     * @param id Breakpoint ID
     */
    void removeBreakpoint(int id);
    
    /**
     * @brief Remove all breakpoints for a file
     * @param filePath Source file path
     */
    void removeBreakpointsForFile(const QString& filePath);
    
    /**
     * @brief Enable or disable a breakpoint
     * @param id Breakpoint ID
     * @param enabled New enabled state
     */
    void setBreakpointEnabled(int id, bool enabled);
    
    /**
     * @brief Set a condition for a breakpoint
     * @param id Breakpoint ID
     * @param condition Condition expression (empty to clear)
     */
    void setBreakpointCondition(int id, const QString& condition);
    
    /**
     * @brief Set a log message for a breakpoint (logpoint)
     * @param id Breakpoint ID
     * @param message Log message (empty to clear)
     */
    void setBreakpointLogMessage(int id, const QString& message);
    
    /**
     * @brief Mark a breakpoint as temporary (auto-remove after hit)
     * @param id Breakpoint ID
     * @param temporary Temporary flag
     */
    void setBreakpointTemporary(int id, bool temporary);
    
    /**
     * @brief Increment hit count for a breakpoint
     * @param id Breakpoint ID
     */
    void incrementHitCount(int id);
    
    /**
     * @brief Reset hit count for a breakpoint
     * @param id Breakpoint ID
     */
    void resetHitCount(int id);

    // ========================================================================
    // Query Operations
    // ========================================================================
    
    /**
     * @brief Get all breakpoints for a file
     * @param filePath Source file path
     * @return List of breakpoints
     */
    QList<Breakpoint> breakpointsForFile(const QString& filePath) const;
    
    /**
     * @brief Get all enabled breakpoints for a file
     * @param filePath Source file path
     * @return List of enabled breakpoints
     */
    QList<Breakpoint> enabledBreakpointsForFile(const QString& filePath) const;
    
    /**
     * @brief Get all breakpoints across all files
     * @return List of all breakpoints
     */
    QList<Breakpoint> allBreakpoints() const;
    
    /**
     * @brief Get all enabled breakpoints
     * @return List of enabled breakpoints
     */
    QList<Breakpoint> allEnabledBreakpoints() const;
    
    /**
     * @brief Get breakpoint at a specific location
     * @param line Line number
     * @param filePath Source file path
     * @return Pointer to breakpoint, or nullptr if not found
     */
    Breakpoint* breakpointAt(int line, const QString& filePath) const;
    
    /**
     * @brief Get breakpoint by ID
     * @param id Breakpoint ID
     * @return Pointer to breakpoint, or nullptr if not found
     */
    Breakpoint* breakpointById(int id) const;
    
    /**
     * @brief Check if there's a breakpoint at a location
     * @param line Line number
     * @param filePath Source file path
     * @return true if breakpoint exists
     */
    bool hasBreakpointAt(int line, const QString& filePath) const;
    
    /**
     * @brief Get total breakpoint count
     * @return Number of breakpoints
     */
    int breakpointCount() const;
    
    /**
     * @brief Clear all breakpoints
     */
    void clearAllBreakpoints();

    // ========================================================================
    // Persistence
    // ========================================================================
    
    /**
     * @brief Save breakpoints to file
     * @param filePath Path to save to
     * @return true if successful
     */
    bool saveToFile(const QString& filePath);
    
    /**
     * @brief Load breakpoints from file
     * @param filePath Path to load from
     * @return true if successful
     */
    bool loadFromFile(const QString& filePath);

signals:
    // ========================================================================
    // Breakpoint Lifecycle
    // ========================================================================
    void breakpointAdded(int id);
    void breakpointRemoved(int id);
    void breakpointEnabledChanged(int id, bool enabled);
    void breakpointConditionChanged(int id, const QString& condition);
    void breakpointHit(int id);
    
    // ========================================================================
    // File-Based Notifications
    // ========================================================================
    void breakpointsChangedForFile(const QString& filePath);
    void allBreakpointsCleared();

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    int generateId() const;
    void validateBreakpoint(Breakpoint& bp);
    
    // ========================================================================
    // Members
    // ========================================================================
    QMap<int, Breakpoint> m_breakpoints;  // id -> breakpoint
    QMap<QString, QList<int>> m_fileToIds;  // file path -> breakpoint ids
    int m_nextId;
};

} // namespace Flux

#endif // FLUX_BREAKPOINT_MANAGER_H
