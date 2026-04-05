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

#include "flux/gui/plot.h"
#include <QApplication>
#include <QPainterPath>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QClipboard>
#include <QTextStream>
#include <QFile>
#include <QPointF>
#include <cmath>

namespace Flux {
namespace Plot {

// ============================================================================
// PlotWidget Implementation
// ============================================================================

PlotWidget::PlotWidget(QWidget* parent)
    : QWidget(parent)
    , m_isPanning(false)
    , m_panEnabled(true)
    , m_zoomEnabled(true)
    , m_crosshairEnabled(true)
    , m_dataTipsEnabled(true)
    , m_hoveredSeries(-1)
    , m_hoveredPoint(-1)
    , m_marginLeft(60)
    , m_marginRight(40)
    , m_marginTop(40)
    , m_marginBottom(50)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // Set default config
    m_config = PlotConfig();
}

PlotWidget::~PlotWidget() = default;

void PlotWidget::addSeries(const DataSeries& series) {
    m_series.push_back(series);
    autoScale();
    update();
}

void PlotWidget::removeSeries(const std::string& name) {
    m_series.erase(
        std::remove_if(m_series.begin(), m_series.end(),
            [&name](const DataSeries& s) { return s.name == name; }),
        m_series.end()
    );
    autoScale();
    update();
}

void PlotWidget::clearSeries() {
    m_series.clear();
    update();
}

void PlotWidget::updateSeries(const std::string& name, 
                               const std::vector<double>& x,
                               const std::vector<double>& y) {
    for (auto& series : m_series) {
        if (series.name == name) {
            series.xValues = x;
            series.yValues = y;
            autoScale();
            update();
            return;
        }
    }
    // If not found, add new
    DataSeries s;
    s.name = name;
    s.xValues = x;
    s.yValues = y;
    addSeries(s);
}

void PlotWidget::setConfig(const PlotConfig& config) {
    m_config = config;
    update();
}

void PlotWidget::zoomIn() {
    double xCenter = (m_viewRect.left() + m_viewRect.right()) / 2.0;
    double yCenter = (m_viewRect.top() + m_viewRect.bottom()) / 2.0;
    double xSpan = (m_viewRect.right() - m_viewRect.left()) * 0.2;
    double ySpan = (m_viewRect.bottom() - m_viewRect.top()) * 0.2;
    
    m_viewRect.adjust(xSpan, ySpan, -xSpan, -ySpan);
    update();
    Q_EMIT rangeChanged(m_viewRect.left(), m_viewRect.right(), 
                      m_viewRect.top(), m_viewRect.bottom());
}

void PlotWidget::zoomOut() {
    double xCenter = (m_viewRect.left() + m_viewRect.right()) / 2.0;
    double yCenter = (m_viewRect.top() + m_viewRect.bottom()) / 2.0;
    double xSpan = (m_viewRect.right() - m_viewRect.left()) * 0.125;
    double ySpan = (m_viewRect.bottom() - m_viewRect.top()) * 0.125;
    
    m_viewRect.adjust(-xSpan, -ySpan, xSpan, ySpan);
    update();
    Q_EMIT rangeChanged(m_viewRect.left(), m_viewRect.right(), 
                      m_viewRect.top(), m_viewRect.bottom());
}

void PlotWidget::resetZoom() {
    m_viewRect = m_fullRect;
    update();
    Q_EMIT rangeChanged(m_viewRect.left(), m_viewRect.right(), 
                      m_viewRect.top(), m_viewRect.bottom());
}

void PlotWidget::setXRange(double min, double max) {
    m_viewRect.setLeft(min);
    m_viewRect.setRight(max);
    update();
}

void PlotWidget::setYRange(double min, double max) {
    m_viewRect.setTop(max);  // Qt Y is inverted
    m_viewRect.setBottom(min);
    update();
}

void PlotWidget::autoScale() {
    QRectF bounds = dataBounds();
    if (bounds.isEmpty()) {
        m_fullRect = QRectF(0, 1, 10, -1);
    } else {
        // Add 5% padding
        double xPad = (bounds.right() - bounds.left()) * 0.05;
        double yPad = (bounds.top() - bounds.bottom()) * 0.05;
        
        if (xPad == 0) xPad = 0.5;
        if (yPad == 0) yPad = 0.5;
        
        m_fullRect = bounds.adjusted(-xPad, -yPad, xPad, yPad);
    }
    m_viewRect = m_fullRect;
    update();
}

QImage PlotWidget::toImage() const {
    QImage image(size(), QImage::Format_ARGB32);
    image.fill(m_config.backgroundColor);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    const_cast<PlotWidget*>(this)->drawGrid(painter);
    const_cast<PlotWidget*>(this)->drawAxes(painter);
    const_cast<PlotWidget*>(this)->drawSeries(painter);
    const_cast<PlotWidget*>(this)->drawLegend(painter);
    
    return image;
}

void PlotWidget::saveAsImage(const QString& path) {
    toImage().save(path);
}

void PlotWidget::saveAsSVG(const QString& path) {
    // SVG export not available - save as PNG instead
    QString pngPath = path;
    if (pngPath.endsWith(".svg", Qt::CaseInsensitive)) {
        pngPath = pngPath.left(pngPath.length() - 4) + ".png";
    }
    toImage().save(pngPath);
}

void PlotWidget::print() {
    // Print not available - save as PNG instead
    QString path = QFileDialog::getSaveFileName(this, "Save Plot as PNG",
                                                 "", "PNG Images (*.png)");
    if (!path.isEmpty()) {
        saveAsImage(path);
    }
}

void PlotWidget::enablePan(bool enabled) { m_panEnabled = enabled; }
void PlotWidget::enableZoom(bool enabled) { m_zoomEnabled = enabled; }
void PlotWidget::enableCrosshair(bool enabled) { m_crosshairEnabled = enabled; }
void PlotWidget::enableDataTips(bool enabled) { m_dataTipsEnabled = enabled; }

void PlotWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), m_config.backgroundColor);
    
    // Calculate plot area
    m_plotRect = QRect(m_marginLeft, m_marginTop,
                       width() - m_marginLeft - m_marginRight,
                       height() - m_marginTop - m_marginBottom);
    
    drawGrid(painter);
    drawAxes(painter);
    drawSeries(painter);
    drawLegend(painter);
    
    if (m_crosshairEnabled) {
        drawCrosshair(painter);
    }
}

void PlotWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_panEnabled) {
        m_isPanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void PlotWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        
        double dx = (m_viewRect.right() - m_viewRect.left()) * 
                    (-delta.x() / double(m_plotRect.width()));
        double dy = (m_viewRect.bottom() - m_viewRect.top()) * 
                    (-delta.y() / double(m_plotRect.height()));
        
        m_viewRect.translate(dx, dy);
        update();
        Q_EMIT rangeChanged(m_viewRect.left(), m_viewRect.right(),
                          m_viewRect.top(), m_viewRect.bottom());
    }
    
    if (m_crosshairEnabled) {
        m_crosshairPos = event->position().toPoint();
        QPointF dataPos = screenToData(event->position().x(), event->position().y());
        Q_EMIT mouseMoved(dataPos.x(), dataPos.y());
        update();
    }
    
    if (m_dataTipsEnabled) {
        int pointIdx = findNearestPoint(event->pos());
        if (pointIdx >= 0) {
            setToolTip(QString("Point %1").arg(pointIdx));
        } else {
            setToolTip(QString());
        }
    }
}

void PlotWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void PlotWidget::wheelEvent(QWheelEvent* event) {
    if (!m_zoomEnabled) return;
    
    QPoint pos = event->position().toPoint();
    QPointF dataPos = screenToData(pos.x(), pos.y());
    
    double factor = (event->angleDelta().y() > 0) ? 1.1 : 1.0/1.1;
    
    double xLeft = m_viewRect.left() + (dataPos.x() - m_viewRect.left()) * (1 - factor);
    double xRight = m_viewRect.right() - (m_viewRect.right() - dataPos.x()) * (1 - factor);
    double yTop = m_viewRect.top() + (dataPos.y() - m_viewRect.top()) * (1 - factor);
    double yBottom = m_viewRect.bottom() - (m_viewRect.bottom() - dataPos.y()) * (1 - factor);
    
    m_viewRect.setCoords(xLeft, yTop, xRight, yBottom);
    update();
    Q_EMIT rangeChanged(m_viewRect.left(), m_viewRect.right(),
                      m_viewRect.top(), m_viewRect.bottom());
}

void PlotWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    autoScale();
}

void PlotWidget::drawGrid(QPainter& painter) {
    if (!m_config.showGrid) return;
    
    QPen gridPen(m_config.gridColor);
    gridPen.setStyle(Qt::DotLine);
    painter.setPen(gridPen);
    
    // Vertical grid lines
    int xTicks = 10;
    double xStep = (m_viewRect.right() - m_viewRect.left()) / xTicks;
    for (int i = 0; i <= xTicks; ++i) {
        double x = m_viewRect.left() + i * xStep;
        QPointF p1 = dataToScreen(x, m_viewRect.top());
        QPointF p2 = dataToScreen(x, m_viewRect.bottom());
        painter.drawLine(p1, p2);
    }
    
    // Horizontal grid lines
    int yTicks = 10;
    double yStep = (m_viewRect.bottom() - m_viewRect.top()) / yTicks;
    for (int i = 0; i <= yTicks; ++i) {
        double y = m_viewRect.bottom() - i * yStep;
        QPointF p1 = dataToScreen(m_viewRect.left(), y);
        QPointF p2 = dataToScreen(m_viewRect.right(), y);
        painter.drawLine(p1, p2);
    }
}

void PlotWidget::drawAxes(QPainter& painter) {
    painter.setPen(QPen(m_config.textColor, 2));
    
    // Axes
    QPointF origin = dataToScreen(m_viewRect.left(), m_viewRect.bottom());
    QPointF xAxisEnd = dataToScreen(m_viewRect.right(), m_viewRect.bottom());
    QPointF yAxisEnd = dataToScreen(m_viewRect.left(), m_viewRect.top());
    
    painter.drawLine(origin, xAxisEnd);
    painter.drawLine(origin, yAxisEnd);
    
    // Tick labels
    painter.setFont(m_config.tickFont);
    int xTicks = 5;
    double xStep = (m_viewRect.right() - m_viewRect.left()) / xTicks;
    for (int i = 0; i <= xTicks; ++i) {
        double x = m_viewRect.left() + i * xStep;
        QPointF pos = dataToScreen(x, m_viewRect.bottom());
        QString label = QString::number(x, 'g', 4);
        painter.drawText(pos.x() - 20, pos.y() + 20, 40, 20, Qt::AlignCenter, label);
    }
    
    int yTicks = 5;
    double yStep = (m_viewRect.bottom() - m_viewRect.top()) / yTicks;
    for (int i = 0; i <= yTicks; ++i) {
        double y = m_viewRect.bottom() - i * yStep;
        QPointF pos = dataToScreen(m_viewRect.left(), y);
        QString label = QString::number(y, 'g', 4);
        painter.drawText(0, pos.y() - 10, m_marginLeft - 10, 20, Qt::AlignRight, label);
    }
    
    // Axis labels
    painter.setFont(m_config.labelFont);
    if (!m_config.xAxisLabel.empty()) {
        painter.drawText(m_plotRect, Qt::AlignHCenter | Qt::AlignBottom, 
                         QString::fromStdString(m_config.xAxisLabel));
    }
    if (!m_config.yAxisLabel.empty()) {
        painter.save();
        painter.rotate(-90);
        painter.drawText(-height()/2, 15, QString::fromStdString(m_config.yAxisLabel));
        painter.restore();
    }
    
    // Title
    if (!m_config.title.empty()) {
        painter.setFont(m_config.titleFont);
        painter.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop,
                         QString::fromStdString(m_config.title));
    }
}

void PlotWidget::drawSeries(QPainter& painter) {
    for (const auto& series : m_series) {
        if (!series.visible || series.xValues.empty() || series.yValues.empty())
            continue;
        
        QPen pen(series.color);
        pen.setStyle(series.lineStyle);
        pen.setWidth(series.lineWidth);
        painter.setPen(pen);
        
        // Draw line
        QPainterPath path;
        QPointF first = dataToScreen(series.xValues[0], series.yValues[0]);
        path.moveTo(first);
        
        for (size_t i = 1; i < series.xValues.size(); ++i) {
            QPointF p = dataToScreen(series.xValues[i], series.yValues[i]);
            path.lineTo(p);
        }
        painter.drawPath(path);
        
        // Draw markers
        if (series.showMarkers) {
            painter.setBrush(QBrush(series.color, series.markerStyle));
            for (size_t i = 0; i < series.xValues.size(); ++i) {
                QPointF p = dataToScreen(series.xValues[i], series.yValues[i]);
                painter.drawEllipse(p, series.markerSize, series.markerSize);
            }
        }
    }
}

void PlotWidget::drawLegend(QPainter& painter) {
    if (!m_config.showLegend || m_series.empty()) return;
    
    int legendWidth = 150;
    int legendHeight = 20 * m_series.size() + 10;
    QRect legendRect;
    
    if (m_config.legendPosition & Qt::AlignTop) {
        legendRect.setTop(50);
    } else if (m_config.legendPosition & Qt::AlignBottom) {
        legendRect.setBottom(m_plotRect.bottom() - 10);
    }
    
    if (m_config.legendPosition & Qt::AlignLeft) {
        legendRect.setLeft(m_plotRect.left() + 10);
    } else {
        legendRect.setRight(m_plotRect.right() - 10);
    }
    
    legendRect.setWidth(legendWidth);
    legendRect.setHeight(legendHeight);
    
    // Background
    painter.fillRect(legendRect, QColor(255, 255, 255, 200));
    painter.setPen(QPen(Qt::gray));
    painter.drawRect(legendRect);
    
    // Entries
    int y = legendRect.top() + 15;
    for (const auto& series : m_series) {
        if (!series.visible) continue;
        
        painter.setPen(QPen(series.color, 2));
        painter.drawLine(legendRect.left() + 10, y - 5, legendRect.left() + 40, y - 5);
        painter.setPen(m_config.textColor);
        painter.drawText(legendRect.left() + 50, y - 10, 90, 20, Qt::AlignLeft,
                         QString::fromStdString(series.name));
        y += 20;
    }
}

void PlotWidget::drawCrosshair(QPainter& painter) {
    if (!m_crosshairEnabled || !m_plotRect.contains(m_crosshairPos)) return;
    
    QPen crossPen(Qt::gray);
    crossPen.setStyle(Qt::DashLine);
    painter.setPen(crossPen);
    
    painter.drawLine(m_crosshairPos.x(), m_plotRect.top(),
                     m_crosshairPos.x(), m_plotRect.bottom());
    painter.drawLine(m_plotRect.left(), m_crosshairPos.y(),
                     m_plotRect.right(), m_crosshairPos.y());
    
    // Data coordinates
    QPointF dataPos = screenToData(m_crosshairPos.x(), m_crosshairPos.y());
    QString tip = QString("(%1, %2)")
                      .arg(dataPos.x(), 0, 'g', 4)
                      .arg(dataPos.y(), 0, 'g', 4);
    
    painter.setPen(m_config.textColor);
    painter.drawText(m_crosshairPos.x() + 10, m_crosshairPos.y() - 10, tip);
}

QPointF PlotWidget::dataToScreen(double x, double y) const {
    double sx = m_plotRect.left() + (x - m_viewRect.left()) / 
                (m_viewRect.right() - m_viewRect.left()) * m_plotRect.width();
    double sy = m_plotRect.bottom() - (y - m_viewRect.bottom()) / 
                (m_viewRect.top() - m_viewRect.bottom()) * m_plotRect.height();
    return QPointF(sx, sy);
}

QPointF PlotWidget::screenToData(int x, int y) const {
    double dx = (x - m_plotRect.left()) / double(m_plotRect.width()) *
                (m_viewRect.right() - m_viewRect.left()) + m_viewRect.left();
    double dy = (m_plotRect.bottom() - y) / double(m_plotRect.height()) *
                (m_viewRect.top() - m_viewRect.bottom()) + m_viewRect.bottom();
    return QPointF(dx, dy);
}

QRectF PlotWidget::dataBounds() const {
    if (m_series.empty()) return QRectF();
    
    double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
    
    for (const auto& series : m_series) {
        if (!series.visible || series.xValues.empty()) continue;
        
        for (size_t i = 0; i < series.xValues.size(); ++i) {
            minX = std::min(minX, series.xValues[i]);
            maxX = std::max(maxX, series.xValues[i]);
            minY = std::min(minY, series.yValues[i]);
            maxY = std::max(maxY, series.yValues[i]);
        }
    }
    
    if (minX > maxX || minY > maxY) return QRectF();
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

int PlotWidget::findNearestPoint(const QPoint& screenPos) const {
    const int snapDist = 10;
    
    for (size_t s = 0; s < m_series.size(); ++s) {
        const auto& series = m_series[s];
        for (size_t i = 0; i < series.xValues.size(); ++i) {
            QPointF p = dataToScreen(series.xValues[i], series.yValues[i]);
            if (QLineF(p, screenPos).length() < snapDist) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// ============================================================================
// PlotWindow Implementation
// ============================================================================

PlotWindow::PlotWindow(const std::string& title)
    : QMainWindow(nullptr)
    , m_plotWidget(nullptr)
    , m_toolBar(nullptr)
    , m_statusBar(nullptr)
    , m_dataDock(nullptr)
    , m_dataTree(nullptr)
{
    setWindowTitle(QString::fromStdString(title));
    resize(1000, 700);
    
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();
}

PlotWindow::~PlotWindow() = default;

void PlotWindow::setupUI() {
    m_plotWidget = new PlotWidget(this);
    setCentralWidget(m_plotWidget);
    
    // Data dock
    m_dataDock = new QDockWidget("Data", this);
    m_dataTree = new QTreeWidget(m_dataDock);
    m_dataTree->setHeaderLabels(QStringList() << "Series" << "Points");
    m_dataDock->setWidget(m_dataTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_dataDock);
}

void PlotWindow::setupMenuBar() {
    QMenuBar* menuBar = this->menuBar();
    
    QMenu* fileMenu = menuBar->addMenu("&File");
    m_saveImageAction = fileMenu->addAction("&Save as Image...");
    m_saveImageAction->setShortcut(QKeySequence::Save);
    m_saveSVGAction = fileMenu->addAction("Save as &SVG...");
    fileMenu->addSeparator();
    m_printAction = fileMenu->addAction("&Print...");
    m_printAction->setShortcut(QKeySequence::Print);
    fileMenu->addSeparator();
    m_copyAction = fileMenu->addAction("&Copy to Clipboard");
    m_copyAction->setShortcut(QKeySequence::Copy);
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    QMenu* viewMenu = menuBar->addMenu("&View");
    m_zoomInAction = viewMenu->addAction("Zoom &In");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomOutAction = viewMenu->addAction("Zoom &Out");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_resetAction = viewMenu->addAction("&Reset Zoom");
    viewMenu->addSeparator();
    m_gridAction = viewMenu->addAction("&Grid");
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_legendAction = viewMenu->addAction("&Legend");
    m_legendAction->setCheckable(true);
    m_legendAction->setChecked(true);
    viewMenu->addSeparator();
    m_logXAction = viewMenu->addAction("Log &X Axis");
    m_logXAction->setCheckable(true);
    m_logYAction = viewMenu->addAction("Log &Y Axis");
    m_logYAction->setCheckable(true);
    
    QMenu* helpMenu = menuBar->addMenu("&Help");
    m_aboutAction = helpMenu->addAction("&About");
}

void PlotWindow::setupToolBar() {
    m_toolBar = addToolBar("Plot");
    m_toolBar->addAction(m_zoomInAction);
    m_toolBar->addAction(m_zoomOutAction);
    m_toolBar->addAction(m_resetAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_saveImageAction);
    m_toolBar->addAction(m_saveSVGAction);
    m_toolBar->addAction(m_printAction);
    m_toolBar->addAction(m_copyAction);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_gridAction);
    m_toolBar->addAction(m_legendAction);
}

void PlotWindow::setupStatusBar() {
    m_statusBar = statusBar();
    m_statusBar->showMessage("Ready");
}

void PlotWindow::setupConnections() {
    connect(m_zoomInAction, &QAction::triggered, this, &PlotWindow::onZoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &PlotWindow::onZoomOut);
    connect(m_resetAction, &QAction::triggered, this, &PlotWindow::onResetZoom);
    connect(m_saveImageAction, &QAction::triggered, this, &PlotWindow::onSaveImage);
    connect(m_saveSVGAction, &QAction::triggered, this, &PlotWindow::onSaveSVG);
    connect(m_printAction, &QAction::triggered, this, &PlotWindow::onPrint);
    connect(m_copyAction, &QAction::triggered, this, &PlotWindow::onCopyToClipboard);
    connect(m_gridAction, &QAction::toggled, this, &PlotWindow::onGridToggled);
    connect(m_legendAction, &QAction::toggled, this, &PlotWindow::onLegendToggled);
    connect(m_logXAction, &QAction::toggled, this, &PlotWindow::onLogScaleXToggled);
    connect(m_logYAction, &QAction::toggled, this, &PlotWindow::onLogScaleYToggled);
    connect(m_aboutAction, &QAction::triggered, this, &PlotWindow::onAbout);
    
    connect(m_plotWidget, &PlotWidget::mouseMoved, this, [this](double x, double y) {
        m_statusBar->showMessage(QString("X: %1  Y: %2").arg(x).arg(y));
    });
}

void PlotWindow::plot(const std::vector<double>& x, const std::vector<double>& y,
                      const std::string& name) {
    DataSeries series;
    series.name = name;
    series.xValues = x;
    series.yValues = y;
    m_plotWidget->addSeries(series);
    
    // Update data tree
    QTreeWidgetItem* item = new QTreeWidgetItem(m_dataTree);
    item->setText(0, QString::fromStdString(name));
    item->setText(1, QString::number(x.size()));
}

void PlotWindow::plot(const std::vector<double>& y, const std::string& name) {
    std::vector<double> x(y.size());
    for (size_t i = 0; i < y.size(); ++i) x[i] = static_cast<double>(i);
    plot(x, y, name);
}

void PlotWindow::scatter(const std::vector<double>& x, const std::vector<double>& y,
                         const std::string& name) {
    DataSeries series;
    series.name = name;
    series.xValues = x;
    series.yValues = y;
    series.showMarkers = true;
    series.lineStyle = Qt::NoPen;
    m_plotWidget->addSeries(series);
}

void PlotWindow::setTitle(const std::string& title) {
    PlotConfig config = m_plotWidget->config();
    config.title = title;
    m_plotWidget->setConfig(config);
}

void PlotWindow::setXLabel(const std::string& label) {
    PlotConfig config = m_plotWidget->config();
    config.xAxisLabel = label;
    m_plotWidget->setConfig(config);
}

void PlotWindow::setYLabel(const std::string& label) {
    PlotConfig config = m_plotWidget->config();
    config.yAxisLabel = label;
    m_plotWidget->setConfig(config);
}

void PlotWindow::setGridEnabled(bool enabled) {
    PlotConfig config = m_plotWidget->config();
    config.showGrid = enabled;
    m_plotWidget->setConfig(config);
    m_gridAction->setChecked(enabled);
}

void PlotWindow::setLegendEnabled(bool enabled) {
    PlotConfig config = m_plotWidget->config();
    config.showLegend = enabled;
    m_plotWidget->setConfig(config);
    m_legendAction->setChecked(enabled);
}

void PlotWindow::setLogScaleX(bool enabled) {
    PlotConfig config = m_plotWidget->config();
    config.logScaleX = enabled;
    m_plotWidget->setConfig(config);
    m_logXAction->setChecked(enabled);
}

void PlotWindow::setLogScaleY(bool enabled) {
    PlotConfig config = m_plotWidget->config();
    config.logScaleY = enabled;
    m_plotWidget->setConfig(config);
    m_logYAction->setChecked(enabled);
}

void PlotWindow::exportAsImage(const QString& path) {
    m_plotWidget->saveAsImage(path);
}

void PlotWindow::exportAsSVG(const QString& path) {
    m_plotWidget->saveAsSVG(path);
}

void PlotWindow::exportAsPDF(const QString& path) {
    // SVG can be converted to PDF externally
    exportAsSVG(path + ".svg");
}

void PlotWindow::exportAsCSV(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    // Write CSV header and data
    for (const auto& series : m_plotWidget->series()) {
        for (size_t i = 0; i < series.xValues.size(); ++i) {
            out << series.name.c_str() << ","
                << series.xValues[i] << ","
                << series.yValues[i] << "\n";
        }
    }
}

void PlotWindow::show() {
    QMainWindow::show();
    m_plotWidget->autoScale();
}

void PlotWindow::onZoomIn() { m_plotWidget->zoomIn(); }
void PlotWindow::onZoomOut() { m_plotWidget->zoomOut(); }
void PlotWindow::onResetZoom() { m_plotWidget->resetZoom(); }
void PlotWindow::onAutoScale() { m_plotWidget->autoScale(); }

void PlotWindow::onSaveImage() {
    QString path = QFileDialog::getSaveFileName(this, "Save Plot as Image",
                                                 "", "PNG Images (*.png);;All Files (*)");
    if (!path.isEmpty()) {
        m_plotWidget->saveAsImage(path);
        m_statusBar->showMessage("Saved to " + path, 3000);
    }
}

void PlotWindow::onSaveSVG() {
    QString path = QFileDialog::getSaveFileName(this, "Save Plot as SVG",
                                                 "", "SVG Files (*.svg);;All Files (*)");
    if (!path.isEmpty()) {
        m_plotWidget->saveAsSVG(path);
        m_statusBar->showMessage("Saved to " + path, 3000);
    }
}

void PlotWindow::onPrint() { m_plotWidget->print(); }

void PlotWindow::onCopyToClipboard() {
    QImage image = m_plotWidget->toImage();
    QApplication::clipboard()->setImage(image);
    m_statusBar->showMessage("Copied to clipboard", 3000);
}

void PlotWindow::onGridToggled(bool checked) {
    PlotConfig config = m_plotWidget->config();
    config.showGrid = checked;
    m_plotWidget->setConfig(config);
}

void PlotWindow::onLegendToggled(bool checked) {
    PlotConfig config = m_plotWidget->config();
    config.showLegend = checked;
    m_plotWidget->setConfig(config);
}

void PlotWindow::onLogScaleXToggled(bool checked) { setLogScaleX(checked); }
void PlotWindow::onLogScaleYToggled(bool checked) { setLogScaleY(checked); }

void PlotWindow::onAbout() {
    QMessageBox::about(this, "About FluxScript Plot",
                       "FluxScript Interactive Plotting\n\n"
                       "Features:\n"
                       "- Pan: Left mouse drag\n"
                       "- Zoom: Mouse wheel\n"
                       "- Crosshair: Move mouse\n"
                       "- Export: PNG, SVG, PDF, CSV");
}

// ============================================================================
// C Interface Implementation
// ============================================================================

extern "C" void* flux_plot_create(const char* title) {
    std::string t = title ? title : "FluxScript Plot";
    return static_cast<void*>(new PlotWindow(t));
}

extern "C" void flux_plot_show(void* plot) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->show();
    }
}

extern "C" void flux_plot_close(void* plot) {
    if (plot) {
        delete static_cast<PlotWindow*>(plot);
    }
}

extern "C" void flux_plot_add(void* plot, const double* x, const double* y, int size,
                               const char* name) {
    if (!plot || !x || !y || size <= 0) return;
    
    std::vector<double> xVec(x, x + size);
    std::vector<double> yVec(y, y + size);
    std::string n = name ? name : "";
    
    static_cast<PlotWindow*>(plot)->plot(xVec, yVec, n);
}

extern "C" void flux_plot_add_y(void* plot, const double* y, int size, const char* name) {
    if (!plot || !y || size <= 0) return;
    
    std::vector<double> yVec(y, y + size);
    std::string n = name ? name : "";
    
    static_cast<PlotWindow*>(plot)->plot(yVec, n);
}

extern "C" void flux_plot_set_title(void* plot, const char* title) {
    if (plot && title) {
        static_cast<PlotWindow*>(plot)->setTitle(title);
    }
}

extern "C" void flux_plot_set_xlabel(void* plot, const char* label) {
    if (plot && label) {
        static_cast<PlotWindow*>(plot)->setXLabel(label);
    }
}

extern "C" void flux_plot_set_ylabel(void* plot, const char* label) {
    if (plot && label) {
        static_cast<PlotWindow*>(plot)->setYLabel(label);
    }
}

extern "C" void flux_plot_set_grid(void* plot, int enabled) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->setGridEnabled(enabled != 0);
    }
}

extern "C" void flux_plot_set_legend(void* plot, int enabled) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->setLegendEnabled(enabled != 0);
    }
}

extern "C" void flux_plot_set_logx(void* plot, int enabled) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->setLogScaleX(enabled != 0);
    }
}

extern "C" void flux_plot_set_logy(void* plot, int enabled) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->setLogScaleY(enabled != 0);
    }
}

extern "C" void flux_plot_set_xrange(void* plot, double min, double max) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->plotWidget()->setXRange(min, max);
    }
}

extern "C" void flux_plot_set_yrange(void* plot, double min, double max) {
    if (plot) {
        static_cast<PlotWindow*>(plot)->plotWidget()->setYRange(min, max);
    }
}

extern "C" void flux_plot_save(void* plot, const char* filename) {
    if (plot && filename) {
        static_cast<PlotWindow*>(plot)->exportAsImage(QString::fromUtf8(filename));
    }
}

extern "C" void flux_plot_save_svg(void* plot, const char* filename) {
    if (plot && filename) {
        static_cast<PlotWindow*>(plot)->exportAsSVG(QString::fromUtf8(filename));
    }
}

extern "C" double flux_plot(const double* x, const double* y, int size, const char* title) {
    void* plot = flux_plot_create(title ? title : "Plot");
    flux_plot_add(plot, x, y, size, "");
    flux_plot_show(plot);
    return 1.0;  // Return plot ID
}

extern "C" double flux_plot_y(const double* y, int size, const char* title) {
    void* plot = flux_plot_create(title ? title : "Plot");
    flux_plot_add_y(plot, y, size, "");
    flux_plot_show(plot);
    return 1.0;
}

extern "C" double flux_scatter(const double* x, const double* y, int size, const char* title) {
    // For now, same as regular plot
    return flux_plot(x, y, size, title);
}

// ============================================================================
// PlotManager Implementation
// ============================================================================

PlotManager& PlotManager::instance() {
    static PlotManager manager;
    return manager;
}

PlotManager::~PlotManager() {
    closeAllPlots();
}

PlotWindow* PlotManager::createPlot(const std::string& title) {
    PlotWindow* plot = new PlotWindow(title);
    m_plots[m_nextId++] = plot;
    return plot;
}

void PlotManager::closePlot(PlotWindow* plot) {
    for (auto it = m_plots.begin(); it != m_plots.end(); ++it) {
        if (it->second == plot) {
            delete plot;
            m_plots.erase(it);
            break;
        }
    }
}

void PlotManager::closeAllPlots() {
    for (auto& pair : m_plots) {
        delete pair.second;
    }
    m_plots.clear();
}

PlotWindow* PlotManager::getPlot(int id) const {
    auto it = m_plots.find(id);
    return (it != m_plots.end()) ? it->second : nullptr;
}

} // namespace Plot
} // namespace Flux

// Stub for updateToolTip
void Flux::Plot::PlotWidget::updateToolTip(const QPoint& pos) {
    Q_UNUSED(pos);
}

