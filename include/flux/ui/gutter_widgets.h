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
 * Note: This is a simple data class, not a QWidget
 */
class GutterMarker {
public:
    explicit GutterMarker(GutterMarkerType type) : m_type(type) {}
    virtual ~GutterMarker() = default;

    GutterMarkerType type() const { return m_type; }
    virtual QColor color() const;
    virtual void paint(QPainter* painter, const QRect& rect);

private:
    GutterMarkerType m_type;
};

} // namespace Flux

#endif // FLUX_GUTTER_WIDGETS_H
