#include "flux/ui/output_panel.h"
#include <QDateTime>
#include <QScrollBar>
#include <QFontDatabase>

namespace Flux {

OutputPanel::OutputPanel(QWidget* parent)
    : QWidget(parent)
    , m_outputEdit(nullptr)
    , m_categoryCombo(nullptr)
    , m_clearBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_scrollBtn(nullptr)
    , m_process(nullptr)
    , m_autoScroll(true)
    , m_showTimestamps(false)
{
    setupUI();
    setupConnections();
    
    // Initialize colors
    m_colors["output"] = QColor(204, 204, 204);  // Light gray
    m_colors["error"] = QColor(255, 100, 100);   // Red
    m_colors["warning"] = QColor(255, 200, 100); // Orange
    m_colors["info"] = QColor(100, 200, 255);    // Light blue
    m_colors["success"] = QColor(100, 255, 100); // Green
}

OutputPanel::~OutputPanel()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void OutputPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Top toolbar
    auto* toolbar = new QWidget();
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 4);
    toolbarLayout->setSpacing(4);
    toolbar->setStyleSheet("background-color: #2d2d2d;");
    
    // Category filter
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItem("All Output");
    m_categoryCombo->addItem("Build");
    m_categoryCombo->addItem("Run");
    m_categoryCombo->addItem("Debug");
    m_categoryCombo->addItem("Errors");
    m_categoryCombo->setFixedHeight(24);
    m_categoryCombo->setStyleSheet(
        "QComboBox { background-color: #3c3c3c; color: #cccccc; border: 1px solid #555; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background-color: #3c3c3c; color: #cccccc; border: 1px solid #555; }"
    );
    toolbarLayout->addWidget(m_categoryCombo);
    
    toolbarLayout->addStretch();
    
    // Auto-scroll toggle
    m_scrollBtn = new QToolButton();
    m_scrollBtn->setText("📜");
    m_scrollBtn->setToolTip("Auto-scroll");
    m_scrollBtn->setCheckable(true);
    m_scrollBtn->setChecked(true);
    m_scrollBtn->setFixedSize(24, 24);
    m_scrollBtn->setStyleSheet(
        "QToolButton { background-color: #3c3c3c; color: #cccccc; border: none; border-radius: 2px; }"
        "QToolButton:checked { background-color: #007acc; }"
        "QToolButton:hover { background-color: #4c4c4c; }"
    );
    toolbarLayout->addWidget(m_scrollBtn);
    
    // Stop button
    m_stopBtn = new QToolButton();
    m_stopBtn->setText("⏹");
    m_stopBtn->setToolTip("Stop Process");
    m_stopBtn->setFixedSize(24, 24);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(
        "QToolButton { background-color: #3c3c3c; color: #ff6b6b; border: none; border-radius: 2px; }"
        "QToolButton:hover { background-color: #cc5555; }"
        "QToolButton:disabled { color: #666666; }"
    );
    toolbarLayout->addWidget(m_stopBtn);
    
    // Clear button
    m_clearBtn = new QToolButton();
    m_clearBtn->setText("🗑");
    m_clearBtn->setToolTip("Clear Output");
    m_clearBtn->setFixedSize(24, 24);
    m_clearBtn->setStyleSheet(
        "QToolButton { background-color: #3c3c3c; color: #cccccc; border: none; border-radius: 2px; }"
        "QToolButton:hover { background-color: #4c4c4c; }"
    );
    toolbarLayout->addWidget(m_clearBtn);
    
    layout->addWidget(toolbar);
    
    // Output text area
    m_outputEdit = new QTextEdit();
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_outputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_outputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_outputEdit->setStyleSheet(
        "QTextEdit { "
        "    background-color: #1e1e1e; "
        "    color: #cccccc; "
        "    border: none; "
        "    font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "    font-size: 12px; "
        "}"
    );
    
    // Set monospace font
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(11);
    m_outputEdit->setFont(monoFont);
    
    layout->addWidget(m_outputEdit, 1);
}

void OutputPanel::setupConnections()
{
    connect(m_clearBtn, &QToolButton::clicked, this, &OutputPanel::onClearClicked);
    connect(m_stopBtn, &QToolButton::clicked, this, &OutputPanel::stop);
    connect(m_scrollBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_autoScroll = checked;
    });
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OutputPanel::onCategoryChanged);
}

void OutputPanel::clear()
{
    m_outputEdit->clear();
}

void OutputPanel::appendText(const QString& text, bool isError)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    
    if (m_showTimestamps) {
        format.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", format);
    }
    
    format.setForeground(isError ? m_colors["error"] : m_colors["output"]);
    cursor.insertText(text, format);
    
    ensureVisible();
}

void OutputPanel::appendOutput(const QString& text)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(m_colors["output"]);
    
    if (m_showTimestamps) {
        QTextCharFormat timeFormat;
        timeFormat.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", timeFormat);
    }
    
    // Split by newlines and process each line
    QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        if (i > 0) cursor.insertBlock();
        cursor.insertText(lines[i], format);
    }
    
    ensureVisible();
    Q_EMIT outputAvailable(text);
}

void OutputPanel::appendError(const QString& text)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(m_colors["error"]);
    format.setFontWeight(QFont::Bold);
    
    if (m_showTimestamps) {
        QTextCharFormat timeFormat;
        timeFormat.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", timeFormat);
    }
    
    cursor.insertText("ERROR: " + text + "\n", format);
    ensureVisible();
}

void OutputPanel::appendWarning(const QString& text)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(m_colors["warning"]);
    
    if (m_showTimestamps) {
        QTextCharFormat timeFormat;
        timeFormat.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", timeFormat);
    }
    
    cursor.insertText("WARNING: " + text + "\n", format);
    ensureVisible();
}

void OutputPanel::appendInfo(const QString& text)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(m_colors["info"]);
    
    if (m_showTimestamps) {
        QTextCharFormat timeFormat;
        timeFormat.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", timeFormat);
    }
    
    cursor.insertText(text + "\n", format);
    ensureVisible();
}

void OutputPanel::appendSuccess(const QString& text)
{
    QTextCursor cursor(m_outputEdit->document());
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(m_colors["success"]);
    format.setFontWeight(QFont::Bold);
    
    if (m_showTimestamps) {
        QTextCharFormat timeFormat;
        timeFormat.setForeground(m_colors["info"]);
        cursor.insertText("[" + timestamp() + "] ", timeFormat);
    }
    
    cursor.insertText("SUCCESS: " + text + "\n", format);
    ensureVisible();
}

void OutputPanel::stop()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished(2000);
        appendInfo("Process stopped.");
        m_stopBtn->setEnabled(false);
    }
}

void OutputPanel::runScript(const QString& scriptPath)
{
    if (m_process && m_process->state() == QProcess::Running) {
        appendWarning("Another process is already running. Stopping it...");
        stop();
    }
    
    appendInfo(QString("Running: %1").arg(scriptPath));
    
    if (!m_process) {
        m_process = new QProcess(this);
        setupConnections();
    }
    
    m_process->start(scriptPath);
    m_stopBtn->setEnabled(true);
}

void OutputPanel::runCommand(const QString& command, const QStringList& args)
{
    if (m_process && m_process->state() == QProcess::Running) {
        appendWarning("Another process is already running. Stopping it...");
        stop();
    }
    
    appendInfo(QString("Running: %1 %2").arg(command).arg(args.join(' ')));
    
    if (!m_process) {
        m_process = new QProcess(this);
        setupConnections();
    }
    
    m_process->start(command, args);
    m_stopBtn->setEnabled(true);
}

void OutputPanel::onProcessReadyReadStandardOutput()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardOutput();
        appendOutput(QString::fromUtf8(data));
    }
}

void OutputPanel::onProcessReadyReadStandardError()
{
    if (m_process) {
        QByteArray data = m_process->readAllStandardError();
        QString errorText = QString::fromUtf8(data);
        if (!errorText.trimmed().isEmpty()) {
            appendError(errorText);
        }
    }
}

void OutputPanel::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_stopBtn->setEnabled(false);
    
    if (exitStatus == QProcess::NormalExit) {
        if (exitCode == 0) {
            appendInfo(QString("Process finished successfully (exit code: %1)").arg(exitCode));
        } else {
            appendError(QString("Process finished with exit code: %1").arg(exitCode));
        }
    } else {
        appendError("Process crashed or was terminated.");
    }
    
    Q_EMIT processFinished(exitCode, exitStatus);
}

void OutputPanel::onProcessError(QProcess::ProcessError error)
{
    m_stopBtn->setEnabled(false);
    
    switch (error) {
    case QProcess::FailedToStart:
        appendError("Failed to start process. Check if the file exists and is executable.");
        break;
    case QProcess::Crashed:
        appendError("Process crashed.");
        break;
    case QProcess::Timedout:
        appendError("Process timed out.");
        break;
    case QProcess::WriteError:
        appendError("Write error occurred.");
        break;
    case QProcess::ReadError:
        appendError("Read error occurred.");
        break;
    default:
        appendError("Unknown error occurred.");
        break;
    }
}

void OutputPanel::onClearClicked()
{
    clear();
}

void OutputPanel::onCategoryChanged(int index)
{
    // Could filter output by category here
    Q_UNUSED(index);
}

QString OutputPanel::timestamp() const
{
    return QTime::currentTime().toString("HH:mm:ss.zzz");
}

void OutputPanel::ensureVisible()
{
    if (m_autoScroll) {
        QScrollBar* scrollbar = m_outputEdit->verticalScrollBar();
        scrollbar->setValue(scrollbar->maximum());
    }
}

} // namespace Flux
