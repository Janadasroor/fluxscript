#include "flux/runtime/script_interpreter.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStack>
#include <QtMath>

namespace Flux {

ScriptInterpreter::ScriptInterpreter(QObject* parent)
    : QObject(parent)
    , m_currentLine(0)
    , m_running(false)
    , m_stopped(false)
    , m_loopEnd(0)
    , m_loopStep(1)
    , m_inLoop(false)
    , m_inIf(false)
    , m_ifCondition(false)
{
}

ScriptInterpreter::~ScriptInterpreter()
{
    stop();
}

bool ScriptInterpreter::loadAndRun(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q_EMIT error(QString("Cannot open file: %1").arg(filePath));
        return false;
    }
    
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();
    
    return execute(code);
}

bool ScriptInterpreter::execute(const QString& code)
{
    m_running = true;
    m_stopped = false;
    m_variables.clear();
    m_executingLines = code.split('\n');
    
    Q_EMIT info("FluxScript Interpreter v1.0.0");
    Q_EMIT info("Starting execution...");
    Q_EMIT info("");
    
    parseAndExecute(m_executingLines);
    
    m_running = false;
    Q_EMIT info("");
    Q_EMIT info("Execution completed.");
    Q_EMIT finished(0);
    
    return true;
}

void ScriptInterpreter::stop()
{
    m_stopped = true;
    m_running = false;
}

void ScriptInterpreter::parseAndExecute(const QStringList& lines)
{
    QStack<int> ifStack;
    
    for (int i = 0; i < lines.size() && !m_stopped; ++i) {
        m_currentLine = i;
        QString line = lines[i].trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith("//")) {
            continue;
        }
        
        // Handle end of if block (simple heuristic)
        if (line == "}" && m_inIf) {
            m_inIf = false;
            m_ifCondition = false;
            continue;
        }
        
        // Skip lines inside false if block
        if (m_inIf && !m_ifCondition) {
            if (line.startsWith("else")) {
                m_ifCondition = true;
            }
            continue;
        }
        
        Q_EMIT lineExecuted(i + 1);
        
        // Execute the line
        if (!executeLine(line, i)) {
            Q_EMIT error(QString("Error at line %1: %2").arg(i + 1).arg(line));
        }
    }
}

bool ScriptInterpreter::executeLine(const QString& line, int lineNum)
{
    Q_UNUSED(lineNum);
    
    // Print statement
    if (line.startsWith("print(")) {
        return handlePrint(line);
    }
    
    // Variable declaration
    if (line.startsWith("let ") || line.startsWith("var ")) {
        return handleLet(line);
    }
    
    // For loop
    if (line.startsWith("for (")) {
        return handleFor(line);
    }
    
    // If statement
    if (line.startsWith("if (")) {
        return handleIf(line);
    }
    
    // Function definition
    if (line.startsWith("function ")) {
        return handleFunction(line);
    }
    
    // Return statement
    if (line.startsWith("return ")) {
        return handleReturn(line);
    }
    
    // Assignment (without let/var)
    if (line.contains("=") && !line.contains("==")) {
        return handleLet(line);
    }
    
    // Unknown statement - try to evaluate as expression
    evaluateExpression(line);
    
    return true;
}

bool ScriptInterpreter::handlePrint(const QString& line)
{
    // Extract content between print( and )
    QRegularExpression re(R"(print\s*\((.*)\)\s*;?)");
    QRegularExpressionMatch match = re.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    QString content = match.captured(1);
    QString result = evaluateString(content);
    Q_EMIT output(result + "\n");
    
    return true;
}

bool ScriptInterpreter::handleLet(const QString& line)
{
    // Match: let x = value; or x = value;
    QRegularExpression re(R"((?:let|var)?\s*(\w+)\s*=\s*(.+)\s*;?)");
    QRegularExpressionMatch match = re.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    QString varName = match.captured(1);
    QString valueExpr = match.captured(2);
    
    QVariant value = evaluateExpression(valueExpr);
    m_variables[varName] = value;
    
    return true;
}

bool ScriptInterpreter::handleFor(const QString& line)
{
    // Match: for (let i = 0; i < 10; i++)
    QRegularExpression re(R"(for\s*\(\s*(?:let|var)?\s*(\w+)\s*=\s*(\d+)\s*;\s*(\w+)\s*<\s*(\d+)\s*;\s*\w+\+\+\s*\))");
    QRegularExpressionMatch match = re.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    QString varName = match.captured(1);
    int start = match.captured(2).toInt();
    int end = match.captured(4).toInt();
    
    // Simple for loop execution
    for (int i = start; i < end && !m_stopped; ++i) {
        m_variables[varName] = i;
    }
    
    return true;
}

bool ScriptInterpreter::handleIf(const QString& line)
{
    // Match: if (condition)
    QRegularExpression re(R"(if\s*\((.+)\)\s*\{?)");
    QRegularExpressionMatch match = re.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    QString condition = match.captured(1);
    m_ifCondition = evaluateCondition(condition);
    m_inIf = true;
    
    return true;
}

bool ScriptInterpreter::handleFunction(const QString& line)
{
    // Store function for later (simplified)
    Q_EMIT info(QString("Function defined: %1").arg(line));
    return true;
}

bool ScriptInterpreter::handleReturn(const QString& line)
{
    Q_EMIT warning("Return statement outside function");
    return true;
}

QVariant ScriptInterpreter::evaluateExpression(const QString& expr)
{
    QString trimmed = expr.trimmed();
    
    // String literal
    if (trimmed.startsWith("\"") && trimmed.endsWith("\"")) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    
    // Boolean
    if (trimmed == "true") return true;
    if (trimmed == "false") return false;
    
    // Variable reference
    if (m_variables.contains(trimmed)) {
        return m_variables[trimmed];
    }
    
    // Try to evaluate as number expression
    bool ok = false;
    double num = computeMath(trimmed);
    if (qAbs(num) > 0.001 || trimmed.contains(QRegularExpression("\\d"))) {
        return num;
    }
    
    // String concatenation
    if (trimmed.contains("+")) {
        return computeStringConcat(trimmed);
    }
    
    return trimmed;
}

QString ScriptInterpreter::evaluateString(const QString& expr)
{
    QString result = expr;
    
    // Handle string concatenation
    if (expr.contains("+")) {
        result = computeStringConcat(expr);
    } else if (expr.startsWith("\"") && expr.endsWith("\"")) {
        result = expr.mid(1, expr.length() - 2);
    } else {
        // Variable substitution
        result = substituteVariables(expr);
    }
    
    return result;
}

double ScriptInterpreter::evaluateNumber(const QString& expr)
{
    bool ok = false;
    double result = computeMath(expr);
    return result;
}

bool ScriptInterpreter::evaluateCondition(const QString& condition)
{
    QString cond = condition.trimmed();
    
    // Greater than
    if (cond.contains(">")) {
        QStringList parts = cond.split(">");
        if (parts.size() == 2) {
            double left = evaluateNumber(parts[0].trimmed());
            double right = evaluateNumber(parts[1].trimmed());
            return left > right;
        }
    }
    
    // Less than
    if (cond.contains("<")) {
        QStringList parts = cond.split("<");
        if (parts.size() == 2) {
            double left = evaluateNumber(parts[0].trimmed());
            double right = evaluateNumber(parts[1].trimmed());
            return left < right;
        }
    }
    
    // Equal
    if (cond.contains("==")) {
        QStringList parts = cond.split("==");
        if (parts.size() == 2) {
            QVariant left = evaluateExpression(parts[0].trimmed());
            QVariant right = evaluateExpression(parts[1].trimmed());
            return left == right;
        }
    }
    
    // Not equal
    if (cond.contains("!=")) {
        QStringList parts = cond.split("!=");
        if (parts.size() == 2) {
            QVariant left = evaluateExpression(parts[0].trimmed());
            QVariant right = evaluateExpression(parts[1].trimmed());
            return left != right;
        }
    }
    
    return false;
}

QString ScriptInterpreter::substituteVariables(const QString& text)
{
    QString result = text;
    
    for (auto it = m_variables.begin(); it != m_variables.end(); ++it) {
        result = result.replace(it.key(), it.value().toString());
    }
    
    return result;
}

double ScriptInterpreter::computeMath(const QString& expr)
{
    QString e = expr.trimmed();
    
    // Check for variable
    if (m_variables.contains(e)) {
        bool ok;
        double val = m_variables[e].toDouble(&ok);
        if (ok) return val;
        return 0;
    }
    
    // Try direct number parse
    bool ok = false;
    double result = e.toDouble(&ok);
    if (ok) return result;
    
    // Simple math parsing (left to right, no precedence for demo)
    QStringList operators;
    QStringList tokens;
    QString current;
    
    for (int i = 0; i < e.length(); ++i) {
        QChar c = e[i];
        if (c.isDigit() || c == '.' || c == '-' || c == ' ') {
            current += c;
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (!current.trimmed().isEmpty()) {
                tokens << current.trimmed();
            }
            operators << c;
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty()) {
        tokens << current.trimmed();
    }
    
    if (tokens.isEmpty()) return 0;
    
    // Evaluate first token
    result = 0;
    bool firstOk = false;
    result = tokens[0].toDouble(&firstOk);
    if (!firstOk && m_variables.contains(tokens[0])) {
        result = m_variables[tokens[0]].toDouble();
    }
    
    // Apply operations
    for (int i = 0; i < operators.size() && i + 1 < tokens.size(); ++i) {
        double nextVal = tokens[i + 1].toDouble();
        if (!ok && m_variables.contains(tokens[i + 1])) {
            nextVal = m_variables[tokens[i + 1]].toDouble();
        }
        
        QString op = operators[i];
        if (op == "+") result += nextVal;
        else if (op == "-") result -= nextVal;
        else if (op == "*") result *= nextVal;
        else if (op == "/") result /= (nextVal != 0 ? nextVal : 1);
    }
    
    return result;
}

QString ScriptInterpreter::computeStringConcat(const QString& expr)
{
    QStringList parts = expr.split('+');
    QString result;
    
    for (const QString& part : parts) {
        QString p = part.trimmed();
        
        // String literal
        if (p.startsWith("\"") && p.endsWith("\"")) {
            result += p.mid(1, p.length() - 2);
        }
        // Variable
        else if (m_variables.contains(p)) {
            result += m_variables[p].toString();
        }
        // Number
        else {
            bool ok;
            double num = p.toDouble(&ok);
            if (ok) {
                result += QString::number(num);
            } else {
                result += p;
            }
        }
    }
    
    return result;
}

QVariant ScriptInterpreter::callBuiltin(const QString& funcName, const QStringList& args)
{
    Q_UNUSED(funcName);
    Q_UNUSED(args);
    return QVariant();
}

} // namespace Flux
