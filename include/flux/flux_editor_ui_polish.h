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

#ifndef FLUX_EDITOR_UI_POLISH_H
#define FLUX_EDITOR_UI_POLISH_H

/**
 * @file flux_editor_ui_polish.h
 * @brief UI/UX polish components for FluxScript editor
 *
 * Phase 4: Polish & Optimization - UI/UX Improvements
 */

#include <QWidget>
#include <QDialog>
#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QMap>
#include <QKeySequence>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QProgressBar>
#include <QHBoxLayout>
#include <functional>

namespace Flux {

/**
 * @brief Theme manager for consistent styling
 */
class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager* instance();
    
    // Theme management
    void setTheme(const QString& themeName);
    QString currentTheme() const { return m_currentTheme; }
    QStringList availableThemes() const;
    
    // Color palette
    QColor backgroundColor() const;
    QColor textColor() const;
    QColor accentColor() const;
    QColor errorColor() const;
    QColor warningColor() const;
    QColor successColor() const;
    
    // Apply theme
    void applyTheme(QWidget* widget);
    void applyThemeToApplication();

Q_SIGNALS:
    void themeChanged(const QString& theme);

private:
    ThemeManager(QObject* parent = nullptr);
    void loadTheme(const QString& themeName);
    QString loadStyleSheet(const QString& themeName);
    
    static ThemeManager* s_instance;
    
    QString m_currentTheme;
    QMap<QString, QColor> m_colors;
    QString m_stylesheet;
};

/**
 * @brief Keyboard shortcut manager
 */
class ShortcutManager : public QObject {
    Q_OBJECT

public:
    static ShortcutManager* instance();
    
    // Shortcut registration
    void registerShortcut(const QString& actionName, const QKeySequence& shortcut,
                         const char* slot, QObject* receiver);
    void registerShortcut(const QString& actionName, const QKeySequence& shortcut,
                         std::function<void()> callback);
    
    // Shortcut access
    QKeySequence shortcut(const QString& actionName) const;
    void setShortcut(const QString& actionName, const QKeySequence& shortcut);
    
    // Presets
    void loadVioSpicePresets();
    void loadDefaultPresets();
    
    // Export/Import
    void exportShortcuts(const QString& filePath);
    void importShortcuts(const QString& filePath);

Q_SIGNALS:
    void shortcutChanged(const QString& actionName, const QKeySequence& newShortcut);

private:
    ShortcutManager(QObject* parent = nullptr);
    
    static ShortcutManager* s_instance;
    
    QMap<QString, QKeySequence> m_shortcuts;
    QMap<QString, std::function<void()>> m_callbacks;
};

/**
 * @brief Dock widget manager for layout persistence
 */
class DockManager : public QObject {
    Q_OBJECT

public:
    explicit DockManager(QMainWindow* mainWindow);
    
    // Layout management
    void saveLayout(const QString& name = "default");
    void restoreLayout(const QString& name = "default");
    void resetLayout();
    
    // Dock state
    QStringList availableDocks() const;
    QStringList visibleDocks() const;
    void setDockVisible(const QString& dockName, bool visible);
    bool isDockVisible(const QString& dockName) const;
    
    // Presets
    void applyPreset(const QString& preset);
    QStringList availablePresets() const;

Q_SIGNALS:
    void layoutSaved(const QString& name);
    void layoutRestored(const QString& name);

private:
    void setupPresets();
    
    QMainWindow* m_mainWindow;
    QMap<QString, QByteArray> m_layouts;
    QMap<QString, QDockWidget*> m_docks;
    
    QMap<QString, QMap<QString, bool>> m_presets;
};

/**
 * @brief Welcome screen for new users
 */
class WelcomeScreen : public QDialog {
    Q_OBJECT

public:
    explicit WelcomeScreen(QWidget* parent = nullptr);
    
    // Content
    void setRecentFiles(const QStringList& files);
    void setTemplates(const QStringList& templates);

private Q_SLOTS:
    void onOpenFile();
    void onNewFile();
    void onRecentFileClicked(const QString& file);
    void onTemplateClicked(const QString& templateName);
    void onShowOnStartupToggled(bool show);

private:
    void setupUI();
    void setupConnections();
    
    QListWidget* m_recentFilesList;
    QListWidget* m_templatesList;
    QCheckBox* m_showOnStartupCheckBox;
    QPushButton* m_openFileButton;
    QPushButton* m_newFileButton;
};

/**
 * @brief Status bar with extended information
 */
class ExtendedStatusBar : public QStatusBar {
    Q_OBJECT

public:
    explicit ExtendedStatusBar(QWidget* parent = nullptr);
    
    // Information display
    void setCursorPosition(int line, int column);
    void setSelectionInfo(int lines, int characters);
    void setEncoding(const QString& encoding);
    void setLanguage(const QString& language);
    void setGitStatus(const QString& branch, bool modified);
    void setSimulationStatus(const QString& status);
    
    // Progress
    void showProgress(const QString& message, int minimum = 0, int maximum = 0);
    void setProgressValue(int value);
    void hideProgress();

private:
    void setupWidgets();
    
    QLabel* m_cursorLabel;
    QLabel* m_selectionLabel;
    QLabel* m_encodingLabel;
    QLabel* m_languageLabel;
    QLabel* m_gitLabel;
    QLabel* m_simulationLabel;
    QProgressBar* m_progressBar;
};

/**
 * @brief System tray integration
 */
class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject* parent = nullptr);
    
    // Tray management
    void show();
    void hide();
    void showMessage(const QString& title, const QString& message,
                    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    
    // Status
    bool isVisible() const;

Q_SIGNALS:
    void activated(QSystemTrayIcon::ActivationReason reason);
    void showMessageClicked();

private:
    void setupTrayIcon();
    void createContextMenu();
    
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
};

/**
 * @brief Editor settings dialog with advanced options
 */
class EditorSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit EditorSettingsDialog(QWidget* parent = nullptr);
    
    // Settings categories
    void showFontSettings();
    void showColorSettings();
    void showKeybindingSettings();
    void showAdvancedSettings();

private Q_SLOTS:
    void onCategoryChanged(int index);
    void onApplyClicked();
    void onOkClicked();
    void onResetClicked();

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    
    // Font page
    void setupFontPage();
    
    // Color page
    void setupColorPage();
    
    // Keybinding page
    void setupKeybindingPage();
    
    // Advanced page
    void setupAdvancedPage();
    
    QStackedWidget* m_stackedWidget;
    QListWidget* m_categoryList;
    
    // Settings widgets
    QFontComboBox* m_fontCombo;
    QSpinBox* m_fontSizeSpin;
    QComboBox* m_themeCombo;
    
    QPushButton* m_applyButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
};

/**
 * @brief Quick open dialog for files and symbols
 */
class QuickOpenDialog : public QDialog {
    Q_OBJECT

public:
    explicit QuickOpenDialog(QWidget* parent = nullptr);
    
    // Search
    void setSearchPaths(const QStringList& paths);
    void setFiles(const QStringList& files);
    void setSymbols(const QStringList& symbols);

Q_SIGNALS:
    void fileSelected(const QString& filePath, int line = -1);
    void symbolSelected(const QString& symbolName);

private Q_SLOTS:
    void onSearchTextChanged(const QString& text);
    void onFileDoubleClicked(QListWidgetItem* item);
    void onEnterPressed();

private:
    void setupUI();
    void updateResults();
    QString highlightMatch(const QString& text, const QString& pattern);
    
    QLineEdit* m_searchEdit;
    QListWidget* m_resultsList;
    QLabel* m_infoLabel;
    
    QStringList m_files;
    QStringList m_symbols;
    QStringList m_filteredFiles;
    QStringList m_filteredSymbols;
};

/**
 * @brief Command palette for quick actions
 */
class CommandPalette : public QDialog {
    Q_OBJECT

public:
    explicit CommandPalette(QWidget* parent = nullptr);
    
    // Commands
    void registerCommand(const QString& name, const QString& description,
                        std::function<void()> callback);

Q_SIGNALS:
    void commandExecuted(QString command);

private Q_SLOTS:
    void onSearchTextChanged(const QString& text);
    void onCommandSelected(QListWidgetItem* item);

private:
    void setupUI();
    void updateCommands();
    
    struct Command {
        QString name;
        QString description;
        std::function<void()> callback;
    };
    
    QLineEdit* m_searchEdit;
    QListWidget* m_commandList;
    
    QList<Command> m_commands;
    QList<Command> m_filteredCommands;
};

/**
 * @brief Breadcrumb navigation for file hierarchy
 */
class BreadcrumbNavigation : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbNavigation(QWidget* parent = nullptr);
    
    // Navigation
    void setPath(const QString& path);
    QString currentPath() const;

Q_SIGNALS:
    void pathClicked(const QString& path);
    void pathChanged(const QString& path);

private:
    void updateBreadcrumb();
    void clearBreadcrumb();
    
    QHBoxLayout* m_layout;
    QString m_currentPath;
    QList<QLabel*> m_segments;
};

} // namespace Flux

#endif // FLUX_EDITOR_UI_POLISH_H
