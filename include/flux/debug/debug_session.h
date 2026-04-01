#ifndef FLUX_DEBUG_SESSION_H
#define FLUX_DEBUG_SESSION_H

#include <QObject>
#include <QProcess>
#include <QPointer>
#include <memory>

namespace Flux {

class BreakpointManager;
class VariableWatch;
class FluxDocument;

/**
 * @brief Debug session state
 */
enum class DebugState {
    Inactive,       // No debug session
    Starting,       // Launching debugger
    Running,        // Program running
    Stopped,        // Stopped at breakpoint
    Stepping,       // Single stepping
    Terminating,    // Shutting down
    Terminated      // Session ended
};

/**
 * @brief Step types for debugging
 */
enum class StepType {
    StepOver,   // Step over function calls
    StepInto,   // Step into function calls
    StepOut     // Step out of current function
};

/**
 * @brief Manages a debugging session for FluxScript
 * 
 * Coordinates:
 * - Breakpoint management
 * - Variable inspection
 * - Execution control (step, continue, stop)
 * - Debug state tracking
 */
class DebugSession : public QObject {
    Q_OBJECT
    
public:
    explicit DebugSession(QObject* parent = nullptr);
    ~DebugSession() override;

    // ========================================================================
    // Session Control
    // ========================================================================
    
    /**
     * @brief Start a debug session for a document
     * @param document Document to debug
     * @return true if session started successfully
     */
    bool start(FluxDocument* document);
    
    /**
     * @brief Stop the debug session
     */
    void stop();
    
    /**
     * @brief Check if a session is active
     * @return true if session is running or stopped
     */
    bool isActive() const;
    
    /**
     * @brief Get current debug state
     * @return Current state
     */
    DebugState state() const { return m_state; }

    // ========================================================================
    // Execution Control
    // ========================================================================
    
    /**
     * @brief Continue execution until next breakpoint
     */
    void continueExecution();
    
    /**
     * @brief Pause execution
     */
    void pause();
    
    /**
     * @brief Execute a single step
     * @param type Type of step operation
     */
    void step(StepType type);
    
    /**
     * @brief Step over (F10)
     */
    void stepOver();
    
    /**
     * @brief Step into (F11)
     */
    void stepInto();
    
    /**
     * @brief Step out (Shift+F11)
     */
    void stepOut();
    
    /**
     * @brief Run to cursor
     * @param line Line number to run to
     */
    void runToLine(int line);
    
    /**
     * @brief Set next statement (jump to line)
     * @param line Line number
     * @return true if successful
     */
    bool setNextStatement(int line);

    // ========================================================================
    // Breakpoint Management
    // ========================================================================
    
    BreakpointManager* breakpointManager() const { return m_breakpointManager; }
    
    /**
     * @brief Check if execution should stop at a line
     * @param filePath File path
     * @param line Line number
     * @return true if breakpoint is hit
     */
    bool checkBreakpoint(const QString& filePath, int line);

    // ========================================================================
    // Variable Access
    // ========================================================================
    
    VariableWatch* variableWatch() const { return m_variableWatch; }
    
    /**
     * @brief Get value of a variable
     * @param name Variable name
     * @return Variable value, or empty if not found
     */
    QString variableValue(const QString& name) const;
    
    /**
     * @brief Set value of a variable
     * @param name Variable name
     * @param value New value
     * @return true if successful
     */
    bool setVariableValue(const QString& name, const QString& value);
    
    /**
     * @brief Evaluate an expression
     * @param expression Expression to evaluate
     * @return Evaluation result
     */
    QString evaluateExpression(const QString& expression);
    
    /**
     * @brief Refresh variable display
     */
    void refreshVariables();

    // ========================================================================
    // Stack Access
    // ========================================================================
    
    /**
     * @brief Get current file
     * @return Current file path
     */
    QString currentFile() const;
    
    /**
     * @brief Get current line
     * @return Current line number (1-based)
     */
    int currentLine() const;
    
    /**
     * @brief Get current function
     * @return Current function name
     */
    QString currentFunction() const;

    // ========================================================================
    // Configuration
    // ========================================================================
    
    void setBreakOnExceptions(bool breakOnExceptions);
    bool breakOnExceptions() const { return m_breakOnExceptions; }
    
    void setStepSkipSystemCode(bool skip);
    bool stepSkipSystemCode() const { return m_stepSkipSystemCode; }

signals:
    // ========================================================================
    // Session Lifecycle
    // ========================================================================
    void sessionStarted();
    void sessionEnded();
    void stateChanged(DebugState newState);
    
    // ========================================================================
    // Execution Events
    // ========================================================================
    void processStarted();
    void processExited(int exitCode);
    void breakpointHit(const QString& filePath, int line);
    void exceptionThrown(const QString& message);
    void pauseRequested();
    
    // ========================================================================
    // Location Updates
    // ========================================================================
    void currentLocationChanged(const QString& file, int line, const QString& function);
    void stackFrameChanged(int frameIndex);
    
    // ========================================================================
    // Variable Updates
    // ========================================================================
    void variablesRefreshed();
    void variableChanged(const QString& name, const QString& newValue);

public slots:
    void onDebuggerOutput();
    void onDebuggerError();
    void onDebuggerFinished(int exitCode, QProcess::ExitStatus status);

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    void setState(DebugState state);
    void setupDebuggerProcess();
    void parseDebuggerOutput(const QString& output);
    void sendDebuggerCommand(const QString& command);
    void updateCurrentLocation(const QString& file, int line);
    
    // ========================================================================
    // Event Handlers
    // ========================================================================
    void handleBreakpointHit(const QString& filePath, int line);
    void handleStepComplete();
    void handleProcessExit(int exitCode);
    
    // ========================================================================
    // Members
    // ========================================================================
    DebugState m_state;
    QPointer<FluxDocument> m_document;
    
    // Debugger process
    std::unique_ptr<QProcess> m_debuggerProcess;
    QString m_debuggerOutputBuffer;
    
    // Debug components
    BreakpointManager* m_breakpointManager;
    VariableWatch* m_variableWatch;
    
    // Current location
    QString m_currentFile;
    int m_currentLine;
    QString m_currentFunction;
    
    // Configuration
    bool m_breakOnExceptions;
    bool m_stepSkipSystemCode;
    
    // Pending operations
    bool m_pendingContinue;
    StepType m_pendingStep;
};

} // namespace Flux

#endif // FLUX_DEBUG_SESSION_H
