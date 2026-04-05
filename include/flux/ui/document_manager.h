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

#ifndef FLUX_DOCUMENT_MANAGER_H
#define FLUX_DOCUMENT_MANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QFileSystemWatcher>
#include <memory>
#include <functional>

class QTabWidget;
class QMenu;

namespace Flux {

class FluxDocument;
class FluxEditor;

/**
 * @brief Manages multiple open documents with tabbed interface
 * 
 * Responsibilities:
 * - Open/close files
 * - Track dirty state
 * - File watching for external changes
 * - Recent files list
 * - Session save/restore
 */
class DocumentManager : public QObject {
    Q_OBJECT
    
public:
    explicit DocumentManager(QTabWidget* tabWidget, QObject* parent = nullptr);
    ~DocumentManager() override;

    // ========================================================================
    // Document Operations
    // ========================================================================
    FluxDocument* openDocument(const QString& filePath);
    FluxDocument* openUntitledDocument();
    bool closeDocument(FluxDocument* doc);
    bool closeDocument(int index);
    bool closeAllDocuments();
    
    // ========================================================================
    // Accessors
    // ========================================================================
    FluxDocument* currentDocument() const;
    FluxDocument* documentAt(int index) const;
    FluxDocument* documentForPath(const QString& filePath) const;
    QList<FluxDocument*> openDocuments() const;
    int currentIndex() const;
    int documentCount() const;
    
    // ========================================================================
    // Save Operations
    // ========================================================================
    bool saveDocument(FluxDocument* doc);
    bool saveDocumentAs(FluxDocument* doc, const QString& filePath);
    bool saveAllDocuments();
    bool saveModifiedDocuments();
    
    // ========================================================================
    // Recent Files
    // ========================================================================
    QStringList recentFiles() const;
    void addRecentFile(const QString& filePath);
    void clearRecentFiles();
    int maxRecentFiles() const;
    void setMaxRecentFiles(int max);
    
    // ========================================================================
    // Session Management
    // ========================================================================
    bool saveSession(const QString& sessionPath);
    bool loadSession(const QString& sessionPath);
    void restoreLastSession();
    
    // ========================================================================
    // Editor Factory
    // ========================================================================
    using EditorFactory = std::function<FluxEditor*(QWidget*)>;
    void setEditorFactory(EditorFactory factory);

signals:
    // ========================================================================
    // Document Signals
    // ========================================================================
    void documentOpened(FluxDocument* doc);
    void documentClosed(FluxDocument* doc, int index);
    void currentDocumentChanged(FluxDocument* doc, int index);
    void documentRenamed(FluxDocument* doc, const QString& oldPath);
    
    // ========================================================================
    // State Signals
    // ========================================================================
    void allDocumentsClosed();
    void documentModifiedStateChanged(FluxDocument* doc, bool isModified);
    void recentFilesChanged();

public slots:
    void switchToDocument(int index);
    void switchToNextDocument();
    void switchToPreviousDocument();
    void showInFileSystem(const QString& filePath);

private slots:
    void onFileChanged(const QString& filePath);
    void onDocumentModifiedChanged(bool isModified);
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void onDocumentContentChanged();

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    bool maybeSave(FluxDocument* doc);
    int indexOfDocument(FluxDocument* doc) const;
    void updateTabText(int index, FluxDocument* doc);
    void setupConnections();
    void loadDefaultSession();
    
    // ========================================================================
    // Members
    // ========================================================================
    QTabWidget* m_tabWidget;
    QFileSystemWatcher* m_fileWatcher;
    
    // Document storage
    QMap<int, FluxDocument*> m_documents;  // tab index -> document
    QMap<QString, int> m_pathToIndex;      // file path -> tab index
    
    // Recent files
    QStringList m_recentFiles;
    int m_maxRecentFiles;
    
    // Editor creation
    EditorFactory m_editorFactory;
    
    // Session
    QString m_lastSessionPath;
    
    // Pending external changes
    QMap<QString, bool> m_pendingExternalChanges;  // path -> confirmed
};

/**
 * @brief Represents a single editable document
 * 
 * Wraps the editor with file I/O, compilation, and metadata
 */
class FluxDocument : public QObject {
    Q_OBJECT
    
public:
    explicit FluxDocument(const QString& filePath, QObject* parent = nullptr);
    explicit FluxDocument(QObject* parent = nullptr);  // Untitled document
    ~FluxDocument() override;

    // ========================================================================
    // File Information
    // ========================================================================
    QString filePath() const { return m_filePath; }
    QString fileName() const;
    bool isUntitled() const { return m_filePath.isEmpty(); }
    bool isDirty() const { return m_isDirty; }
    bool isReadOnly() const { return m_isReadOnly; }
    
    // ========================================================================
    // Content Access
    // ========================================================================
    QString content() const;
    void setContent(const QString& content);
    
    // ========================================================================
    // Editor Access
    // ========================================================================
    FluxEditor* editor() const { return m_editor; }
    void setEditor(FluxEditor* editor);
    
    // ========================================================================
    // File Operations
    // ========================================================================
    bool load();
    bool save();
    bool saveAs(const QString& filePath);
    void reload();
    
    // ========================================================================
    // Compiler Integration
    // ========================================================================
    // These would integrate with the FluxScript compiler backend
    // std::unique_ptr<CompilationUnit> compile() const;
    // QList<Diagnostic> diagnostics() const;
    // SymbolTable symbols() const;
    
signals:
    // ========================================================================
    // Document Signals
    // ========================================================================
    void contentChanged();
    void fileNameChanged(const QString& newName);
    void modifiedChanged(bool isModified);
    void loaded();
    void saved();
    void reloadRequested();
    
    // ========================================================================
    // Compiler Signals
    // ========================================================================
    void diagnosticsUpdated();
    void symbolsUpdated();

protected:
    bool event(QEvent* event) override;

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    void setupEditor();
    void setDirty(bool dirty);
    void updateWatcher();
    
    // ========================================================================
    // Members
    // ========================================================================
    QString m_filePath;
    FluxEditor* m_editor;
    bool m_isDirty;
    bool m_isReadOnly;
    bool m_isLoading;
    
    // Cached data
    QString m_cachedContent;
    // SymbolTable m_symbols;
    // QList<Diagnostic> m_diagnostics;
};

} // namespace Flux

#endif // FLUX_DOCUMENT_MANAGER_H
