#ifndef FLUX_SERVICES_FILE_SYSTEM_SERVICE_H
#define FLUX_SERVICES_FILE_SYSTEM_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QFileSystemWatcher>
#include <memory>

namespace Flux::Services {

/**
 * @brief File operation result
 */
struct FileOperationResult {
    bool success;
    QString message;
    QString newPath;
    
    static FileOperationResult ok(const QString& message = QString()) {
        return {true, message, QString()};
    }
    
    static FileOperationResult error(const QString& message) {
        return {false, message, QString()};
    }
};

/**
 * @brief File system service with full CRUD operations
 * 
 * Features:
 * - File operations (create, read, update, delete, rename, move, copy)
 * - Directory operations (create, delete, rename, enumerate)
 * - File watching for external changes
 * - Recent files tracking
 * - Favorites/bookmarks
 * - Search and filtering
 */
class FileSystemService : public QObject {
    Q_OBJECT
    
public:
    explicit FileSystemService(QObject* parent = nullptr);
    ~FileSystemService() override;

    // ========================================================================
    // Singleton Access
    // ========================================================================
    
    static FileSystemService* instance();

    // ========================================================================
    // File Operations
    // ========================================================================
    
    /**
     * @brief Check if file exists
     */
    Q_INVOKABLE bool fileExists(const QString& path) const;
    
    /**
     * @brief Check if path is a file
     */
    Q_INVOKABLE bool isFile(const QString& path) const;
    
    /**
     * @brief Read file contents
     */
    Q_INVOKABLE QString readFile(const QString& path);
    
    /**
     * @brief Write contents to file
     */
    Q_INVOKABLE FileOperationResult writeFile(const QString& path, const QString& content);
    
    /**
     * @brief Create a new file
     */
    Q_INVOKABLE FileOperationResult createFile(const QString& path);
    
    /**
     * @brief Delete a file
     */
    Q_INVOKABLE FileOperationResult deleteFile(const QString& path);
    
    /**
     * @brief Rename a file
     */
    Q_INVOKABLE FileOperationResult renameFile(const QString& oldPath, const QString& newName);
    
    /**
     * @brief Move a file to new location
     */
    Q_INVOKABLE FileOperationResult moveFile(const QString& sourcePath, const QString& destPath);
    
    /**
     * @brief Copy a file to new location
     */
    Q_INVOKABLE FileOperationResult copyFile(const QString& sourcePath, const QString& destPath);
    
    /**
     * @brief Get file size in bytes
     */
    Q_INVOKABLE qint64 getFileSize(const QString& path) const;
    
    /**
     * @brief Get file modification date
     */
    Q_INVOKABLE QDateTime getFileModified(const QString& path) const;

    // ========================================================================
    // Directory Operations
    // ========================================================================
    
    /**
     * @brief Check if directory exists
     */
    Q_INVOKABLE bool directoryExists(const QString& path) const;
    
    /**
     * @brief Check if path is a directory
     */
    Q_INVOKABLE bool isDirectory(const QString& path) const;
    
    /**
     * @brief Create a directory
     */
    Q_INVOKABLE FileOperationResult createDirectory(const QString& path);
    
    /**
     * @brief Create directory and all parent directories
     */
    Q_INVOKABLE FileOperationResult createDirectories(const QString& path);
    
    /**
     * @brief Delete a directory
     */
    Q_INVOKABLE FileOperationResult deleteDirectory(const QString& path, bool recursive = false);
    
    /**
     * @brief Rename a directory
     */
    Q_INVOKABLE FileOperationResult renameDirectory(const QString& oldPath, const QString& newName);
    
    /**
     * @brief Move a directory to new location
     */
    Q_INVOKABLE FileOperationResult moveDirectory(const QString& sourcePath, const QString& destPath);
    
    /**
     * @brief Copy a directory to new location
     */
    Q_INVOKABLE FileOperationResult copyDirectory(const QString& sourcePath, const QString& destPath, 
                                                   bool recursive = true);
    
    /**
     * @brief List directory contents
     */
    Q_INVOKABLE QStringList listDirectory(const QString& path, 
                                           const QString& filter = QString(),
                                           bool includeHidden = false);
    
    /**
     * @brief Get all files in directory
     */
    Q_INVOKABLE QStringList getFiles(const QString& path, 
                                      const QString& filter = QString(),
                                      bool recursive = false);
    
    /**
     * @brief Get all directories in directory
     */
    Q_INVOKABLE QStringList getDirectories(const QString& path, bool recursive = false);
    
    /**
     * @brief Get parent directory path
     */
    Q_INVOKABLE QString getParentDirectory(const QString& path) const;
    
    /**
     * @brief Get current directory
     */
    Q_INVOKABLE QString getCurrentDirectory() const;
    
    /**
     * @brief Set current directory
     */
    Q_INVOKABLE void setCurrentDirectory(const QString& path);

    // ========================================================================
    // Path Utilities
    // ========================================================================
    
    /**
     * @brief Combine path segments
     */
    Q_INVOKABLE static QString combinePath(const QString& path1, const QString& path2);
    
    /**
     * @brief Get file name from path
     */
    Q_INVOKABLE static QString getFileName(const QString& path);
    
    /**
     * @brief Get file extension
     */
    Q_INVOKABLE static QString getExtension(const QString& path);
    
    /**
     * @brief Get file name without extension
     */
    Q_INVOKABLE static QString getBaseName(const QString& path);
    
    /**
     * @brief Check if path is absolute
     */
    Q_INVOKABLE static bool isAbsolutePath(const QString& path);
    
    /**
     * @brief Convert to absolute path
     */
    Q_INVOKABLE static QString toAbsolutePath(const QString& path);
    
    /**
     * @brief Convert to relative path
     */
    Q_INVOKABLE static QString toRelativePath(const QString& path, const QString& basePath);
    
    /**
     * @brief Normalize path separators
     */
    Q_INVOKABLE static QString normalizePath(const QString& path);
    
    /**
     * @brief Get canonical (resolved) path
     */
    Q_INVOKABLE static QString getCanonicalPath(const QString& path);

    // ========================================================================
    // File Watching
    // ========================================================================
    
    /**
     * @brief Start watching a file for changes
     */
    Q_INVOKABLE bool watchFile(const QString& path);
    
    /**
     * @brief Stop watching a file
     */
    Q_INVOKABLE void unwatchFile(const QString& path);
    
    /**
     * @brief Stop watching all files
     */
    Q_INVOKABLE void unwatchAllFiles();
    
    /**
     * @brief Get list of watched files
     */
    Q_INVOKABLE QStringList watchedFiles() const;

    // ========================================================================
    // Recent Files
    // ========================================================================
    
    /**
     * @brief Add file to recent files list
     */
    Q_INVOKABLE void addRecentFile(const QString& path);
    
    /**
     * @brief Get recent files
     */
    Q_INVOKABLE QStringList recentFiles() const;
    
    /**
     * @brief Clear recent files
     */
    Q_INVOKABLE void clearRecentFiles();
    
    /**
     * @brief Remove file from recent files
     */
    Q_INVOKABLE void removeRecentFile(const QString& path);
    
    /**
     * @brief Set maximum recent files count
     */
    Q_INVOKABLE void setMaxRecentFiles(int max);
    
    /**
     * @brief Get maximum recent files count
     */
    Q_INVOKABLE int maxRecentFiles() const;

    // ========================================================================
    // Favorites/Bookmarks
    // ========================================================================
    
    /**
     * @brief Add path to favorites
     */
    Q_INVOKABLE void addFavorite(const QString& path, const QString& name = QString());
    
    /**
     * @brief Remove path from favorites
     */
    Q_INVOKABLE void removeFavorite(const QString& path);
    
    /**
     * @brief Get all favorites
     */
    Q_INVOKABLE QMap<QString, QString> favorites() const;
    
    /**
     * @brief Check if path is a favorite
     */
    Q_INVOKABLE bool isFavorite(const QString& path) const;
    
    /**
     * @brief Update favorite name
     */
    Q_INVOKABLE void updateFavorite(const QString& path, const QString& newName);

    // ========================================================================
    // Search
    // ========================================================================
    
    /**
     * @brief Search for files matching pattern
     */
    Q_INVOKABLE QStringList searchFiles(const QString& directory, 
                                         const QString& pattern,
                                         bool recursive = true);
    
    /**
     * @brief Search for files containing text
     */
    Q_INVOKABLE QStringList searchContent(const QString& directory,
                                           const QString& text,
                                           const QString& fileFilter = QString(),
                                           bool caseSensitive = false);

    // ========================================================================
    // Special Folders
    // ========================================================================
    
    /**
     * @brief Get home directory
     */
    Q_INVOKABLE static QString getHomeDirectory();
    
    /**
     * @brief Get temp directory
     */
    Q_INVOKABLE static QString getTempDirectory();
    
    /**
     * @brief Get application data directory
     */
    Q_INVOKABLE static QString getAppDataDirectory();
    
    /**
     * @brief Get desktop directory
     */
    Q_INVOKABLE static QString getDesktopDirectory();
    
    /**
     * @brief Get documents directory
     */
    Q_INVOKABLE static QString getDocumentsDirectory();

signals:
    // ========================================================================
    // File Change Events
    // ========================================================================
    
    void fileChanged(const QString& path);
    void fileCreated(const QString& path);
    void fileDeleted(const QString& path);
    void fileRenamed(const QString& oldPath, const QString& newPath);
    
    // ========================================================================
    // Directory Change Events
    // ========================================================================
    
    void directoryChanged(const QString& path);
    void directoryCreated(const QString& path);
    void directoryDeleted(const QString& path);
    
    // ========================================================================
    // Recent Files Events
    // ========================================================================
    
    void recentFilesChanged();
    
    // ========================================================================
    // Favorites Events
    // ========================================================================
    
    void favoritesChanged();

private slots:
    void onFileSystemFileChanged(const QString& path);
    void onFileSystemDirectoryChanged(const QString& path);

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    static FileSystemService* s_instance;
    
    QFileSystemWatcher* m_watcher;
    QStringList m_recentFiles;
    QMap<QString, QString> m_favorites;  // path -> display name
    int m_maxRecentFiles;
    
    // Persistence
    void loadSettings();
    void saveSettings();
};

} // namespace Flux::Services

#endif // FLUX_SERVICES_FILE_SYSTEM_SERVICE_H
