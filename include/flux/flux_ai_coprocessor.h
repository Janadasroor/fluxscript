#ifndef FLUX_AI_COPROCESSOR_H
#define FLUX_AI_COPROCESSOR_H

/**
 * @file flux_ai_coprocessor.h
 * @brief AI Co-Pilot integration for FluxScript with Gemini API support
 * 
 * Phase 3: Advanced Features - AI Co-Pilot Integration
 */

#include <QWidget>
#include <QDialog>
#include <QDockWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTextCursor>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QStackedWidget>
#include <QProgressBar>
#include <QTableWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTreeWidget>
#include <QFormLayout>
#include <QPlainTextEdit>

namespace Flux {

/**
 * @brief AI message structure
 */
struct AIMessage {
    enum class Role {
        User,
        Assistant,
        System
    };

    Role role;
    QString content;
    QDateTime timestamp;
    QMap<QString, QVariant> metadata;
};

/**
 * @brief Code suggestion from AI
 */
struct CodeSuggestion {
    QString suggestion;
    QString explanation;
    int confidence;  // 0-100
    QStringList affectedLines;
    QString category;  // "fix", "optimization", "feature"
};

/**
 * @brief ERC violation information
 */
struct ERCViolation {
    enum class Severity {
        Error,
        Warning,
        Info
    };

    Severity severity;
    QString description;
    QString solution;
    QString aiSuggestion;
    int lineNumber;
    QString componentRef;
    bool resolved;
    
    // Equality operator for QMap
    bool operator==(const ERCViolation& other) const {
        return severity == other.severity &&
               description == other.description &&
               lineNumber == other.lineNumber &&
               componentRef == other.componentRef &&
               resolved == other.resolved;
    }
};

/**
 * @brief Markdown-styled text editor for AI responses
 */
class MarkdownTextEdit : public QTextEdit {
    Q_OBJECT

public:
    explicit MarkdownTextEdit(QWidget* parent = nullptr);
    
    void appendMessage(const AIMessage& message);
    void appendCodeBlock(const QString& code, const QString& language = "flux");
    void appendText(const QString& text, bool isUser = false);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QString formatAsMarkdown(const QString& text);
    QString highlightCode(const QString& code, const QString& language);
};

/**
 * @brief AI chat panel for code assistance
 */
class AICopilotPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit AICopilotPanel(QWidget* parent = nullptr);
    ~AICopilotPanel() override;

    // Configuration
    void setApiKey(const QString& apiKey);
    QString apiKey() const { return m_apiKey; }
    void setModel(const QString& model);
    QString model() const { return m_model; }

    // Context
    void setEditorContext(const QString& code, int cursorPosition);
    void setSelectionContext(const QString& selection);
    void setFileContext(const QString& filePath);

    // AI interaction
    void sendPrompt(const QString& prompt);
    void sendCodeReviewRequest();
    void sendERCFixRequest(const QString& violations);
    void sendCodeGenerationRequest(const QString& description);
    void cancelRequest();

    // State
    bool isProcessing() const { return m_isProcessing; }
    QString lastResponse() const { return m_lastResponse; }

public Q_SLOTS:
    void onCodeSelected(const QString& code);
    void onERCViolationsReceived(const QList<ERCViolation>& violations);
    void clearChat();
    void exportChat();

Q_SIGNALS:
    void codeGenerated(const QString& code);
    void codeReviewed(const QList<CodeSuggestion>& suggestions);
    void ercFixSuggested(const QString& violation, const QString& fix);
    void promptSent(const QString& prompt);
    void responseReceived(const QString& response);
    void errorOccurred(const QString& error);
    void processingStarted();
    void processingCompleted();

private Q_SLOTS:
    void onSendClicked();
    void onNetworkReplyFinished(QNetworkReply* reply);
    void onNetworkError(QNetworkReply::NetworkError code);
    void onTimeout();

private:
    void setupUI();
    void setupConnections();
    void sendMessageToAPI(const QString& prompt);
    void parseAIResponse(const QByteArray& jsonData);
    void addMessageToChat(const AIMessage& message);
    QString buildPromptWithContext(const QString& prompt);
    QString extractCodeFromResponse(const QString& response);

    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    
    MarkdownTextEdit* m_chatDisplay;
    QLineEdit* m_promptInput;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;
    QPushButton* m_stopButton;
    
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QStackedWidget* m_inputStack;

    // Network
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_timeoutTimer;

    // State
    QString m_apiKey;
    QString m_model;
    QString m_editorContext;
    QString m_selectionContext;
    QString m_fileContext;
    QString m_lastResponse;
    bool m_isProcessing;

    // Chat history
    QList<AIMessage> m_chatHistory;
    int m_maxHistorySize;
};

/**
 * @brief ERC violation panel with AI assistance
 */
class ERCViolationPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ERCViolationPanel(QWidget* parent = nullptr);
    ~ERCViolationPanel() override;

    // Violation management
    void addViolation(const ERCViolation& violation);
    void updateViolation(int id, const ERCViolation& violation);
    void removeViolation(int id);
    void clearViolations();

    // Access
    QList<ERCViolation> violations() const { return m_violations; }
    int violationCount() const { return m_violations.size(); }
    int errorCount() const;
    int warningCount() const;

public Q_SLOTS:
    void onAIAssistanceRequested(int violationId);
    void onApplyFix(int violationId, const QString& fix);
    void onMarkResolved(int violationId);

Q_SIGNALS:
    void violationSelected(int id, int lineNumber);
    void violationResolved(int id);
    void aiAssistanceRequested(int id, const ERCViolation& violation);
    void fixApplied(int id, const QString& fix);
    void violationsCleared();

private:
    void setupUI();
    void setupConnections();
    void updateViolationList();
    QString severityToString(ERCViolation::Severity severity) const;
    QColor severityColor(ERCViolation::Severity severity) const;
    QIcon severityIcon(ERCViolation::Severity severity) const;

    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    
    QTreeWidget* m_violationTree;
    QPushButton* m_clearButton;
    QPushButton* m_fixAllButton;
    QLabel* m_summaryLabel;

    // Violations
    QList<ERCViolation> m_violations;
    QMap<int, ERCViolation> m_violationMap;  // id -> violation
    int m_nextId;
};

/**
 * @brief Code generation wizard
 */
class CodeGenerationWizard : public QWidget {
    Q_OBJECT

public:
    explicit CodeGenerationWizard(QWidget* parent = nullptr);

    // Wizard state
    void setDescription(const QString& description);
    void setParameters(const QMap<QString, QVariant>& params);
    void setOutputFormat(const QString& format);

    // Generation
    void generate();
    void cancel();

    // Access
    QString generatedCode() const { return m_generatedCode; }
    bool isGenerating() const { return m_isGenerating; }

Q_SIGNALS:
    void generationStarted();
    void generationCompleted(const QString& code);
    void generationFailed(const QString& error);
    void cancelled();

private Q_SLOTS:
    void onGenerateClicked();
    void onAICodeReceived(const QString& code);
    void onAIError(const QString& error);

private:
    void setupUI();
    void setupConnections();
    void updatePreview();

    // UI Components
    QVBoxLayout* m_mainLayout;
    
    QTextEdit* m_descriptionEdit;
    QFormLayout* m_paramsLayout;
    QComboBox* m_formatCombo;
    QTextEdit* m_previewEdit;
    
    QPushButton* m_generateButton;
    QPushButton* m_cancelButton;
    QPushButton* m_insertButton;
    QProgressBar* m_progressBar;

    // State
    QString m_generatedCode;
    bool m_isGenerating;
    AICopilotPanel* m_aiPanel;
};

/**
 * @brief Subcircuit editor with AI assistance
 */
class SubcircuitEditor : public QWidget {
    Q_OBJECT

public:
    explicit SubcircuitEditor(QWidget* parent = nullptr);

    // Subcircuit data
    void setSubcircuitName(const QString& name);
    void setPins(const QStringList& pins);
    void setParameters(const QMap<QString, QString>& params);
    void setBody(const QString& body);

    // Access
    QString subcircuitName() const { return m_name; }
    QStringList pins() const { return m_pins; }
    QMap<QString, QString> parameters() const { return m_parameters; }
    QString body() const { return m_bodyEdit->toPlainText(); }

public Q_SLOTS:
    void generateFromDescription(const QString& description);
    void validateSubcircuit();
    void exportToLibrary();

Q_SIGNALS:
    void subcircuitModified();
    void validationCompleted(bool valid, const QStringList& errors);
    void exportedToLibrary(const QString& path);

private:
    void setupUI();
    void setupConnections();
    void updatePinList();
    void updateParameterList();
    QString generateSubcircuitNetlist();

    // UI Components
    QVBoxLayout* m_mainLayout;
    
    QLineEdit* m_nameEdit;
    QListWidget* m_pinList;
    QLineEdit* m_pinInput;
    QPushButton* m_addPinButton;
    QPushButton* m_removePinButton;
    
    QTableWidget* m_paramTable;
    QPushButton* m_addParamButton;
    QPushButton* m_removeParamButton;
    
    QTextEdit* m_bodyEdit;
    
    QPushButton* m_generateButton;
    QPushButton* m_validateButton;
    QPushButton* m_exportButton;

    // State
    QString m_name;
    QStringList m_pins;
    QMap<QString, QString> m_parameters;

    AICopilotPanel* m_aiPanel;
};

/**
 * @brief LTspice compatibility checker
 */
class LTspiceCompatibilityChecker : public QObject {
    Q_OBJECT

public:
    explicit LTspiceCompatibilityChecker(QObject* parent = nullptr);

    // Check compatibility
    bool checkCompatibility(const QString& code);
    QStringList getWarnings() const { return m_warnings; }
    QStringList getErrors() const { return m_errors; }
    QMap<QString, QString> getSuggestions() const { return m_suggestions; }

    // LTspice feature detection
    bool hasLTspiceDirectives(const QString& code);
    bool hasBehavioralSources(const QString& code);
    bool hasLTspiceFunctions(const QString& code);

    // Conversion
    QString convertToNgspice(const QString& ltspiceCode);
    QString convertLTspiceFunctions(const QString& code);

Q_SIGNALS:
    void compatibilityCheckCompleted(bool compatible, const QStringList& warnings);
    void conversionCompleted(const QString& convertedCode);

private:
    void setupPatterns();
    bool checkDirective(const QString& directive);
    bool checkFunction(const QString& function);
    QString suggestAlternative(const QString& ltspiceFeature);

    // Patterns
    QMap<QString, QRegularExpression> m_directivePatterns;
    QMap<QString, QRegularExpression> m_functionPatterns;
    QMap<QString, QString> m_alternatives;

    // Results
    QStringList m_warnings;
    QStringList m_errors;
    QMap<QString, QString> m_suggestions;
};

/**
 * @brief Symbol generator from subcircuit
 */
class SymbolGenerator : public QObject {
    Q_OBJECT

public:
    explicit SymbolGenerator(QObject* parent = nullptr);

    // Generate symbol
    bool generateSymbol(const QString& subcircuitNetlist, const QString& outputPath);
    
    // Pin mapping
    void setPinMapping(const QMap<QString, QString>& mapping);
    QMap<QString, QString> pinMapping() const { return m_pinMapping; }

    // Auto-detect pins
    QStringList detectPins(const QString& subcircuitNetlist);

Q_SIGNALS:
    void symbolGenerated(const QString& path);
    void generationFailed(const QString& error);

private:
    void parseSubcircuit(const QString& netlist);
    void generateSymbolFile();
    void generatePinMap();

    QString m_subcircuitName;
    QStringList m_pins;
    QMap<QString, QString> m_pinMapping;
    QString m_outputPath;
};

} // namespace Flux

#endif // FLUX_AI_COPROCESSOR_H
