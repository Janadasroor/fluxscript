#ifndef FLUX_PLOT_H
#define FLUX_PLOT_H

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QImage>
#include <QColor>
#include <QRectF>
#include <QPointF>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTreeWidget>

namespace Flux {
namespace Plot {

// Data series
struct DataSeries {
    std::string name;
    std::vector<double> xValues;
    std::vector<double> yValues;
    QColor color;
    Qt::PenStyle lineStyle;
    int lineWidth;
    bool showMarkers;
    Qt::BrushStyle markerStyle;
    int markerSize;
    bool visible;
    
    DataSeries() : color(Qt::blue), lineStyle(Qt::SolidLine), lineWidth(2),
                   showMarkers(false), markerStyle(Qt::SolidPattern),
                   markerSize(4), visible(true) {}
};

// Plot configuration
struct PlotConfig {
    std::string title;
    std::string xAxisLabel;
    std::string yAxisLabel;
    bool showGrid;
    bool showLegend;
    Qt::Alignment legendPosition;
    bool logScaleX;
    bool logScaleY;
    QColor backgroundColor;
    QColor gridColor;
    QColor textColor;
    QFont titleFont;
    QFont labelFont;
    QFont tickFont;
    int width;
    int height;
    
    PlotConfig() : showGrid(true), showLegend(true), 
                   legendPosition(Qt::AlignTop | Qt::AlignRight),
                   logScaleX(false), logScaleY(false),
                   backgroundColor(Qt::white), gridColor(QColor(200, 200, 200)),
                   textColor(Qt::black), width(800), height(600) {
        titleFont = QFont("Arial", 14, QFont::Bold);
        labelFont = QFont("Arial", 12);
        tickFont = QFont("Arial", 10);
    }
};

// Interactive plot widget
class PlotWidget : public QWidget {
    Q_OBJECT

    friend class PlotWindow;  // Allow PlotWindow to access m_series
    
public:
    explicit PlotWidget(QWidget* parent = nullptr);
    ~PlotWidget();
    
    void addSeries(const DataSeries& series);
    void removeSeries(const std::string& name);
    void clearSeries();
    void updateSeries(const std::string& name, const std::vector<double>& x, 
                      const std::vector<double>& y);
    
    void setConfig(const PlotConfig& config);
    const PlotConfig& config() const { return m_config; }
    
    
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void setXRange(double min, double max);
    void setYRange(double min, double max);
    void autoScale();
    
    QImage toImage() const;
    void saveAsImage(const QString& path);
    void saveAsSVG(const QString& path);
    void print();
    
    void enablePan(bool enabled);
    void enableZoom(bool enabled);
    void enableCrosshair(bool enabled);
    void enableDataTips(bool enabled);

    // Public access for export
    const std::vector<DataSeries>& series() const { return m_series; }

Q_SIGNALS:
    void dataPointClicked(int seriesIndex, int pointIndex);
    void rangeChanged(double xMin, double xMax, double yMin, double yMax);
    void mouseMoved(double x, double y);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private Q_SLOTS:
    void updateToolTip(const QPoint& pos);
    
private:
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    void drawSeries(QPainter& painter);
    void drawLegend(QPainter& painter);
    void drawCrosshair(QPainter& painter);
    
    QPointF dataToScreen(double x, double y) const;
    QPointF screenToData(int x, int y) const;
    QRectF dataBounds() const;
    int findNearestPoint(const QPoint& screenPos) const;

    std::vector<DataSeries> m_series;
    PlotConfig m_config;
    
    QRectF m_viewRect;
    QRectF m_fullRect;
    bool m_isPanning;
    QPoint m_lastPanPos;
    bool m_panEnabled;
    bool m_zoomEnabled;
    bool m_crosshairEnabled;
    bool m_dataTipsEnabled;
    QPoint m_crosshairPos;
    int m_hoveredSeries;
    int m_hoveredPoint;
    
    int m_marginLeft;
    int m_marginRight;
    int m_marginTop;
    int m_marginBottom;
    QRect m_plotRect;
};

// Plot window
class PlotWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit PlotWindow(const std::string& title = "FluxScript Plot");
    ~PlotWindow();
    
    PlotWidget* plotWidget() { return m_plotWidget; }
    const PlotWidget* plotWidget() const { return m_plotWidget; }
    
    void plot(const std::vector<double>& x, const std::vector<double>& y,
              const std::string& name = "");
    void plot(const std::vector<double>& y, const std::string& name = "");
    void scatter(const std::vector<double>& x, const std::vector<double>& y,
                 const std::string& name = "");
    
    void addPlot(const std::vector<double>& x, const std::vector<double>& y,
                 const std::string& name = "");
    
    void setTitle(const std::string& title);
    void setXLabel(const std::string& label);
    void setYLabel(const std::string& label);
    void setGridEnabled(bool enabled);
    void setLegendEnabled(bool enabled);
    void setLogScaleX(bool enabled);
    void setLogScaleY(bool enabled);
    
    void exportAsImage(const QString& path);
    void exportAsSVG(const QString& path);
    void exportAsPDF(const QString& path);
    void exportAsCSV(const QString& path);
    
    void show();
    
private Q_SLOTS:
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    void onAutoScale();
    void onSaveImage();
    void onSaveSVG();
    void onPrint();
    void onCopyToClipboard();
    void onGridToggled(bool checked);
    void onLegendToggled(bool checked);
    void onLogScaleXToggled(bool checked);
    void onLogScaleYToggled(bool checked);
    void onAbout();
    
private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    
    PlotWidget* m_plotWidget;
    QToolBar* m_toolBar;
    QStatusBar* m_statusBar;
    QDockWidget* m_dataDock;
    QTreeWidget* m_dataTree;
    
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    QAction* m_resetAction;
    QAction* m_saveImageAction;
    QAction* m_saveSVGAction;
    QAction* m_printAction;
    QAction* m_copyAction;
    QAction* m_gridAction;
    QAction* m_legendAction;
    QAction* m_logXAction;
    QAction* m_logYAction;
    QAction* m_aboutAction;
};

// C interface for JIT
extern "C" {
    void* flux_plot_create(const char* title);
    void flux_plot_show(void* plot);
    void flux_plot_close(void* plot);
    
    void flux_plot_add(void* plot, const double* x, const double* y, int size,
                       const char* name);
    void flux_plot_add_y(void* plot, const double* y, int size, const char* name);
    
    void flux_plot_set_title(void* plot, const char* title);
    void flux_plot_set_xlabel(void* plot, const char* label);
    void flux_plot_set_ylabel(void* plot, const char* label);
    void flux_plot_set_grid(void* plot, int enabled);
    void flux_plot_set_legend(void* plot, int enabled);
    void flux_plot_set_logx(void* plot, int enabled);
    void flux_plot_set_logy(void* plot, int enabled);
    void flux_plot_set_xrange(void* plot, double min, double max);
    void flux_plot_set_yrange(void* plot, double min, double max);
    
    void flux_plot_save(void* plot, const char* filename);
    void flux_plot_save_svg(void* plot, const char* filename);
    
    double flux_plot(const double* x, const double* y, int size, const char* title);
    double flux_plot_y(const double* y, int size, const char* title);
    double flux_scatter(const double* x, const double* y, int size, const char* title);
}

// Plot manager
class PlotManager {
public:
    static PlotManager& instance();
    
    PlotWindow* createPlot(const std::string& title = "Plot");
    void closePlot(PlotWindow* plot);
    void closeAllPlots();
    
    PlotWindow* getPlot(int id) const;
    std::vector<PlotWindow*> allPlots() const;
    int plotCount() const { return static_cast<int>(m_plots.size()); }
    
private:
    PlotManager() = default;
    ~PlotManager();
    
    std::map<int, PlotWindow*> m_plots;
    int m_nextId = 1;
};

} // namespace Plot
} // namespace Flux

#endif // FLUX_PLOT_H
