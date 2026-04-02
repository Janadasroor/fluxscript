#include "flux/ui/flux_editor.h"
#include "flux/ui/flux_highlighter.h"
#include "flux/ui/flux_completer.h"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QScrollBar>
#include <QCompleter>
#include <QAbstractItemView>
#include <QShortcut>
#include <QToolTip>
#include <QTextBlock>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QStringListModel>
#include <QInputDialog>

namespace Flux {

// ============================================================================
// GutterMarker Implementation
// ============================================================================

GutterMarker::GutterMarker(GutterMarkerType type, const QString& tooltip)
    : m_type(type)
    , m_tooltip(tooltip)
{
}

QColor GutterMarker::color() const
{
    switch (m_type) {
    case GutterMarkerType::Breakpoint:
        return QColor(220, 50, 50);
    case GutterMarkerType::Error:
        return QColor(255, 100, 100);
    case GutterMarkerType::Warning:
        return QColor(255, 200, 50);
    case GutterMarkerType::Info:
        return QColor(100, 150, 255);
    case GutterMarkerType::Bookmark:
        return QColor(50, 200, 50);
    case GutterMarkerType::ExecutionPoint:
        return QColor(255, 150, 0);
    default:
        return QColor(150, 150, 150);
    }
}

void GutterMarker::paint(QPainter* painter, const QRect& rect)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    switch (m_type) {
    case GutterMarkerType::Breakpoint: {
        painter->setBrush(color());
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(rect.adjusted(2, 2, -2, -2));
        break;
    }
    case GutterMarkerType::Error: {
        painter->setPen(QPen(color(), 2));
        painter->drawLine(rect.topLeft(), rect.bottomRight());
        painter->drawLine(rect.topRight(), rect.bottomLeft());
        break;
    }
    case GutterMarkerType::Warning: {
        painter->setPen(QPen(color(), 2));
        QPolygon triangle;
        triangle << QPoint(rect.center().x(), rect.top() + 2)
                 << QPoint(rect.left() + 2, rect.bottom() - 2)
                 << QPoint(rect.right() - 2, rect.bottom() - 2);
        painter->drawPolygon(triangle);
        break;
    }
    case GutterMarkerType::ExecutionPoint: {
        painter->setBrush(color());
        painter->setPen(Qt::NoPen);
        QPolygon arrow;
        arrow << rect.topLeft() << rect.topRight() << QPoint(rect.center().x(), rect.bottom());
        painter->drawPolygon(arrow);
        break;
    }
    default: {
        painter->setBrush(color());
        painter->setPen(Qt::NoPen);
        painter->drawRect(rect.adjusted(3, 3, -3, -3));
        break;
    }
    }

    painter->restore();
}

QSize GutterMarker::sizeHint() const
{
    return QSize(16, 16);
}

// ============================================================================
// LineNumberArea Implementation
// ============================================================================

LineNumberArea::LineNumberArea(FluxEditor* editor)
    : QWidget(editor)
    , m_codeEditor(editor)
{
    setAutoFillBackground(false);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    m_codeEditor->lineNumberAreaPaintEvent(event);
}

// ============================================================================
// FluxEditor Implementation
// ============================================================================

FluxEditor::FluxEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_highlighter(nullptr)
    , m_completer(nullptr)
    , m_lineNumberArea(nullptr)
    , m_activeDebugLine(-1)
    , m_showWhitespace(false)
    , m_minimapEnabled(false)
    , m_needsFoldUpdate(true)
    , m_currentFindIndex(-1)
{
    setupEditor();
    setupConnections();
    setupShortcuts();
}

FluxEditor::~FluxEditor()
{
    clearGutterMarkers();
    clearAllBreakpoints();
}

void FluxEditor::setupEditor()
{
    // Create line number area
    m_lineNumberArea = new LineNumberArea(this);

    // Set monospace font
    QFont font("Fira Code", 12);
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    // Set tab settings
    setTabStopDistance(QFontMetricsF(font).horizontalAdvance(' ') * 4);

    // Enable line wrapping (optional)
    setLineWrapMode(QPlainTextEdit::NoWrap);

    // Set minimum size
    setMinimumSize(400, 300);

    // Setup highlighter
    m_highlighter = new Highlighter(document());
    
    // Setup completer
    setupCompleter();

    // Update gutter width
    updateLineNumberAreaWidth(0);
}

void FluxEditor::setupCompleter()
{
    auto* completer = new Flux::Completer(this);
    completer->updateFluxKeywords();
    completer->updateFluxBuiltins();
    setCompleter(completer);
}

void FluxEditor::setupConnections()
{
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &FluxEditor::onCursorPositionChanged);
    connect(this, &QPlainTextEdit::modificationChanged,
            this, &FluxEditor::documentModified);
    connect(this, &QPlainTextEdit::textChanged,
            this, [this]() { m_needsFoldUpdate = true; });
}

void FluxEditor::setupShortcuts()
{
    // Ctrl+Space for completion
    auto* completionShortcut = new QShortcut(QKeySequence("Ctrl+Space"), this);
    connect(completionShortcut, &QShortcut::activated, this, [this]() {
        if (m_completer) {
            showCompletionPopup();
        }
    });

    // F5 for run
    auto* runShortcut = new QShortcut(QKeySequence("F5"), this);
    connect(runShortcut, &QShortcut::activated, this, &FluxEditor::onRunRequested);

    // Ctrl+F5 for build
    auto* buildShortcut = new QShortcut(QKeySequence("Ctrl+F5"), this);
    connect(buildShortcut, &QShortcut::activated, this, &FluxEditor::onBuildRequested);

    // F11 for toggle breakpoint
    auto* breakpointShortcut = new QShortcut(QKeySequence("F11"), this);
    connect(breakpointShortcut, &QShortcut::activated, this, [this]() {
        int line = textCursor().blockNumber() + 1;
        toggleBreakpoint(line);
    });

    // Ctrl+F for find
    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        // Could open a find dialog
        findAndHighlight(m_findText);
    });

    // F3 for find next
    auto* findNextShortcut = new QShortcut(QKeySequence::FindNext, this);
    connect(findNextShortcut, &QShortcut::activated, this, &FluxEditor::findNext);

    // Shift+F3 for find previous
    auto* findPrevShortcut = new QShortcut(QKeySequence::FindPrevious, this);
    connect(findPrevShortcut, &QShortcut::activated, this, &FluxEditor::findPrevious);

    // Ctrl++ for fold
    auto* foldShortcut = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(foldShortcut, &QShortcut::activated, this, [this]() {
        toggleFold(textCursor().blockNumber() + 1);
    });

    // Ctrl+- for unfold
    auto* unfoldShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(unfoldShortcut, &QShortcut::activated, this, [this]() {
        unfoldRegion(textCursor().blockNumber() + 1);
    });

    // Ctrl+Shift+0 for fold all
    auto* foldAllShortcut = new QShortcut(QKeySequence("Ctrl+Shift+0"), this);
    connect(foldAllShortcut, &QShortcut::activated, this, &FluxEditor::foldAll);

    // Ctrl+Shift+9 for unfold all
    auto* unfoldAllShortcut = new QShortcut(QKeySequence("Ctrl+Shift+9"), this);
    connect(unfoldAllShortcut, &QShortcut::activated, this, &FluxEditor::unfoldAll);

    // Ctrl+G for go to line
    auto* goToLineShortcut = new QShortcut(QKeySequence("Ctrl+G"), this);
    connect(goToLineShortcut, &QShortcut::activated, this, [this]() {
        // Could open a go-to-line dialog
        goToLine(textCursor().blockNumber() + 1);
    });

    // F2 for go to definition
    auto* goToDefShortcut = new QShortcut(QKeySequence("F2"), this);
    connect(goToDefShortcut, &QShortcut::activated, this, [this]() {
        QString symbol = textUnderCursor();
        if (!symbol.isEmpty()) {
            emit goToDefinitionRequested(symbol);
        }
    });

    // Shift+F12 for find references
    auto* findRefsShortcut = new QShortcut(QKeySequence("Shift+F12"), this);
    connect(findRefsShortcut, &QShortcut::activated, this, [this]() {
        QString symbol = textUnderCursor();
        if (!symbol.isEmpty()) {
            emit findReferencesRequested(symbol);
        }
    });
}

void FluxEditor::setCompleter(QCompleter* completer)
{
    if (m_completer) {
        disconnect(m_completer, nullptr, this, nullptr);
    }

    m_completer = completer;

    if (!m_completer) {
        return;
    }

    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);

    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated),
            this, &FluxEditor::insertCompletion);
}

QCompleter* FluxEditor::completer() const
{
    return m_completer;
}

void FluxEditor::updateCompletionKeywords(const QStringList& keywords)
{
    if (m_completer) {
        auto* model = qobject_cast<QStringListModel*>(m_completer->model());
        if (model) {
            model->setStringList(keywords);
        }
    }
}

void FluxEditor::setErrorLines(const QMap<int, QString>& errors)
{
    m_errorLines = errors;
    updateExtraSelections();
}

void FluxEditor::setWarningLines(const QMap<int, QString>& warnings)
{
    m_warningLines = warnings;
    updateExtraSelections();
}

void FluxEditor::clearDiagnostics()
{
    m_errorLines.clear();
    m_warningLines.clear();
    updateExtraSelections();
}

void FluxEditor::setActiveDebugLine(int line)
{
    m_activeDebugLine = line;
    updateExtraSelections();
    highlightCurrentLine();
}

void FluxEditor::clearDebugLine()
{
    m_activeDebugLine = -1;
    updateExtraSelections();
}

bool FluxEditor::toggleBreakpoint(int line)
{
    bool enabled = hasBreakpoint(line);
    setBreakpoint(line, !enabled);
    return !enabled;
}

bool FluxEditor::hasBreakpoint(int line) const
{
    return m_breakpoints.value(line, false);
}

void FluxEditor::setBreakpoint(int line, bool enabled)
{
    if (enabled) {
        m_breakpoints[line] = true;

        // Add gutter marker
        auto* marker = new GutterMarker(GutterMarkerType::Breakpoint);
        addGutterMarker(line, marker);

        emit breakpointToggled(line, true);
    } else {
        m_breakpoints.remove(line);
        removeGutterMarker(line, GutterMarkerType::Breakpoint);
        emit breakpointToggled(line, false);
    }
}

void FluxEditor::clearAllBreakpoints()
{
    m_breakpoints.clear();

    for (auto it = m_gutterMarkers.begin(); it != m_gutterMarkers.end(); ++it) {
        int line = it.key();
        removeGutterMarker(line, GutterMarkerType::Breakpoint);
    }
}

QList<int> FluxEditor::breakpointLines() const
{
    return m_breakpoints.keys();
}

void FluxEditor::toggleFold(int line)
{
    FoldRegion* region = foldRegionAt(line);
    if (region) {
        region->isFolded = !region->isFolded;
        updateFoldRegions();
    }
}

void FluxEditor::foldRegion(int startLine, int endLine)
{
    FoldRegion region;
    region.startLine = startLine;
    region.endLine = endLine;
    region.isFolded = true;
    m_foldRegions.append(region);
    updateFoldRegions();
}

void FluxEditor::unfoldRegion(int line)
{
    FoldRegion* region = foldRegionAt(line);
    if (region) {
        region->isFolded = false;
        updateFoldRegions();
    }
}

void FluxEditor::foldAll()
{
    updateFoldRegions();
    for (auto& region : m_foldRegions) {
        region.isFolded = true;
    }
    updateFoldRegions();
}

void FluxEditor::unfoldAll()
{
    m_foldRegions.clear();
    updateFoldRegions();
}

bool FluxEditor::isLineVisible(int line) const
{
    for (const auto& region : m_foldRegions) {
        if (region.isFolded && line > region.startLine && line <= region.endLine) {
            return false;
        }
    }
    return true;
}

void FluxEditor::goToLine(int line)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
    setTextCursor(cursor);
    centerCursor();
}

void FluxEditor::goToBeginning()
{
    goToLine(1);
}

void FluxEditor::goToEnd()
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
    centerCursor();
}

void FluxEditor::findAndHighlight(const QString& text, Qt::CaseSensitivity caseSensitivity)
{
    if (text.isEmpty()) {
        return;
    }

    m_findText = text;
    m_findSelections.clear();
    m_currentFindIndex = -1;

    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::Start);
    
    QTextDocument::FindFlags flags;
    // In Qt6, we need to manually handle case sensitivity
    // by using the appropriate search pattern

    int count = 0;
    while (!cursor.isNull()) {
        QTextCursor found;
        if (caseSensitivity == Qt::CaseInsensitive) {
            // Case insensitive search using regex
            QRegularExpression regex(QRegularExpression::escape(text), 
                                     QRegularExpression::CaseInsensitiveOption);
            found = document()->find(regex, cursor);
        } else {
            found = document()->find(text, cursor);
        }
        
        if (!found.isNull()) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(QColor(255, 255, 0, 100));
            selection.format.setForeground(Qt::black);
            selection.cursor = found;
            m_findSelections.append(selection);
            count++;
            cursor = found;
            cursor.setPosition(cursor.position() + 1);  // Move forward to avoid infinite loop
        } else {
            break;
        }
    }

    updateExtraSelections();

    if (count > 0) {
        m_currentFindIndex = 0;
        setTextCursor(m_findSelections[0].cursor);
    }
}

void FluxEditor::findNext()
{
    if (m_findSelections.isEmpty()) {
        return;
    }

    m_currentFindIndex = (m_currentFindIndex + 1) % m_findSelections.size();
    setTextCursor(m_findSelections[m_currentFindIndex].cursor);
}

void FluxEditor::findPrevious()
{
    if (m_findSelections.isEmpty()) {
        return;
    }

    m_currentFindIndex = (m_currentFindIndex - 1 + m_findSelections.size()) % m_findSelections.size();
    setTextCursor(m_findSelections[m_currentFindIndex].cursor);
}

void FluxEditor::addGutterMarker(int line, GutterMarker* marker)
{
    if (!marker) {
        return;
    }

    m_gutterMarkers[line].append(marker);
    m_lineNumberArea->update();
}

void FluxEditor::removeGutterMarker(int line, GutterMarkerType type)
{
    auto& markers = m_gutterMarkers[line];
    for (auto it = markers.begin(); it != markers.end(); ++it) {
        if ((*it)->type() == type) {
            delete *it;
            markers.erase(it);
            break;
        }
    }

    if (markers.isEmpty()) {
        m_gutterMarkers.remove(line);
    }

    m_lineNumberArea->update();
}

void FluxEditor::clearGutterMarkers()
{
    for (auto& markers : m_gutterMarkers) {
        qDeleteAll(markers);
    }
    m_gutterMarkers.clear();
    m_lineNumberArea->update();
}

QList<GutterMarker*> FluxEditor::gutterMarkersAt(int line) const
{
    return m_gutterMarkers.value(line);
}

int FluxEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int width = 20 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    // Add space for gutter markers
    width += 20;

    return width;
}

void FluxEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(40, 40, 40));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    int width = lineNumberAreaWidth() - 20;

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            // Highlight current line number
            if (blockNumber == textCursor().blockNumber()) {
                painter.setPen(QColor(200, 200, 200));
            } else {
                painter.setPen(QColor(120, 120, 120));
            }

            painter.drawText(0, top, width, bottom - top,
                             Qt::AlignRight | Qt::AlignVCenter, number);

            // Draw gutter markers
            drawGutterMarkers(&painter, blockNumber, top, 16);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void FluxEditor::drawGutterMarkers(QPainter* painter, int line, int y, int width)
{
    auto markers = gutterMarkersAt(line);
    int yOffset = y + 2;

    for (GutterMarker* marker : markers) {
        QRect markerRect(lineNumberAreaWidth() - width - 2, yOffset, width, width);
        marker->paint(painter, markerRect);
        yOffset += width + 2;
    }
}

void FluxEditor::highlightCurrentLine()
{
    updateExtraSelections();
}

void FluxEditor::highlightMatchingBracket()
{
    // Implementation for bracket matching
    QTextCursor cursor = textCursor();
    if (cursor.atEnd()) {
        return;
    }

    cursor.setPosition(cursor.position());
    if (cursor.block().isValid()) {
        QString text = cursor.block().text();
        int pos = cursor.positionInBlock();

        if (pos < text.length()) {
            QChar ch = text[pos];
            if (ch == '(' || ch == ')' || ch == '[' || ch == ']' ||
                ch == '{' || ch == '}') {
                // Find matching bracket and highlight
                // (Implementation would go here)
            }
        }
    }
}

void FluxEditor::showWhitespace(bool show)
{
    m_showWhitespace = show;
    updateExtraSelections();
}

void FluxEditor::setMinimapEnabled(bool enabled)
{
    m_minimapEnabled = enabled;
    triggerMinimapUpdate();
}

bool FluxEditor::isMinimapEnabled() const
{
    return m_minimapEnabled;
}

void FluxEditor::triggerMinimapUpdate()
{
    if (m_minimapEnabled) {
        // Emit signal or notify minimap widget
    }
}

void FluxEditor::onRunRequested()
{
    emit runRequested();
}

void FluxEditor::onBuildRequested()
{
    emit buildRequested();
}

void FluxEditor::onDocumentSaved()
{
    emit documentSaved();
}

void FluxEditor::keyPressEvent(QKeyEvent* e)
{
    // Handle completion popup
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
            if (m_completer->currentIndex().isValid()) {
                insertCompletion(m_completer->currentCompletion());
                return;
            }
            break;
        case Qt::Key_Escape:
            m_completer->popup()->hide();
            return;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            // Let completer handle navigation
            QPlainTextEdit::keyPressEvent(e);
            return;
        default:
            break;
        }
    }

    // Handle Tab for indentation
    if (e->key() == Qt::Key_Tab && !e->modifiers()) {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            // Indent selection
            cursor.beginEditBlock();
            QTextBlock startBlock = document()->findBlock(cursor.selectionStart());
            QTextBlock endBlock = document()->findBlock(cursor.selectionEnd());
            
            QTextBlock block = startBlock;
            while (block.isValid()) {
                cursor.setPosition(block.position());
                cursor.insertText("    ");  // 4 spaces
                if (block == endBlock || block.next().position() >= cursor.selectionEnd()) {
                    break;
                }
                block = block.next();
            }
            cursor.endEditBlock();
            return;
        }
    }

    // Handle Enter for auto-indent
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        QTextCursor cursor = textCursor();
        QTextBlock block = cursor.block();
        QString text = block.text();
        
        // Calculate indentation from previous line
        int indent = 0;
        for (QChar ch : text) {
            if (ch == ' ' || ch == '\t') {
                indent++;
            } else {
                break;
            }
        }
        
        // Check if we should add extra indent (e.g., after opening brace)
        if (text.trimmed().endsWith('{')) {
            indent += 4;
        }
        
        cursor.beginEditBlock();
        cursor.insertText("\n" + QString(indent, ' '));
        cursor.endEditBlock();
        return;
    }

    QPlainTextEdit::keyPressEvent(e);

    // Trigger completion on character typed
    if (m_completer && e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) {
        showCompletionPopup();
    }
}

void FluxEditor::keyReleaseEvent(QKeyEvent* e)
{
    QPlainTextEdit::keyReleaseEvent(e);

    // Update completion based on context
    if (m_completer) {
        QString word = textUnderCursor();
        if (word.length() >= 2) {
            showCompletionPopup();
        }
    }
}

void FluxEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void FluxEditor::mousePressEvent(QMouseEvent* e)
{
    // Handle Ctrl+Click for go to definition
    if (e->modifiers() & Qt::ControlModifier) {
        QString symbol = textUnderCursor();
        if (!symbol.isEmpty()) {
            emit goToDefinitionRequested(symbol);
            return;
        }
    }

    // Handle gutter click for breakpoints
    if (e->button() == Qt::LeftButton) {
        int x = e->pos().x();
        if (x < lineNumberAreaWidth()) {
            int line = cursorForPosition(e->pos()).blockNumber() + 1;
            toggleBreakpoint(line);
            return;
        }
    }

    QPlainTextEdit::mousePressEvent(e);
}

void FluxEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    // Select word under cursor
    if (e->button() == Qt::LeftButton) {
        QTextCursor cursor = cursorForPosition(e->pos());
        QString word = textUnderCursor();
        if (!word.isEmpty()) {
            // Select the word
            cursor.select(QTextCursor::WordUnderCursor);
            setTextCursor(cursor);
        }
    }

    QPlainTextEdit::mouseDoubleClickEvent(e);
}

void FluxEditor::wheelEvent(QWheelEvent* e)
{
    // Handle Ctrl+Wheel for zoom
    if (e->modifiers() & Qt::ControlModifier) {
        int delta = e->angleDelta().y();
        QFont font = this->font();
        int size = font.pointSize();

        if (delta > 0) {
            size = qMin(size + 1, 72);
        } else {
            size = qMax(size - 1, 8);
        }

        font.setPointSize(size);
        setFont(font);
        e->accept();
        return;
    }

    QPlainTextEdit::wheelEvent(e);
}

void FluxEditor::focusInEvent(QFocusEvent* e)
{
    QPlainTextEdit::focusInEvent(e);
    highlightCurrentLine();
}

void FluxEditor::paintEvent(QPaintEvent* e)
{
    QPlainTextEdit::paintEvent(e);

    // Draw viewport indicator for debug line
    if (m_activeDebugLine >= 0) {
        QPainter painter(viewport());
        QTextBlock block = document()->findBlockByNumber(m_activeDebugLine);
        if (block.isValid()) {
            QRect blockRect = blockBoundingGeometry(block).translated(contentOffset()).toRect();
            painter.fillRect(blockRect, QColor(255, 150, 0, 50));
        }
    }
}

void FluxEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void FluxEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void FluxEditor::onCursorPositionChanged()
{
    m_lineNumberArea->update();
    highlightCurrentLine();
    highlightMatchingBracket();

    int line = textCursor().blockNumber() + 1;
    int column = textCursor().positionInBlock();

    emit cursorLineChanged(line);
    emit cursorColumnChanged(column);
}

QString FluxEditor::textUnderCursor() const
{
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText().trimmed();
}

void FluxEditor::insertCompletion(const QString& completion)
{
    if (m_completer) {
        m_completer->popup()->hide();
    }

    QTextCursor cursor = textCursor();
    int extra = completion.length() - textUnderCursor().length();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, textUnderCursor().length());
    cursor.insertText(completion);
    setTextCursor(cursor);
}

void FluxEditor::showCompletionPopup()
{
    if (!m_completer) {
        return;
    }

    QString word = textUnderCursor();
    if (word.isEmpty()) {
        return;
    }

    m_completer->setCompletionPrefix(word);
    QRect popupRect = cursorRect();
    popupRect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                       m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(popupRect);
}

bool FluxEditor::completionPopupVisible() const
{
    return m_completer && m_completer->popup()->isVisible();
}

void FluxEditor::updateFoldRegions()
{
    if (!m_needsFoldUpdate) {
        return;
    }

    // Detect foldable regions (blocks enclosed in braces)
    m_foldRegions.clear();

    QTextBlock block = document()->firstBlock();
    int braceDepth = 0;
    int foldStart = -1;

    while (block.isValid()) {
        QString text = block.text();

        for (QChar ch : text) {
            if (ch == '{') {
                if (braceDepth == 0) {
                    foldStart = block.blockNumber();
                }
                braceDepth++;
            } else if (ch == '}') {
                braceDepth--;
                if (braceDepth == 0 && foldStart >= 0) {
                    FoldRegion region;
                    region.startLine = foldStart;
                    region.endLine = block.blockNumber();
                    region.isFolded = false;
                    m_foldRegions.append(region);
                    foldStart = -1;
                }
            }
        }

        block = block.next();
    }

    m_needsFoldUpdate = false;
}

int FluxEditor::findFoldEndLine(int startLine) const
{
    for (const auto& region : m_foldRegions) {
        if (region.startLine == startLine) {
            return region.endLine;
        }
    }
    return startLine;
}

Flux::FoldRegion* FluxEditor::foldRegionAt(int line) const
{
    for (auto& region : const_cast<QList<FoldRegion>&>(m_foldRegions)) {
        if (line >= region.startLine && line <= region.endLine) {
            return &region;
        }
    }
    return nullptr;
}

void FluxEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> extra;

    // Current line highlight
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor(50, 50, 60);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extra.append(selection);
    }

    // Debug line highlight
    if (m_activeDebugLine >= 0) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(255, 150, 0, 80));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        QTextCursor cursor(document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, m_activeDebugLine);
        selection.cursor = cursor;
        extra.append(selection);
    }

    // Error lines
    for (auto it = m_errorLines.begin(); it != m_errorLines.end(); ++it) {
        QTextEdit::ExtraSelection selection;
        selection.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        selection.format.setUnderlineColor(QColor(255, 100, 100));
        selection.format.setToolTip(it.value());
        QTextCursor cursor(document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, it.key());
        selection.cursor = cursor;
        extra.append(selection);
    }

    // Warning lines
    for (auto it = m_warningLines.begin(); it != m_warningLines.end(); ++it) {
        QTextEdit::ExtraSelection selection;
        selection.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        selection.format.setUnderlineColor(QColor(255, 200, 50));
        selection.format.setToolTip(it.value());
        QTextCursor cursor(document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, it.key());
        selection.cursor = cursor;
        extra.append(selection);
    }

    // Find selections
    extra.append(m_findSelections);

    setExtraSelections(extra);
}

QColor FluxEditor::markerColorForType(GutterMarkerType type) const
{
    GutterMarker marker(type);
    return marker.color();
}

// ============================================================================
// Context Menu Implementation
// ============================================================================

void FluxEditor::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    menu->setStyleSheet(
        "QMenu {"
        "    background-color: #252525;"
        "    color: #C8C8C8;"
        "    border: 1px solid #454545;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    padding: 6px 32px 6px 16px;"
        "    border: none;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #3A5A8A;"
        "    color: #FFFFFF;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #454545;"
        "    margin: 4px 8px;"
        "}"
    );

    // Get cursor position and selected text
    QTextCursor cursor = cursorForPosition(event->pos());
    QString selectedText = textCursor().selectedText();
    QString wordUnderCursor = textUnderCursor();

    // Build custom context menu
    QList<QAction*> actions = menu->actions();
    
    // Add "Go to Definition" at the top if there's a word under cursor
    if (!wordUnderCursor.isEmpty()) {
        QAction* goToDefAction = new QAction("Go to Definition", menu);
        goToDefAction->setShortcut(QKeySequence("F2"));
        connect(goToDefAction, &QAction::triggered, this, [this, wordUnderCursor]() {
            emit goToDefinitionRequested(wordUnderCursor);
        });
        
        QAction* findRefsAction = new QAction("Find References", menu);
        findRefsAction->setShortcut(QKeySequence("Shift+F12"));
        connect(findRefsAction, &QAction::triggered, this, [this, wordUnderCursor]() {
            emit findReferencesRequested(wordUnderCursor);
        });
        
        menu->insertAction(actions.value(0), goToDefAction);
        menu->insertAction(actions.value(1), findRefsAction);
        menu->insertSeparator(actions.value(2));
    }

    // Add comment/uncomment actions
    QAction* commentAction = new QAction("Toggle Line Comment", menu);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    connect(commentAction, &QAction::triggered, this, &FluxEditor::toggleLineComment);
    
    QAction* blockCommentAction = new QAction("Toggle Block Comment", menu);
    blockCommentAction->setShortcut(QKeySequence("Ctrl+Shift+/"));
    connect(blockCommentAction, &QAction::triggered, this, &FluxEditor::toggleBlockComment);
    
    // Find where to insert comment actions (after Undo/Redo)
    QAction* insertAfter = nullptr;
    for (QAction* action : actions) {
        if (action->text() == "Cu&t") {
            insertAfter = action;
            break;
        }
    }
    if (insertAfter) {
        menu->insertSeparator(insertAfter);
        menu->insertAction(insertAfter, blockCommentAction);
        menu->insertAction(insertAfter, commentAction);
    }

    // Add selection actions
    QMenu* selectMenu = menu->addMenu("Selection");
    selectMenu->setIcon(QIcon::fromTheme("edit-select-all"));
    
    QAction* selectAllAction = new QAction("Select All", selectMenu);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &FluxEditor::selectAll);
    selectMenu->addAction(selectAllAction);
    
    QAction* selectLineAction = new QAction("Select Current Line", selectMenu);
    selectLineAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(selectLineAction, &QAction::triggered, this, &FluxEditor::selectCurrentLine);
    selectMenu->addAction(selectLineAction);
    
    QAction* selectWordAction = new QAction("Select Current Word", selectMenu);
    selectWordAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(selectWordAction, &QAction::triggered, this, &FluxEditor::selectCurrentWord);
    selectMenu->addAction(selectWordAction);

    // Add navigation submenu
    QMenu* navigateMenu = menu->addMenu("Navigate");
    navigateMenu->setIcon(QIcon::fromTheme("go-next"));
    
    QAction* goToLineAction = new QAction("Go to Line...", navigateMenu);
    goToLineAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(goToLineAction, &QAction::triggered, this, &FluxEditor::showGoToLineDialog);
    navigateMenu->addAction(goToLineAction);
    
    QAction* goToBeginningAction = new QAction("Go to Beginning", navigateMenu);
    goToBeginningAction->setShortcut(QKeySequence("Ctrl+Home"));
    connect(goToBeginningAction, &QAction::triggered, this, &FluxEditor::goToBeginning);
    navigateMenu->addAction(goToBeginningAction);
    
    QAction* goToEndAction = new QAction("Go to End", navigateMenu);
    goToEndAction->setShortcut(QKeySequence("Ctrl+End"));
    connect(goToEndAction, &QAction::triggered, this, &FluxEditor::goToEnd);
    navigateMenu->addAction(goToEndAction);
    
    navigateMenu->addSeparator();
    
    QAction* matchBraceAction = new QAction("Go to Matching Brace", navigateMenu);
    matchBraceAction->setShortcut(QKeySequence("Ctrl+M"));
    connect(matchBraceAction, &QAction::triggered, this, &FluxEditor::goToMatchingBrace);
    navigateMenu->addAction(matchBraceAction);

    // Add folding submenu
    QMenu* foldMenu = menu->addMenu("Folding");
    foldMenu->setIcon(QIcon::fromTheme("view-list-details"));
    
    QAction* toggleFoldAction = new QAction("Toggle Fold at Cursor", foldMenu);
    toggleFoldAction->setShortcut(QKeySequence("Ctrl++"));
    connect(toggleFoldAction, &QAction::triggered, this, [this]() {
        toggleFold(textCursor().blockNumber() + 1);
    });
    foldMenu->addAction(toggleFoldAction);
    
    QAction* foldAllAction = new QAction("Fold All", foldMenu);
    foldAllAction->setShortcut(QKeySequence("Ctrl+Shift+0"));
    connect(foldAllAction, &QAction::triggered, this, &FluxEditor::foldAll);
    foldMenu->addAction(foldAllAction);
    
    QAction* unfoldAllAction = new QAction("Unfold All", foldMenu);
    unfoldAllAction->setShortcut(QKeySequence("Ctrl+Shift+9"));
    connect(unfoldAllAction, &QAction::triggered, this, &FluxEditor::unfoldAll);
    foldMenu->addAction(unfoldAllAction);

    // Add breakpoint action
    int line = cursor.blockNumber() + 1;
    bool breakpointExists = hasBreakpoint(line);
    QAction* breakpointAction = new QAction(
        breakpointExists ? "Remove Breakpoint" : "Add Breakpoint", 
        menu
    );
    breakpointAction->setShortcut(QKeySequence("F9"));
    connect(breakpointAction, &QAction::triggered, this, [this, line]() {
        toggleBreakpoint(line);
    });
    
    // Insert breakpoint action before the standard actions
    menu->addSeparator();
    menu->addAction(breakpointAction);

    // Add "Run to Cursor" for debugging
    QAction* runToCursorAction = new QAction("Run to Line", menu);
    runToCursorAction->setShortcut(QKeySequence("Ctrl+F10"));
    connect(runToCursorAction, &QAction::triggered, this, [this, line]() {
        // Could emit a signal for debugger integration
        qDebug() << "Run to line:" << line;
    });
    menu->addAction(runToCursorAction);

    // Add advanced find/replace
    menu->addSeparator();
    
    QAction* findAction = new QAction("Find...", menu);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        // Could open a find dialog
        findAndHighlight(m_findText);
    });
    menu->addAction(findAction);
    
    QAction* findNextAction = new QAction("Find Next", menu);
    findNextAction->setShortcut(QKeySequence::FindNext);
    connect(findNextAction, &QAction::triggered, this, &FluxEditor::findNext);
    menu->addAction(findNextAction);
    
    QAction* findPrevAction = new QAction("Find Previous", menu);
    findPrevAction->setShortcut(QKeySequence::FindPrevious);
    connect(findPrevAction, &QAction::triggered, this, &FluxEditor::findPrevious);
    menu->addAction(findPrevAction);

    // Add clipboard history (if available)
    menu->addSeparator();
    
    QAction* pasteHistoryAction = new QAction("Paste from History...", menu);
    pasteHistoryAction->setShortcut(QKeySequence("Ctrl+Shift+V"));
    connect(pasteHistoryAction, &QAction::triggered, this, &FluxEditor::showPasteHistory);
    menu->addAction(pasteHistoryAction);

    // Add line operations
    menu->addSeparator();
    
    QMenu* lineMenu = menu->addMenu("Line Operations");
    
    QAction* duplicateLineAction = new QAction("Duplicate Line", lineMenu);
    duplicateLineAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicateLineAction, &QAction::triggered, this, &FluxEditor::duplicateLine);
    lineMenu->addAction(duplicateLineAction);
    
    QAction* deleteLineAction = new QAction("Delete Line", lineMenu);
    deleteLineAction->setShortcut(QKeySequence("Ctrl+Shift+K"));
    connect(deleteLineAction, &QAction::triggered, this, &FluxEditor::deleteCurrentLine);
    lineMenu->addAction(deleteLineAction);
    
    QAction* moveLineUpAction = new QAction("Move Line Up", lineMenu);
    moveLineUpAction->setShortcut(QKeySequence("Alt+Up"));
    connect(moveLineUpAction, &QAction::triggered, this, &FluxEditor::moveLineUp);
    lineMenu->addAction(moveLineUpAction);
    
    QAction* moveLineDownAction = new QAction("Move Line Down", lineMenu);
    moveLineDownAction->setShortcut(QKeySequence("Alt+Down"));
    connect(moveLineDownAction, &QAction::triggered, this, &FluxEditor::moveLineDown);
    lineMenu->addAction(moveLineDownAction);
    
    QAction* sortLinesAction = new QAction("Sort Selected Lines", lineMenu);
    connect(sortLinesAction, &QAction::triggered, this, &FluxEditor::sortSelectedLines);
    lineMenu->addAction(sortLinesAction);

    // Add format/indentation
    menu->addSeparator();
    
    QAction* indentAction = new QAction("Indent Line", menu);
    indentAction->setShortcut(QKeySequence("Tab"));
    connect(indentAction, &QAction::triggered, this, [this]() {
        // Indent current line
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("    ");
        cursor.endEditBlock();
    });
    menu->addAction(indentAction);
    
    QAction* unindentAction = new QAction("Unindent Line", menu);
    unindentAction->setShortcut(QKeySequence("Shift+Tab"));
    connect(unindentAction, &QAction::triggered, this, &FluxEditor::unindentLine);
    menu->addAction(unindentAction);

    // Add editor settings quick access
    menu->addSeparator();
    QMenu* settingsMenu = menu->addMenu("Settings");
    
    QAction* toggleWhitespaceAction = new QAction("Show Whitespace", settingsMenu);
    toggleWhitespaceAction->setCheckable(true);
    toggleWhitespaceAction->setChecked(m_showWhitespace);
    connect(toggleWhitespaceAction, &QAction::triggered, this, [this](bool show) {
        showWhitespace(show);
    });
    settingsMenu->addAction(toggleWhitespaceAction);
    
    QAction* toggleWordWrapAction = new QAction("Word Wrap", settingsMenu);
    toggleWordWrapAction->setCheckable(true);
    toggleWordWrapAction->setChecked(lineWrapMode() != QPlainTextEdit::NoWrap);
    connect(toggleWordWrapAction, &QAction::triggered, this, [this](bool wrap) {
        setLineWrapMode(wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    });
    settingsMenu->addAction(toggleWordWrapAction);

    // Show the menu
    menu->exec(event->globalPos());
    delete menu;
}

// ============================================================================
// Context Menu Helper Methods
// ============================================================================

void FluxEditor::toggleLineComment()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        // Toggle comment for selected lines
        toggleCommentForSelection();
    } else {
        // Toggle comment for current line
        int line = cursor.blockNumber();
        QTextBlock block = document()->findBlockByNumber(line);
        QString text = block.text();
        
        cursor.beginEditBlock();
        if (text.trimmed().startsWith("//")) {
            // Uncomment
            int pos = block.position();
            int offset = text.indexOf("//");
            cursor.setPosition(pos + offset);
            cursor.deleteChar();
            cursor.deleteChar();
        } else {
            // Comment
            cursor.setPosition(block.position());
            cursor.insertText("// ");
        }
        cursor.endEditBlock();
    }
}

void FluxEditor::toggleBlockComment()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return;
    }
    
    QString selectedText = cursor.selectedText();
    
    cursor.beginEditBlock();
    if (selectedText.startsWith("/*") && selectedText.endsWith("*/")) {
        // Uncomment block
        cursor.setPosition(cursor.selectionStart());
        cursor.setPosition(cursor.selectionStart() + 2, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setPosition(cursor.selectionEnd() - 2);
        cursor.setPosition(cursor.selectionEnd() + 2, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else {
        // Comment block
        cursor.setPosition(cursor.selectionStart());
        cursor.insertText("/*");
        cursor.setPosition(cursor.selectionEnd() + 2);
        cursor.insertText("*/");
    }
    cursor.endEditBlock();
}

void FluxEditor::toggleCommentForSelection()
{
    QTextCursor cursor = textCursor();
    int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();
    
    // Check if all lines are commented
    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock block = document()->findBlockByNumber(i);
        if (!block.text().trimmed().startsWith("//")) {
            allCommented = false;
            break;
        }
    }
    
    cursor.beginEditBlock();
    if (allCommented) {
        // Uncomment all lines
        for (int i = endBlock; i >= startBlock; --i) {
            QTextBlock block = document()->findBlockByNumber(i);
            QString text = block.text();
            int offset = text.indexOf("//");
            if (offset >= 0) {
                cursor.setPosition(block.position() + offset);
                cursor.deleteChar();
                cursor.deleteChar();
            }
        }
    } else {
        // Comment all lines
        for (int i = endBlock; i >= startBlock; --i) {
            QTextBlock block = document()->findBlockByNumber(i);
            cursor.setPosition(block.position());
            cursor.insertText("// ");
        }
    }
    cursor.endEditBlock();
}

void FluxEditor::selectCurrentLine()
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
}

void FluxEditor::selectCurrentWord()
{
    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    setTextCursor(cursor);
}

void FluxEditor::showGoToLineDialog()
{
    int maxLine = document()->blockCount();
    bool ok;
    int line = QInputDialog::getInt(
        this,
        "Go to Line",
        QString("Line (1-%1):").arg(maxLine),
        textCursor().blockNumber() + 1,
        1,
        maxLine,
        1,
        &ok
    );
    
    if (ok) {
        goToLine(line);
    }
}

void FluxEditor::goToMatchingBrace()
{
    QTextCursor cursor = textCursor();
    QString text = cursor.block().text();
    int pos = cursor.positionInBlock();
    
    if (pos >= text.length()) {
        return;
    }
    
    QChar ch = text[pos];
    QChar matching;
    bool forward = true;
    
    if (ch == '(') {
        matching = ')';
    } else if (ch == ')') {
        matching = '(';
        forward = false;
    } else if (ch == '[') {
        matching = ']';
    } else if (ch == ']') {
        matching = '[';
        forward = false;
    } else if (ch == '{') {
        matching = '}';
    } else if (ch == '}') {
        matching = '{';
        forward = false;
    } else {
        return;
    }
    
    int depth = 1;
    if (forward) {
        for (int i = pos + 1; i < text.length(); ++i) {
            if (text[i] == ch) {
                depth++;
            } else if (text[i] == matching) {
                depth--;
                if (depth == 0) {
                    cursor.setPosition(cursor.block().position() + i);
                    setTextCursor(cursor);
                    return;
                }
            }
        }
    } else {
        for (int i = pos - 1; i >= 0; --i) {
            if (text[i] == matching) {
                depth--;
                if (depth == 0) {
                    cursor.setPosition(cursor.block().position() + i);
                    setTextCursor(cursor);
                    return;
                }
            } else if (text[i] == ch) {
                depth++;
            }
        }
    }
}

void FluxEditor::showPasteHistory()
{
    QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    
    // For now, just paste from clipboard
    // A full implementation would show a dialog with clipboard history
    paste();
}

void FluxEditor::duplicateLine()
{
    QTextCursor cursor = textCursor();
    int lineNum = cursor.blockNumber();
    QTextBlock block = document()->findBlockByNumber(lineNum);
    
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.insertText("\n" + block.text());
    cursor.endEditBlock();
}

void FluxEditor::deleteCurrentLine()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    
    if (cursor.hasSelection()) {
        int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
        int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();
        
        cursor.setPosition(document()->findBlockByNumber(startBlock).position());
        cursor.setPosition(document()->findBlockByNumber(endBlock).position() + document()->findBlockByNumber(endBlock).length(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else {
        int lineNum = cursor.blockNumber();
        QTextBlock block = document()->findBlockByNumber(lineNum);
        
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
    
    cursor.endEditBlock();
}

void FluxEditor::moveLineUp()
{
    QTextCursor cursor = textCursor();
    int lineNum = cursor.blockNumber();
    
    if (lineNum == 0) {
        return;
    }
    
    QTextBlock currentBlock = document()->findBlockByNumber(lineNum);
    QTextBlock previousBlock = document()->findBlockByNumber(lineNum - 1);
    
    QString currentText = currentBlock.text();
    QString previousText = previousBlock.text();
    
    cursor.beginEditBlock();
    cursor.setPosition(currentBlock.position());
    cursor.setPosition(currentBlock.position() + currentBlock.length(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    
    cursor.setPosition(previousBlock.position());
    cursor.insertText(currentText + "\n");
    cursor.endEditBlock();
}

void FluxEditor::moveLineDown()
{
    QTextCursor cursor = textCursor();
    int lineNum = cursor.blockNumber();
    
    if (lineNum >= document()->blockCount() - 1) {
        return;
    }
    
    QTextBlock currentBlock = document()->findBlockByNumber(lineNum);
    QTextBlock nextBlock = document()->findBlockByNumber(lineNum + 1);
    
    QString currentText = currentBlock.text();
    QString nextText = nextBlock.text();
    
    cursor.beginEditBlock();
    cursor.setPosition(currentBlock.position());
    cursor.setPosition(currentBlock.position() + currentBlock.length(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    
    cursor.setPosition(nextBlock.position() + nextBlock.length());
    cursor.insertText("\n" + currentText);
    cursor.endEditBlock();
}

void FluxEditor::sortSelectedLines()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return;
    }
    
    int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();
    
    QStringList lines;
    for (int i = startBlock; i <= endBlock; ++i) {
        lines.append(document()->findBlockByNumber(i).text());
    }
    
    lines.sort();
    
    cursor.beginEditBlock();
    for (int i = endBlock; i >= startBlock; --i) {
        QTextBlock block = document()->findBlockByNumber(i);
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(lines[i - startBlock]);
    }
    cursor.endEditBlock();
}

void FluxEditor::unindentLine()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::StartOfLine);
    
    // Remove up to 4 spaces or 1 tab
    QString text = cursor.block().text();
    if (text.startsWith("    ")) {
        cursor.setPosition(cursor.position() + 4, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (text.startsWith("\t")) {
        cursor.setPosition(cursor.position() + 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (text.startsWith(" ")) {
        int spaces = 0;
        for (QChar ch : text) {
            if (ch == ' ') {
                spaces++;
            } else {
                break;
            }
        }
        if (spaces > 0) {
            cursor.setPosition(cursor.position() + qMin(spaces, 4), QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        }
    }
    cursor.endEditBlock();
}

} // namespace Flux
