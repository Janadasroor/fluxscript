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

#include "flux/ui/project_tree.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QFileInfo>

namespace Flux {

ProjectTree::ProjectTree(QWidget* parent)
    : QTreeView(parent)
{
    setupTree();
    setupContextMenu();
}

ProjectTree::~ProjectTree()
{
    closeProject();
}

void ProjectTree::setupTree()
{
    // Create file system model
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setReadOnly(false);

    // Set filters - show files and directories, no hidden files
    m_fileSystemModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);

    // Name filters for showing relevant files
    m_nameFilters = QStringList() << "*.flux" << "*.lib" << "*.cir" << "*.txt" << "*.md"
                                   << "*.cpp" << "*.h" << "*.hpp" << "*.qml" << "*.qss"
                                   << "CMakeLists.txt" << "*.cmake" << "*.json" << "*.xml";
    m_fileSystemModel->setNameFilters(m_nameFilters);
    m_fileSystemModel->setNameFilterDisables(false); // Hide filtered files instead of disabling

    // Set the model
    setModel(m_fileSystemModel);

    // Configure tree view
    setAnimated(true);
    setIndentation(20);
    setSortingEnabled(true);
    setExpandsOnDoubleClick(false); // We handle double-click ourselves
    setContextMenuPolicy(Qt::CustomContextMenu);
    setIconSize(QSize(18, 18));
    setStyleSheet(
        "QTreeView { "
        "    background-color: #252526; "
        "    color: #cccccc; "
        "    border: none; "
        "    outline: none; "
        "    font-size: 12px; "
        "}"
        "QTreeView::item { "
        "    padding: 3px; "
        "    border: none; "
        "}"
        "QTreeView::item:hover { "
        "    background-color: #2a2d2e; "
        "}"
        "QTreeView::item:selected { "
        "    background-color: #37373d; "
        "}"
    );

    // Hide size and type columns, keep only name
    hideColumn(1); // Size
    hideColumn(2); // Type
    hideColumn(3); // Date Modified
    
    // Set header
    header()->setStretchLastSection(true);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->hide(); // Hide header completely for cleaner look
    
    // Connect signals
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        if (isFile(index)) {
            QString filePath = m_fileSystemModel->filePath(index);
            Q_EMIT fileClicked(filePath);
        }
    });
    
    connect(m_fileSystemModel, &QFileSystemModel::fileRenamed,
            this, &ProjectTree::onFileRenamed);
}

void ProjectTree::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_openAction = new QAction(tr("Open"), this);
    m_openAction->setShortcut(Qt::Key_Return);
    m_openAction->setIcon(QIcon::fromTheme("document-open"));
    
    m_newFileAction = new QAction(tr("New File..."), this);
    m_newFileAction->setIcon(QIcon::fromTheme("document-new"));
    
    m_newFolderAction = new QAction(tr("New Folder..."), this);
    m_newFolderAction->setIcon(QIcon::fromTheme("folder-new"));
    
    m_renameAction = new QAction(tr("Rename"), this);
    m_renameAction->setShortcut(Qt::Key_F2);
    m_renameAction->setIcon(QIcon::fromTheme("edit-rename"));
    
    m_deleteAction = new QAction(tr("Delete"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    
    m_refreshAction = new QAction(tr("Refresh"), this);
    m_refreshAction->setShortcut(Qt::Key_F5);
    m_refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    
    m_showInExplorerAction = new QAction(tr("Show in File Manager"), this);
    m_showInExplorerAction->setIcon(QIcon::fromTheme("system-file-manager"));
    
    m_contextMenu->addAction(m_openAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_newFileAction);
    m_contextMenu->addAction(m_newFolderAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_renameAction);
    m_contextMenu->addAction(m_deleteAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_refreshAction);
    m_contextMenu->addAction(m_showInExplorerAction);
    
    // Connect actions
    connect(m_openAction, &QAction::triggered, this, [this]() {
        QString filePath = getSelectedFilePath();
        if (!filePath.isEmpty() && isFile(selectionModel()->currentIndex())) {
            Q_EMIT fileDoubleClicked(filePath);
        }
    });
    
    connect(m_newFileAction, &QAction::triggered, this, [this]() {
        createNewFile();
    });
    
    connect(m_newFolderAction, &QAction::triggered, this, [this]() {
        createNewFolder();
    });
    
    connect(m_renameAction, &QAction::triggered, this, [this]() {
        QString oldPath = getSelectedFilePath();
        if (!oldPath.isEmpty()) {
            QFileInfo fileInfo(oldPath);
            bool ok = false;
            QString newName = QInputDialog::getText(
                this,
                tr("Rename"),
                tr("Enter new name:"),
                QLineEdit::Normal,
                fileInfo.fileName(),
                &ok
            );
            
            if (ok && !newName.isEmpty() && newName != fileInfo.fileName()) {
                renameFile(oldPath, newName);
            }
        }
    });
    
    connect(m_deleteAction, &QAction::triggered, this, [this]() {
        QString filePath = getSelectedFilePath();
        if (!filePath.isEmpty()) {
            QFileInfo fileInfo(filePath);
            QString itemType = fileInfo.isDir() ? "folder" : "file";
            
            QMessageBox::StandardButton result = QMessageBox::question(
                this,
                tr("Confirm Delete"),
                tr("Are you sure you want to delete this %1?\n\n%2").arg(itemType, fileInfo.fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            
            if (result == QMessageBox::Yes) {
                deleteFile(filePath);
            }
        }
    });
    
    connect(m_refreshAction, &QAction::triggered, this, [this]() {
        if (m_hasProject) {
            m_fileSystemModel->setRootPath(m_projectPath);
        }
    });
    
    connect(m_showInExplorerAction, &QAction::triggered, this, [this]() {
        QString filePath = getSelectedFilePath();
        if (!filePath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    });
}

void ProjectTree::contextMenuEvent(QContextMenuEvent* event)
{
    QModelIndex index = indexAt(event->pos());

    if (index.isValid()) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);

        // Update menu state based on selection
        bool isFileSelected = isFile(index);
        bool isFolderSelected = isFolder(index);

        m_openAction->setVisible(isFileSelected);
        m_openAction->setEnabled(isFileSelected);
        
        m_newFileAction->setVisible(true);
        m_newFileAction->setEnabled(m_hasProject);
        
        m_newFolderAction->setVisible(true);
        m_newFolderAction->setEnabled(m_hasProject);
        
        m_renameAction->setVisible(isFileSelected || isFolderSelected);
        m_renameAction->setEnabled(isFileSelected || isFolderSelected);
        
        m_deleteAction->setVisible(isFileSelected || isFolderSelected);
        m_deleteAction->setEnabled(isFileSelected || isFolderSelected);

        m_contextMenu->exec(event->globalPos());
    } else {
        // Right-click on empty area - show options for root
        m_openAction->setVisible(false);
        m_newFileAction->setVisible(true);
        m_newFileAction->setEnabled(m_hasProject);
        m_newFolderAction->setVisible(true);
        m_newFolderAction->setEnabled(m_hasProject);
        m_renameAction->setVisible(false);
        m_deleteAction->setVisible(false);

        m_contextMenu->exec(event->globalPos());
    }
    
    // Reset visibility for next time
    m_openAction->setVisible(true);
}

void ProjectTree::mouseDoubleClickEvent(QMouseEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    
    if (index.isValid()) {
        if (isFile(index)) {
            QString filePath = m_fileSystemModel->filePath(index);
            Q_EMIT fileDoubleClicked(filePath);
        } else if (isFolder(index)) {
            // Toggle folder expansion
            if (isExpanded(index)) {
                collapse(index);
            } else {
                expand(index);
            }
        }
    }
    
    QTreeView::mouseDoubleClickEvent(event);
}

void ProjectTree::openProject(const QString& projectPath)
{
    QFileInfo fileInfo(projectPath);
    
    if (!fileInfo.exists() || !fileInfo.isDir()) {
        QMessageBox::warning(this, tr("Invalid Project"),
            tr("The specified project path does not exist or is not a directory."));
        return;
    }
    
    m_projectPath = fileInfo.absoluteFilePath();
    m_hasProject = true;
    
    // Set root path
    m_fileSystemModel->setRootPath(m_projectPath);
    setRootIndex(m_fileSystemModel->index(m_projectPath));
    
    // Expand first level
    expandAll();
    QModelIndex root = rootIndex();
    for (int i = 0; i < model()->rowCount(root); ++i) {
        QModelIndex child = model()->index(i, 0, root);
        expand(child);
    }
    
    Q_EMIT projectOpened(m_projectPath);
}

void ProjectTree::closeProject()
{
    m_projectPath.clear();
    m_hasProject = false;
    setRootIndex(QModelIndex());
    Q_EMIT projectClosed();
}

QString ProjectTree::projectPath() const
{
    return m_projectPath;
}

bool ProjectTree::hasProject() const
{
    return m_hasProject;
}

void ProjectTree::createNewFile(const QString& name)
{
    QString baseName = name;
    if (baseName.isEmpty()) {
        bool ok = false;
        baseName = QInputDialog::getText(
            this,
            tr("New File"),
            tr("Enter file name:"),
            QLineEdit::Normal,
            "untitled.flux",
            &ok
        );
        
        if (!ok || baseName.isEmpty()) {
            return;
        }
    }
    
    // Determine target directory
    QString targetDir = m_projectPath;
    QModelIndex currentIndex = selectionModel()->currentIndex();
    if (currentIndex.isValid() && isFolder(currentIndex)) {
        targetDir = m_fileSystemModel->filePath(currentIndex);
    }
    
    QString filePath = QDir(targetDir).filePath(baseName);
    
    // Check if file already exists
    if (QFile::exists(filePath)) {
        QMessageBox::warning(this, tr("File Exists"),
            tr("A file with this name already exists."));
        return;
    }
    
    // Create file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.close();
        
        // Refresh and select the new file
        m_fileSystemModel->setRootPath(m_projectPath);
        
        // Emit signal for opening the file
        Q_EMIT fileDoubleClicked(filePath);
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to create file: %1").arg(file.errorString()));
    }
}

void ProjectTree::createNewFolder(const QString& name)
{
    QString baseName = name;
    if (baseName.isEmpty()) {
        bool ok = false;
        baseName = QInputDialog::getText(
            this,
            tr("New Folder"),
            tr("Enter folder name:"),
            QLineEdit::Normal,
            "New Folder",
            &ok
        );
        
        if (!ok || baseName.isEmpty()) {
            return;
        }
    }
    
    // Determine target directory
    QString targetDir = m_projectPath;
    QModelIndex currentIndex = selectionModel()->currentIndex();
    if (currentIndex.isValid() && isFolder(currentIndex)) {
        targetDir = m_fileSystemModel->filePath(currentIndex);
    }
    
    QString dirPath = QDir(targetDir).filePath(baseName);
    
    // Check if folder already exists
    if (QDir(dirPath).exists()) {
        QMessageBox::warning(this, tr("Folder Exists"),
            tr("A folder with this name already exists."));
        return;
    }
    
    // Create folder
    if (QDir().mkpath(dirPath)) {
        m_fileSystemModel->setRootPath(m_projectPath);
    } else {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to create folder."));
    }
}

void ProjectTree::deleteFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    
    bool success = false;
    if (fileInfo.isDir()) {
        success = QDir().rmdir(filePath);
    } else {
        success = QFile::remove(filePath);
    }
    
    if (!success) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to delete %1.").arg(fileInfo.fileName()));
    }
}

void ProjectTree::renameFile(const QString& oldPath, const QString& newName)
{
    QFileInfo fileInfo(oldPath);
    QString newPath = fileInfo.absoluteDir().filePath(newName);
    
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, tr("Name Conflict"),
            tr("A file or folder with this name already exists."));
        return;
    }
    
    if (!QFile::rename(oldPath, newPath)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to rename file."));
    }
}

QString ProjectTree::getSelectedFilePath() const
{
    QModelIndex index = selectionModel()->currentIndex();
    if (index.isValid()) {
        return m_fileSystemModel->filePath(index);
    }
    return QString();
}

QFileInfo ProjectTree::getSelectedFileInfo() const
{
    QString filePath = getSelectedFilePath();
    return QFileInfo(filePath);
}

bool ProjectTree::isFile(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return false;
    }
    QFileInfo fileInfo = m_fileSystemModel->fileInfo(index);
    return fileInfo.isFile();
}

bool ProjectTree::isFolder(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return false;
    }
    QFileInfo fileInfo = m_fileSystemModel->fileInfo(index);
    return fileInfo.isDir();
}

void ProjectTree::onContextMenuAction_triggered(QAction* action)
{
    Q_UNUSED(action);
}

void ProjectTree::onFileRenamed(const QString& oldPath, const QString& newPath)
{
    Q_UNUSED(oldPath);
    Q_UNUSED(newPath);
    // File was renamed, model handles the update
    // Could emit a signal if needed for document manager
}

} // namespace Flux
