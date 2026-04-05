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

#include "flux/flux_ai_coprocessor.h"

#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QTextBlock>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QBuffer>

namespace Flux {

// ============================================================================
// MarkdownTextEdit Implementation
// ============================================================================

MarkdownTextEdit::MarkdownTextEdit(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setAcceptRichText(true);
    
    // Set monospace font for code blocks
    QFont codeFont("Consolas", 10);
    codeFont.setStyleHint(QFont::Monospace);
    document()->setDefaultFont(codeFont);
}

void MarkdownTextEdit::appendMessage(const AIMessage& message)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);

    // Format based on role
    if (message.role == AIMessage::Role::User) {
        cursor.insertHtml(QString("<div style='background: #2d2d30; padding: 8px; margin: 4px 0; "
                                  "border-radius: 4px; color: #cccccc;'>"
                                  "<b style='color: #569cd6;'>You</b> (%1)<br>%2</div>")
            .arg(message.timestamp.toString("HH:mm:ss"))
            .arg(message.content.toHtmlEscaped()));
    } else {
        cursor.insertHtml(QString("<div style='background: #1e1e1e; padding: 8px; margin: 4px 0; "
                                  "border-radius: 4px; color: #d4d4d4;'>"
                                  "<b style='color: #4ec9b0;'>AI Assistant</b> (%1)<br>%2</div>")
            .arg(message.timestamp.toString("HH:mm:ss"))
            .arg(formatAsMarkdown(message.content)));
    }

    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void MarkdownTextEdit::appendCodeBlock(const QString& code, const QString& language)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    
    QString highlightedCode = highlightCode(code, language);
    cursor.insertHtml(QString("<div style='background: #1e1e1e; padding: 12px; margin: 8px 0; "
                              "border-radius: 4px; border-left: 3px solid #569cd6; "
                              "font-family: Consolas, monospace; white-space: pre;'>%1</div>")
        .arg(highlightedCode));
    
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void MarkdownTextEdit::appendText(const QString& text, bool isUser)
{
    AIMessage message;
    message.role = isUser ? AIMessage::Role::User : AIMessage::Role::Assistant;
    message.content = text;
    message.timestamp = QDateTime::currentDateTime();
    appendMessage(message);
}

QString MarkdownTextEdit::formatAsMarkdown(const QString& text)
{
    QString formatted = text;

    // Simple HTML formatting instead of regex with lambdas (Qt6 compatibility)
    
    // Code blocks - simple replacement
    formatted.replace("```", "<pre style='background: #1e1e1e; padding: 8px; border-radius: 4px;'>");
    formatted.replace("\n```", "</pre>");

    // Inline code
    formatted.replace(QRegularExpression("`([^`]+)`"), "<code style='background: #3c3c3c; padding: 2px 4px; border-radius: 2px;'>\\1</code>");

    // Bold
    formatted.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<b>\\1</b>");

    // Italic
    formatted.replace(QRegularExpression("\\*([^*]+)\\*"), "<i>\\1</i>");

    // Links
    formatted.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href='\\2' style='color: #569cd6;'>\\1</a>");

    // Line breaks
    formatted.replace("\n", "<br>");

    return formatted;
}

QString MarkdownTextEdit::highlightCode(const QString& code, const QString& language)
{
    Q_UNUSED(language);
    
    // Simple syntax highlighting for FluxScript
    QString highlighted = code.toHtmlEscaped();
    
    // Keywords
    QStringList keywords = {"def", "var", "let", "in", "if", "else", "then", "for", "while", 
                           "return", "break", "continue", "extern", "import", "export"};
    for (const QString& kw : keywords) {
        highlighted.replace(QRegularExpression("\\b" + kw + "\\b"),
            QString("<span style='color: #569cd6;'>%1</span>").arg(kw));
    }
    
    // Numbers
    highlighted.replace(QRegularExpression("\\b(\\d+\\.?\\d*)\\b"),
        "<span style='color: #b5cea8;'>\\1</span>");
    
    // Strings
    highlighted.replace(QRegularExpression("\"([^\"]*)\""),
        "<span style='color: #ce9178;'>\"\\1\"</span>");
    
    // Comments
    highlighted.replace(QRegularExpression("//([^\\n]*)"),
        "<span style='color: #6a9955;'>//\\1</span>");
    
    // Functions
    highlighted.replace(QRegularExpression("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\("),
        "<span style='color: #dcdcaa;'>\\1</span>(");
    
    return highlighted;
}

void MarkdownTextEdit::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    
    QAction* copyCodeAction = new QAction("Copy Code Block", menu);
    connect(copyCodeAction, &QAction::triggered, this, [this]() {
        // Would extract and copy the code block under cursor
    });
    
    menu->addAction(copyCodeAction);
    menu->exec(event->globalPos());
    delete menu;
}

// ============================================================================
// AICopilotPanel Implementation
// ============================================================================

AICopilotPanel::AICopilotPanel(QWidget* parent)
    : QDockWidget("AI Co-Pilot", parent)
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_timeoutTimer(nullptr)
    , m_isProcessing(false)
    , m_maxHistorySize(50)
{
    setObjectName("AICopilotPanel");
    setupUI();
    setupConnections();
}

AICopilotPanel::~AICopilotPanel()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void AICopilotPanel::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Chat display
    m_chatDisplay = new MarkdownTextEdit(m_centralWidget);
    m_chatDisplay->setMinimumHeight(300);
    m_mainLayout->addWidget(m_chatDisplay);

    // Progress bar
    m_progressBar = new QProgressBar(m_centralWidget);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_mainLayout->addWidget(m_progressBar);

    // Status label
    m_statusLabel = new QLabel("Ready", m_centralWidget);
    m_statusLabel->setStyleSheet("color: #888888; padding: 4px;");
    m_mainLayout->addWidget(m_statusLabel);

    // Input area
    QWidget* inputWidget = new QWidget(m_centralWidget);
    auto* inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);

    m_promptInput = new QLineEdit(inputWidget);
    m_promptInput->setPlaceholderText("Ask AI for help... (e.g., 'Fix ERC errors', 'Generate testbench')");
    // m_promptInput->setReturnKey(Qt::Key_Return);
    inputLayout->addWidget(m_promptInput);

    m_sendButton = new QPushButton("Send", inputWidget);
    m_sendButton->setStyleSheet(
        "QPushButton { background: #0e639c; color: white; border: none; padding: 8px 16px; "
        "border-radius: 2px; font-weight: bold; } "
        "QPushButton:hover { background: #1177bb; } "
        "QPushButton:disabled { background: #555555; color: #888888; }"
    );
    inputLayout->addWidget(m_sendButton);

    m_stopButton = new QPushButton("Stop", inputWidget);
    m_stopButton->setVisible(false);
    m_stopButton->setStyleSheet(
        "QPushButton { background: #f44336; color: white; border: none; padding: 8px 16px; "
        "border-radius: 2px; } "
        "QPushButton:hover { background: #e53935; }"
    );
    inputLayout->addWidget(m_stopButton);

    m_clearButton = new QPushButton("Clear", inputWidget);
    m_clearButton->setStyleSheet(
        "QPushButton { background: #555555; color: white; border: none; padding: 8px 16px; "
        "border-radius: 2px; } "
        "QPushButton:hover { background: #666666; }"
    );
    inputLayout->addWidget(m_clearButton);

    m_mainLayout->addWidget(inputWidget);

    m_centralWidget->setLayout(m_mainLayout);
    setWidget(m_centralWidget);

    // Set initial size
    resize(400, 500);
}

void AICopilotPanel::setupConnections()
{
    m_networkManager = new QNetworkAccessManager(this);

    connect(m_sendButton, &QPushButton::clicked,
            this, &AICopilotPanel::onSendClicked);
    connect(m_stopButton, &QPushButton::clicked,
            this, &AICopilotPanel::cancelRequest);
    connect(m_clearButton, &QPushButton::clicked,
            this, &AICopilotPanel::clearChat);
    connect(m_promptInput, &QLineEdit::returnPressed,
            this, &AICopilotPanel::onSendClicked);

    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &AICopilotPanel::onNetworkReplyFinished);
}

void AICopilotPanel::setApiKey(const QString& apiKey)
{
    m_apiKey = apiKey;
}

void AICopilotPanel::setModel(const QString& model)
{
    m_model = model;
}

void AICopilotPanel::setEditorContext(const QString& code, int cursorPosition)
{
    m_editorContext = code;
    Q_UNUSED(cursorPosition);
}

void AICopilotPanel::setSelectionContext(const QString& selection)
{
    m_selectionContext = selection;
}

void AICopilotPanel::setFileContext(const QString& filePath)
{
    m_fileContext = filePath;
}

void AICopilotPanel::sendPrompt(const QString& prompt)
{
    m_promptInput->setText(prompt);
    onSendClicked();
}

void AICopilotPanel::sendCodeReviewRequest()
{
    sendPrompt("Please review this code for potential issues and suggest improvements:");
}

void AICopilotPanel::sendERCFixRequest(const QString& violations)
{
    sendPrompt(QString("I have the following ERC violations:\n\n%1\n\nPlease suggest fixes.").arg(violations));
}

void AICopilotPanel::sendCodeGenerationRequest(const QString& description)
{
    sendPrompt(QString("Generate FluxScript code for: %1").arg(description));
}

void AICopilotPanel::cancelRequest()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    
    m_isProcessing = false;
    m_sendButton->setVisible(true);
    m_stopButton->setVisible(false);
    m_progressBar->setVisible(false);
    m_statusLabel->setText("Cancelled");
}

void AICopilotPanel::clearChat()
{
    m_chatHistory.clear();
    m_chatDisplay->clear();
    m_statusLabel->setText("Chat cleared");
}

void AICopilotPanel::exportChat()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Chat History", "", "Text Files (*.txt);;HTML Files (*.html)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        if (fileName.endsWith(".html")) {
            file.write(m_chatDisplay->toHtml().toUtf8());
        } else {
            for (const auto& msg : m_chatHistory) {
                file.write(QString("[%1] %2: %3\n")
                    .arg(msg.timestamp.toString())
                    .arg(msg.role == AIMessage::Role::User ? "User" : "AI")
                    .arg(msg.content)
                    .toUtf8());
            }
        }
    }
}

void AICopilotPanel::onSendClicked()
{
    QString prompt = m_promptInput->text().trimmed();
    if (prompt.isEmpty()) {
        return;
    }
    
    if (m_isProcessing) {
        return;
    }
    
    // Add user message to chat
    AIMessage userMsg;
    userMsg.role = AIMessage::Role::User;
    userMsg.content = prompt;
    userMsg.timestamp = QDateTime::currentDateTime();
    addMessageToChat(userMsg);
    
    m_promptInput->clear();
    
    // Send to API
    sendMessageToAPI(prompt);
}

void AICopilotPanel::sendMessageToAPI(const QString& prompt)
{
    m_isProcessing = true;
    m_sendButton->setVisible(false);
    m_stopButton->setVisible(true);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("AI is thinking...");
    
    Q_EMIT promptSent(prompt);
    Q_EMIT processingStarted();
    
    // Build request
    QJsonObject request;
    request["model"] = m_model.isEmpty() ? "gemini-pro" : m_model;
    
    QJsonArray messages;
    
    // System message
    QJsonObject systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = "You are an expert assistant for FluxScript, a high-performance mathematical language for circuit simulation. Help users write correct and efficient FluxScript code.";
    messages.append(systemMsg);
    
    // Add context
    QString fullPrompt = buildPromptWithContext(prompt);
    
    // User message
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = fullPrompt;
    messages.append(userMsg);
    
    request["messages"] = messages;
    request["temperature"] = 0.7;
    request["max_tokens"] = 2000;
    
    QJsonDocument doc(request);
    
    // Create request (would use actual Gemini API endpoint)
    QUrl url("https://api.example.com/v1/chat/completions");  // Placeholder
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    
    m_currentReply = m_networkManager->post(networkRequest, doc.toJson());
    
    // Setup timeout
    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
            if (m_currentReply && m_currentReply->isRunning()) {
                m_currentReply->abort();
                m_statusLabel->setText("Request timed out");
            }
        });
    }
    m_timeoutTimer->start(60000);  // 60 second timeout
}

void AICopilotPanel::onNetworkReplyFinished(QNetworkReply* reply)
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    
    m_currentReply = nullptr;
    m_isProcessing = false;
    m_sendButton->setVisible(true);
    m_stopButton->setVisible(false);
    m_progressBar->setVisible(false);
    
    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("API Error: %1").arg(reply->errorString());
        m_statusLabel->setText(error);
        Q_EMIT errorOccurred(error);
        return;
    }
    
    QByteArray responseData = reply->readAll();
    parseAIResponse(responseData);
    
    reply->deleteLater();
}

void AICopilotPanel::onNetworkError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);
    m_statusLabel->setText("Network error occurred");
}

void AICopilotPanel::onTimeout()
{
    m_statusLabel->setText("Request timed out");
    cancelRequest();
}

void AICopilotPanel::parseAIResponse(const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        m_statusLabel->setText("Failed to parse response");
        return;
    }
    
    QJsonObject root = doc.object();
    QJsonArray choices = root["choices"].toArray();
    
    if (choices.isEmpty()) {
        m_statusLabel->setText("No response from AI");
        return;
    }
    
    QJsonObject choice = choices[0].toObject();
    QJsonObject message = choice["message"].toObject();
    QString content = message["content"].toString();
    
    m_lastResponse = content;
    
    // Add AI response to chat
    AIMessage aiMsg;
    aiMsg.role = AIMessage::Role::Assistant;
    aiMsg.content = content;
    aiMsg.timestamp = QDateTime::currentDateTime();
    addMessageToChat(aiMsg);
    
    m_statusLabel->setText("Response received");
    Q_EMIT responseReceived(content);
    Q_EMIT processingCompleted();
    
    // Check for code blocks
    QString code = extractCodeFromResponse(content);
    if (!code.isEmpty()) {
        Q_EMIT codeGenerated(code);
    }
}

void AICopilotPanel::addMessageToChat(const AIMessage& message)
{
    m_chatHistory.append(message);
    
    // Trim history if needed
    while (m_chatHistory.size() > m_maxHistorySize) {
        m_chatHistory.removeFirst();
    }
    
    m_chatDisplay->appendMessage(message);
}

QString AICopilotPanel::buildPromptWithContext(const QString& prompt)
{
    QString fullPrompt = prompt;
    
    if (!m_selectionContext.isEmpty()) {
        fullPrompt += QString("\n\nSelected code:\n```\n%1\n```").arg(m_selectionContext);
    }
    
    if (!m_editorContext.isEmpty()) {
        // Include first 500 chars of context
        QString context = m_editorContext.left(500);
        if (m_editorContext.length() > 500) {
            context += "...";
        }
        fullPrompt += QString("\n\nFile context:\n```\n%1\n```").arg(context);
    }
    
    if (!m_fileContext.isEmpty()) {
        fullPrompt += QString("\n\nFile: %1").arg(m_fileContext);
    }
    
    return fullPrompt;
}

QString AICopilotPanel::extractCodeFromResponse(const QString& response)
{
    QRegularExpression codeRegex("```(?:\\w+)?\\n(.*?)```", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = codeRegex.match(response);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return QString();
}

void AICopilotPanel::onCodeSelected(const QString& code)
{
    setSelectionContext(code);
    m_statusLabel->setText("Code selection set as context");
}

void AICopilotPanel::onERCViolationsReceived(const QList<ERCViolation>& violations)
{
    QString violationText;
    for (const auto& v : violations) {
        violationText += QString("- Line %1: %2\n").arg(v.lineNumber).arg(v.description);
    }
    sendERCFixRequest(violationText);
}

// ============================================================================
// ERCViolationPanel Implementation
// ============================================================================

ERCViolationPanel::ERCViolationPanel(QWidget* parent)
    : QDockWidget("ERC Violations", parent)
    , m_nextId(1)
{
    setObjectName("ERCViolationPanel");
    setupUI();
    setupConnections();
}

ERCViolationPanel::~ERCViolationPanel()
{
}

void ERCViolationPanel::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Summary label
    m_summaryLabel = new QLabel("0 errors, 0 warnings", m_centralWidget);
    m_summaryLabel->setStyleSheet("font-weight: bold; padding: 8px; background: #2d2d30;");
    m_mainLayout->addWidget(m_summaryLabel);

    // Violation tree
    m_violationTree = new QTreeWidget(m_centralWidget);
    m_violationTree->setHeaderLabels(QStringList() << "Type" << "Description" << "Location");
    m_violationTree->setColumnWidth(0, 80);
    m_violationTree->setColumnWidth(1, 250);
    m_violationTree->setColumnWidth(2, 100);
    m_violationTree->setAlternatingRowColors(true);
    m_mainLayout->addWidget(m_violationTree);

    // Buttons
    QWidget* buttonWidget = new QWidget(m_centralWidget);
    auto* buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(4, 4, 4, 4);

    m_fixAllButton = new QPushButton("Fix All with AI", buttonWidget);
    m_fixAllButton->setStyleSheet(
        "QPushButton { background: #0e639c; color: white; border: none; padding: 6px 12px; "
        "border-radius: 2px; } "
        "QPushButton:hover { background: #1177bb; }"
    );
    buttonLayout->addWidget(m_fixAllButton);

    m_clearButton = new QPushButton("Clear", buttonWidget);
    m_clearButton->setStyleSheet(
        "QPushButton { background: #555555; color: white; border: none; padding: 6px 12px; "
        "border-radius: 2px; } "
        "QPushButton:hover { background: #666666; }"
    );
    buttonLayout->addWidget(m_clearButton);

    buttonLayout->addStretch();

    m_mainLayout->addWidget(buttonWidget);

    m_centralWidget->setLayout(m_mainLayout);
    setWidget(m_centralWidget);
}

void ERCViolationPanel::setupConnections()
{
    connect(m_clearButton, &QPushButton::clicked,
            this, &ERCViolationPanel::clearViolations);
    connect(m_fixAllButton, &QPushButton::clicked,
            this, [this]() {
                for (const auto& v : m_violations) {
                    if (!v.resolved) {
                        onAIAssistanceRequested(m_violationMap.key(v));
                    }
                }
            });

    connect(m_violationTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item) {
                int id = item->data(0, Qt::UserRole).toInt();
                if (m_violationMap.contains(id)) {
                    Q_EMIT violationSelected(id, m_violationMap[id].lineNumber);
                }
            });
}

void ERCViolationPanel::addViolation(const ERCViolation& violation)
{
    int id = m_nextId++;
    m_violations.append(violation);
    m_violationMap[id] = violation;
    updateViolationList();
}

void ERCViolationPanel::updateViolation(int id, const ERCViolation& violation)
{
    m_violationMap[id] = violation;
    
    // Update in list
    for (int i = 0; i < m_violations.size(); ++i) {
        if (m_violations[i].lineNumber == violation.lineNumber) {
            m_violations[i] = violation;
            break;
        }
    }
    
    updateViolationList();
}

void ERCViolationPanel::removeViolation(int id)
{
    m_violationMap.remove(id);
    m_violations.removeIf([id, this](const ERCViolation& v) {
        return m_violationMap.key(v) == id;
    });
    updateViolationList();
}

void ERCViolationPanel::clearViolations()
{
    m_violations.clear();
    m_violationMap.clear();
    updateViolationList();
    Q_EMIT violationsCleared();
}

int ERCViolationPanel::errorCount() const
{
    int count = 0;
    for (const auto& v : m_violations) {
        if (v.severity == ERCViolation::Severity::Error && !v.resolved) {
            count++;
        }
    }
    return count;
}

int ERCViolationPanel::warningCount() const
{
    int count = 0;
    for (const auto& v : m_violations) {
        if (v.severity == ERCViolation::Severity::Warning && !v.resolved) {
            count++;
        }
    }
    return count;
}

void ERCViolationPanel::updateViolationList()
{
    m_violationTree->clear();
    
    int errors = 0;
    int warnings = 0;
    
    for (auto it = m_violationMap.begin(); it != m_violationMap.end(); ++it) {
        int id = it.key();
        const ERCViolation& v = it.value();
        
        if (v.resolved) {
            continue;
        }
        
        auto* item = new QTreeWidgetItem(m_violationTree);
        item->setData(0, Qt::UserRole, id);
        
        item->setIcon(0, severityIcon(v.severity));
        item->setText(0, severityToString(v.severity));
        item->setText(1, v.description);
        item->setText(2, QString("Line %1").arg(v.lineNumber));
        
        if (v.severity == ERCViolation::Severity::Error) {
            errors++;
            for (int i = 0; i < m_violationTree->columnCount(); ++i) {
                item->setForeground(i, QColor(255, 100, 100));
            }
        } else {
            warnings++;
            for (int i = 0; i < m_violationTree->columnCount(); ++i) {
                item->setForeground(i, QColor(255, 200, 100));
            }
        }
        
        // Add context menu
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    
    m_summaryLabel->setText(QString("%1 errors, %2 warnings").arg(errors).arg(warnings));
}

QString ERCViolationPanel::severityToString(ERCViolation::Severity severity) const
{
    switch (severity) {
    case ERCViolation::Severity::Error: return "Error";
    case ERCViolation::Severity::Warning: return "Warning";
    case ERCViolation::Severity::Info: return "Info";
    default: return "Unknown";
    }
}

QColor ERCViolationPanel::severityColor(ERCViolation::Severity severity) const
{
    switch (severity) {
    case ERCViolation::Severity::Error: return QColor(255, 100, 100);
    case ERCViolation::Severity::Warning: return QColor(255, 200, 100);
    case ERCViolation::Severity::Info: return QColor(100, 150, 255);
    default: return QColor(150, 150, 150);
    }
}

QIcon ERCViolationPanel::severityIcon(ERCViolation::Severity severity) const
{
    Q_UNUSED(severity);
    // Would return appropriate icons
    return QIcon();
}

void ERCViolationPanel::onAIAssistanceRequested(int violationId)
{
    if (!m_violationMap.contains(violationId)) {
        return;
    }
    
    const ERCViolation& v = m_violationMap[violationId];
    Q_EMIT aiAssistanceRequested(violationId, v);
}

void ERCViolationPanel::onApplyFix(int violationId, const QString& fix)
{
    if (!m_violationMap.contains(violationId)) {
        return;
    }
    
    ERCViolation& v = m_violationMap[violationId];
    v.aiSuggestion = fix;
    v.resolved = true;
    
    Q_EMIT fixApplied(violationId, fix);
    updateViolationList();
}

void ERCViolationPanel::onMarkResolved(int violationId)
{
    if (!m_violationMap.contains(violationId)) {
        return;
    }
    
    m_violationMap[violationId].resolved = true;
    updateViolationList();
}

// ============================================================================
// LTspiceCompatibilityChecker Implementation
// ============================================================================

LTspiceCompatibilityChecker::LTspiceCompatibilityChecker(QObject* parent)
    : QObject(parent)
{
    setupPatterns();
}

void LTspiceCompatibilityChecker::setupPatterns()
{
    // LTspice-specific directives
    m_directivePatterns["LTspice"] = QRegularExpression("\\.symattr");
    m_directivePatterns["Plot"] = QRegularExpression("\\.plot");
    m_directivePatterns["WaveColor"] = QRegularExpression("WaveColor");
    
    // LTspice-specific functions
    m_functionPatterns["buf"] = QRegularExpression("\\bbuf\\s*\\(");
    m_functionPatterns["inv"] = QRegularExpression("\\binv\\s*\\(");
    m_functionPatterns["uramp"] = QRegularExpression("\\buramp\\s*\\(");
    m_functionPatterns["limit"] = QRegularExpression("\\blimit\\s*\\(");
    m_functionPatterns["idtmod"] = QRegularExpression("\\bidtmod\\s*\\(");
    m_functionPatterns["table"] = QRegularExpression("\\btable\\s*\\(");
    
    // Alternatives
    m_alternatives["buf(x)"] = "x > 0.5 ? 1 : 0";
    m_alternatives["inv(x)"] = "x > 0.5 ? 0 : 1";
    m_alternatives["uramp(x)"] = "x > 0 ? x : 0";
    m_alternatives["limit(x,y,z)"] = "x < y ? y : (x > z ? z : x)";
    m_alternatives["idtmod(x)"] = "Use idt() with reset condition";
    m_alternatives["table(x,a,b)"] = "Use if-then-else or piecewise function";
}

bool LTspiceCompatibilityChecker::checkCompatibility(const QString& code)
{
    m_warnings.clear();
    m_errors.clear();
    m_suggestions.clear();
    
    bool compatible = true;
    
    // Check for LTspice directives
    for (auto it = m_directivePatterns.begin(); it != m_directivePatterns.end(); ++it) {
        if (it.value().match(code).hasMatch()) {
            m_warnings.append(QString("LTspice directive detected: %1").arg(it.key()));
            m_suggestions[it.key()] = suggestAlternative(it.key());
        }
    }
    
    // Check for LTspice functions
    for (auto it = m_functionPatterns.begin(); it != m_functionPatterns.end(); ++it) {
        if (it.value().match(code).hasMatch()) {
            m_warnings.append(QString("LTspice function detected: %1").arg(it.key()));
            m_suggestions[it.key()] = suggestAlternative(it.key());
        }
    }
    
    return compatible && m_warnings.isEmpty();
}

bool LTspiceCompatibilityChecker::hasLTspiceDirectives(const QString& code)
{
    for (const auto& pattern : m_directivePatterns) {
        if (pattern.match(code).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool LTspiceCompatibilityChecker::hasBehavioralSources(const QString& code)
{
    return code.contains(QRegularExpression("B[1-9][0-9]*\\s+"));
}

bool LTspiceCompatibilityChecker::hasLTspiceFunctions(const QString& code)
{
    for (const auto& pattern : m_functionPatterns) {
        if (pattern.match(code).hasMatch()) {
            return true;
        }
    }
    return false;
}

QString LTspiceCompatibilityChecker::convertToNgspice(const QString& ltspiceCode)
{
    QString converted = ltspiceCode;
    
    // Convert LTspice functions
    converted = convertLTspiceFunctions(converted);
    
    // Remove LTspice-specific directives
    converted.remove(QRegularExpression("\\.symattr[^\\n]*"));
    converted.remove(QRegularExpression("\\.plot[^\\n]*"));
    
    return converted;
}

QString LTspiceCompatibilityChecker::convertLTspiceFunctions(const QString& code)
{
    QString converted = code;
    
    // Replace LTspice functions with ngspice equivalents
    for (auto it = m_alternatives.begin(); it != m_alternatives.end(); ++it) {
        converted.replace(QRegularExpression("\\b" + it.key().left(it.key().indexOf('(')) + "\\s*\\([^)]*\\)"),
            it.value());
    }
    
    return converted;
}

QString LTspiceCompatibilityChecker::suggestAlternative(const QString& ltspiceFeature)
{
    return m_alternatives.value(ltspiceFeature, "No direct alternative available");
}

// ============================================================================
// SubcircuitEditor Implementation
// ============================================================================

SubcircuitEditor::SubcircuitEditor(QWidget* parent)
    : QWidget(parent)
    , m_aiPanel(nullptr)
{
    setupUI();
    setupConnections();
}

void SubcircuitEditor::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Name input
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Subcircuit Name:"));
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText("e.g., OPAMP, 555_TIMER");
    nameLayout->addWidget(m_nameEdit);
    m_mainLayout->addLayout(nameLayout);
    
    // Pin list
    auto* pinLayout = new QHBoxLayout();
    pinLayout->addWidget(new QLabel("Pins:"));
    m_pinList = new QListWidget();
    m_pinInput = new QLineEdit();
    m_pinInput->setPlaceholderText("Add pin (e.g., V+, V-, OUT)");
    pinLayout->addWidget(m_pinInput);
    m_addPinButton = new QPushButton("Add");
    pinLayout->addWidget(m_addPinButton);
    m_removePinButton = new QPushButton("Remove");
    pinLayout->addWidget(m_removePinButton);
    m_mainLayout->addLayout(pinLayout);
    m_mainLayout->addWidget(m_pinList);
    
    // Parameters
    auto* paramLayout = new QHBoxLayout();
    paramLayout->addWidget(new QLabel("Parameters:"));
    m_paramTable = new QTableWidget(0, 2);
    m_paramTable->setHorizontalHeaderLabels(QStringList() << "Name" << "Value");
    m_mainLayout->addWidget(m_paramTable);
    
    auto* paramButtonLayout = new QHBoxLayout();
    m_addParamButton = new QPushButton("Add Parameter");
    paramButtonLayout->addWidget(m_addParamButton);
    m_removeParamButton = new QPushButton("Remove");
    paramButtonLayout->addWidget(m_removeParamButton);
    paramButtonLayout->addStretch();
    m_mainLayout->addLayout(paramButtonLayout);
    
    // Body editor
    m_bodyEdit = new QTextEdit();
    m_bodyEdit->setPlaceholderText("Subcircuit netlist or behavioral model...");
    m_mainLayout->addWidget(m_bodyEdit);
    
    // Action buttons
    auto* buttonLayout = new QHBoxLayout();
    m_generateButton = new QPushButton("Generate from Description");
    buttonLayout->addWidget(m_generateButton);
    m_validateButton = new QPushButton("Validate");
    buttonLayout->addWidget(m_validateButton);
    m_exportButton = new QPushButton("Export to Library");
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    m_mainLayout->addLayout(buttonLayout);
}

void SubcircuitEditor::setupConnections()
{
    connect(m_addPinButton, &QPushButton::clicked,
            this, &SubcircuitEditor::updatePinList);
    connect(m_removePinButton, &QPushButton::clicked,
            this, [this]() {
                auto* item = m_pinList->currentItem();
                if (item) {
                    delete item;
                }
            });
    
    connect(m_addParamButton, &QPushButton::clicked,
            this, [this]() {
                int row = m_paramTable->rowCount();
                m_paramTable->insertRow(row);
            });
    connect(m_removeParamButton, &QPushButton::clicked,
            this, [this]() {
                int row = m_paramTable->currentRow();
                if (row >= 0) {
                    m_paramTable->removeRow(row);
                }
            });
    
    connect(m_generateButton, &QPushButton::clicked,
            this, [this]() {
                // Would open dialog for description input
            });
    connect(m_validateButton, &QPushButton::clicked,
            this, &SubcircuitEditor::validateSubcircuit);
    connect(m_exportButton, &QPushButton::clicked,
            this, &SubcircuitEditor::exportToLibrary);
}

void SubcircuitEditor::setSubcircuitName(const QString& name)
{
    m_name = name;
    m_nameEdit->setText(name);
}

void SubcircuitEditor::setPins(const QStringList& pins)
{
    m_pins = pins;
    m_pinList->clear();
    m_pinList->addItems(pins);
}

void SubcircuitEditor::setParameters(const QMap<QString, QString>& params)
{
    m_parameters = params;
    m_paramTable->setRowCount(params.size());
    
    int row = 0;
    for (auto it = params.begin(); it != params.end(); ++it) {
        m_paramTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_paramTable->setItem(row, 1, new QTableWidgetItem(it.value()));
        row++;
    }
}

void SubcircuitEditor::generateFromDescription(const QString& description)
{
    if (m_aiPanel) {
        m_aiPanel->sendCodeGenerationRequest(description);
    }
}

void SubcircuitEditor::validateSubcircuit()
{
    QStringList errors;
    
    // Check name
    if (m_name.isEmpty()) {
        errors.append("Subcircuit name is required");
    }
    
    // Check pins
    if (m_pins.isEmpty()) {
        errors.append("At least one pin is required");
    }
    
    // Check body
    if (m_bodyEdit->toPlainText().isEmpty()) {
        errors.append("Subcircuit body is required");
    }
    
    Q_EMIT validationCompleted(errors.isEmpty(), errors);
}

void SubcircuitEditor::exportToLibrary()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Export Subcircuit to Library", "", "FluxScript Files (*.flux)"
    );
    
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(generateSubcircuitNetlist().toUtf8());
            Q_EMIT exportedToLibrary(path);
        }
    }
}

void SubcircuitEditor::updatePinList()
{
    QString pin = m_pinInput->text().trimmed();
    if (!pin.isEmpty()) {
        m_pinList->addItem(pin);
        m_pinInput->clear();
    }
}

void SubcircuitEditor::updateParameterList()
{
    m_parameters.clear();
    for (int row = 0; row < m_paramTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = m_paramTable->item(row, 0);
        QTableWidgetItem* valueItem = m_paramTable->item(row, 1);
        if (nameItem && valueItem) {
            m_parameters[nameItem->text()] = valueItem->text();
        }
    }
}

QString SubcircuitEditor::generateSubcircuitNetlist()
{
    updateParameterList();
    
    QString netlist = QString(".SUBCKT %1 %2\n").arg(m_name).arg(m_pins.join(" "));
    
    // Add parameters
    if (!m_parameters.isEmpty()) {
        netlist += "+ PARAMS: ";
        QStringList params;
        for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
            params.append(QString("%1=%2").arg(it.key()).arg(it.value()));
        }
        netlist += params.join(" ");
        netlist += "\n";
    }
    
    // Add body
    netlist += m_bodyEdit->toPlainText() + "\n";
    
    netlist += ".ENDS\n";
    
    return netlist;
}

// ============================================================================
// SymbolGenerator Implementation
// ============================================================================

SymbolGenerator::SymbolGenerator(QObject* parent)
    : QObject(parent)
{
}

bool SymbolGenerator::generateSymbol(const QString& subcircuitNetlist, const QString& outputPath)
{
    m_outputPath = outputPath;
    parseSubcircuit(subcircuitNetlist);
    generatePinMap();
    generateSymbolFile();
    return true;
}

void SymbolGenerator::setPinMapping(const QMap<QString, QString>& mapping)
{
    m_pinMapping = mapping;
}

QStringList SymbolGenerator::detectPins(const QString& subcircuitNetlist)
{
    QStringList pins;
    
    // Parse .SUBCKT line
    QRegularExpression subcktRegex("\\.SUBCKT\\s+\\S+\\s+(.+?)(?:$|\\n)");
    QRegularExpressionMatch match = subcktRegex.match(subcircuitNetlist);
    
    if (match.hasMatch()) {
        QString pinString = match.captured(1);
        pins = pinString.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }
    
    return pins;
}

void SymbolGenerator::parseSubcircuit(const QString& netlist)
{
    m_pins = detectPins(netlist);
    
    // Extract subcircuit name
    QRegularExpression nameRegex("\\.SUBCKT\\s+(\\S+)");
    QRegularExpressionMatch match = nameRegex.match(netlist);
    if (match.hasMatch()) {
        m_subcircuitName = match.captured(1);
    }
}

void SymbolGenerator::generatePinMap()
{
    // Auto-generate pin positions based on pin count
    int pinCount = m_pins.size();
    
    // Simple layout: pins on left and right
    int leftPins = (pinCount + 1) / 2;
    int rightPins = pinCount - leftPins;
    
    for (int i = 0; i < pinCount; ++i) {
        QString pin = m_pins[i];
        QString position;
        
        if (i < leftPins) {
            position = QString("left:%1").arg(i);
        } else {
            position = QString("right:%1").arg(i - leftPins);
        }
        
        m_pinMapping[pin] = position;
    }
}

void SymbolGenerator::generateSymbolFile()
{
    // Would generate symbol file in appropriate format
    Q_EMIT symbolGenerated(m_outputPath);
}

// ============================================================================
// CodeGenerationWizard Implementation
// ============================================================================

CodeGenerationWizard::CodeGenerationWizard(QWidget* parent)
    : QWidget(parent)
{
    // Stub implementation
}

void CodeGenerationWizard::onGenerateClicked()
{
    // Stub
}

void CodeGenerationWizard::onAICodeReceived(const QString& code)
{
    Q_UNUSED(code);
}

void CodeGenerationWizard::onAIError(const QString& error)
{
    Q_UNUSED(error);
}

} // namespace Flux
