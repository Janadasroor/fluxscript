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

#ifndef FLUX_EDITOR_H
#define FLUX_EDITOR_H

#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QMap>
#include <QList>
#include <QPointer>
#include <memory>

class QCompleter;
class QPaintEvent;
class QKeyEvent;
class QResizeEvent;
class QMouseEvent;

namespace Flux {

class Highlighter;
class LineNumberArea;
class GutterMarker;
class FoldRegion;

/**
 * @brief Gutter marker types for visual indicators in the line number area
 */
enum class GutterMarkerType {
    Breakpoint,
    Error,
    Warning,
    Info,
    Bookmark,
    ExecutionPoint
};

/**
 * @brief Visual marker displayed in the gutter (line number area)
 */
class GutterMarker {
public:
    GutterMarker(GutterMarkerType type, const QString& tooltip = QString());
    
    GutterMarkerType type() const { return m_type; }
    QString tooltip() const { return m_tooltip; }
    QColor color() const;
    
    virtual ~GutterMarker() = default;
    virtual void paint(QPainter* painter, const QRect& rect);
    virtual QSize sizeHint() const;
    
private:
    GutterMarkerType m_type;
    QString m_tooltip;
};

/**
 * @brief Code folding region information
 */
struct FoldRegion {
    int startLine;
    int endLine;
    bool isFolded;
    QString foldText;  // Optional text to show when folded
};

/**
 * @brief High-performance code editor for FluxScript with advanced features.
 * 
 * Features:
 * - Syntax highlighting integration
 * - Code completion support
 * - Line numbers and folding
 * - Breakpoint and error markers
 * - Current line and debug line highlighting
 * - Bracket matching
 * - Minimap integration
 */
class FluxEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit FluxEditor(QWidget* parent = nullptr);
    ~FluxEditor() override;

    // ========================================================================
    // Completion
    // ========================================================================
    void setCompleter(QCompleter* completer);
    QCompleter* completer() const;
    virtual void updateCompletionKeywords(const QStringList& keywords);

    // ========================================================================
    // Diagnostics & Error Display
    // ========================================================================
    virtual void setErrorLines(const QMap<int, QString>& errors);
    virtual void setWarningLines(const QMap<int, QString>& warnings);
    void clearDiagnostics();
    
    // ========================================================================
    // Debugging Support
    // ========================================================================
    virtual void setActiveDebugLine(int line);
    void clearDebugLine();
    
    // Breakpoint management
    bool toggleBreakpoint(int line);
    bool hasBreakpoint(int line) const;
    void setBreakpoint(int line, bool enabled);
    void clearAllBreakpoints();
    QList<int> breakpointLines() const;
    
    // ========================================================================
    // Code Folding
    // ========================================================================
    void toggleFold(int line);
    void foldRegion(int startLine, int endLine);
    void unfoldRegion(int line);
    void foldAll();
    void unfoldAll();
    bool isLineVisible(int line) const;
    
    // ========================================================================
    // Navigation
    // ========================================================================
    void goToLine(int line);
    void goToBeginning();
    void goToEnd();
    void findAndHighlight(const QString& text, Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);
    void findNext();
    void findPrevious();
    
    // ========================================================================
    // Gutter Markers
    // ========================================================================
    void addGutterMarker(int line, GutterMarker* marker);
    void removeGutterMarker(int line, GutterMarkerType type);
    void clearGutterMarkers();
    QList<GutterMarker*> gutterMarkersAt(int line) const;
    
    // ========================================================================
    // Line Number Area
    // ========================================================================
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth();

    // ========================================================================
    // Visual Helpers
    // ========================================================================
    void highlightCurrentLine();
    void highlightMatchingBracket();
    void showWhitespace(bool show);
    
    // ========================================================================
    // Minimap Integration
    // ========================================================================
    void setMinimapEnabled(bool enabled);
    bool isMinimapEnabled() const;
    void triggerMinimapUpdate();

signals:
    // ========================================================================
    // Editor Signals
    // ========================================================================
    void breakpointToggled(int line, bool enabled);
    void cursorLineChanged(int line);
    void cursorColumnChanged(int column);
    void documentModified();
    void documentSaved();
    
    // Navigation signals
    void goToDefinitionRequested(const QString& symbol);
    void findReferencesRequested(const QString& symbol);
    
    // Build/Run signals
    void runRequested();
    void buildRequested();

public slots:
    virtual void onRunRequested();
    virtual void onBuildRequested();
    void onDocumentSaved();

protected:
    // ========================================================================
    // Event Handlers
    // ========================================================================
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);
    void onCursorPositionChanged();

private:
    // ========================================================================
    // Initialization
    // ========================================================================
    void setupEditor();
    void setupCompleter();
    void setupConnections();
    void setupShortcuts();
    
    // ========================================================================
    // Completion
    // ========================================================================
    QString textUnderCursor() const;
    void insertCompletion(const QString& completion);
    void showCompletionPopup();
    bool completionPopupVisible() const;
    
    // ========================================================================
    // Folding
    // ========================================================================
    void updateFoldRegions();
    int findFoldEndLine(int startLine) const;
    FoldRegion* foldRegionAt(int line) const;
    
    // ========================================================================
    // Visual Helpers
    // ========================================================================
    void updateExtraSelections();
    QColor markerColorForType(GutterMarkerType type) const;
    void drawGutterMarkers(QPainter* painter, int line, int y, int width);

    // ========================================================================
    // Context Menu Helpers
    // ========================================================================
    void toggleLineComment();
    void toggleBlockComment();
    void toggleCommentForSelection();
    void selectCurrentLine();
    void selectCurrentWord();
    void showGoToLineDialog();
    void goToMatchingBrace();
    void showPasteHistory();
    void duplicateLine();
    void deleteCurrentLine();
    void moveLineUp();
    void moveLineDown();
    void sortSelectedLines();
    void unindentLine();

    // ========================================================================
    // Members
    // ========================================================================
    Highlighter* m_highlighter;
    QCompleter* m_completer;
    LineNumberArea* m_lineNumberArea;
    
    // Diagnostics
    QMap<int, QString> m_errorLines;
    QMap<int, QString> m_warningLines;
    
    // Debugging
    int m_activeDebugLine;
    QMap<int, bool> m_breakpoints;  // line -> enabled
    
    // Folding
    QList<FoldRegion> m_foldRegions;
    
    // Gutter markers
    QMap<int, QList<GutterMarker*>> m_gutterMarkers;
    
    // Visual state
    bool m_showWhitespace;
    bool m_minimapEnabled;
    bool m_needsFoldUpdate;
    
    // Find/replace state
    QString m_findText;
    QList<QTextEdit::ExtraSelection> m_findSelections;
    int m_currentFindIndex;
};

/**
 * @brief Line number area widget displayed in the gutter
 */
class LineNumberArea : public QWidget {
    Q_OBJECT
    
public:
    explicit LineNumberArea(FluxEditor* editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    FluxEditor* m_codeEditor;
};

} // namespace Flux

#endif // FLUX_EDITOR_H
