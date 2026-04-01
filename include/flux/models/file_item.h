#ifndef FLUX_MODELS_FILE_ITEM_H
#define FLUX_MODELS_FILE_ITEM_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QIcon>
#include <QFileInfo>

namespace Flux::Models {

/**
 * @brief File types for icon and filtering
 */
enum class FileType {
    Unknown,
    Folder,
    File,
    FluxScript,      // .flux files
    Header,          // .h, .hpp files
    Source,          // .cpp, .c files
    Text,            // .txt, .md files
    Config,          // .json, .xml, .yaml files
    Archive,         // .zip, .tar, .gz files
    Executable       // .exe, .sh, .bat files
};

/**
 * @brief File information wrapper with additional metadata
 */
class FileItem : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString extension READ extension CONSTANT)
    Q_PROPERTY(qint64 size READ size CONSTANT)
    Q_PROPERTY(QDateTime created READ created CONSTANT)
    Q_PROPERTY(QDateTime modified READ modified CONSTANT)
    Q_PROPERTY(bool isFolder READ isFolder CONSTANT)
    Q_PROPERTY(bool isFile READ isFile CONSTANT)
    Q_PROPERTY(bool isHidden READ isHidden CONSTANT)
    Q_PROPERTY(bool isReadOnly READ isReadOnly CONSTANT)
    Q_PROPERTY(FileType fileType READ fileType CONSTANT)
    Q_PROPERTY(QString iconKey READ iconKey CONSTANT)
    
public:
    explicit FileItem(const QString& path, QObject* parent = nullptr);
    explicit FileItem(const QFileInfo& fileInfo, QObject* parent = nullptr);
    ~FileItem() override;

    // ========================================================================
    // File Information
    // ========================================================================
    
    QString path() const { return m_path; }
    QString name() const { return m_name; }
    QString extension() const { return m_extension; }
    qint64 size() const { return m_size; }
    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }
    
    // ========================================================================
    // Type Information
    // ========================================================================
    
    bool isFolder() const { return m_isFolder; }
    bool isFile() const { return !m_isFolder; }
    bool isHidden() const { return m_isHidden; }
    bool isReadOnly() const { return m_isReadOnly; }
    bool isExecutable() const { return m_isExecutable; }
    FileType fileType() const { return m_fileType; }
    
    // ========================================================================
    // Display
    // ========================================================================
    
    QString displayName() const;
    QString iconKey() const;
    QIcon icon() const;
    QString sizeString() const;
    QString modifiedString() const;
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    bool exists() const;
    bool refresh();
    
    // ========================================================================
    // Static Helpers
    // ========================================================================
    
    static FileType detectFileType(const QString& fileName);
    static QString formatSize(qint64 bytes);
    static QString formatDateTime(const QDateTime& dt);

signals:
    void changed();

private:
    void updateFromFileInfo();
    
    QString m_path;
    QString m_name;
    QString m_extension;
    qint64 m_size = 0;
    QDateTime m_created;
    QDateTime m_modified;
    
    bool m_isFolder = false;
    bool m_isHidden = false;
    bool m_isReadOnly = false;
    bool m_isExecutable = false;
    FileType m_fileType = FileType::Unknown;
};

/**
 * @brief Directory item with children support
 */
class DirectoryItem : public FileItem {
    Q_OBJECT
    
    Q_PROPERTY(QList<FileItem*> children READ children NOTIFY childrenChanged)
    Q_PROPERTY(int childrenCount READ childrenCount NOTIFY childrenChanged)
    Q_PROPERTY(bool isExpanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(bool hasChildren READ hasChildren NOTIFY childrenChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    
public:
    explicit DirectoryItem(const QString& path, QObject* parent = nullptr);
    ~DirectoryItem() override;

    // ========================================================================
    // Children
    // ========================================================================
    
    QList<FileItem*> children() const { return m_children; }
    int childrenCount() const { return m_children.count(); }
    bool hasChildren() const;
    bool isLoading() const { return m_isLoading; }
    
    // ========================================================================
    // Expansion State
    // ========================================================================
    
    bool isExpanded() const { return m_isExpanded; }
    void setExpanded(bool expanded);
    
    // ========================================================================
    // Loading
    // ========================================================================
    
    Q_INVOKABLE void loadChildren();
    Q_INVOKABLE void unloadChildren();
    Q_INVOKABLE void refreshChildren();
    
    // ========================================================================
    // Filtering
    // ========================================================================
    
    void setFilter(const QString& filter);
    void setShowHidden(bool show);
    void sortByName();
    void sortByType();
    void sortBySize();
    void sortByDate();
    
    // ========================================================================
    // Search
    // ========================================================================
    
    QList<FileItem*> search(const QString& pattern, bool recursive = true);

signals:
    void childrenChanged();
    void expandedChanged(bool expanded);
    void loadingChanged(bool loading);
    void childAdded(FileItem* child);
    void childRemoved(FileItem* child);

private:
    void clearChildren();
    void detectChildren();
    
    QList<FileItem*> m_children;
    bool m_isExpanded = false;
    bool m_isLoading = false;
    bool m_childrenLoaded = false;
    bool m_showHidden = false;
    QString m_filter;
};

} // namespace Flux::Models

#endif // FLUX_MODELS_FILE_ITEM_H
