#include "flux/ui/document_manager.h"
#include "flux/ui/flux_editor.h"

#include <QTabWidget>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>

namespace Flux {

// ============================================================================
// FluxDocument Implementation
// ============================================================================

FluxDocument::FluxDocument(const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_editor(nullptr)
    , m_isDirty(false)
    , m_isReadOnly(false)
    , m_isLoading(false)
{
    setupEditor();
    load();
}

FluxDocument::FluxDocument(QObject* parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_isDirty(false)
    , m_isReadOnly(false)
    , m_isLoading(false)
{
    setupEditor();
}

FluxDocument::~FluxDocument()
{
}

void FluxDocument::setupEditor()
{
    // Editor is created externally and set via setEditor()
}

QString FluxDocument::fileName() const
{
    if (m_filePath.isEmpty()) {
        return "Untitled";
    }
    return QFileInfo(m_filePath).fileName();
}

QString FluxDocument::content() const
{
    if (m_editor) {
        return m_editor->toPlainText();
    }
    return m_cachedContent;
}

void FluxDocument::setContent(const QString& content)
{
    if (m_editor) {
        m_editor->setPlainText(content);
    } else {
        m_cachedContent = content;
    }
    m_isDirty = false;
}

void FluxDocument::setEditor(FluxEditor* editor)
{
    m_editor = editor;
    if (m_editor && !m_cachedContent.isEmpty()) {
        m_editor->setPlainText(m_cachedContent);
    }

    // Connect editor signals
    if (m_editor) {
        connect(m_editor, &QPlainTextEdit::modificationChanged,
                this, [this](bool changed) {
                    setDirty(changed);
                });
        connect(m_editor, &QPlainTextEdit::textChanged,
                this, &FluxDocument::contentChanged);
    }
}

bool FluxDocument::load()
{
    if (m_filePath.isEmpty()) {
        return true;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    m_isLoading = true;
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    setContent(content);
    m_isLoading = false;
    m_isDirty = false;

    // Detect if file is read-only
    QFileInfo fileInfo(m_filePath);
    m_isReadOnly = !fileInfo.isWritable();

    emit loaded();
    return true;
}

bool FluxDocument::save()
{
    if (m_filePath.isEmpty()) {
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << content();
    file.flush();
    file.close();

    m_isDirty = false;
    emit saved();
    return true;
}

bool FluxDocument::saveAs(const QString& filePath)
{
    m_filePath = filePath;
    return save();
}

void FluxDocument::reload()
{
    if (m_isDirty) {
        QMessageBox::StandardButton result = QMessageBox::question(
            nullptr,
            "Reload Document",
            "This document has unsaved changes. Reload anyway?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    load();
    emit reloadRequested();
}

void FluxDocument::setDirty(bool dirty)
{
    if (m_isDirty != dirty && !m_isLoading) {
        m_isDirty = dirty;
        emit modifiedChanged(dirty);
    }
}

bool FluxDocument::event(QEvent* event)
{
    return QObject::event(event);
}

// ============================================================================
// DocumentManager Implementation
// ============================================================================

DocumentManager::DocumentManager(QTabWidget* tabWidget, QObject* parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_fileWatcher(nullptr)
    , m_maxRecentFiles(10)
    , m_editorFactory(nullptr)
{
    m_fileWatcher = new QFileSystemWatcher(this);

    setupConnections();
    loadDefaultSession();
}

DocumentManager::~DocumentManager()
{
    closeAllDocuments();
}

void DocumentManager::setupConnections()
{
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &DocumentManager::onCurrentTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &DocumentManager::onTabCloseRequested);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &DocumentManager::onFileChanged);
}

FluxDocument* DocumentManager::openDocument(const QString& filePath)
{
    // Check if already open
    if (m_pathToIndex.contains(filePath)) {
        int index = m_pathToIndex[filePath];
        m_tabWidget->setCurrentIndex(index);
        return m_documents.value(index);
    }

    // Create new document
    auto* doc = new FluxDocument(filePath, this);
    if (!doc->load()) {
        delete doc;
        return nullptr;
    }

    // Create editor widget
    FluxEditor* editor = nullptr;
    if (m_editorFactory) {
        editor = m_editorFactory(m_tabWidget);
    } else {
        editor = new FluxEditor(m_tabWidget);
    }

    doc->setEditor(editor);

    // Add to tab widget
    int index = m_tabWidget->addTab(editor, doc->fileName());
    m_documents[index] = doc;
    m_pathToIndex[filePath] = index;

    updateTabText(index, doc);

    // Add to file watcher
    m_fileWatcher->addPath(filePath);

    addRecentFile(filePath);

    emit documentOpened(doc);

    return doc;
}

FluxDocument* DocumentManager::openUntitledDocument()
{
    auto* doc = new FluxDocument(this);

    // Create editor widget
    FluxEditor* editor = nullptr;
    if (m_editorFactory) {
        editor = m_editorFactory(m_tabWidget);
    } else {
        editor = new FluxEditor(m_tabWidget);
    }

    doc->setEditor(editor);

    // Add to tab widget
    int index = m_tabWidget->addTab(editor, "Untitled");
    m_documents[index] = doc;

    updateTabText(index, doc);

    emit documentOpened(doc);
    return doc;
}

bool DocumentManager::closeDocument(FluxDocument* doc)
{
    if (!doc) {
        return false;
    }

    int index = indexOfDocument(doc);
    if (index < 0) {
        return false;
    }

    return closeDocument(index);
}

bool DocumentManager::closeDocument(int index)
{
    if (index < 0 || index >= m_tabWidget->count()) {
        return false;
    }

    FluxDocument* doc = m_documents.value(index);
    if (!doc) {
        return false;
    }

    // Check for unsaved changes
    if (!maybeSave(doc)) {
        return false;
    }

    // Remove from file watcher
    if (!doc->isUntitled()) {
        m_fileWatcher->removePath(doc->filePath());
    }

    // Remove from maps
    m_pathToIndex.remove(doc->filePath());
    m_documents.remove(index);

    // Close tab
    m_tabWidget->removeTab(index);

    // Delete document
    emit documentClosed(doc, index);
    doc->deleteLater();

    // Check if all documents closed
    if (m_documents.isEmpty()) {
        emit allDocumentsClosed();
    }

    return true;
}

bool DocumentManager::closeAllDocuments()
{
    // Save all modified
    if (!saveModifiedDocuments()) {
        return false;
    }

    // Close all tabs
    while (m_tabWidget->count() > 0) {
        closeDocument(0);
    }

    return true;
}

FluxDocument* DocumentManager::currentDocument() const
{
    int index = m_tabWidget->currentIndex();
    return m_documents.value(index);
}

FluxDocument* DocumentManager::documentAt(int index) const
{
    return m_documents.value(index);
}

FluxDocument* DocumentManager::documentForPath(const QString& filePath) const
{
    int index = m_pathToIndex.value(filePath, -1);
    return m_documents.value(index);
}

QList<FluxDocument*> DocumentManager::openDocuments() const
{
    return m_documents.values();
}

int DocumentManager::currentIndex() const
{
    return m_tabWidget->currentIndex();
}

int DocumentManager::documentCount() const
{
    return m_tabWidget->count();
}

bool DocumentManager::saveDocument(FluxDocument* doc)
{
    if (!doc) {
        return false;
    }

    if (doc->isUntitled()) {
        return saveDocumentAs(doc, QString());
    }

    return doc->save();
}

bool DocumentManager::saveDocumentAs(FluxDocument* doc, const QString& filePath)
{
    if (!doc) {
        return false;
    }

    QString newPath = filePath;
    if (newPath.isEmpty()) {
        // Show save dialog (would be implemented in GUI)
        return false;
    }

    if (doc->saveAs(newPath)) {
        // Update path mapping
        QString oldPath = doc->filePath();
        int index = indexOfDocument(doc);

        m_pathToIndex.remove(oldPath);
        m_pathToIndex[newPath] = index;

        // Update file watcher
        if (!oldPath.isEmpty()) {
            m_fileWatcher->removePath(oldPath);
        }
        m_fileWatcher->addPath(newPath);

        updateTabText(index, doc);
        addRecentFile(newPath);

        emit documentRenamed(doc, oldPath);
        return true;
    }

    return false;
}

bool DocumentManager::saveAllDocuments()
{
    bool success = true;
    for (auto* doc : m_documents) {
        if (doc->isDirty()) {
            if (!saveDocument(doc)) {
                success = false;
            }
        }
    }
    return success;
}

bool DocumentManager::saveModifiedDocuments()
{
    for (auto* doc : m_documents) {
        if (doc->isDirty()) {
            if (!maybeSave(doc)) {
                return false;
            }
        }
    }
    return true;
}

QStringList DocumentManager::recentFiles() const
{
    return m_recentFiles;
}

void DocumentManager::addRecentFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    // Remove if already in list
    m_recentFiles.removeAll(filePath);

    // Add to front
    m_recentFiles.prepend(filePath);

    // Trim to max
    while (m_recentFiles.size() > m_maxRecentFiles) {
        m_recentFiles.removeLast();
    }

    emit recentFilesChanged();
}

void DocumentManager::clearRecentFiles()
{
    m_recentFiles.clear();
    emit recentFilesChanged();
}

int DocumentManager::maxRecentFiles() const
{
    return m_maxRecentFiles;
}

void DocumentManager::setMaxRecentFiles(int max)
{
    m_maxRecentFiles = max;

    while (m_recentFiles.size() > max) {
        m_recentFiles.removeLast();
    }

    emit recentFilesChanged();
}

bool DocumentManager::saveSession(const QString& sessionPath)
{
    QSettings settings(sessionPath, QSettings::IniFormat);
    settings.beginGroup("Session");

    // Save open files
    QStringList files;
    for (auto* doc : m_documents) {
        if (!doc->isUntitled()) {
            files.append(doc->filePath());
        }
    }
    settings.setValue("openFiles", files);

    // Save current tab
    settings.setValue("currentIndex", currentIndex());

    // Save recent files
    settings.setValue("recentFiles", m_recentFiles);

    settings.endGroup();
    return true;
}

bool DocumentManager::loadSession(const QString& sessionPath)
{
    QSettings settings(sessionPath, QSettings::IniFormat);
    settings.beginGroup("Session");

    // Load recent files
    m_recentFiles = settings.value("recentFiles", QStringList()).toStringList();
    emit recentFilesChanged();

    // Load open files
    QStringList files = settings.value("openFiles", QStringList()).toStringList();
    for (const QString& file : files) {
        if (QFile::exists(file)) {
            openDocument(file);
        }
    }

    // Restore current tab
    int currentIndex = settings.value("currentIndex", 0).toInt();
    if (currentIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(currentIndex);
    }

    settings.endGroup();

    m_lastSessionPath = sessionPath;
    return true;
}

void DocumentManager::restoreLastSession()
{
    if (!m_lastSessionPath.isEmpty()) {
        loadSession(m_lastSessionPath);
    }
}

void DocumentManager::setEditorFactory(EditorFactory factory)
{
    m_editorFactory = factory;
}

void DocumentManager::switchToDocument(int index)
{
    m_tabWidget->setCurrentIndex(index);
}

void DocumentManager::switchToNextDocument()
{
    int currentIndex = m_tabWidget->currentIndex();
    int nextIndex = (currentIndex + 1) % m_tabWidget->count();
    m_tabWidget->setCurrentIndex(nextIndex);
}

void DocumentManager::switchToPreviousDocument()
{
    int currentIndex = m_tabWidget->currentIndex();
    int prevIndex = (currentIndex - 1 + m_tabWidget->count()) % m_tabWidget->count();
    m_tabWidget->setCurrentIndex(prevIndex);
}

void DocumentManager::showInFileSystem(const QString& filePath)
{
    // Would emit signal for file browser to show file
    Q_UNUSED(filePath);
}

void DocumentManager::onFileChanged(const QString& filePath)
{
    FluxDocument* doc = documentForPath(filePath);
    if (!doc) {
        return;
    }

    // Check if file was modified externally
    QFileInfo fileInfo(filePath);
    QDateTime fileTime = fileInfo.lastModified();

    if (fileTime > fileInfo.birthTime()) {
        // File was modified, ask user
        m_pendingExternalChanges[filePath] = false;

        QMessageBox::StandardButton result = QMessageBox::question(
            nullptr,
            "File Changed",
            QString("The file '%1' was modified externally. Reload?").arg(filePath),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );

        if (result == QMessageBox::Yes) {
            doc->reload();
            m_pendingExternalChanges[filePath] = true;
        } else if (result == QMessageBox::Cancel) {
            // Re-add to watcher
            m_fileWatcher->addPath(filePath);
        }
    }
}

void DocumentManager::onDocumentModifiedChanged(bool isModified)
{
    auto* doc = qobject_cast<FluxDocument*>(sender());
    if (doc) {
        emit documentModifiedStateChanged(doc, isModified);

        // Update tab text
        int index = indexOfDocument(doc);
        if (index >= 0) {
            updateTabText(index, doc);
        }
    }
}

void DocumentManager::onCurrentTabChanged(int index)
{
    FluxDocument* doc = m_documents.value(index);
    emit currentDocumentChanged(doc, index);
}

void DocumentManager::onTabCloseRequested(int index)
{
    closeDocument(index);
}

void DocumentManager::onDocumentContentChanged()
{
    auto* doc = qobject_cast<FluxDocument*>(sender());
    if (doc) {
        emit documentModifiedStateChanged(doc, doc->isDirty());
    }
}

bool DocumentManager::maybeSave(FluxDocument* doc)
{
    if (!doc->isDirty()) {
        return true;
    }

    QMessageBox::StandardButton result = QMessageBox::question(
        nullptr,
        "Save Document",
        QString("Save changes to '%1'?").arg(doc->fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    switch (result) {
    case QMessageBox::Save:
        return saveDocument(doc);
    case QMessageBox::Discard:
        return true;
    case QMessageBox::Cancel:
        return false;
    default:
        return false;
    }
}

int DocumentManager::indexOfDocument(FluxDocument* doc) const
{
    return m_documents.key(doc, -1);
}

void DocumentManager::updateTabText(int index, FluxDocument* doc)
{
    if (!doc) {
        return;
    }

    QString text = doc->fileName();
    if (doc->isDirty()) {
        text += " (*)";
    }
    if (doc->isReadOnly()) {
        text += " (Read-only)";
    }

    m_tabWidget->setTabText(index, text);
    m_tabWidget->setTabToolTip(index, doc->filePath());
}

void DocumentManager::loadDefaultSession()
{
    QString defaultSessionPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/session.ini";

    QSettings settings(defaultSessionPath, QSettings::IniFormat);
    if (settings.contains("Session/restoreLastSession")) {
        bool restore = settings.value("Session/restoreLastSession", false).toBool();
        if (restore) {
            restoreLastSession();
        }
    }
}

} // namespace Flux
