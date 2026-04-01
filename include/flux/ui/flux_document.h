#ifndef FLUX_DOCUMENT_H
#define FLUX_DOCUMENT_H

#include <QObject>
#include <QString>

namespace Flux {

class FluxEditor;

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
};

} // namespace Flux

#endif // FLUX_DOCUMENT_H
