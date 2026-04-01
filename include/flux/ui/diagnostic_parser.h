#ifndef FLUX_DIAGNOSTIC_PARSER_H
#define FLUX_DIAGNOSTIC_PARSER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QRegularExpression>

namespace Flux {

/**
 * @brief Diagnostic severity levels
 */
enum class DiagnosticSeverity {
    Hint,
    Warning,
    Error,
    Fatal
};

/**
 * @brief Diagnostic information from compilation or linting
 */
struct DiagnosticInfo {
    DiagnosticSeverity severity;
    QString filePath;
    int line;
    int column;
    int length;
    QString message;
    QString ruleId;      // Optional: for linting rules
    QString suggestion;  // Optional: auto-fix suggestion
    
    DiagnosticInfo() : severity(DiagnosticSeverity::Hint), line(-1), column(-1), length(0) {}
};

/**
 * @brief Parses compiler errors and displays them in the editor
 * 
 * Supports multiple error formats:
 * - GCC/Clang style: file:line:col: severity: message
 * - MSVC style: file(line,col) : severity: message
 * - Custom FluxScript format
 */
class DiagnosticParser : public QObject {
    Q_OBJECT
    
public:
    explicit DiagnosticParser(QObject* parent = nullptr);
    ~DiagnosticParser() override;

    // ========================================================================
    // Parsing
    // ========================================================================
    
    /**
     * @brief Parse compiler output into diagnostics
     * @param output Compiler output text
     * @return List of parsed diagnostics
     */
    QList<DiagnosticInfo> parse(const QString& output);
    
    /**
     * @brief Parse a single line of output
     * @param line Single line of compiler output
     * @return Diagnostic if parsed, empty optional otherwise
     */
    std::optional<DiagnosticInfo> parseLine(const QString& line);

    // ========================================================================
    // Diagnostic Management
    // ========================================================================
    
    /**
     * @brief Set diagnostics for a specific file
     * @param filePath Source file path
     * @param diagnostics List of diagnostics
     */
    void setDiagnostics(const QString& filePath, const QList<DiagnosticInfo>& diagnostics);
    
    /**
     * @brief Get diagnostics for a specific file
     * @param filePath Source file path
     * @return List of diagnostics
     */
    QList<DiagnosticInfo> diagnosticsForFile(const QString& filePath) const;
    
    /**
     * @brief Get all diagnostics across all files
     * @return List of all diagnostics
     */
    QList<DiagnosticInfo> allDiagnostics() const;
    
    /**
     * @brief Get error count for a file
     * @param filePath Source file path
     * @return Number of errors
     */
    int errorCount(const QString& filePath) const;
    
    /**
     * @brief Get warning count for a file
     * @param filePath Source file path
     * @return Number of warnings
     */
    int warningCount(const QString& filePath) const;
    
    /**
     * @brief Clear diagnostics for a file
     * @param filePath Source file path
     */
    void clearDiagnostics(const QString& filePath);
    
    /**
     * @brief Clear all diagnostics
     */
    void clearAllDiagnostics();

signals:
    // ========================================================================
    // Diagnostic Updates
    // ========================================================================
    void diagnosticsUpdated(const QString& filePath, const QList<DiagnosticInfo>& diagnostics);
    void diagnosticAdded(const QString& filePath, const DiagnosticInfo& diagnostic);
    void diagnosticRemoved(const QString& filePath, const DiagnosticInfo& diagnostic);
    void allDiagnosticsCleared();

private:
    // ========================================================================
    // Pattern Matching
    // ========================================================================
    void setupPatterns();
    bool matchGccFormat(const QString& line, DiagnosticInfo& diag);
    bool matchMsvcFormat(const QString& line, DiagnosticInfo& diag);
    bool matchFluxFormat(const QString& line, DiagnosticInfo& diag);
    
    // ========================================================================
    // Helpers
    // ========================================================================
    DiagnosticSeverity parseSeverity(const QString& severityStr);
    QString severityToString(DiagnosticSeverity severity);
    
    // ========================================================================
    // Members
    // ========================================================================
    QMap<QString, QList<DiagnosticInfo>> m_diagnostics;  // file path -> diagnostics
    
    // Regular expressions for different error formats
    QRegularExpression m_gccPattern;
    QRegularExpression m_msvcPattern;
    QRegularExpression m_fluxPattern;
};

} // namespace Flux

#endif // FLUX_DIAGNOSTIC_PARSER_H
