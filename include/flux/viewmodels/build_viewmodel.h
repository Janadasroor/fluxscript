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

#ifndef FLUX_VIEWMODELS_BUILD_VIEWMODEL_H
#define FLUX_VIEWMODELS_BUILD_VIEWMODEL_H

#include "flux/mvvm/viewmodel_base.h"
#include "flux/mvvm/observable_property.h"
#include "flux/mvvm/relay_command.h"

namespace Flux::MVVM {

/**
 * @brief Build state
 */
enum class BuildState {
    Idle,
    Building,
    Success,
    Failed,
    Running,
    Stopped
};

/**
 * @brief Build output severity
 */
enum class OutputSeverity {
    Info,
    Warning,
    Error,
    Success
};

/**
 * @brief Build output line
 */
struct BuildOutputLine {
    OutputSeverity severity;
    QString text;
    QString file;
    int line;
    int column;
    qint64 timestamp;
    
    BuildOutputLine() : severity(OutputSeverity::Info), line(-1), column(-1), timestamp(0) {}
};

/**
 * @brief Build configuration
 */
struct BuildConfig {
    QString buildType = "Debug";  // Debug, Release
    int optimizationLevel = 2;    // 0-3
    bool debugInfo = true;
    bool warningsAsErrors = false;
    QStringList additionalArgs;
    QString workingDirectory;
    QString outputDirectory;
};

/**
 * @brief ViewModel for build and run functionality
 */
class BuildViewModel : public ViewModelBase {
    Q_OBJECT
    
public:
    explicit BuildViewModel(QObject* parent = nullptr);
    ~BuildViewModel() override;

    // ========================================================================
    // Build State
    // ========================================================================
    
    /**
     * @brief Current build state
     */
    ObservableProperty<BuildState> CurrentState;
    ObservableProperty<bool> IsBuilding;
    ObservableProperty<bool> IsRunning;
    ObservableProperty<bool> CanBuild;
    ObservableProperty<bool> CanRun;
    
    /**
     * @brief Build progress
     */
    ObservableProperty<int> ProgressValue;
    ObservableProperty<int> ProgressMaximum;
    ObservableProperty<QString> ProgressText;
    ObservableProperty<bool> IsProgressIndeterminate;
    
    /**
     * @brief Build timing
     */
    ObservableProperty<qint64> BuildDuration;
    ObservableProperty<QString> BuildDurationText;
    
    /**
     * @brief Last build result
     */
    ObservableProperty<int> LastExitCode;
    ObservableProperty<bool> LastBuildSucceeded;
    ObservableProperty<int> ErrorCount;
    ObservableProperty<int> WarningCount;
    
    // ========================================================================
    // Build Output
    // ========================================================================
    
    /**
     * @brief Build output lines
     */
    ObservableProperty<QList<BuildOutputLine>> BuildOutput;
    ObservableProperty<QString> BuildOutputText;  // Concatenated output
    ObservableProperty<int> OutputLineCount;
    
    // ========================================================================
    // Build Configuration
    // ========================================================================
    
    ObservableProperty<BuildConfig> BuildConfiguration;
    ObservableProperty<QString> CurrentBuildType;
    ObservableProperty<int> OptimizationLevel;
    ObservableProperty<bool> DebugInfoEnabled;
    
    // ========================================================================
    // Run Configuration
    // ========================================================================
    
    ObservableProperty<QString> RunArguments;
    ObservableProperty<QString> WorkingDirectory;
    ObservableProperty<bool> RunInTerminal;
    
    // ========================================================================
    // Commands
    // ========================================================================
    
    std::shared_ptr<RelayCommand> BuildCommand;
    std::shared_ptr<RelayCommand> RebuildCommand;
    std::shared_ptr<RelayCommand> CleanCommand;
    std::shared_ptr<RelayCommand> RunCommand;
    std::shared_ptr<RelayCommand> BuildAndRunCommand;
    std::shared_ptr<RelayCommand> StopBuildCommand;
    std::shared_ptr<RelayCommand> StopRunCommand;
    
    std::shared_ptr<RelayCommand> SetBuildTypeCommand;
    std::shared_ptr<RelayCommand> SetOptimizationLevelCommand;
    
    std::shared_ptr<RelayCommand> ClearOutputCommand;
    std::shared_ptr<RelayCommand> CopyOutputCommand;
    std::shared_ptr<RelayCommand> SaveOutputCommand;

    // ========================================================================
    // Build Operations
    // ========================================================================
    
    /**
     * @brief Build the current document
     * @param filePath File to build (empty for current)
     * @return true if build started
     */
    Q_INVOKABLE bool build(const QString& filePath = QString());
    
    /**
     * @brief Rebuild (clean + build)
     * @param filePath File to rebuild
     * @return true if rebuild started
     */
    Q_INVOKABLE bool rebuild(const QString& filePath = QString());
    
    /**
     * @brief Clean build artifacts
     * @param filePath File to clean
     * @return true if clean started
     */
    Q_INVOKABLE bool clean(const QString& filePath = QString());
    
    /**
     * @brief Run the current document
     * @param filePath File to run
     * @param arguments Command line arguments
     * @return true if run started
     */
    Q_INVOKABLE bool run(const QString& filePath = QString(), const QString& arguments = QString());
    
    /**
     * @brief Build and run
     * @param filePath File to build and run
     * @return true if successful
     */
    Q_INVOKABLE bool buildAndRun(const QString& filePath = QString());
    
    /**
     * @brief Stop the current build
     */
    Q_INVOKABLE void stopBuild();
    
    /**
     * @brief Stop the running process
     */
    Q_INVOKABLE void stopRun();

    // ========================================================================
    // Output Management
    // ========================================================================
    
    /**
     * @brief Append output line
     * @param text Output text
     * @param severity Output severity
     */
    Q_INVOKABLE void appendOutput(const QString& text, OutputSeverity severity = OutputSeverity::Info);
    
    /**
     * @brief Append error line
     * @param text Error text
     * @param file Source file
     * @param line Line number
     * @param column Column number
     */
    Q_INVOKABLE void appendError(const QString& text, const QString& file = QString(), 
                                  int line = -1, int column = -1);
    
    /**
     * @brief Append warning line
     * @param text Warning text
     */
    Q_INVOKABLE void appendWarning(const QString& text);
    
    /**
     * @brief Clear build output
     */
    Q_INVOKABLE void clearOutput();
    
    /**
     * @brief Get output as text
     * @return Full output text
     */
    Q_INVOKABLE QString getOutputText() const;
    
    /**
     * @brief Copy output to clipboard
     */
    Q_INVOKABLE void copyOutputToClipboard();
    
    /**
     * @brief Save output to file
     * @param filePath Output file path
     * @return true if successful
     */
    Q_INVOKABLE bool saveOutputToFile(const QString& filePath);

    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set build type
     * @param buildType Debug or Release
     */
    Q_INVOKABLE void setBuildType(const QString& buildType);
    
    /**
     * @brief Set optimization level
     * @param level Optimization level (0-3)
     */
    Q_INVOKABLE void setOptimizationLevel(int level);
    
    /**
     * @brief Enable/disable debug info
     * @param enabled Enable state
     */
    Q_INVOKABLE void setDebugInfoEnabled(bool enabled);
    
    /**
     * @brief Set run arguments
     * @param arguments Command line arguments
     */
    Q_INVOKABLE void setRunArguments(const QString& arguments);
    
    /**
     * @brief Set working directory
     * @param directory Working directory path
     */
    Q_INVOKABLE void setWorkingDirectory(const QString& directory);

    // ========================================================================
    // Error Navigation
    // ========================================================================
    
    /**
     * @brief Go to next error in output
     * @return true if navigated
     */
    Q_INVOKABLE bool goToNextError();
    
    /**
     * @brief Go to previous error in output
     * @return true if navigated
     */
    Q_INVOKABLE bool goToPreviousError();
    
    /**
     * @brief Get error at output line
     * @param outputLine Output line index
     * @return Error location info
     */
    Q_INVOKABLE std::optional<BuildOutputLine> getErrorAt(int outputLine) const;

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    void onLoad() override;
    void onUnload() override;
    bool validate() override;

signals:
    // ========================================================================
    // Build Events
    // ========================================================================
    
    void buildStarted(const QString& filePath);
    void buildFinished(bool success, qint64 duration);
    void buildStopped();
    
    void runStarted(const QString& filePath);
    void runFinished(int exitCode);
    void runStopped();
    
    void outputAppended(const BuildOutputLine& line);
    void outputCleared();
    
    void stateChanged(BuildState newState);
    void progressChanged(int value, int maximum);
    void errorCountChanged(int count);
    void warningCountChanged(int count);

protected:
    // ========================================================================
    // Protected Helpers
    // ========================================================================
    
    void setupCommands();
    void setupBindings();
    void updateCommandStates();
    void updateStateProperties();
    
    void parseOutputLine(const QString& line);
    void updateErrorWarningCounts();
    QString formatDuration(qint64 milliseconds) const;
    
    bool canBuild() const;
    bool canRun() const;
    bool canStop() const;

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    BuildState m_currentState = BuildState::Idle;
    qint64 m_buildStartTime = 0;
    int m_currentErrorIndex = -1;
    
    // Process tracking
    int m_processId = -1;
    QString m_currentFilePath;
};

} // namespace Flux::MVVM

#endif // FLUX_VIEWMODELS_BUILD_VIEWMODEL_H
