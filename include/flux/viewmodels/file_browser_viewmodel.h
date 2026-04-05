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

#ifndef FLUX_VIEWMODELS_FILE_BROWSER_VIEWMODEL_H
#define FLUX_VIEWMODELS_FILE_BROWSER_VIEWMODEL_H

#include "flux/mvvm/viewmodel_base.h"
#include "flux/mvvm/observable_property.h"
#include "flux/mvvm/relay_command.h"
#include "flux/models/file_item.h"
#include <memory>

namespace Flux::Services {
    class FileSystemService;
}

namespace Flux::MVVM {

/**
 * @brief Browser view modes
 */
enum class BrowserViewMode {
    Tree,           // Tree view with folders
    List,           // List view
    Details,        // Details view with columns
    Tiles,          // Icon tiles
    Thumbnails      // Thumbnail view
};

/**
 * @brief Sort options
 */
enum class SortOption {
    Name,
    DateModified,
    DateCreated,
    Size,
    Type,
    Extension
};

/**
 * @brief Sort order
 */
enum class SortOrder {
    Ascending,
    Descending
};

/**
 * @brief ViewModel for file browser functionality
 * 
 * Features:
 * - Navigate directories
 * - Create/delete/rename files and folders
 * - Copy/move operations
 * - Search and filter
 * - Multiple view modes
 * - Drag and drop
 * - Context menu actions
 * - Multi-selection
 */
class FileBrowserViewModel : public ViewModelBase {
    Q_OBJECT
    
public:
    explicit FileBrowserViewModel(QObject* parent = nullptr);
    ~FileBrowserViewModel() override;

    // ========================================================================
    // Navigation
    // ========================================================================
    
    /**
     * @brief Current directory path
     */
    ObservableProperty<QString> CurrentPath;
    ObservableProperty<QString> CurrentPathDisplay;
    
    /**
     * @brief Navigation history
     */
    ObservableProperty<int> HistoryIndex;
    ObservableProperty<int> HistoryCount;
    
    /**
     * @brief Root directory (for scoping)
     */
    ObservableProperty<QString> RootPath;
    ObservableProperty<bool> HasRoot;

    // ========================================================================
    // View Settings
    // ========================================================================
    
    /**
     * @brief Current view mode
     */
    ObservableProperty<BrowserViewMode> ViewMode;
    ObservableProperty<SortOption> SortBy;
    ObservableProperty<SortOrder> SortOrder;
    ObservableProperty<bool> ShowHiddenFiles;
    ObservableProperty<bool> ShowFileIcons;
    ObservableProperty<bool> ShowFoldersFirst;
    
    /**
     * @brief Filter
     */
    ObservableProperty<QString> FilterText;
    ObservableProperty<QString> FileFilter;
    ObservableProperty<bool> FilterActive;

    // ========================================================================
    // Selection
    // ========================================================================
    
    /**
     * @brief Selected items
     */
    ObservableProperty<QList<Models::FileItem*>> SelectedItems;
    ObservableProperty<Models::FileItem*> SelectedItem;
    ObservableProperty<int> SelectedCount;
    ObservableProperty<bool> HasSelection;
    
    /**
     * @brief Hovered item (for tooltips)
     */
    ObservableProperty<Models::FileItem*> HoveredItem;

    // ========================================================================
    // Items
    // ========================================================================
    
    /**
     * @brief Current directory items
     */
    ObservableProperty<QList<Models::FileItem*>> Items;
    ObservableProperty<int> ItemCount;
    ObservableProperty<int> FolderCount;
    ObservableProperty<int> FileCount;
    
    /**
     * @brief Loading state
     */
    ObservableProperty<bool> IsLoading;
    ObservableProperty<QString> LoadingMessage;
    ObservableProperty<bool> HasError;
    ObservableProperty<QString> ErrorMessage;

    // ========================================================================
    // Clipboard (for copy/cut)
    // ========================================================================
    
    ObservableProperty<QList<Models::FileItem*>> ClipboardItems;
    ObservableProperty<bool> IsCutOperation;
    ObservableProperty<bool> HasClipboardItems;

    // ========================================================================
    // Recent & Favorites
    // ========================================================================
    
    ObservableProperty<QStringList> RecentFiles;
    ObservableProperty<QMap<QString, QString>> Favorites;

    // ========================================================================
    // Commands - Navigation
    // ========================================================================
    
    std::shared_ptr<RelayCommand> NavigateCommand;
    std::shared_ptr<RelayCommand> NavigateUpCommand;
    std::shared_ptr<RelayCommand> NavigateBackCommand;
    std::shared_ptr<RelayCommand> NavigateForwardCommand;
    std::shared_ptr<RelayCommand> NavigateHomeCommand;
    std::shared_ptr<RelayCommand> RefreshCommand;
    
    std::shared_ptr<RelayCommandT<QString>> SetRootPathCommand;
    std::shared_ptr<RelayCommand> ClearRootPathCommand;

    // ========================================================================
    // Commands - File Operations
    // ========================================================================
    
    std::shared_ptr<RelayCommand> NewFileCommand;
    std::shared_ptr<RelayCommand> NewFolderCommand;
    std::shared_ptr<RelayCommand> OpenCommand;
    std::shared_ptr<RelayCommand> OpenWithCommand;
    std::shared_ptr<RelayCommand> DeleteCommand;
    std::shared_ptr<RelayCommand> RenameCommand;
    
    std::shared_ptr<RelayCommand> CopyCommand;
    std::shared_ptr<RelayCommand> CutCommand;
    std::shared_ptr<RelayCommand> PasteCommand;
    std::shared_ptr<RelayCommand> DuplicateCommand;
    
    std::shared_ptr<RelayCommand> SelectAllCommand;
    std::shared_ptr<RelayCommand> InvertSelectionCommand;
    std::shared_ptr<RelayCommand> ClearSelectionCommand;

    // ========================================================================
    // Commands - View
    // ========================================================================
    
    std::shared_ptr<RelayCommand> SetViewModeCommand;
    std::shared_ptr<RelayCommand> ToggleViewModeCommand;
    std::shared_ptr<RelayCommand> SortByNameCommand;
    std::shared_ptr<RelayCommand> SortByDateCommand;
    std::shared_ptr<RelayCommand> SortBySizeCommand;
    std::shared_ptr<RelayCommand> SortByTypeCommand;
    std::shared_ptr<RelayCommand> ToggleSortOrderCommand;
    std::shared_ptr<RelayCommand> ToggleHiddenFilesCommand;
    
    std::shared_ptr<RelayCommand> SearchCommand;
    std::shared_ptr<RelayCommand> ClearFilterCommand;

    // ========================================================================
    // Commands - Favorites
    // ========================================================================
    
    std::shared_ptr<RelayCommand> AddFavoriteCommand;
    std::shared_ptr<RelayCommand> RemoveFavoriteCommand;

    // ========================================================================
    // Commands - Context Menu
    // ========================================================================
    
    std::shared_ptr<RelayCommand> ShowPropertiesCommand;
    std::shared_ptr<RelayCommand> ShowInFileManagerCommand;
    std::shared_ptr<RelayCommand> CopyPathCommand;
    std::shared_ptr<RelayCommand> CopyRelativePathCommand;

    // ========================================================================
    // Navigation Methods
    // ========================================================================
    
    /**
     * @brief Navigate to directory
     */
    Q_INVOKABLE bool navigateTo(const QString& path);
    
    /**
     * @brief Navigate to parent directory
     */
    Q_INVOKABLE bool navigateUp();
    
    /**
     * @brief Navigate back in history
     */
    Q_INVOKABLE bool navigateBack();
    
    /**
     * @brief Navigate forward in history
     */
    Q_INVOKABLE bool navigateForward();
    
    /**
     * @brief Navigate to home directory
     */
    Q_INVOKABLE bool navigateHome();
    
    /**
     * @brief Navigate to specific path
     */
    Q_INVOKABLE bool navigateToPath(const QString& path);

    // ========================================================================
    // File Operations
    // ========================================================================
    
    /**
     * @brief Create new file
     */
    Q_INVOKABLE bool createFile(const QString& name = QString());
    
    /**
     * @brief Create new folder
     */
    Q_INVOKABLE bool createFolder(const QString& name = QString());
    
    /**
     * @brief Open selected file
     */
    Q_INVOKABLE bool openSelected();
    
    /**
     * @brief Open file with application
     */
    Q_INVOKABLE bool openWith(const QString& application);
    
    /**
     * @brief Delete selected items
     */
    Q_INVOKABLE bool deleteSelected(bool permanent = false);
    
    /**
     * @brief Rename selected item
     */
    Q_INVOKABLE bool renameSelected(const QString& newName);
    
    /**
     * @brief Copy selected items to clipboard
     */
    Q_INVOKABLE void copySelected();
    
    /**
     * @brief Cut selected items to clipboard
     */
    Q_INVOKABLE void cutSelected();
    
    /**
     * @brief Paste items from clipboard
     */
    Q_INVOKABLE bool paste();
    
    /**
     * @brief Duplicate selected items
     */
    Q_INVOKABLE bool duplicateSelected();

    // ========================================================================
    // Selection
    // ========================================================================
    
    /**
     * @brief Select all items
     */
    Q_INVOKABLE void selectAll();
    
    /**
     * @brief Clear selection
     */
    Q_INVOKABLE void clearSelection();
    
    /**
     * @brief Invert selection
     */
    Q_INVOKABLE void invertSelection();
    
    /**
     * @brief Select item
     */
    Q_INVOKABLE void selectItem(Models::FileItem* item, bool addToSelection = false);
    
    /**
     * @brief Check if item is selected
     */
    Q_INVOKABLE bool isSelected(Models::FileItem* item) const;

    // ========================================================================
    // View Settings
    // ========================================================================
    
    /**
     * @brief Set view mode
     */
    Q_INVOKABLE void setViewMode(BrowserViewMode mode);
    
    /**
     * @brief Toggle view mode
     */
    Q_INVOKABLE void toggleViewMode();
    
    /**
     * @brief Set sort option
     */
    Q_INVOKABLE void sortBy(SortOption option);
    
    /**
     * @brief Toggle sort order
     */
    Q_INVOKABLE void toggleSortOrder();
    
    /**
     * @brief Toggle hidden files visibility
     */
    Q_INVOKABLE void toggleHiddenFiles();
    
    /**
     * @brief Set file filter
     */
    Q_INVOKABLE void setFileFilter(const QString& filter);
    
    /**
     * @brief Clear file filter
     */
    Q_INVOKABLE void clearFileFilter();

    // ========================================================================
    // Search
    // ========================================================================
    
    /**
     * @brief Search for files
     */
    Q_INVOKABLE void search(const QString& pattern);
    
    /**
     * @brief Clear search
     */
    Q_INVOKABLE void clearSearch();

    // ========================================================================
    // Favorites
    // ========================================================================
    
    /**
     * @brief Add current path to favorites
     */
    Q_INVOKABLE void addFavorite(const QString& name = QString());
    
    /**
     * @brief Remove path from favorites
     */
    Q_INVOKABLE void removeFavorite(const QString& path);
    
    /**
     * @brief Navigate to favorite
     */
    Q_INVOKABLE bool navigateToFavorite(const QString& path);

    // ========================================================================
    // Drag and Drop
    // ========================================================================
    
    /**
     * @brief Check if drop is valid
     */
    Q_INVOKABLE bool canDrop(const QString& mimeType, int row, int column) const;
    
    /**
     * @brief Handle drop
     */
    Q_INVOKABLE bool drop(const QString& mimeType, const QByteArray& data, 
                          Models::FileItem* targetItem);

    // ========================================================================
    // Utilities
    // ========================================================================
    
    /**
     * @brief Get item at index
     */
    Q_INVOKABLE Models::FileItem* itemAt(int index) const;
    
    /**
     * @brief Get folder item at index
     */
    Q_INVOKABLE Models::FileItem* folderAt(int index) const;
    
    /**
     * @brief Get file item at index
     */
    Q_INVOKABLE Models::FileItem* fileAt(int index) const;
    
    /**
     * @brief Get item by path
     */
    Q_INVOKABLE Models::FileItem* itemByPath(const QString& path) const;
    
    /**
     * @brief Get path for navigation
     */
    Q_INVOKABLE QString pathForNavigation() const;
    
    /**
     * @brief Get breadcrumb items
     */
    Q_INVOKABLE QStringList getBreadcrumbs() const;

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    void onLoad() override;
    void onUnload() override;
    bool validate() override;

signals:
    // ========================================================================
    // Navigation Events
    // ========================================================================
    
    void navigated(const QString& path);
    void navigationFailed(const QString& path, const QString& error);
    void historyChanged();
    
    // ========================================================================
    // File Events
    // ========================================================================
    
    void fileCreated(const QString& path);
    void fileDeleted(const QString& path);
    void fileRenamed(const QString& oldPath, const QString& newPath);
    
    // ========================================================================
    // Selection Events
    // ========================================================================
    
    void selectionChanged();
    void itemActivated(Models::FileItem* item);
    
    // ========================================================================
    // View Events
    // ========================================================================
    
    void viewModeChanged(BrowserViewMode mode);
    void itemsChanged();
    void loadingChanged(bool loading);

protected:
    // ========================================================================
    // Protected Helpers
    // ========================================================================
    
    void setupCommands();
    void setupBindings();
    void updateCommandStates();
    
    void loadDirectory(const QString& path);
    void sortItems();
    void filterItems();
    void updateCounts();
    void updatePathDisplay();
    void updateHistory();
    
    bool canNavigate() const;
    bool canNavigateUp() const;
    bool canNavigateBack() const;
    bool canNavigateForward() const;
    bool canCreateFile() const;
    bool canCreateFolder() const;
    bool canDelete() const;
    bool canRename() const;
    bool canCopy() const;
    bool canCut() const;
    bool canPaste() const;
    bool canOpen() const;

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    Services::FileSystemService* m_fileService;
    
    // Navigation
    QStringList m_history;
    int m_historyIndex;
    QStringList m_forwardHistory;
    
    // Items
    QList<Models::FileItem*> m_allItems;
    QList<Models::FileItem*> m_filteredItems;
    QList<Models::FileItem*> m_folders;
    QList<Models::FileItem*> m_files;
    
    // Selected item paths (for persistence across refresh)
    QStringList m_selectedPaths;
    
    // Search
    bool m_isSearching;
    QString m_searchPattern;
};

} // namespace Flux::MVVM

#endif // FLUX_VIEWMODELS_FILE_BROWSER_VIEWMODEL_H
