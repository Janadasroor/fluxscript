#include "flux/ui/minimap_widget.h"
#include "flux/ui/flux_editor.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QScrollBar>
#include <QTimer>
#include <QTextBlock>
#include <QTextDocument>

namespace Flux {

MinimapWidget::MinimapWidget(QWidget* parent)
    : QWidget(parent)
    , m_editor(nullptr)
    , m_needsRender(true)
    , m_dirty(true)
    , m_showViewportIndicator(true)
    , m_width(120)
    , m_renderSize(4)
    , m_updateDelay(200)
    , m_enabled(true)
    , m_mouseOver(false)
    , m_mousePressed(false)
    , m_updateTimerId(-1)
{
    setMinimumWidth(m_width);
    setMaximumWidth(m_width);
    setAutoFillBackground(false);

    // Set up timer for delayed updates
    m_updateTimerId = startTimer(m_updateDelay);
}

MinimapWidget::~MinimapWidget()
{
    if (m_updateTimerId >= 0) {
        killTimer(m_updateTimerId);
    }
}

void MinimapWidget::setEditor(FluxEditor* editor)
{
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }

    m_editor = editor;

    if (m_editor) {
        connect(m_editor, &FluxEditor::textChanged,
                this, [this]() { m_dirty = true; });
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
                this, [this]() { m_needsRender = true; update(); });
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, [this]() { m_needsRender = true; update(); });
        connect(m_editor->verticalScrollBar(), &QScrollBar::rangeChanged,
                this, [this]() { m_needsRender = true; update(); });
    }

    m_dirty = true;
    m_needsRender = true;
}

void MinimapWidget::setEnabled(bool enabled)
{
    m_enabled = enabled;
    setVisible(enabled);
    m_needsRender = true;
    update();
}

void MinimapWidget::setWidth(int width)
{
    m_width = width;
    setMinimumWidth(m_width);
    setMaximumWidth(m_width);
    m_needsRender = true;
    update();
}

void MinimapWidget::setShowViewportIndicator(bool show)
{
    m_showViewportIndicator = show;
    m_needsRender = true;
    update();
}

void MinimapWidget::setRenderSize(int size)
{
    m_renderSize = size;
    m_dirty = true;
    m_needsRender = true;
    update();
}

void MinimapWidget::setUpdateDelay(int ms)
{
    if (m_updateTimerId >= 0) {
        killTimer(m_updateTimerId);
    }

    m_updateDelay = ms;

    if (m_updateDelay > 0) {
        m_updateTimerId = startTimer(m_updateDelay);
    }
}

void MinimapWidget::setViewportRect(const QRect& rect)
{
    m_viewportRect = rect;
    m_needsRender = true;
    update();
}

void MinimapWidget::updateMinimap()
{
    if (!m_enabled || !m_editor) {
        return;
    }

    if (m_dirty) {
        renderToImage();
        m_dirty = false;
    }

    m_needsRender = true;
    update();
}

void MinimapWidget::forceUpdate()
{
    m_dirty = true;
    m_needsRender = true;
    update();
}

void MinimapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (!m_enabled) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!m_editor) {
        return;
    }

    // Draw minimap image
    if (m_minimapImage.isNull() && m_dirty) {
        renderToImage();
    }

    if (!m_minimapImage.isNull()) {
        // Scale image to fit widget
        QRect targetRect(0, 0, m_width, height());
        painter.drawImage(targetRect, m_minimapImage);
    } else {
        // Fallback: render text directly
        renderTextLayer(painter);
    }

    // Draw viewport indicator
    if (m_showViewportIndicator) {
        renderViewportIndicator(painter);
    }
}

void MinimapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_dirty = true;
    m_needsRender = true;
}

void MinimapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_editor) {
        m_mousePressed = true;

        int line = yToLine(event->pos().y());
        emit minimapClicked(line);

        // Scroll editor to line
        m_editor->goToLine(line);
    }

    QWidget::mousePressEvent(event);
}

void MinimapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mousePressed && m_editor) {
        int line = yToLine(event->pos().y());
        emit minimapClicked(line);
    }

    QWidget::mouseMoveEvent(event);
}

void MinimapWidget::enterEvent(QEnterEvent* event)
{
    m_mouseOver = true;
    m_needsRender = true;
    update();
    QWidget::enterEvent(event);
}

void MinimapWidget::leaveEvent(QEvent* event)
{
    m_mouseOver = false;
    m_mousePressed = false;
    m_needsRender = true;
    update();
    QWidget::leaveEvent(event);
}

void MinimapWidget::renderToImage()
{
    if (!m_editor || !m_editor->document()) {
        return;
    }

    // Create image for minimap
    QSize imageSize(m_width, height());
    m_minimapImage = QImage(imageSize, QImage::Format_ARGB32);
    m_minimapImage.fill(QColor(30, 30, 30));

    QPainter painter(&m_minimapImage);
    painter.setRenderHint(QPainter::Antialiasing);

    renderTextLayer(painter);
}

void MinimapWidget::renderTextLayer(QPainter& painter)
{
    if (!m_editor || !m_editor->document()) {
        return;
    }

    QTextDocument* doc = m_editor->document();
    int blockCount = doc->blockCount();

    if (blockCount == 0) {
        return;
    }

    // Calculate scaling
    qreal lineHeight = static_cast<qreal>(height()) / blockCount;
    int charWidth = m_renderSize;

    QFont font("Monospace", charWidth);
    font.setStyleHint(QFont::Monospace);
    painter.setFont(font);

    int y = 0;
    QTextBlock block = doc->firstBlock();

    while (block.isValid() && y < height()) {
        if (m_editor->isLineVisible(block.blockNumber())) {
            QString text = block.text();

            // Truncate text to fit width
            if (text.length() * charWidth > m_width) {
                text = text.left(m_width / charWidth);
            }

            // Use default text color
            QColor textColor = QColor(200, 200, 200);

            painter.setPen(textColor);
            painter.drawText(0, y, m_width, static_cast<int>(lineHeight),
                             Qt::AlignLeft | Qt::AlignTop, text);
        }

        y += static_cast<int>(lineHeight);
        block = block.next();
    }
}

void MinimapWidget::renderViewportIndicator(QPainter& painter)
{
    if (!m_editor) {
        return;
    }

    QRect viewportRect = viewportRectInMinimap();

    painter.save();
    painter.setPen(QPen(QColor(100, 100, 100), 1));
    painter.setBrush(QColor(60, 60, 60, 100));
    painter.drawRect(viewportRect);
    painter.restore();
}

int MinimapWidget::lineToY(int line) const
{
    if (!m_editor || !m_editor->document()) {
        return 0;
    }

    int blockCount = m_editor->document()->blockCount();
    if (blockCount == 0) {
        return 0;
    }

    qreal lineHeight = static_cast<qreal>(height()) / blockCount;
    return static_cast<int>(line * lineHeight);
}

int MinimapWidget::yToLine(int y) const
{
    if (!m_editor || !m_editor->document()) {
        return 1;
    }

    int blockCount = m_editor->document()->blockCount();
    if (blockCount == 0) {
        return 1;
    }

    qreal lineHeight = static_cast<qreal>(height()) / blockCount;
    return static_cast<int>(y / lineHeight) + 1;
}

QRect MinimapWidget::viewportRectInMinimap() const
{
    if (!m_editor) {
        return QRect(0, 0, m_width, height());
    }

    QScrollBar* scrollBar = m_editor->verticalScrollBar();
    if (!scrollBar) {
        return QRect(0, 0, m_width, height());
    }

    int minVal = scrollBar->minimum();
    int maxVal = scrollBar->maximum();
    int pageStep = scrollBar->pageStep();
    int value = scrollBar->value();

    if (maxVal <= minVal) {
        return QRect(0, 0, m_width, height());
    }

    // Calculate viewport position in minimap
    qreal totalRange = maxVal - minVal;
    qreal viewportSize = static_cast<qreal>(pageStep) / totalRange;
    qreal viewportPos = static_cast<qreal>(value) / totalRange;

    int viewportHeight = static_cast<int>(viewportSize * height());
    int viewportY = static_cast<int>(viewportPos * height());

    // Clamp to widget bounds
    viewportHeight = qBound(20, viewportHeight, height());
    viewportY = qBound(0, viewportY, height() - viewportHeight);

    return QRect(0, viewportY, m_width, viewportHeight);
}

QColor MinimapWidget::colorForChar(QChar ch) const
{
    // Simple color mapping based on character type
    if (ch.isLetter()) {
        return QColor(197, 134, 192);  // Keywords/identifiers
    } else if (ch.isDigit()) {
        return QColor(181, 206, 168);  // Numbers
    } else if (ch == '"' || ch == '\'') {
        return QColor(206, 145, 120);  // Strings
    } else if (ch == '/' || ch == '#') {
        return QColor(90, 100, 110);   // Comments
    } else {
        return QColor(150, 150, 150);  // Operators/other
    }
}

} // namespace Flux
