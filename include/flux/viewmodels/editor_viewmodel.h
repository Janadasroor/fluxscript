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

#ifndef FLUX_VIEWMODELS_EDITOR_VIEWMODEL_H
#define FLUX_VIEWMODELS_EDITOR_VIEWMODEL_H

#include "flux/mvvm/viewmodel_base.h"
#include "flux/mvvm/observable_property.h"
#include "flux/mvvm/relay_command.h"
#include <memory>

namespace Flux {

class FluxDocument;

namespace MVVM {

/**
 * @brief Cursor position information
 */
struct CursorPosition {
    int line = 1;
    int column = 1;
    int selectionStart = -1;
    int selectionEnd = -1;
    int selectionLength = 0;
    
    bool hasSelection() const { return selectionLength > 0; }
};

/**
 * @brief Editor configuration
 */
struct EditorConfig {
    QString fontFamily = "Fira Code";
    int fontSize = 14;
    int tabWidth = 4;
    bool useSpacesForTabs = true;
    bool showLineNumbers = true;
    bool showMinimap = true;
    bool wordWrap = false;
    bool autoSave = false;
    int autoSaveInterval = 60;
};

/**
 * @brief ViewModel for the code editor
 * 
 * Provides data binding between the editor view and document model.
 * Follows MVVM pattern with observable properties and commands.
 */
class EditorViewModel : public ViewModelBase {
    Q_OBJECT
    
public:
    explicit EditorViewModel(QObject* parent = nullptr);
    ~EditorViewModel() override;

    // ========================================================================
    // Document Management
    // ========================================================================
    
    /**
     * @brief Current document
     */
    ObservableProperty<QString> CurrentDocumentPath;
    ObservableProperty<QString> CurrentDocumentContent;
    ObservableProperty<QString> CurrentDocumentTitle;
    ObservableProperty<bool> IsDocumentDirty;
    ObservableProperty<bool> IsDocumentReadOnly;
    
    // ========================================================================
    // Editor State
    // ========================================================================
    
    /**
     * @brief Current cursor position
     */
    ObservableProperty<CursorPosition> CursorPosition;
    ObservableProperty<int> CurrentLine;
    ObservableProperty<int> CurrentColumn;
    
    /**
     * @brief Editor configuration
     */
    ObservableProperty<EditorConfig> EditorConfiguration;
    ObservableProperty<QString> CurrentTheme;
    ObservableProperty<double> ZoomLevel;
    
    /**
     * @brief UI state
     */
    ObservableProperty<bool> HasFocus;
    ObservableProperty<bool> IsModified;
    ObservableProperty<QString> StatusMessage;
    ObservableProperty<int> TotalLines;
    ObservableProperty<int> TotalColumns;
    
    // ========================================================================
    // Search/Replace
    // ========================================================================
    
    ObservableProperty<QString> SearchText;
    ObservableProperty<QString> ReplaceText;
    ObservableProperty<bool> SearchCaseSensitive;
    ObservableProperty<bool> SearchRegex;
    ObservableProperty<bool> SearchWrapAround;
    ObservableProperty<int> SearchMatchIndex;
    ObservableProperty<int> SearchMatchCount;
    
    // ========================================================================
    // Commands
    // ========================================================================
    
    std::shared_ptr<RelayCommand> SaveCommand;
    std::shared_ptr<RelayCommand> SaveAsCommand;
    std::shared_ptr<RelayCommand> OpenCommand;
    std::shared_ptr<RelayCommand> NewCommand;
    std::shared_ptr<RelayCommand> CloseCommand;
    std::shared_ptr<RelayCommand> UndoCommand;
    std::shared_ptr<RelayCommand> RedoCommand;
    std::shared_ptr<RelayCommand> CutCommand;
    std::shared_ptr<RelayCommand> CopyCommand;
    std::shared_ptr<RelayCommand> PasteCommand;
    std::shared_ptr<RelayCommand> FindCommand;
    std::shared_ptr<RelayCommand> ReplaceCommand;
    std::shared_ptr<RelayCommand> GoToLineCommand;
    std::shared_ptr<RelayCommand> ToggleBreakpointCommand;
    std::shared_ptr<RelayCommand> RunCommand;
    std::shared_ptr<RelayCommand> BuildCommand;
    std::shared_ptr<RelayCommand> ZoomInCommand;
    std::shared_ptr<RelayCommand> ZoomOutCommand;
    std::shared_ptr<RelayCommand> ResetZoomCommand;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    void onLoad() override;
    void onUnload() override;

    // ========================================================================
    // Document Operations
    // ========================================================================
    
    /**
     * @brief Open a document from file
     * @param filePath Path to file
     * @return true if successful
     */
    Q_INVOKABLE bool openDocument(const QString& filePath);
    
    /**
     * @brief Create a new untitled document
     */
    Q_INVOKABLE void newDocument();
    
    /**
     * @brief Save the current document
     * @return true if successful
     */
    Q_INVOKABLE bool saveDocument();
    
    /**
     * @brief Save the current document with a new name
     * @param filePath New file path
     * @return true if successful
     */
    Q_INVOKABLE bool saveDocumentAs(const QString& filePath);
    
    /**
     * @brief Close the current document
     * @return true if successful (user confirmed discard if dirty)
     */
    Q_INVOKABLE bool closeCurrentDocument();
    
    /**
     * @brief Reload the current document from disk
     */
    Q_INVOKABLE void reloadDocument();

    // ========================================================================
    // Text Operations
    // ========================================================================
    
    /**
     * @brief Insert text at current cursor position
     * @param text Text to insert
     */
    Q_INVOKABLE void insertText(const QString& text);
    
    /**
     * @brief Get selected text
     * @return Selected text
     */
    Q_INVOKABLE QString selectedText() const;
    
    /**
     * @brief Set selected text
     * @param text New selected text (replaces selection)
     */
    Q_INVOKABLE void setSelectedText(const QString& text);
    
    /**
     * @brief Get text at a specific line
     * @param line Line number (1-based)
     * @return Line text
     */
    Q_INVOKABLE QString getLineText(int line) const;
    
    /**
     * @brief Set text at a specific line
     * @param line Line number (1-based)
     * @param text New line text
     */
    Q_INVOKABLE void setLineText(int line, const QString& text);
    
    /**
     * @brief Get total line count
     * @return Number of lines
     */
    Q_INVOKABLE int lineCount() const;

    // ========================================================================
    // Search Operations
    // ========================================================================
    
    /**
     * @brief Find next occurrence of search text
     * @return true if found
     */
    Q_INVOKABLE bool findNext();
    
    /**
     * @brief Find previous occurrence of search text
     * @return true if found
     */
    Q_INVOKABLE bool findPrevious();
    
    /**
     * @brief Replace current match
     * @return true if replaced
     */
    Q_INVOKABLE bool replaceCurrent();
    
    /**
     * @brief Replace all matches
     * @return Number of replacements
     */
    Q_INVOKABLE int replaceAll();
    
    /**
     * @brief Clear search results
     */
    Q_INVOKABLE void clearSearch();

    // ========================================================================
    // Navigation
    // ========================================================================
    
    /**
     * @brief Go to a specific line
     * @param line Line number (1-based)
     */
    Q_INVOKABLE void goToLine(int line);
    
    /**
     * @brief Go to beginning of file
     */
    Q_INVOKABLE void goToBeginning();
    
    /**
     * @brief Go to end of file
     */
    Q_INVOKABLE void goToEnd();
    
    /**
     * @brief Go to specific position
     * @param line Line number
     * @param column Column number
     */
    Q_INVOKABLE void goToPosition(int line, int column);

    // ========================================================================
    // Zoom
    // ========================================================================
    
    /**
     * @brief Zoom in
     */
    Q_INVOKABLE void zoomIn();
    
    /**
     * @brief Zoom out
     */
    Q_INVOKABLE void zoomOut();
    
    /**
     * @brief Reset zoom to 100%
     */
    Q_INVOKABLE void resetZoom();
    
    /**
     * @brief Set zoom level
     * @param level Zoom level (1.0 = 100%)
     */
    Q_INVOKABLE void setZoomLevel(double level);

    // ========================================================================
    // Validation
    // ========================================================================
    
    bool validate() override;
    bool hasErrors() const override;

signals:
    // ========================================================================
    // Editor Events
    // ========================================================================
    
    void documentOpened(const QString& filePath);
    void documentClosed();
    void documentSaved(const QString& filePath);
    void documentModified(bool isModified);
    void cursorPositionChanged(int line, int column);
    void selectionChanged(int start, int end);
    void contentChanged();
    void themeChanged(const QString& theme);
    void zoomChanged(double level);
    
    // Search events
    void searchStarted(const QString& text);
    void searchCompleted(int matchCount);
    void searchMatchChanged(int currentIndex, int totalCount);

protected:
    // ========================================================================
    // Protected Helpers
    // ========================================================================
    
    void setupCommands();
    void setupBindings();
    void updateCanExecute();
    void updateStatusMessage(const QString& message);
    void updateTitle();
    
    bool canSave() const;
    bool canUndo() const;
    bool canRedo() const;
    bool canFind() const;
    bool canGoToLine() const;

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    // Document reference (managed by DocumentManager)
    FluxDocument* m_currentDocument = nullptr;
    
    // Undo/Redo state
    bool m_canUndo = false;
    bool m_canRedo = false;
    
    // Search state
    int m_searchResultCount = 0;
    int m_currentSearchIndex = -1;
    
    // Internal document path
    QString m_documentPath;
};

} // namespace MVVM
} // namespace Flux

#endif // FLUX_VIEWMODELS_EDITOR_VIEWMODEL_H
