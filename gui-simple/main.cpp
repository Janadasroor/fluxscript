#include <QApplication>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QCloseEvent>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QSplitter>
#include <QTreeWidget>
#include <QComboBox>
#include <QCompleter>
#include <QAbstractItemView>
#include <QStandardItemModel>
#include <QShortcut>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QFontComboBox>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QTimer>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QTextBlock>
#include <QPushButton>
#include <QLineEdit>
#include <QPixmap>
#include <QWheelEvent>

// ============================================================================
// Syntax Highlighter
// ============================================================================

class FluxScriptHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    
public:
    explicit FluxScriptHighlighter(QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        setupFormats();
        setupRules();
    }
    
protected:
    void highlightBlock(const QString& text) override {
        for (const auto& rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }
    
private:
    void setupFormats() {
        m_keywordFormat.setForeground(QColor("#569cd6"));
        m_keywordFormat.setFontWeight(QFont::Bold);
        m_builtinFormat.setForeground(QColor("#4ec9b0"));
        m_numberFormat.setForeground(QColor("#b5cea8"));
        m_stringFormat.setForeground(QColor("#ce9178"));
        m_commentFormat.setForeground(QColor("#6a9955"));
        m_commentFormat.setFontItalic(true);
        m_functionFormat.setForeground(QColor("#dcdcaa"));
        m_operatorFormat.setForeground(QColor("#d4d4d4"));
    }
    
    void setupRules() {
        QStringList keywords = {"def", "function", "if", "else", "elif", "for", "while",
            "break", "continue", "return", "var", "let", "const", "class", "import"};
        for (const QString& kw : keywords) {
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), m_keywordFormat});
        }
        
        QStringList builtins = {"sin", "cos", "tan", "exp", "log", "sqrt", "abs",
            "floor", "ceil", "round", "print", "len", "range", "min", "max", "sum"};
        for (const QString& func : builtins) {
            m_rules.append({QRegularExpression("\\b" + func + "\\b"), m_builtinFormat});
        }
        
        m_rules.append({QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"), m_functionFormat});
        m_rules.append({QRegularExpression("\\b[0-9]+\\.?[0-9]*\\b"), m_numberFormat});
        m_rules.append({QRegularExpression("\"[^\"]*\""), m_stringFormat});
        m_rules.append({QRegularExpression("#[^\\n]*"), m_commentFormat});
    }
    
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    QVector<HighlightRule> m_rules;
    QTextCharFormat m_keywordFormat, m_builtinFormat, m_numberFormat;
    QTextCharFormat m_stringFormat, m_commentFormat, m_functionFormat, m_operatorFormat;
};

// ============================================================================
// File Browser
// ============================================================================

class FileBrowser : public QTreeWidget {
    Q_OBJECT
    
public:
    explicit FileBrowser(QWidget* parent = nullptr) : QTreeWidget(parent) {
        setHeaderHidden(true);
        setIconSize(QSize(20, 20));
        setStyleSheet("QTreeWidget { background: #252526; color: #cccccc; border: none; }");
        setupIcons();
        refresh(QDir::homePath());
    }
    
    void setupIcons() {
        // Map file extensions to icon names
        m_iconMap = {
            {"flux", "text-x-script"},
            {"py", "text-x-python"},
            {"cpp", "text-x-c++src"},
            {"c", "text-x-csrc"},
            {"h", "text-x-chdr"},
            {"hpp", "text-x-c++hdr"},
            {"java", "text-x-java"},
            {"js", "text-x-javascript"},
            {"ts", "text-x-typescript"},
            {"html", "text-html"},
            {"css", "text-css"},
            {"json", "application-json"},
            {"xml", "application-xml"},
            {"yaml", "text-x-yaml"},
            {"yml", "text-x-yaml"},
            {"md", "text-x-markdown"},
            {"txt", "text-plain"},
            {"sh", "text-x-script"},
            {"bash", "text-x-script"},
            {"rs", "text-x-rust"},
            {"go", "text-x-go"},
            {"rb", "text-x-ruby"},
            {"php", "text-x-php"},
            {"sql", "application-sql"},
            {"svg", "image-svg+xml"},
            {"png", "image-x-png"},
            {"jpg", "image-x-jpg"},
            {"jpeg", "image-x-jpg"},
            {"gif", "image-x-gif"},
            {"pdf", "application-pdf"},
            {"zip", "application-x-zip"},
            {"tar", "application-x-tar"},
            {"gz", "application-x-gzip"},
            {"exe", "application-x-executable"},
            {"bin", "application-x-executable"},
        };
    }
    
    QIcon getIconForFile(const QFileInfo& fileInfo) {
        QString suffix = fileInfo.suffix().toLower();
        QString baseName = fileInfo.baseName().toLower();
        
        // Check for specific file types
        if (baseName == "cmakelists" || baseName == "makefile") {
            return QIcon::fromTheme("text-x-makefile", getColoredIcon("#569cd6", "📋"));
        }
        
        // Check extension map
        if (m_iconMap.contains(suffix)) {
            QString iconName = m_iconMap[suffix];
            QIcon icon = QIcon::fromTheme(iconName);
            if (!icon.isNull()) return icon;
            // Fallback to colored emoji icon
            return getColoredIcon(getColorForExtension(suffix), getEmojiForExtension(suffix));
        }
        
        // Default file icon
        if (fileInfo.isExecutable()) {
            return QIcon::fromTheme("application-x-executable", getColoredIcon("#4ec9b0", "⚙️"));
        }
        
        return QIcon::fromTheme("text-x-generic", getColoredIcon("#cccccc", "📄"));
    }
    
    QIcon getColoredIcon(const QString& color, const QString& emoji) {
        // Create a simple colored icon with emoji
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, emoji);
        return QIcon(pixmap);
    }
    
    QString getColorForExtension(const QString& ext) {
        static QMap<QString, QString> colorMap = {
            {"flux", "#569cd6"}, {"py", "#3776ab"}, {"cpp", "#00599C"},
            {"c", "#555555"}, {"h", "#555555"}, {"java", "#b07219"},
            {"js", "#f7df1e"}, {"ts", "#3178c6"}, {"html", "#e34c26"},
            {"css", "#563d7c"}, {"json", "#cbcb41"}, {"xml", "#e34c26"},
            {"md", "#083fa1"}, {"txt", "#cccccc"}, {"sh", "#89e051"},
            {"rs", "#dea584"}, {"go", "#00add8"}, {"rb", "#701516"},
            {"php", "#4f5d95"}, {"sql", "#e38c00"}, {"pdf", "#f40f02"},
            {"zip", "#ffd700"}, {"exe", "#4ec9b0"},
        };
        return colorMap.value(ext, "#cccccc");
    }
    
    QString getEmojiForExtension(const QString& ext) {
        static QMap<QString, QString> emojiMap = {
            {"flux", "⚡"}, {"py", "🐍"}, {"cpp", "⚙️"}, {"c", "©️"},
            {"h", "📐"}, {"java", "☕"}, {"js", "📜"}, {"ts", "📘"},
            {"html", "🌐"}, {"css", "🎨"}, {"json", "📦"}, {"xml", "📑"},
            {"yaml", "📝"}, {"md", "📖"}, {"txt", "📄"}, {"sh", "💻"},
            {"rs", "🦀"}, {"go", "🐹"}, {"rb", "💎"}, {"php", "🐘"},
            {"sql", "🗄️"}, {"svg", "🖼️"}, {"png", "🖼️"}, {"jpg", "📷"},
            {"pdf", "📕"}, {"zip", "📦"}, {"exe", "⚙️"},
        };
        return emojiMap.value(ext, "📄");
    }
    
signals:
    void fileDoubleClicked(const QString& path);
    
private slots:
    void refresh(const QString& rootPath) {
        clear();
        QDir dir(rootPath);
        
        QTreeWidgetItem* rootItem = new QTreeWidgetItem(this);
        rootItem->setText(0, dir.dirName());
        rootItem->setData(0, Qt::UserRole, rootPath);
        rootItem->setIcon(0, QIcon::fromTheme("folder", getColoredIcon("#dcb67a", "📁")));
        
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::DirsFirst);
        
        for (const QFileInfo& entry : entries) {
            QTreeWidgetItem* item = new QTreeWidgetItem(rootItem);
            item->setText(0, entry.fileName());
            item->setData(0, Qt::UserRole, entry.absoluteFilePath());
            
            if (entry.isDir()) {
                item->setIcon(0, QIcon::fromTheme("folder", getColoredIcon("#dcb67a", "📁")));
            } else {
                item->setIcon(0, getIconForFile(entry));
            }
        }
        rootItem->setExpanded(true);
    }
    
private:
    QMap<QString, QString> m_iconMap;
};

// ============================================================================
// Settings Dialog
// ============================================================================

class SettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Editor Settings");
        auto* layout = new QFormLayout(this);
        
        m_fontCombo = new QFontComboBox();
        layout->addRow("Font:", m_fontCombo);
        
        m_fontSizeSpin = new QSpinBox();
        m_fontSizeSpin->setRange(8, 72);
        m_fontSizeSpin->setValue(12);
        layout->addRow("Font Size", m_fontSizeSpin);
        
        m_tabWidthSpin = new QSpinBox();
        m_tabWidthSpin->setRange(1, 8);
        m_tabWidthSpin->setValue(4);
        layout->addRow("Tab Width", m_tabWidthSpin);
        
        m_lineNumbersCheck = new QCheckBox("Show line numbers");
        m_lineNumbersCheck->setChecked(true);
        layout->addRow(m_lineNumbersCheck);
        
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addRow(buttons);
    }
    
    QFont font() const { return m_fontCombo->currentFont(); }
    int tabWidth() const { return m_tabWidthSpin->value(); }
    bool showLineNumbers() const { return m_lineNumbersCheck->isChecked(); }
    
private:
    QFontComboBox* m_fontCombo;
    QSpinBox* m_fontSizeSpin;
    QSpinBox* m_tabWidthSpin;
    QCheckBox* m_lineNumbersCheck;
};

// ============================================================================
// Search Dialog
// ============================================================================

class SearchDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SearchDialog(QPlainTextEdit* editor, QWidget* parent = nullptr)
        : QDialog(parent), m_editor(editor) {
        setWindowTitle("Find");
        auto* layout = new QVBoxLayout(this);
        
        auto* findLayout = new QHBoxLayout();
        findLayout->addWidget(new QLabel("Find:"));
        m_findEdit = new QLineEdit();
        findLayout->addWidget(m_findEdit);
        layout->addLayout(findLayout);
        
        auto* buttonLayout = new QHBoxLayout();
        QPushButton* findBtn = new QPushButton("Find Next");
        connect(findBtn, &QPushButton::clicked, this, &SearchDialog::findNext);
        buttonLayout->addWidget(findBtn);
        
        QPushButton* closeBtn = new QPushButton("Close");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        buttonLayout->addWidget(closeBtn);
        
        layout->addLayout(buttonLayout);
    }
    
private slots:
    void findNext() {
        QString text = m_findEdit->text();
        if (!text.isEmpty()) m_editor->find(text);
    }
    
private:
    QPlainTextEdit* m_editor;
    QLineEdit* m_findEdit;
};

// ============================================================================
// Main Editor Window
// ============================================================================

class EditorWindow : public QMainWindow {
    Q_OBJECT
    
public:
    EditorWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("🚀 FluxScript Editor - Qt " + QString::fromStdString(QT_VERSION_STR));
        setMinimumSize(1400, 900);
        
        setupUI();
        setupMenuBar();
        setupToolBar();
        setupStatusBar();
        setupConnections();
        
        // Welcome message
        m_editor->setPlainText(
            "# ════════════════════════════════════════════════════════\n"
            "#  FluxScript Editor - Built with Qt " + QString::fromStdString(QT_VERSION_STR) + "\n"
            "# ════════════════════════════════════════════════════════\n\n"
            "def main():\n"
            "    x = 10\n"
            "    y = 20\n"
            "    return x * y + 5\n\n"
            "# Press F5 to run!\n"
        );
        
        m_currentFile = "untitled.flux";
        updateTitle();
        m_outputConsole->appendPlainText("✓ FluxScript Editor ready (Qt " + QString::fromStdString(QT_VERSION_STR) + ")");
    }
    
protected:
    void closeEvent(QCloseEvent* event) override {
        if (maybeSave()) { event->accept(); } else { event->ignore(); }
    }

private slots:
    void onNew() {
        if (maybeSave()) {
            m_editor->clear();
            m_currentFile = "untitled.flux";
            updateTitle();
        }
    }
    
    void onOpen() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open", "", "FluxScript Files (*.flux)");
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_editor->setPlainText(file.readAll());
                m_currentFile = filePath;
                updateTitle();
            }
        }
    }
    
    void onSave() {
        if (m_currentFile == "untitled.flux") onSaveAs();
        else doSave();
    }
    
    void onSaveAs() {
        QString filePath = QFileDialog::getSaveFileName(this, "Save", "", "FluxScript Files (*.flux)");
        if (!filePath.isEmpty()) { m_currentFile = filePath; doSave(); }
    }
    
    void onRun() {
        m_outputConsole->appendPlainText("\n╔═══════════════════════════════════════════════════════╗");
        m_outputConsole->appendPlainText("║  Running: " + m_currentFile.leftJustified(44) + "║");
        
        if (m_currentFile != "untitled.flux") doSave();
        
        if (m_process) m_process->deleteLater();
        m_process = new QProcess(this);
        
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            m_outputConsole->appendPlainText(QString::fromUtf8(m_process->readAllStandardOutput()));
        });
        
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) m_outputConsole->appendPlainText("✓ Success");
            else m_outputConsole->appendPlainText("✗ Failed (code: " + QString::number(exitCode) + ")");
            m_process->deleteLater();
        });
        
        m_process->start("./build/flux-minimal", {m_currentFile});
    }
    
    void onAbout() {
        QMessageBox::about(this, "About", "<h2>🚀 FluxScript Editor</h2><p>Qt " + 
            QString::fromStdString(QT_VERSION_STR) + " + LLVM 18</p>");
    }
    
    void onTextChanged() { updateTitle(); }
    
    void onCursorChanged() {
        QTextCursor cursor = m_editor->textCursor();
        m_positionLabel->setText(QString("Line: %1, Col: %2").arg(cursor.blockNumber()+1).arg(cursor.columnNumber()+1));
    }
    
    void onZoomIn() {
        QFont font = m_editor->font();
        int size = font.pointSize();
        if (size <= 0) size = 12;
        font.setPointSize(size + 2);
        m_editor->setFont(font);
        m_zoomLabel->setText(QString("Zoom: %1%").arg(font.pointSize() * 100 / 12));
    }
    
    void onZoomOut() {
        QFont font = m_editor->font();
        int size = font.pointSize();
        if (size <= 0) size = 12;
        if (size > 8) {
            font.setPointSize(size - 2);
            m_editor->setFont(font);
            m_zoomLabel->setText(QString("Zoom: %1%").arg(font.pointSize() * 100 / 12));
        }
    }
    
    void onZoomReset() {
        QFont font = m_editor->font();
        font.setPointSize(12);
        m_editor->setFont(font);
        m_zoomLabel->setText("Zoom: 100%");
    }
    
    void onFileDoubleClicked(const QString& path) {
        QFileInfo info(path);
        if (info.isFile() && info.suffix() == "flux") {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_editor->setPlainText(file.readAll());
                m_currentFile = path;
                updateTitle();
            }
        }
    }

private:
    void setupUI() {
        auto* centralWidget = new QWidget(this);
        auto* mainLayout = new QHBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        
        auto* leftSplitter = new QSplitter(Qt::Horizontal);
        
        m_fileBrowser = new FileBrowser();
        m_fileBrowser->setMaximumWidth(250);
        connect(m_fileBrowser, &FileBrowser::fileDoubleClicked, this, &EditorWindow::onFileDoubleClicked);
        leftSplitter->addWidget(m_fileBrowser);
        
        auto* rightSplitter = new QSplitter(Qt::Vertical);
        
        m_editor = new QPlainTextEdit();
        m_editor->setFont(QFont("Consolas", 12));
        m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_editor->setStyleSheet("QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; selection-background-color: #264f78; }");
        
        new FluxScriptHighlighter(m_editor->document());
        rightSplitter->addWidget(m_editor);
        
        m_outputConsole = new QPlainTextEdit();
        m_outputConsole->setMaximumHeight(200);
        m_outputConsole->setReadOnly(true);
        m_outputConsole->setStyleSheet("QPlainTextEdit { background: #1a1a1a; color: #cccccc; }");
        rightSplitter->addWidget(m_outputConsole);
        
        leftSplitter->addWidget(rightSplitter);
        leftSplitter->setStretchFactor(1, 1);
        mainLayout->addWidget(leftSplitter);
        setCentralWidget(centralWidget);
    }
    
    void setupMenuBar() {
        auto* menuBar = this->menuBar();
        
        auto* fileMenu = menuBar->addMenu("&File");
        fileMenu->addAction("&New", this, &EditorWindow::onNew, QKeySequence::New);
        fileMenu->addAction("&Open...", this, &EditorWindow::onOpen, QKeySequence::Open);
        fileMenu->addAction("&Save", this, &EditorWindow::onSave, QKeySequence::Save);
        fileMenu->addSeparator();
        fileMenu->addAction("E&xit", this, &QMainWindow::close);
        
        auto* editMenu = menuBar->addMenu("&Edit");
        editMenu->addAction("&Find...", this, [this]() {
            if (!m_searchDialog) m_searchDialog = new SearchDialog(m_editor, this);
            m_searchDialog->show();
        });
        editMenu->addAction("Find", QKeySequence::Find, this, [this]() {
            if (!m_searchDialog) m_searchDialog = new SearchDialog(m_editor, this);
            m_searchDialog->show();
        });
        editMenu->addSeparator();
        
        auto* zoomMenu = editMenu->addMenu("Zoom");
        zoomMenu->addAction("Zoom &In", this, &EditorWindow::onZoomIn, QKeySequence::ZoomIn);
        zoomMenu->addAction("Zoom &Out", this, &EditorWindow::onZoomOut, QKeySequence::ZoomOut);
        zoomMenu->addAction("&Reset Zoom", this, &EditorWindow::onZoomReset, QKeySequence(Qt::CTRL | Qt::Key_0));
        
        auto* runMenu = menuBar->addMenu("&Run");
        runMenu->addAction("&Run", this, &EditorWindow::onRun);
        new QShortcut(QKeySequence(Qt::Key_F5), this, this, &EditorWindow::onRun);
        
        auto* toolsMenu = menuBar->addMenu("&Tools");
        toolsMenu->addAction("Settings...", this, [this]() {
            SettingsDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                m_editor->setFont(dlg.font());
                m_editor->setTabStopDistance(QFontMetrics(m_editor->font()).horizontalAdvance(' ') * dlg.tabWidth());
            }
        });
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, [this]() {
            SettingsDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                m_editor->setFont(dlg.font());
                m_editor->setTabStopDistance(QFontMetrics(m_editor->font()).horizontalAdvance(' ') * dlg.tabWidth());
            }
        });
        
        auto* helpMenu = menuBar->addMenu("&Help");
        helpMenu->addAction("&About", this, &EditorWindow::onAbout);
    }
    
    void setupToolBar() {
        auto* toolBar = addToolBar("Main");
        toolBar->setObjectName("MainToolBar");
        toolBar->addAction("📄 New", this, &EditorWindow::onNew);
        toolBar->addAction("📂 Open", this, &EditorWindow::onOpen);
        toolBar->addAction("💾 Save", this, &EditorWindow::onSave);
        toolBar->addSeparator();
        toolBar->addAction("▶️ Run", this, &EditorWindow::onRun);
    }
    
    void setupStatusBar() {
        m_positionLabel = new QLabel("Line: 1, Col: 1");
        statusBar()->addPermanentWidget(m_positionLabel);
        
        m_zoomLabel = new QLabel("Zoom: 100%");
        statusBar()->addPermanentWidget(m_zoomLabel);
    }
    
    void setupConnections() {
        connect(m_editor, &QPlainTextEdit::textChanged, this, &EditorWindow::onTextChanged);
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &EditorWindow::onCursorChanged);
        
        // Install event filter for mouse wheel zoom
        m_editor->installEventFilter(this);
    }
    
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_editor && event->type() == QEvent::Wheel) {
            QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
            
            // Check if Ctrl is pressed
            if (wheelEvent->modifiers() & Qt::ControlModifier) {
                int delta = wheelEvent->angleDelta().y();
                
                if (delta > 0) {
                    onZoomIn();
                } else {
                    onZoomOut();
                }
                
                return true;  // Consume the event
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }
    
    void updateTitle() {
        QString title = "🚀 FluxScript Editor - " + QFileInfo(m_currentFile).fileName();
        if (m_editor->document()->isModified()) title += " [*]";
        setWindowTitle(title);
    }
    
    bool maybeSave() {
        if (!m_editor->document()->isModified()) return true;
        auto ret = QMessageBox::warning(this, "Unsaved", "Save changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Save) { onSave(); return true; }
        return ret != QMessageBox::Cancel;
    }
    
    void doSave() {
        QFile file(m_currentFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_editor->toPlainText().toUtf8());
            m_editor->document()->setModified(false);
            updateTitle();
        }
    }
    
    QPlainTextEdit* m_editor;
    QPlainTextEdit* m_outputConsole;
    FileBrowser* m_fileBrowser;
    QLabel* m_positionLabel;
    QLabel* m_zoomLabel;
    QString m_currentFile;
    QProcess* m_process = nullptr;
    SearchDialog* m_searchDialog = nullptr;
};

#include "main.moc"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FluxScript Editor");
    
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#1e1e1e"));
    darkPalette.setColor(QPalette::WindowText, QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Base, QColor("#252526"));
    darkPalette.setColor(QPalette::Text, QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Highlight, QColor("#264f78"));
    app.setPalette(darkPalette);
    app.setStyle("Fusion");
    
    EditorWindow editor;
    editor.show();
    
    return app.exec();
}
