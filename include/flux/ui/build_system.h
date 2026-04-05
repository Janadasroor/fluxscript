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

#ifndef FLUX_BUILD_SYSTEM_H
#define FLUX_BUILD_SYSTEM_H

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QMap>
#include <QPointer>
#include <memory>

namespace Flux {

class FluxDocument;
class DiagnosticParser;

/**
 * @brief Build output with diagnostic information
 */
struct BuildOutput {
    QString output;
    QString errors;
    QString warnings;
    int exitCode;
    qint64 duration;  // milliseconds
    bool success;
};

/**
 * @brief Integrates with the FluxScript JIT compiler for build/run operations
 * 
 * Responsibilities:
 * - Compile FluxScript documents
 * - Run compiled code
 * - Parse and report diagnostics
 * - Manage build state
 */
class BuildSystem : public QObject {
    Q_OBJECT
    
public:
    enum class BuildState {
        Idle,
        Building,
        Running,
        Success,
        Error,
        Stopped
    };
    
    explicit BuildSystem(QObject* parent = nullptr);
    ~BuildSystem() override;

    // ========================================================================
    // Build Operations
    // ========================================================================
    void build(FluxDocument* document);
    void buildAndRun(FluxDocument* document);
    void run(FluxDocument* document);
    void stop();
    
    // ========================================================================
    // State Access
    // ========================================================================
    BuildState currentState() const { return m_state; }
    bool isBuilding() const { return m_state == BuildState::Building; }
    bool isRunning() const { return m_state == BuildState::Running; }
    
    BuildOutput lastBuildOutput() const { return m_lastOutput; }
    QList<Diagnostic> lastDiagnostics() const;
    
    // ========================================================================
    // Diagnostic Parser
    // ========================================================================
    DiagnosticParser* diagnosticParser() const { return m_diagnosticParser; }
    
    // ========================================================================
    // Configuration
    // ========================================================================
    void setOptimizationLevel(int level);  // 0-3
    int optimizationLevel() const { return m_optimizationLevel; }
    
    void enableDebugInfo(bool enable);
    bool debugInfoEnabled() const { return m_debugInfoEnabled; }

signals:
    // ========================================================================
    // Build Lifecycle
    // ========================================================================
    void buildStarted(FluxDocument* document);
    void buildFinished(bool success);
    void runStarted(FluxDocument* document);
    void runFinished(int exitCode);
    void stopped();
    
    // ========================================================================
    // Output
    // ========================================================================
    void outputReceived(const QString& output);
    void errorReceived(const QString& error);
    void warningReceived(const QString& warning);
    
    // ========================================================================
    // State Changes
    // ========================================================================
    void stateChanged(BuildState newState);
    void diagnosticsUpdated(const QList<Diagnostic>& diagnostics);

public slots:
    void onDocumentModified();
    void clearOutput();

private slots:
    void onBuildOutput();
    void onBuildError();
    void onBuildFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRunOutput();
    void onRunError();
    void onRunFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    void setState(BuildState state);
    void compileDocument(FluxDocument* document);
    void executeCompiled(FluxDocument* document);
    QString buildArguments(FluxDocument* document) const;
    
    // ========================================================================
    // Members
    // ========================================================================
    BuildState m_state;
    BuildOutput m_lastOutput;
    
    QPointer<FluxDocument> m_currentDocument;
    
    // Build process
    std::unique_ptr<QProcess> m_buildProcess;
    std::unique_ptr<QProcess> m_runProcess;
    
    // Diagnostics
    DiagnosticParser* m_diagnosticParser;
    QList<Diagnostic> m_currentDiagnostics;
    
    // Configuration
    int m_optimizationLevel;
    bool m_debugInfoEnabled;
    
    // Timing
    QElapsedTimer m_buildTimer;
};

/**
 * @brief Diagnostic information from compilation
 */
struct Diagnostic {
    enum class Severity {
        Hint,
        Warning,
        Error,
        Fatal
    };
    
    Severity severity;
    QString filePath;
    int line;
    int column;
    int length;
    QString message;
    QString ruleId;      // Optional: for linting rules
    QString suggestion;  // Optional: auto-fix suggestion
};

} // namespace Flux

#endif // FLUX_BUILD_SYSTEM_H
