#ifndef FLUX_GUTTER_WIDGETS_H
#define FLUX_GUTTER_WIDGETS_H

#include <QWidget>
#include <QColor>
#include <QPainter>
#include <QMap>

namespace Flux {

class FluxEditor;

/**
 * @brief Gutter marker types for visual indicators in the line number area
 */
enum class GutterMarkerType {
    Breakpoint,
    Error,
    Warning,
    Info,
    Bookmark,
    ExecutionPoint,
    FoldMarker
};

/**
 * @brief Visual marker displayed in the gutter (line number area)
 */
class GutterMarker : public QWidget {
    Q_OBJECT
    
public:
    explicit GutterMarker(GutterMarkerType type, QWidget* parent = nullptr);
    explicit GutterMarker(GutterMarkerType type, const QString& tooltip, QWidget* parent = nullptr);
    ~GutterMarker() override;
    
    GutterMarkerType type() const { return m_type; }
    QString tooltip() const { return m_toolTip; }
    QColor color() const;
    
    void setType(GutterMarkerType type);
    void setColor(const QColor& color);
    
    // Rendering
    void paint(QPainter* painter, const QRect& rect);
    QSize sizeHint() const override;
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void setupStyle();
    QIcon iconForType() const;
    
    GutterMarkerType m_type;
    QColor m_customColor;
    bool m_useCustomColor;
};

/**
 * @brief Breakpoint marker with interactive toggle
 */
class BreakpointMarker : public GutterMarker {
    Q_OBJECT
    
public:
    explicit BreakpointMarker(QWidget* parent = nullptr);
    explicit BreakpointMarker(bool enabled, QWidget* parent = nullptr);
    
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    
    void setCondition(const QString& condition);
    QString condition() const { return m_condition; }
    
    bool hasCondition() const { return !m_condition.isEmpty(); }
    bool isHit() const { return m_isHit; }
    void setHit(bool hit);
    
signals:
    void toggled(bool enabled);
    void clicked();
    void rightClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_enabled;
    bool m_isHit;
    QString m_condition;
    bool m_mouseOver;
};

/**
 * @brief Error/Warning marker with severity indication
 */
class DiagnosticMarker : public GutterMarker {
    Q_OBJECT
    
public:
    explicit DiagnosticMarker(QWidget* parent = nullptr);
    
    enum class Severity {
        Error,
        Warning,
        Info,
        Hint
    };
    
    Severity severity() const { return m_severity; }
    void setSeverity(Severity severity);
    
    void setMessage(const QString& message);
    QString message() const { return m_message; }
    
    void setLineColumn(int line, int column);
    int line() const { return m_line; }
    int column() const { return m_column; }
    
signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QColor colorForSeverity() const;
    QString iconForSeverity() const;
    
    Severity m_severity;
    QString m_message;
    int m_line;
    int m_column;
};

/**
 * @brief Execution point marker (current debug line)
 */
class ExecutionMarker : public GutterMarker {
    Q_OBJECT
    
public:
    explicit ExecutionMarker(QWidget* parent = nullptr);
    
    void setThreadId(int threadId);
    int threadId() const { return m_threadId; }
    
    void setStackFrame(int frameIndex);
    int stackFrame() const { return m_frameIndex; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_threadId;
    int m_frameIndex;
};

/**
 * @brief Fold marker for code folding
 */
class FoldMarker : public GutterMarker {
    Q_OBJECT
    
public:
    explicit FoldMarker(QWidget* parent = nullptr);
    explicit FoldMarker(bool expanded, QWidget* parent = nullptr);
    
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    
    void setFoldRange(int startLine, int endLine);
    int startLine() const { return m_startLine; }
    int endLine() const { return m_endLine; }
    
signals:
    void toggled(bool expanded);
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void drawArrow(QPainter* painter, const QRect& rect);
    
    bool m_expanded;
    int m_startLine;
    int m_endLine;
    bool m_mouseOver;
};

/**
 * @brief Bookmark marker for quick navigation
 */
class BookmarkMarker : public GutterMarker {
    Q_OBJECT
    
public:
    explicit BookmarkMarker(QWidget* parent = nullptr);
    
    void setLabel(const QString& label);
    QString label() const { return m_label; }
    
    void setIndex(int index);
    int index() const { return m_index; }

signals:
    void clicked();
    void removed();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_label;
    int m_index;
};

/**
 * @brief Gutter manager for handling all markers in an editor
 */
class GutterManager : public QObject {
    Q_OBJECT
    
public:
    explicit GutterManager(FluxEditor* editor);
    ~GutterManager() override;
    
    // Marker management
    void addMarker(int line, GutterMarker* marker);
    void removeMarker(int line, GutterMarkerType type);
    void removeMarker(int line, int markerIndex);
    void clearMarkers(int line);
    void clearAllMarkers();
    
    // Query
    QList<GutterMarker*> markersAt(int line) const;
    GutterMarker* markerAt(int line, GutterMarkerType type) const;
    QMap<int, QList<GutterMarker*>> allMarkers() const;
    
    // Painting
    int markerWidth() const;
    void paintMarkers(QPainter* painter, int line, const QRect& rect);
    
    // Signals from markers
    void handleMarkerClicked(GutterMarker* marker);

signals:
    void markerClicked(GutterMarker* marker, int line);
    void markerToggled(GutterMarker* marker, int line, bool state);
    void markersChanged(int line);

private slots:
    void onMarkerDestroyed(QObject* marker);

private:
    void setupConnections();
    int gutterWidth() const;
    
    FluxEditor* m_editor;
    QMap<int, QList<GutterMarker*>> m_markers;  // line -> markers
    int m_markerWidth;
};

} // namespace Flux

#endif // FLUX_GUTTER_WIDGETS_H
