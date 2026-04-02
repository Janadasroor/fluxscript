#ifndef FLUX_SCRIPT_INTERPRETER_H
#define FLUX_SCRIPT_INTERPRETER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include <QTextStream>
#include <QRegularExpression>

namespace Flux {

/**
 * @brief Simple script interpreter for FluxScript demo
 * 
 * Parses and executes basic FluxScript syntax:
 * - print() statements
 * - Variable declarations (let, var)
 * - Basic math operations
 * - Functions
 * - Loops (for)
 * - Conditionals (if/else)
 */
class ScriptInterpreter : public QObject {
    Q_OBJECT

public:
    explicit ScriptInterpreter(QObject* parent = nullptr);
    ~ScriptInterpreter() override;

    // Execute script from file
    bool loadAndRun(const QString& filePath);
    
    // Execute script from string
    bool execute(const QString& code);
    
    // Stop execution
    void stop();
    
    // Check if running
    bool isRunning() const { return m_running; }

Q_SIGNALS:
    void output(const QString& text);
    void error(const QString& text);
    void warning(const QString& text);
    void info(const QString& text);
    void finished(int exitCode);
    void lineExecuted(int lineNumber);

private:
    // Parser functions
    void parseAndExecute(const QStringList& lines);
    bool executeLine(const QString& line, int lineNum);
    
    // Statement handlers
    bool handlePrint(const QString& line);
    bool handleLet(const QString& line);
    bool handleFor(const QString& line);
    bool handleIf(const QString& line);
    bool handleFunction(const QString& line);
    bool handleReturn(const QString& line);
    
    // Expression evaluation
    QVariant evaluateExpression(const QString& expr);
    QString evaluateString(const QString& expr);
    double evaluateNumber(const QString& expr);
    bool evaluateCondition(const QString& condition);
    
    // Helper functions
    QString substituteVariables(const QString& text);
    double computeMath(const QString& expr);
    QString computeStringConcat(const QString& expr);
    
    // Built-in functions
    QVariant callBuiltin(const QString& funcName, const QStringList& args);
    
    // State
    QMap<QString, QVariant> m_variables;
    QMap<QString, QString> m_functions;
    QStringList m_executingLines;
    int m_currentLine;
    bool m_running;
    bool m_stopped;
    
    // Control flow
    int m_loopEnd;
    int m_loopStep;
    QString m_loopVar;
    bool m_inLoop;
    bool m_inIf;
    bool m_ifCondition;
};

} // namespace Flux

#endif // FLUX_SCRIPT_INTERPRETER_H
