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

#ifndef FLUX_MINIMAP_WIDGET_H
#define FLUX_MINIMAP_WIDGET_H

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QScrollBar>

namespace Flux {

class FluxEditor;

/**
 * @brief Minimap/overview widget for large files
 * 
 * Provides a zoomed-out visual overview of the entire document
 * with clickable navigation and viewport indicator.
 */
class MinimapWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit MinimapWidget(QWidget* parent = nullptr);
    ~MinimapWidget() override;

    // ========================================================================
    // Editor Integration
    // ========================================================================
    void setEditor(FluxEditor* editor);
    FluxEditor* editor() const { return m_editor; }
    
    // ========================================================================
    // Display Options
    // ========================================================================
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    void setWidth(int width);
    int width() const { return m_width; }
    
    void setShowViewportIndicator(bool show);
    bool showViewportIndicator() const { return m_showViewportIndicator; }
    
    void setRenderSize(int size);  // Character size for rendering
    int renderSize() const { return m_renderSize; }
    
    void setUpdateDelay(int ms);
    int updateDelay() const { return m_updateDelay; }

    // ========================================================================
    // Viewport Management
    // ========================================================================
    void setViewportRect(const QRect& rect);
    QRect viewportRect() const { return m_viewportRect; }

public slots:
    void updateMinimap();
    void forceUpdate();

signals:
    void viewportClicked(int line);
    void minimapClicked(int line);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // ========================================================================
    // Rendering
    // ========================================================================
    void renderToImage();
    void renderTextLayer(QPainter& painter);
    void renderViewportIndicator(QPainter& painter);
    QColor colorForChar(QChar ch) const;
    
    // ========================================================================
    // Geometry
    // ========================================================================
    int lineToY(int line) const;
    int yToLine(int y) const;
    QRect viewportRectInMinimap() const;
    
    // ========================================================================
    // Members
    // ========================================================================
    FluxEditor* m_editor;
    QImage m_minimapImage;
    bool m_needsRender;
    bool m_dirty;
    
    // Viewport indicator
    QRect m_viewportRect;
    bool m_showViewportIndicator;
    
    // Display options
    int m_width;
    int m_renderSize;
    int m_updateDelay;
    bool m_enabled;
    
    // Mouse state
    bool m_mouseOver;
    bool m_mousePressed;
    
    // Timer for delayed updates
    int m_updateTimerId;
};

} // namespace Flux

#endif // FLUX_MINIMAP_WIDGET_H
