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

#ifndef FLUX_EDITOR_SETTINGS_H
#define FLUX_EDITOR_SETTINGS_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QSettings>
#include <memory>

namespace Flux {

struct EditorTheme;

/**
 * @brief Centralized settings management for the FluxScript editor
 * 
 * Provides a unified interface for all editor configuration options
 * with automatic persistence and change notification.
 */
class EditorSettings : public QObject {
    Q_OBJECT
    
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY settingsChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(int tabWidth READ tabWidth WRITE setTabWidth NOTIFY settingsChanged)
    Q_PROPERTY(bool useSpacesForTabs READ useSpacesForTabs WRITE setUseSpacesForTabs NOTIFY settingsChanged)
    Q_PROPERTY(bool showLineNumbers READ showLineNumbers WRITE setShowLineNumbers NOTIFY settingsChanged)
    Q_PROPERTY(bool showMinimap READ showMinimap WRITE setShowMinimap NOTIFY settingsChanged)
    Q_PROPERTY(QString currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY themeChanged)

public:
    explicit EditorSettings(QObject* parent = nullptr);
    ~EditorSettings() override;

    // ========================================================================
    // Singleton Access
    // ========================================================================
    static EditorSettings* instance();
    
    // ========================================================================
    // Font Settings
    // ========================================================================
    QString fontFamily() const;
    int fontSize() const;
    QFont font() const;
    
    // ========================================================================
    // Indentation Settings
    // ========================================================================
    int tabWidth() const;
    bool useSpacesForTabs() const;
    int indentWidth() const;
    
    // ========================================================================
    // Display Settings
    // ========================================================================
    bool showLineNumbers() const;
    bool showMinimap() const;
    bool showWhitespace() const;
    bool highlightCurrentLine() const;
    bool highlightMatchingBrackets() const;
    bool showFoldMarkers() const;
    bool showBookmarkMarkers() const;
    int minimapWidth() const;
    
    // ========================================================================
    // Behavior Settings
    // ========================================================================
    bool autoSave() const;
    int autoSaveInterval() const;  // in seconds
    bool confirmOnClose() const;
    bool confirmOnDiscard() const;
    bool restoreLastSession() const;
    int maxRecentFiles() const;
    bool enableCodeCompletion() const;
    int completionTriggerDelay() const;  // in milliseconds
    bool enableSnippets() const;
    bool enableFolding() const;
    
    // ========================================================================
    // Theme Settings
    // ========================================================================
    QString currentTheme() const;
    EditorTheme loadTheme(const QString& themeName);
    void saveTheme(const QString& themeName, const EditorTheme& theme);
    QStringList availableThemes() const;
    
    // ========================================================================
    // Key Bindings
    // ========================================================================
    // QKeySequence saveAction() const;
    // QKeySequence openAction() const;
    // QKeySequence buildAction() const;
    // QKeySequence runAction() const;
    // QKeySequence completionAction() const;
    
    // ========================================================================
    // Persistence
    // ========================================================================
    void load();
    void save();
    void resetToDefaults();
    QString settingsFilePath() const;
    
    // ========================================================================
    // Advanced Settings
    // ========================================================================
    bool enableSemanticHighlighting() const;
    int syntaxHighlightDelay() const;  // in milliseconds
    bool enableDiagnostics() const;
    bool showErrorsInline() const;
    int maxFileSize() const;  // in MB, 0 = unlimited

public slots:
    // ========================================================================
    // Setters
    // ========================================================================
    void setFontFamily(const QString& family);
    void setFontSize(int size);
    void setTabWidth(int width);
    void setUseSpacesForTabs(bool use);
    void setShowLineNumbers(bool show);
    void setShowMinimap(bool show);
    void setShowWhitespace(bool show);
    void setHighlightCurrentLine(bool highlight);
    void setHighlightMatchingBrackets(bool highlight);
    void setAutoSave(bool enabled);
    void setAutoSaveInterval(int seconds);
    void setCurrentTheme(const QString& theme);
    void setEnableCodeCompletion(bool enabled);
    void setEnableFolding(bool enabled);

signals:
    // ========================================================================
    // Change Notifications
    // ========================================================================
    void settingsChanged();
    void themeChanged();
    void fontChanged();
    void behaviorChanged();

private:
    // ========================================================================
    // Initialization
    // ========================================================================
    void setupDefaults();
    void ensureSettingsFile();
    
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    QSettings* createSettings() const;
    void applyFont();
    
    // ========================================================================
    // Members
    // ========================================================================
    QString m_fontFamily;
    int m_fontSize;
    int m_tabWidth;
    bool m_useSpacesForTabs;
    
    bool m_showLineNumbers;
    bool m_showMinimap;
    bool m_showWhitespace;
    bool m_highlightCurrentLine;
    bool m_highlightMatchingBrackets;
    bool m_showFoldMarkers;
    int m_minimapWidth;
    
    bool m_autoSave;
    int m_autoSaveInterval;
    bool m_confirmOnClose;
    bool m_confirmOnDiscard;
    bool m_restoreLastSession;
    int m_maxRecentFiles;
    bool m_enableCodeCompletion;
    int m_completionTriggerDelay;
    bool m_enableSnippets;
    bool m_enableFolding;
    
    QString m_currentTheme;
    
    bool m_enableSemanticHighlighting;
    int m_syntaxHighlightDelay;
    bool m_enableDiagnostics;
    bool m_showErrorsInline;
    int m_maxFileSize;
    
    static EditorSettings* s_instance;
};

} // namespace Flux

#endif // FLUX_EDITOR_SETTINGS_H
