#ifndef FLUX_PROJECT_TREE_H
#define FLUX_PROJECT_TREE_H

#include <QTreeView>
#include <QFileSystemModel>
#include <QMenu>
#include <QString>
#include <QDir>

namespace Flux {

/**
 * @brief Project tree view for displaying and managing project files
 */
class ProjectTree : public QTreeView
{
    Q_OBJECT

public:
    explicit ProjectTree(QWidget* parent = nullptr);
    ~ProjectTree() override;

    // Project management
    void openProject(const QString& projectPath);
    void closeProject();
    QString projectPath() const;
    bool hasProject() const;

    // File operations
    void createNewFile(const QString& name = QString());
    void createNewFolder(const QString& name = QString());
    void deleteFile(const QString& filePath);
    void renameFile(const QString& oldPath, const QString& newName);

Q_SIGNALS:
    void fileClicked(const QString& filePath);
    void fileDoubleClicked(const QString& filePath);
    void projectOpened(const QString& projectPath);
    void projectClosed();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private Q_SLOTS:
    void onContextMenuAction_triggered(QAction* action);
    void onFileRenamed(const QString& oldPath, const QString& newPath);

private:
    void setupTree();
    void setupContextMenu();
    void filterFiles();
    
    QString getSelectedFilePath() const;
    QFileInfo getSelectedFileInfo() const;
    bool isFile(const QModelIndex& index) const;
    bool isFolder(const QModelIndex& index) const;

    // Context menu actions
    QAction* m_openAction = nullptr;
    QAction* m_newFileAction = nullptr;
    QAction* m_newFolderAction = nullptr;
    QAction* m_renameAction = nullptr;
    QAction* m_deleteAction = nullptr;
    QAction* m_refreshAction = nullptr;
    QAction* m_showInExplorerAction = nullptr;
    QMenu* m_contextMenu = nullptr;

    // File system model
    QFileSystemModel* m_fileSystemModel = nullptr;
    
    // Project state
    QString m_projectPath;
    bool m_hasProject = false;

    // Filters
    QStringList m_nameFilters;
};

} // namespace Flux

#endif // FLUX_PROJECT_TREE_H
