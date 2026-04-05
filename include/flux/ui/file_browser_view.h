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

#ifndef FLUX_UI_FILE_BROWSER_VIEW_H
#define FLUX_UI_FILE_BROWSER_VIEW_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QTableView>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QToolBar>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace Flux::MVVM {
    class FileBrowserViewModel;
}

namespace Flux::UI {

/**
 * @brief Custom tree view for file browser
 */
class FileBrowserTreeView : public QTreeView {
    Q_OBJECT
    
public:
    explicit FileBrowserTreeView(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
signals:
    void itemActivated(const QString& path);
    void contextMenuRequested(const QPoint& pos, const QString& path);
    void dropOnItem(const QString& path, const QMimeData* data);

private:
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Custom list view for file browser
 */
class FileBrowserListView : public QListView {
    Q_OBJECT
    
public:
    explicit FileBrowserListView(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
signals:
    void itemActivated(const QString& path);
    void contextMenuRequested(const QPoint& pos, const QString& path);

private:
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Details view (table) for file browser
 */
class FileBrowserDetailsView : public QTableView {
    Q_OBJECT
    
public:
    explicit FileBrowserDetailsView(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    
signals:
    void itemActivated(const QString& path);
    void contextMenuRequested(const QPoint& pos, const QString& path);

private:
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Navigation toolbar for file browser
 */
class FileBrowserToolBar : public QToolBar {
    Q_OBJECT
    
public:
    explicit FileBrowserToolBar(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
    // Navigation buttons
    QPushButton* backButton() const { return m_backButton; }
    QPushButton* forwardButton() const { return m_forwardButton; }
    QPushButton* upButton() const { return m_upButton; }
    QPushButton* refreshButton() const { return m_refreshButton; }
    
    // Path edit
    QLineEdit* pathEdit() const { return m_pathEdit; }
    
    // View controls
    QComboBox* viewModeCombo() const { return m_viewModeCombo; }
    
protected:
    void setupActions();
    
signals:
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void refresh();
    void pathChanged(const QString& path);
    void viewModeChanged(int mode);

private:
    QPushButton* m_backButton;
    QPushButton* m_forwardButton;
    QPushButton* m_upButton;
    QPushButton* m_refreshButton;
    
    QLineEdit* m_pathEdit;
    QComboBox* m_viewModeCombo;
    
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Filter and search bar
 */
class FileBrowserFilterBar : public QWidget {
    Q_OBJECT
    
public:
    explicit FileBrowserFilterBar(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
    QLineEdit* searchEdit() const { return m_searchEdit; }
    QComboBox* filterCombo() const { return m_filterCombo; }
    
signals:
    void searchChanged(const QString& text);
    void filterChanged(const QString& filter);
    void clearFilter();

private:
    void setupUI();
    
    QLineEdit* m_searchEdit;
    QComboBox* m_filterCombo;
    QPushButton* m_clearButton;
    
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Status bar for file browser
 */
class FileBrowserStatusBar : public QWidget {
    Q_OBJECT
    
public:
    explicit FileBrowserStatusBar(QWidget* parent = nullptr);
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    
    void updateStatus();
    
signals:
    void selectionInfoRequested();

private:
    void setupUI();
    
    QLabel* m_itemCountLabel;
    QLabel* m_selectedCountLabel;
    QLabel* m_sizeLabel;
    QLabel* m_statusLabel;
    
    MVVM::FileBrowserViewModel* m_viewModel;
};

/**
 * @brief Main file browser widget with full functionality
 * 
 * Features:
 * - Tree/List/Details view modes
 * - Navigation toolbar with back/forward/up
 * - Path bar with autocomplete
 * - Filter and search
 * - Context menu with all actions
 * - Drag and drop support
 * - Status bar with selection info
 * - Split view (optional)
 */
class FileBrowserView : public QWidget {
    Q_OBJECT
    
public:
    explicit FileBrowserView(QWidget* parent = nullptr);
    ~FileBrowserView() override;
    
    void setViewModel(MVVM::FileBrowserViewModel* viewModel);
    MVVM::FileBrowserViewModel* viewModel() const { return m_viewModel; }
    
    // View mode
    void setViewMode(int mode);
    int viewMode() const;
    
    // Navigation
    void navigateTo(const QString& path);
    QString currentPath() const;
    
    // UI components
    FileBrowserTreeView* treeView() const { return m_treeView; }
    FileBrowserListView* listView() const { return m_listView; }
    FileBrowserDetailsView* detailsView() const { return m_detailsView; }
    FileBrowserToolBar* toolBar() const { return m_toolBar; }
    FileBrowserFilterBar* filterBar() const { return m_filterBar; }
    FileBrowserStatusBar* statusBar() const { return m_statusBar; }

protected:
    void setupUI();
    void setupConnections();
    void createContextMenu();
    void updateViewMode();
    
signals:
    void fileActivated(const QString& path);
    void folderActivated(const QString& path);
    void navigationRequested(const QString& path);
    void contextMenuRequested(const QPoint& pos);

private slots:
    void onItemActivated(const QString& path);
    void onContextMenuRequested(const QPoint& pos, const QString& path);
    void onNavigateBack();
    void onNavigateForward();
    void onNavigateUp();
    void onRefresh();
    void onPathChanged(const QString& path);
    void onViewModeChanged(int mode);
    void onSearchChanged(const QString& text);
    void onFilterChanged(const QString& filter);
    void onViewModelPropertyChanged(const QString& name);

private:
    // Layout
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_topLayout;
    
    // Components
    FileBrowserToolBar* m_toolBar;
    FileBrowserFilterBar* m_filterBar;
    
    QStackedWidget* m_viewStack;
    FileBrowserTreeView* m_treeView;
    FileBrowserListView* m_listView;
    FileBrowserDetailsView* m_detailsView;
    
    FileBrowserStatusBar* m_statusBar;
    
    // Context menu
    QMenu* m_contextMenu;
    QMenu* m_newMenu;
    QMenu* m_sortMenu;
    QMenu* m_viewMenu;
    
    // View model
    MVVM::FileBrowserViewModel* m_viewModel;
    
    // Current view mode
    int m_currentViewMode;
};

/**
 * @brief Split file browser (dual pane)
 */
class SplitFileBrowserView : public QSplitter {
    Q_OBJECT
    
public:
    explicit SplitFileBrowserView(QWidget* parent = nullptr);
    
    FileBrowserView* leftBrowser() const { return m_leftBrowser; }
    FileBrowserView* rightBrowser() const { return m_rightBrowser; }
    
    void setViewModels(MVVM::FileBrowserViewModel* left, MVVM::FileBrowserViewModel* right);
    
signals:
    void fileActivated(const QString& path, bool isLeft);

private:
    FileBrowserView* m_leftBrowser;
    FileBrowserView* m_rightBrowser;
};

} // namespace Flux::UI

#endif // FLUX_UI_FILE_BROWSER_VIEW_H
