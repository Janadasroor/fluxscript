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
