#include "bridge.h"
#include <flux/jit_engine.h>
#include <QApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDial>
#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLCDNumber>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QMetaProperty>
#include <QProgressBar>
#include <QPushButton>
#include <QAbstractButton>
#include <QRadioButton>
#include <QToolButton>
#include <QCommandLinkButton>
#include <QChartView>
#include <QtCharts/QSplineSeries>
#include <QScatterSeries>
#include <QAreaSeries>
#include <QLineSeries>
#include <QBarSeries>
#include <QStackedBarSeries>
#include <QHorizontalBarSeries>
#include <QBarSet>
#include <QPieSeries>
#include <QPieSlice>
#include <QValueAxis>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QScrollArea>
#include <QStatusBar>
#include <QSvgWidget>
#include <QToolBar>
#include <QCalendarWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QProgressDialog>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#define QT_LOG if (true) std::cerr << "[qt] "

// ===========================================================================
// Handle encoding: bit-cast void* <-> double
// ===========================================================================
static double ptr_to_double(const void* p) {
    double d;
    std::memcpy(&d, &p, sizeof(d));
    return d;
}
static void* double_to_ptr(double d) {
    void* p;
    std::memcpy(&p, &d, sizeof(p));
    return p;
}
static const char* dbl_to_str(double d) {
    return static_cast<const char*>(double_to_ptr(d));
}

// ===========================================================================
// FluxQtBridge singleton — QObject registry, signal routing
// ===========================================================================
FluxQtBridge& FluxQtBridge::instance() {
    static FluxQtBridge s;
    return s;
}

FluxQtBridge::FluxQtBridge(QObject* parent) : QObject(parent) {}

double FluxQtBridge::registerObject(QObject* obj) {
    std::lock_guard lock(m_mutex);
    m_registry[obj] = QPointer(obj);
    return ptr_to_double(obj);
}

QObject* FluxQtBridge::resolveHandle(double handle, const char* caller) {
    std::lock_guard lock(m_mutex);
    auto* raw = double_to_ptr(handle);
    auto it = m_registry.find(raw);
    if (it == m_registry.end() || it->isNull()) {
        if (it != m_registry.end()) m_registry.erase(it);
        if (caller) QT_LOG << caller << ": handle " << handle << " not found\n";
        return nullptr;
    }
    return it->data();
}

void FluxQtBridge::connectSignalByName(double handle, const char* signal, const char* funcName) {
    auto* obj = resolveHandle(handle);
    if (!obj) return;
    m_signalNameMap[obj] = QString::fromUtf8(funcName);
    QByteArray normalized = QMetaObject::normalizedSignature(signal);
    int sigIdx = obj->metaObject()->indexOfSignal(normalized.constData());
    int slotIdx = FluxQtBridge::staticMetaObject.indexOfSlot("onBridgeEvent()");
    if (sigIdx >= 0 && slotIdx >= 0)
        QMetaObject::connect(obj, sigIdx, this, slotIdx);
}

void FluxQtBridge::onBridgeEvent() {
    auto* sender = qobject_cast<QObject*>(QObject::sender());
    if (!sender) return;
    auto it = m_signalNameMap.constFind(sender);
    if (it == m_signalNameMap.constEnd()) return;
    extern void callFluxFunction(const char* name);
    callFluxFunction(it.value().toUtf8().constData());
}

void FluxQtBridge::clearRegistry() {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSet<QObject*> deleted;
    for (auto it = m_registry.begin(); it != m_registry.end(); ++it) {
        QObject* obj = it->data();
        if (obj && !deleted.contains(obj) && obj != m_persistentWindow) {
            deleted.insert(obj);
            delete obj;
        }
    }
    m_registry.clear();
    m_signalNameMap.clear();
    // Re-register persistent window so its handle stays valid
    if (m_persistentWindow) {
        m_registry[m_persistentWindow] = QPointer<QObject>(m_persistentWindow);
    }
}

void FluxQtBridge::setPersistentWindow(QMainWindow* win) {
    m_persistentWindow = win;
    if (win) registerObject(win);
}

// ===========================================================================
// Property helpers
// ===========================================================================

#define RESOLVE(var, type, handle) \
    auto* var = qobject_cast<type*>(FluxQtBridge::instance().resolveHandle(handle, __func__)); \
    if (!var) return

#define RESOLVE_DBL(var, type, handle) \
    auto* var = qobject_cast<type*>(FluxQtBridge::instance().resolveHandle(handle, __func__)); \
    if (!var) return 0.0

static double get_numeric_prop(double h, const char* name) {
    auto* obj = FluxQtBridge::instance().resolveHandle(h);
    if (!obj) return 0.0;
    auto val = obj->property(name);
    if (val.typeId() == QMetaType::Double) return val.toDouble();
    if (val.typeId() == QMetaType::Int) return static_cast<double>(val.toInt());
    if (val.typeId() == QMetaType::Bool) return val.toBool() ? 1.0 : 0.0;
    if (val.typeId() == QMetaType::QString) {
        auto s = val.toString().toUtf8();
        return ptr_to_double(strdup(s.constData()));
    }
    return 0.0;
}

static void set_numeric_prop(double h, const char* name, double value) {
    auto* obj = FluxQtBridge::instance().resolveHandle(h);
    if (!obj) return;
    auto prop = obj->metaObject()->property(obj->metaObject()->indexOfProperty(name));
    if (!prop.isValid()) return;
    if (prop.typeId() == QMetaType::Double) obj->setProperty(name, value);
    else if (prop.typeId() == QMetaType::Int) obj->setProperty(name, static_cast<int>(value));
    else if (prop.typeId() == QMetaType::Bool) obj->setProperty(name, value != 0.0);
    else obj->setProperty(name, value);
}

// ===========================================================================
// Drag-and-drop support
// ===========================================================================
static std::string g_lastDroppedFile;

class DropFilter : public QObject {
public:
    DropFilter(QObject* parent, const char* callback)
        : QObject(parent), m_callback(callback) {
        parent->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
        if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            auto urls = de->mimeData()->urls();
            if (!urls.isEmpty()) {
                QString path = urls.first().toLocalFile();
                if (path.endsWith(".flux", Qt::CaseInsensitive) ||
                    path.endsWith(".flxsch", Qt::CaseInsensitive)) {
                    g_lastDroppedFile = path.toStdString();
                    extern void callFluxFunction(const char* name);
                    callFluxFunction(m_callback.c_str());
                }
            }
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
private:
    std::string m_callback;
};

// ===========================================================================
// Widget creation
// ===========================================================================
extern "C" {

static double flux_qt_create_window(double title_dbl) {
    auto* w = new QDialog;
    w->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->resize(400, 500);
    w->show();
    return FluxQtBridge::instance().registerObject(w);
}

static double flux_qt_create_button(double text_dbl) {
    auto* b = new QPushButton(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(b);
}

static double flux_qt_create_label(double text_dbl) {
    auto* l = new QLabel(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(l);
}

static double flux_qt_create_slider(double orient) {
    auto* s = new QSlider(static_cast<int>(orient) == 0 ? Qt::Horizontal : Qt::Vertical);
    s->setRange(0, 100);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_create_dial() {
    auto* d = new QDial;
    d->setRange(0, 100);
    d->setWrapping(true);
    d->setNotchTarget(10.0);
    d->setNotchesVisible(true);
    return FluxQtBridge::instance().registerObject(d);
}

static double flux_qt_create_lcd() {
    auto* l = new QLCDNumber(8);
    return FluxQtBridge::instance().registerObject(l);
}

static double flux_qt_create_combobox() {
    auto* c = new QComboBox;
    return FluxQtBridge::instance().registerObject(c);
}

static double flux_qt_create_spinbox() {
    auto* s = new QSpinBox;
    s->setRange(0, 1000000);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_create_progressbar() {
    auto* p = new QProgressBar;
    p->setRange(0, 100);
    return FluxQtBridge::instance().registerObject(p);
}

static double flux_qt_create_checkbox(double text_dbl) {
    auto* c = new QCheckBox(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(c);
}

static double flux_qt_create_lineedit(double text_dbl) {
    auto* e = new QLineEdit(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(e);
}

static double flux_qt_create_textedit() {
    auto* e = new QTextEdit;
    return FluxQtBridge::instance().registerObject(e);
}

static double flux_qt_create_tableview(double rows, double cols) {
    auto* t = new QTableWidget(static_cast<int>(rows), static_cast<int>(cols));
    return FluxQtBridge::instance().registerObject(t);
}

static double flux_qt_create_tabwidget() {
    auto* tw = new QTabWidget;
    return FluxQtBridge::instance().registerObject(tw);
}

static double flux_qt_create_groupbox(double title_dbl) {
    auto* gb = new QGroupBox(QString::fromUtf8(dbl_to_str(title_dbl)));
    return FluxQtBridge::instance().registerObject(gb);
}

static double flux_qt_create_radiobutton(double text_dbl) {
    auto* rb = new QRadioButton(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(rb);
}

// ── QToolButton ──
static double flux_qt_create_toolbutton(double text_dbl) {
    auto* tb = new QToolButton;
    if (std::strlen(dbl_to_str(text_dbl)) > 0)
        tb->setText(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(tb);
}

static void flux_qt_toolbutton_set_arrow_type(double h, double type) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (tb) tb->setArrowType(static_cast<Qt::ArrowType>(static_cast<int>(type)));
}

static void flux_qt_toolbutton_set_popup_mode(double h, double mode) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (tb) tb->setPopupMode(static_cast<QToolButton::ToolButtonPopupMode>(static_cast<int>(mode)));
}

static void flux_qt_toolbutton_set_toolbutton_style(double h, double style) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (tb) tb->setToolButtonStyle(static_cast<Qt::ToolButtonStyle>(static_cast<int>(style)));
}

static void flux_qt_toolbutton_set_auto_raise(double h, double enabled) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (tb) tb->setAutoRaise(enabled != 0.0);
}

static void flux_qt_toolbutton_set_menu(double h, double menu_h) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    auto* menu = qobject_cast<QMenu*>(FluxQtBridge::instance().resolveHandle(menu_h));
    if (tb && menu) tb->setMenu(menu);
}

static void flux_qt_toolbutton_show_menu(double h) {
    auto* tb = qobject_cast<QToolButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (tb) tb->showMenu();
}

// ── QCommandLinkButton ──
static double flux_qt_create_commandlinkbutton(double text_dbl) {
    auto* clb = new QCommandLinkButton(QString::fromUtf8(dbl_to_str(text_dbl)));
    return FluxQtBridge::instance().registerObject(clb);
}

static double flux_qt_commandlinkbutton_set_description(double h, double desc_dbl) {
    auto* clb = qobject_cast<QCommandLinkButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (!clb) return 0.0;
    clb->setDescription(QString::fromUtf8(dbl_to_str(desc_dbl)));
    return 1.0;
}

static void flux_qt_set_abstractbutton_icon(double h, double path_dbl) {
    auto* btn = qobject_cast<QAbstractButton*>(FluxQtBridge::instance().resolveHandle(h));
    if (btn) btn->setIcon(QIcon(QString::fromUtf8(dbl_to_str(path_dbl))));
}

static double flux_qt_create_splitter(double orient) {
    auto* s = new QSplitter(static_cast<int>(orient) == 0 ? Qt::Horizontal : Qt::Vertical);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_create_listwidget() {
    auto* lw = new QListWidget;
    return FluxQtBridge::instance().registerObject(lw);
}

static double flux_qt_create_stackedwidget() {
    auto* sw = new QStackedWidget;
    return FluxQtBridge::instance().registerObject(sw);
}

static double flux_qt_create_widget() {
    auto* w = new QWidget;
    return FluxQtBridge::instance().registerObject(w);
}

static double flux_qt_create_mainwindow(double title_dbl) {
    auto* mw = new QMainWindow;
    mw->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
    mw->setAttribute(Qt::WA_DeleteOnClose);
    mw->resize(800, 600);
    mw->show();
    return FluxQtBridge::instance().registerObject(mw);
}

static double flux_qt_get_window() {
    auto* pw = FluxQtBridge::instance().persistentWindow();
    return pw ? ptr_to_double(pw) : 0.0;
}

static double flux_qt_create_scrollarea() {
    auto* sa = new QScrollArea;
    return FluxQtBridge::instance().registerObject(sa);
}

// ===========================================================================
// Layout
// ===========================================================================
static double flux_qt_create_layout(double type_dbl) {
    const char* type = dbl_to_str(type_dbl);
    QLayout* lay = nullptr;
    if (std::strcmp(type, "vbox") == 0)
        lay = new QVBoxLayout;
    else if (std::strcmp(type, "grid") == 0)
        lay = new QGridLayout;
    else
        lay = new QHBoxLayout;
    return FluxQtBridge::instance().registerObject(lay);
}

static void flux_qt_set_layout(double container_h, double layout_h) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(container_h, __func__));
    RESOLVE(lay, QLayout, layout_h);
    if (w) w->setLayout(lay);
}

static void flux_qt_add_widget(double container, double widget) {
    auto* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(container, __func__));
    auto* child = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget, __func__));
    if (parent && child) {
        if (parent->layout()) {
            parent->layout()->addWidget(child);
        } else {
            auto* lay = new QVBoxLayout(parent);
            lay->addWidget(child);
        }
    }
}

static void flux_qt_layout_add_widget(double layout_h, double widget_h) {
    RESOLVE(lay, QLayout, layout_h);
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h, __func__));
    if (lay && w) lay->addWidget(w);
}

static void flux_qt_grid_add_widget(
    double layout_h, double widget_h,
    double row, double col, double rowSpan, double colSpan) {
    RESOLVE(grid, QGridLayout, layout_h);
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h, __func__));
    if (grid && w)
        grid->addWidget(w, static_cast<int>(row), static_cast<int>(col),
                        static_cast<int>(rowSpan), static_cast<int>(colSpan));
}

static void flux_qt_set_window_size(double h, double w, double hh) {
    auto* widget = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (widget) widget->resize(static_cast<int>(w), static_cast<int>(hh));
}

static void flux_qt_layout_set_margins(double layout_h, double l, double t, double r, double b) {
    RESOLVE(lay, QLayout, layout_h);
    lay->setContentsMargins(static_cast<int>(l), static_cast<int>(t), static_cast<int>(r), static_cast<int>(b));
}

static void flux_qt_layout_set_spacing(double layout_h, double spacing) {
    RESOLVE(lay, QLayout, layout_h);
    lay->setSpacing(static_cast<int>(spacing));
}

static void flux_qt_layout_set_stretch(double layout_h, double widget_h, double stretch) {
    RESOLVE(box, QBoxLayout, layout_h);
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h, __func__));
    if (box && w) box->setStretchFactor(w, static_cast<int>(stretch));
}

// ===========================================================================
// Misc widgets (message box, LCD, combobox, table, timer)
// ===========================================================================
static void flux_qt_msg_box(double title_dbl, double text_dbl) {
    QMessageBox::information(nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)),
        QString::fromUtf8(dbl_to_str(text_dbl)));
}

static void flux_qt_lcd_display(double h, double value) {
    auto* lcd = qobject_cast<QLCDNumber*>(FluxQtBridge::instance().resolveHandle(h));
    if (lcd) lcd->display(value);
}

static void flux_qt_combo_add_item(double h, double text_dbl) {
    auto* combo = qobject_cast<QComboBox*>(FluxQtBridge::instance().resolveHandle(h));
    if (combo) combo->addItem(QString::fromUtf8(dbl_to_str(text_dbl)));
}

static void flux_qt_combo_clear(double h) {
    auto* combo = qobject_cast<QComboBox*>(FluxQtBridge::instance().resolveHandle(h));
    if (combo) combo->clear();
}

static void flux_qt_combo_set_current_index(double h, double idx) {
    auto* combo = qobject_cast<QComboBox*>(FluxQtBridge::instance().resolveHandle(h));
    if (combo) combo->setCurrentIndex(static_cast<int>(idx));
}

static double flux_qt_table_row_count(double h) {
    auto* table = qobject_cast<QTableWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return table ? static_cast<double>(table->rowCount()) : 0.0;
}

static double flux_qt_table_col_count(double h) {
    auto* table = qobject_cast<QTableWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return table ? static_cast<double>(table->columnCount()) : 0.0;
}

static void flux_qt_table_set_value(double h, double row, double col, double value) {
    auto* table = qobject_cast<QTableWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (table) {
        auto* item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, value);
        table->setItem(static_cast<int>(row), static_cast<int>(col), item);
    }
}

static void flux_qt_table_set_item(double h, double row, double col, double text_dbl) {
    auto* table = qobject_cast<QTableWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (table)
        table->setItem(static_cast<int>(row), static_cast<int>(col),
            new QTableWidgetItem(QString::fromUtf8(dbl_to_str(text_dbl))));
}

static void flux_qt_table_set_header(double h, double col, double text_dbl) {
    auto* table = qobject_cast<QTableWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (table)
        table->setHorizontalHeaderItem(static_cast<int>(col),
            new QTableWidgetItem(QString::fromUtf8(dbl_to_str(text_dbl))));
}

static double flux_qt_create_timer(double interval_ms, double callback_dbl) {
    auto* timer = new QTimer;
    timer->setInterval(static_cast<int>(interval_ms));
    auto h = FluxQtBridge::instance().registerObject(timer);
    FluxQtBridge::instance().connectSignalByName(h, "timeout()", dbl_to_str(callback_dbl));
    return h;
}

static void flux_qt_timer_start(double h) {
    auto* timer = qobject_cast<QTimer*>(FluxQtBridge::instance().resolveHandle(h));
    if (timer) timer->start();
}

static void flux_qt_timer_stop(double h) {
    auto* timer = qobject_cast<QTimer*>(FluxQtBridge::instance().resolveHandle(h));
    if (timer) timer->stop();
}

static double flux_qt_tab_add(double h, double widget_h, double title_dbl) {
    auto* tw = qobject_cast<QTabWidget*>(FluxQtBridge::instance().resolveHandle(h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (tw && w) tw->addTab(w, QString::fromUtf8(dbl_to_str(title_dbl)));
    return 0.0;
}

static void flux_qt_tab_set_current(double h, double idx) {
    auto* tw = qobject_cast<QTabWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setCurrentIndex(static_cast<int>(idx));
}

static double flux_qt_groupbox_set_layout(double h, double layout_h) {
    auto* gb = qobject_cast<QGroupBox*>(FluxQtBridge::instance().resolveHandle(h));
    auto* lay = static_cast<QLayout*>(double_to_ptr(layout_h));
    if (gb && lay) { gb->setLayout(lay); return 1.0; }
    return 0.0;
}

// ── QSplitter ──
static void flux_qt_splitter_add(double h, double widget_h) {
    auto* sp = qobject_cast<QSplitter*>(FluxQtBridge::instance().resolveHandle(h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (sp && w) sp->addWidget(w);
}

// ── QListWidget ──
static void flux_qt_list_add_item(double h, double text_dbl) {
    auto* lw = qobject_cast<QListWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (lw) lw->addItem(QString::fromUtf8(dbl_to_str(text_dbl)));
}

static double flux_qt_list_current_row(double h) {
    auto* lw = qobject_cast<QListWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return lw ? static_cast<double>(lw->currentRow()) : -1.0;
}

static void flux_qt_list_set_current_row(double h, double row) {
    auto* lw = qobject_cast<QListWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (lw) lw->setCurrentRow(static_cast<int>(row));
}

static void flux_qt_list_clear(double h) {
    auto* lw = qobject_cast<QListWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (lw) lw->clear();
}

static double flux_qt_list_count(double h) {
    auto* lw = qobject_cast<QListWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return lw ? static_cast<double>(lw->count()) : 0.0;
}

// ── QStackedWidget ──
static double flux_qt_stacked_add(double h, double widget_h) {
    auto* sw = qobject_cast<QStackedWidget*>(FluxQtBridge::instance().resolveHandle(h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (sw && w) { sw->addWidget(w); return static_cast<double>(sw->count() - 1); }
    return -1.0;
}

static void flux_qt_stacked_set_current(double h, double idx) {
    auto* sw = qobject_cast<QStackedWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (sw) sw->setCurrentIndex(static_cast<int>(idx));
}

static double flux_qt_stacked_count(double h) {
    auto* sw = qobject_cast<QStackedWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return sw ? static_cast<double>(sw->count()) : 0.0;
}

// ── QMainWindow ──
static double flux_qt_get_menubar(double h) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(h));
    return mw ? ptr_to_double(mw->menuBar()) : 0.0;
}

static void flux_qt_set_central_widget(double h, double widget_h) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (mw && w) mw->setCentralWidget(w);
}

static void flux_qt_set_statusbar_text(double h, double text_dbl) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(h));
    if (mw) mw->statusBar()->showMessage(QString::fromUtf8(dbl_to_str(text_dbl)));
}

// ── QMenuBar / QMenu / QAction ──
static double flux_qt_create_menu(double title_dbl) {
    auto* menu = new QMenu(QString::fromUtf8(dbl_to_str(title_dbl)));
    return ptr_to_double(menu);
}

static double flux_qt_menu_add_menu(double mb_h, double title_dbl) {
    auto* mb = qobject_cast<QMenuBar*>(FluxQtBridge::instance().resolveHandle(mb_h));
    if (!mb) mb = static_cast<QMenuBar*>(double_to_ptr(mb_h));
    if (!mb) return 0.0;
    auto* menu = mb->addMenu(QString::fromUtf8(dbl_to_str(title_dbl)));
    return ptr_to_double(menu);
}

static double flux_qt_menu_add_action(double menu_h, double title_dbl, double callback_dbl) {
    auto* menu = static_cast<QMenu*>(double_to_ptr(menu_h));
    if (!menu) return 0.0;
    auto* action = menu->addAction(QString::fromUtf8(dbl_to_str(title_dbl)));
    auto h = FluxQtBridge::instance().registerObject(action);
    FluxQtBridge::instance().connectSignalByName(h, "triggered()", dbl_to_str(callback_dbl));
    return h;
}

// ── Dialogs ──
static double flux_qt_open_file_dialog(double title_dbl, double filter_dbl) {
    auto s = QFileDialog::getOpenFileName(nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)), QString(),
        QString::fromUtf8(dbl_to_str(filter_dbl)));
    return ptr_to_double(strdup(s.toUtf8().constData()));
}

static double flux_qt_save_file_dialog(double title_dbl, double filter_dbl) {
    auto s = QFileDialog::getSaveFileName(nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)), QString(),
        QString::fromUtf8(dbl_to_str(filter_dbl)));
    return ptr_to_double(strdup(s.toUtf8().constData()));
}

static double flux_qt_input_dialog(double title_dbl, double label_dbl) {
    bool ok = false;
    auto s = QInputDialog::getText(nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)),
        QString::fromUtf8(dbl_to_str(label_dbl)),
        QLineEdit::Normal, QString(), &ok);
    return ok ? ptr_to_double(strdup(s.toUtf8().constData())) : 0.0;
}

// ── Color Dialog ──
static double g_colorR = 0, g_colorG = 0, g_colorB = 0;

static double flux_qt_color_dialog(double title_dbl) {
    QColor c = QColorDialog::getColor(Qt::white, nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)));
    if (!c.isValid()) return 0.0;
    g_colorR = c.red();
    g_colorG = c.green();
    g_colorB = c.blue();
    return 1.0;
}

static double flux_qt_color_r() { return g_colorR; }
static double flux_qt_color_g() { return g_colorG; }
static double flux_qt_color_b() { return g_colorB; }

// ── Font Dialog ──
static double g_fontSize = 12;
static char g_fontFamily[256] = "Sans";

static double flux_qt_font_dialog(double title_dbl) {
    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, QFont(), nullptr,
        QString::fromUtf8(dbl_to_str(title_dbl)));
    if (!ok) return 0.0;
    g_fontSize = font.pointSize() > 0 ? font.pointSize() : 12;
    auto fam = font.family().toUtf8();
    strncpy(g_fontFamily, fam.constData(), sizeof(g_fontFamily) - 1);
    g_fontFamily[sizeof(g_fontFamily) - 1] = '\0';
    return 1.0;
}

static double flux_qt_font_size() { return g_fontSize; }
static double flux_qt_font_family() { return ptr_to_double(strdup(g_fontFamily)); }

// ── QScrollArea ──
static void flux_qt_scroll_set_widget(double h, double widget_h) {
    auto* sa = qobject_cast<QScrollArea*>(FluxQtBridge::instance().resolveHandle(h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (sa && w) sa->setWidget(w);
}

// ── SVG ──
static double flux_qt_create_svgwidget(double path_dbl) {
    auto* svg = new QSvgWidget(QString::fromUtf8(dbl_to_str(path_dbl)));
    return FluxQtBridge::instance().registerObject(svg);
}

static void flux_qt_svgwidget_load(double h, double path_dbl) {
    auto* svg = qobject_cast<QSvgWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (svg) svg->load(QString::fromUtf8(dbl_to_str(path_dbl)));
}

static void flux_qt_svgwidget_set_size(double h, double w, double hh) {
    auto* svg = qobject_cast<QSvgWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (svg) svg->setFixedSize(static_cast<int>(w), static_cast<int>(hh));
}

// ── Raytraced-image pixel buffer ──
static std::vector<QRgb> g_rayPixels;

static void flux_qt_put_pixel(double r, double g, double b) {
    static int pc = 0;
    if (pc < 3) fprintf(stderr, "[px %d] r=%f g=%f b=%f\n", pc, r, g, b);
    pc++;
    int ri = static_cast<int>(r * 255.0);
    int gi = static_cast<int>(g * 255.0);
    int bi = static_cast<int>(b * 255.0);
    g_rayPixels.push_back(qRgb(std::clamp(ri, 0, 255),
                               std::clamp(gi, 0, 255),
                               std::clamp(bi, 0, 255)));
}

static void flux_qt_render_image(double label_h, double w, double h) {
    auto* label = qobject_cast<QLabel*>(FluxQtBridge::instance().resolveHandle(label_h, __func__));
    if (!label || g_rayPixels.empty()) { g_rayPixels.clear(); return; }
    int width = static_cast<int>(w), height = static_cast<int>(h);
    QImage img(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height && y * width < (int)g_rayPixels.size(); y++) {
        for (int x = 0; x < width && y * width + x < (int)g_rayPixels.size(); x++) {
            img.setPixel(x, y, g_rayPixels[y * width + x]);
        }
    }
    label->setPixmap(QPixmap::fromImage(img).scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    g_rayPixels.clear();
}

static double flux_qt_ray_pixel_count() {
    return static_cast<double>(g_rayPixels.size());
}

// ── Images (QPixmap via QLabel) ──
static double flux_qt_label_load_image(double label_h, double path_dbl) {
    auto* label = qobject_cast<QLabel*>(FluxQtBridge::instance().resolveHandle(label_h));
    if (!label) return 0.0;
    QPixmap pix(QString::fromUtf8(dbl_to_str(path_dbl)));
    if (pix.isNull()) return 0.0;
    label->setPixmap(pix.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return 1.0;
}

static double flux_qt_label_set_pixmap(double label_h, double w, double hh) {
    auto* label = qobject_cast<QLabel*>(FluxQtBridge::instance().resolveHandle(label_h));
    if (!label) return 0.0;
    QPixmap pix(static_cast<int>(w), static_cast<int>(hh));
    pix.fill(Qt::transparent);
    label->setPixmap(pix);
    return 1.0;
}

// ── Icons ──
static void flux_qt_set_window_icon(double win_h, double path_dbl) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(win_h));
    if (w) w->setWindowIcon(QIcon(QString::fromUtf8(dbl_to_str(path_dbl))));
}

static void flux_qt_set_button_icon(double btn_h, double path_dbl) {
    auto* btn = qobject_cast<QPushButton*>(FluxQtBridge::instance().resolveHandle(btn_h));
    if (btn) btn->setIcon(QIcon(QString::fromUtf8(dbl_to_str(path_dbl))));
}

static void flux_qt_set_action_icon(double action_h, double path_dbl) {
    auto* action = qobject_cast<QAction*>(FluxQtBridge::instance().resolveHandle(action_h));
    if (action) action->setIcon(QIcon(QString::fromUtf8(dbl_to_str(path_dbl))));
}

// ── QToolBar ──
static double flux_qt_create_toolbar(double mw_h, double title_dbl) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(mw_h));
    if (!mw) return 0.0;
    auto* tb = mw->addToolBar(QString::fromUtf8(dbl_to_str(title_dbl)));
    return ptr_to_double(tb);
}

static double flux_qt_toolbar_add_action(double tb_h, double text_dbl, double callback_dbl) {
    auto* tb = static_cast<QToolBar*>(double_to_ptr(tb_h));
    if (!tb) return 0.0;
    auto* action = tb->addAction(QString::fromUtf8(dbl_to_str(text_dbl)));
    auto h = FluxQtBridge::instance().registerObject(action);
    FluxQtBridge::instance().connectSignalByName(h, "triggered()", dbl_to_str(callback_dbl));
    return h;
}

static double flux_qt_toolbar_add_separator(double tb_h) {
    auto* tb = static_cast<QToolBar*>(double_to_ptr(tb_h));
    if (!tb) return 0.0;
    auto* action = tb->addSeparator();
    return action ? ptr_to_double(action) : 0.0;
}

// ── Qt Charts ──
static double flux_qt_create_chartview() {
    auto* chart = new QChart;
    auto* cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    return FluxQtBridge::instance().registerObject(cv);
}

static void flux_qt_chart_set_title(double cv_h, double title_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
}

static void flux_qt_chart_set_animation(double cv_h, double enabled) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setAnimationOptions(enabled != 0.0 ? QChart::SeriesAnimations : QChart::NoAnimation);
}

static void flux_qt_chart_set_theme(double cv_h, double theme) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setTheme(static_cast<QChart::ChartTheme>(static_cast<int>(theme)));
}

static void flux_qt_chart_set_background(double cv_h, double r, double g, double b) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setBackgroundBrush(QBrush(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b))));
}

static void flux_qt_chart_set_drop_shadow(double cv_h, double enabled) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setDropShadowEnabled(enabled != 0.0);
}

static void flux_qt_chart_set_margins(double cv_h, double l, double t, double r, double b) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->setMargins(QMargins(static_cast<int>(l), static_cast<int>(t), static_cast<int>(r), static_cast<int>(b)));
}

static void flux_qt_chart_legend_show(double cv_h, double visible) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->legend()->setVisible(visible != 0.0);
}

static void flux_qt_chart_legend_align(double cv_h, double align) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (cv) cv->chart()->legend()->setAlignment(static_cast<Qt::Alignment>(static_cast<int>(align)));
}

// ── Line / Spline / Scatter / Area helpers ──
static void flux_qt_xy_series_append(double h, double x, double y) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->append(x, y);
}

static void flux_qt_series_append_data(double h, double data_dbl) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (!s) return;
    std::string data = dbl_to_str(data_dbl);
    char* buf = strdup(data.c_str());
    char* p = strtok(buf, ";");
    while (p) {
        char* comma = strchr(p, ',');
        if (comma) {
            *comma = '\0';
            char* endX = nullptr; double x = std::strtod(p, &endX);
            char* endY = nullptr; double y = std::strtod(comma + 1, &endY);
            if (endX != p && endY != comma + 1) s->append(x, y);
        }
        p = strtok(nullptr, ";");
    }
    free(buf);
}

static void flux_qt_xy_series_set_pen(double h, double r, double g, double b, double width) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->setPen(QPen(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)), width));
}

static double flux_qt_xy_series_count(double h) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    return s ? static_cast<double>(s->count()) : 0.0;
}

static void flux_qt_xy_series_clear(double h) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->clear();
}

static void flux_qt_xy_series_scroll(double h, double x, double y, double maxCount) {
    auto* s = qobject_cast<QXYSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (!s) return;
    s->append(x, y);
    int max = static_cast<int>(maxCount);
    while (s->count() > max)
        s->remove(0);
}

static void flux_qt_progress_set_range(double h, double min, double max) {
    auto* p = qobject_cast<QProgressBar*>(FluxQtBridge::instance().resolveHandle(h));
    if (p) p->setRange(static_cast<int>(min), static_cast<int>(max));
}

static double flux_qt_chart_add_line_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QLineSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_chart_add_spline_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QSplineSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_chart_add_scatter_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QScatterSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static void flux_qt_scatter_series_set_marker_size(double h, double size) {
    auto* s = qobject_cast<QScatterSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->setMarkerSize(size);
}

// ── Area Series (returns upper QLineSeries handle for data) ──
static double flux_qt_chart_add_area_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* upper = new QLineSeries;
    upper->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    auto* area = new QAreaSeries(upper);
    cv->chart()->addSeries(area);
    return FluxQtBridge::instance().registerObject(upper);
}

// ── Bar Series (vertical) ──
static double flux_qt_chart_add_bar_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QBarSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_chart_add_stacked_bar_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QStackedBarSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_create_bar_set(double label_dbl) {
    auto* bs = new QBarSet(QString::fromUtf8(dbl_to_str(label_dbl)));
    return FluxQtBridge::instance().registerObject(bs);
}

static void flux_qt_bar_set_append(double h, double value) {
    auto* bs = qobject_cast<QBarSet*>(FluxQtBridge::instance().resolveHandle(h));
    if (bs) *bs << value;
}

static void flux_qt_bar_set_remove(double h, double index) {
    auto* bs = qobject_cast<QBarSet*>(FluxQtBridge::instance().resolveHandle(h, __func__));
    if (bs) bs->remove(static_cast<int>(index));
}

static void flux_qt_bar_set_set_color(double h, double r, double g, double b) {
    auto* bs = qobject_cast<QBarSet*>(FluxQtBridge::instance().resolveHandle(h));
    if (bs) bs->setColor(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)));
}

static void flux_qt_bar_series_append_set(double series_h, double set_h) {
    auto* s = qobject_cast<QAbstractBarSeries*>(FluxQtBridge::instance().resolveHandle(series_h));
    auto* bs = qobject_cast<QBarSet*>(FluxQtBridge::instance().resolveHandle(set_h));
    if (s && bs) s->append(bs);
}

// ── Horizontal Bar Series ──
static double flux_qt_chart_add_hbar_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QHorizontalBarSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

// ── Pie Series ──
static double flux_qt_chart_add_pie_series(double cv_h, double name_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* s = new QPieSeries;
    s->setName(QString::fromUtf8(dbl_to_str(name_dbl)));
    cv->chart()->addSeries(s);
    return FluxQtBridge::instance().registerObject(s);
}

static double flux_qt_pie_series_append(double h, double label_dbl, double value) {
    auto* s = qobject_cast<QPieSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (!s) return 0.0;
    auto* slice = s->append(QString::fromUtf8(dbl_to_str(label_dbl)), value);
    return FluxQtBridge::instance().registerObject(slice);
}

static void flux_qt_pie_slice_set_color(double h, double r, double g, double b) {
    auto* slice = qobject_cast<QPieSlice*>(FluxQtBridge::instance().resolveHandle(h));
    if (slice) slice->setColor(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)));
}

static void flux_qt_pie_slice_set_label_visible(double h, double visible) {
    auto* slice = qobject_cast<QPieSlice*>(FluxQtBridge::instance().resolveHandle(h));
    if (slice) slice->setLabelVisible(visible != 0.0);
}

static void flux_qt_pie_slice_set_exploded(double h, double exploded) {
    auto* slice = qobject_cast<QPieSlice*>(FluxQtBridge::instance().resolveHandle(h));
    if (slice) slice->setExploded(exploded != 0.0);
}

static void flux_qt_pie_series_set_hole_size(double h, double size) {
    auto* s = qobject_cast<QPieSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->setHoleSize(size);
}

static void flux_qt_pie_series_set_labels_position(double h, double pos) {
    auto* s = qobject_cast<QPieSeries*>(FluxQtBridge::instance().resolveHandle(h));
    if (s) s->setLabelsPosition(static_cast<QPieSlice::LabelPosition>(static_cast<int>(pos)));
}

// ── Axes ──
static double flux_qt_chart_create_default_axes(double cv_h) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    cv->chart()->createDefaultAxes();
    return 1.0;
}

static double flux_qt_chart_add_axis(double cv_h, double align) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return 0.0;
    auto* axis = new QValueAxis;
    cv->chart()->addAxis(axis, static_cast<Qt::Alignment>(static_cast<int>(align)));
    return FluxQtBridge::instance().registerObject(axis);
}

static void flux_qt_axis_set_range(double h, double min, double max) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setRange(min, max);
}

static void flux_qt_axis_set_min(double h, double min) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setMin(min);
}

static void flux_qt_axis_set_max(double h, double max) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setMax(max);
}

static void flux_qt_axis_set_title(double h, double title_dbl) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setTitleText(QString::fromUtf8(dbl_to_str(title_dbl)));
}

static void flux_qt_axis_set_label_format(double h, double fmt_dbl) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setLabelFormat(QString::fromUtf8(dbl_to_str(fmt_dbl)));
}

static void flux_qt_axis_set_grid_visible(double h, double visible) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setGridLineVisible(visible != 0.0);
}

static void flux_qt_axis_set_labels_visible(double h, double visible) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setLabelsVisible(visible != 0.0);
}

static void flux_qt_axis_set_line_visible(double h, double visible) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setLineVisible(visible != 0.0);
}

static void flux_qt_axis_set_tick_count(double h, double count) {
    auto* axis = qobject_cast<QValueAxis*>(FluxQtBridge::instance().resolveHandle(h));
    if (axis) axis->setTickCount(static_cast<int>(count));
}

static void flux_qt_series_attach_axis(double series_h, double axis_h) {
    auto* series = qobject_cast<QAbstractSeries*>(FluxQtBridge::instance().resolveHandle(series_h));
    auto* axis = qobject_cast<QAbstractAxis*>(FluxQtBridge::instance().resolveHandle(axis_h));
    if (series && axis) series->attachAxis(axis);
}

static void flux_qt_chart_add_series(double cv_h, double series_h) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    auto* s = qobject_cast<QAbstractSeries*>(FluxQtBridge::instance().resolveHandle(series_h));
    if (cv && s) cv->chart()->addSeries(s);
}

static void flux_qt_chart_save_png(double cv_h, double path_dbl) {
    auto* cv = qobject_cast<QChartView*>(FluxQtBridge::instance().resolveHandle(cv_h));
    if (!cv) return;
    QPixmap pix = cv->grab();
    pix.save(QString::fromUtf8(dbl_to_str(path_dbl)), "PNG");
}

// ── QTreeWidget ──
static double flux_qt_create_treewidget() {
    auto* tw = new QTreeWidget;
    tw->setHeaderLabel("Items");
    return FluxQtBridge::instance().registerObject(tw);
}

static void flux_qt_tree_set_header(double h, double col, double text_dbl) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) {
        auto* hi = tw->headerItem();
        if (hi) hi->setText(static_cast<int>(col), QString::fromUtf8(dbl_to_str(text_dbl)));
    }
}

static void flux_qt_tree_set_column_count(double h, double count) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setColumnCount(static_cast<int>(count));
}

static void flux_qt_tree_set_column_width(double h, double col, double w) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setColumnWidth(static_cast<int>(col), static_cast<int>(w));
}

static void flux_qt_tree_resize_to_contents(double h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->resizeColumnToContents(0);
}

// ── Iteration ──
static double flux_qt_tree_top_level_count(double h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    return tw ? static_cast<double>(tw->topLevelItemCount()) : 0.0;
}

static double flux_qt_tree_top_level_item(double h, double index) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tw) return 0.0;
    auto* item = tw->topLevelItem(static_cast<int>(index));
    return item ? ptr_to_double(item) : 0.0;
}

static double flux_qt_tree_item_child_count(double item_h) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    return item ? static_cast<double>(item->childCount()) : 0.0;
}

static double flux_qt_tree_item_child(double item_h, double index) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (!item) return 0.0;
    auto* child = item->child(static_cast<int>(index));
    return child ? ptr_to_double(child) : 0.0;
}

static double flux_qt_tree_item_parent(double item_h) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (!item) return 0.0;
    auto* p = item->parent();
    return p ? ptr_to_double(p) : 0.0;
}

static double flux_qt_tree_item_row(double item_h) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (!item) return 0.0;
    auto* p = item->parent();
    return p ? static_cast<double>(p->indexOfChild(item)) : 0.0;
}

static double flux_qt_tree_invisible_root(double h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tw) return 0.0;
    return ptr_to_double(tw->invisibleRootItem());
}

// ── Items ──
static double flux_qt_tree_add_item(double h, double text_dbl) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tw) return 0.0;
    auto* item = new QTreeWidgetItem;
    item->setText(0, QString::fromUtf8(dbl_to_str(text_dbl)));
    tw->addTopLevelItem(item);
    return ptr_to_double(item);
}

static double flux_qt_tree_add_item_col(double h, double col, double text_dbl) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tw) return 0.0;
    auto* item = new QTreeWidgetItem;
    item->setText(static_cast<int>(col), QString::fromUtf8(dbl_to_str(text_dbl)));
    tw->addTopLevelItem(item);
    return ptr_to_double(item);
}

static double flux_qt_tree_add_child(double parent_h, double text_dbl) {
    auto* parent = static_cast<QTreeWidgetItem*>(double_to_ptr(parent_h));
    if (!parent) return 0.0;
    auto* item = new QTreeWidgetItem;
    item->setText(0, QString::fromUtf8(dbl_to_str(text_dbl)));
    parent->addChild(item);
    return ptr_to_double(item);
}

static void flux_qt_tree_item_set_text(double item_h, double col, double text_dbl) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setText(static_cast<int>(col), QString::fromUtf8(dbl_to_str(text_dbl)));
}

static double flux_qt_tree_item_text(double item_h, double col) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (!item) return 0.0;
    auto s = item->text(static_cast<int>(col));
    return ptr_to_double(strdup(s.toUtf8().constData()));
}

static void flux_qt_tree_item_set_icon(double item_h, double col, double path_dbl) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setIcon(static_cast<int>(col), QIcon(QString::fromUtf8(dbl_to_str(path_dbl))));
}

static void flux_qt_tree_item_set_checked(double item_h, double col, double checked) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setCheckState(static_cast<int>(col), checked != 0.0 ? Qt::Checked : Qt::Unchecked);
}

static double flux_qt_tree_item_checked(double item_h, double col) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (!item) return 0.0;
    return item->checkState(static_cast<int>(col)) == Qt::Checked ? 1.0 : 0.0;
}

static void flux_qt_tree_item_set_expanded(double item_h, double expanded) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setExpanded(expanded != 0.0);
}

static void flux_qt_tree_item_set_tooltip(double item_h, double col, double text_dbl) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setToolTip(static_cast<int>(col), QString::fromUtf8(dbl_to_str(text_dbl)));
}

static void flux_qt_tree_item_set_foreground(double item_h, double col, double r, double g, double b) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setForeground(static_cast<int>(col), QBrush(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b))));
}

static void flux_qt_tree_item_set_background(double item_h, double col, double r, double g, double b) {
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (item) item->setBackground(static_cast<int>(col), QBrush(QColor(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b))));
}

// ── Selection ──
static double flux_qt_tree_current_item(double h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tw) return 0.0;
    auto* item = tw->currentItem();
    return item ? ptr_to_double(item) : 0.0;
}

static void flux_qt_tree_set_current_item(double h, double item_h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (tw && item) tw->setCurrentItem(item);
}

static void flux_qt_tree_set_selection_mode(double h, double mode) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setSelectionMode(static_cast<QAbstractItemView::SelectionMode>(static_cast<int>(mode)));
}

// ── Visual ──
static void flux_qt_tree_set_header_visible(double h, double visible) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setHeaderHidden(visible == 0.0);
}

static void flux_qt_tree_set_alternating(double h, double enabled) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setAlternatingRowColors(enabled != 0.0);
}

static void flux_qt_tree_set_animated(double h, double enabled) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setAnimated(enabled != 0.0);
}

static void flux_qt_tree_set_indentation(double h, double pixels) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->setIndentation(static_cast<int>(pixels));
}

// ── Utilities ──
static void flux_qt_tree_clear(double h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->clear();
}

static void flux_qt_tree_sort(double h, double col, double order) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (tw) tw->sortItems(static_cast<int>(col), order != 0.0 ? Qt::AscendingOrder : Qt::DescendingOrder);
}

static void flux_qt_tree_remove_item(double h, double item_h) {
    auto* tw = qobject_cast<QTreeWidget*>(FluxQtBridge::instance().resolveHandle(h));
    auto* item = static_cast<QTreeWidgetItem*>(double_to_ptr(item_h));
    if (tw && item) {
        auto* parent = item->parent();
        if (parent) parent->removeChild(item);
        else {
            int idx = tw->indexOfTopLevelItem(item);
            if (idx >= 0) tw->takeTopLevelItem(idx);
        }
        delete item;
    }
}

// ── Signals ──
static void flux_qt_tree_on_item_clicked(double h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "itemClicked(QTreeWidgetItem*,int)", dbl_to_str(callback_dbl));
}

static void flux_qt_tree_on_current_item_changed(double h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)", dbl_to_str(callback_dbl));
}

static void flux_qt_tree_on_item_expanded(double h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "itemExpanded(QTreeWidgetItem*)", dbl_to_str(callback_dbl));
}

static void flux_qt_tree_on_item_collapsed(double h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "itemCollapsed(QTreeWidgetItem*)", dbl_to_str(callback_dbl));
}

static void flux_qt_tree_on_item_activated(double h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "itemActivated(QTreeWidgetItem*,int)", dbl_to_str(callback_dbl));
}

// ── QDockWidget ──
static double flux_qt_create_dockwidget(double title_dbl) {
    auto* dock = new QDockWidget(QString::fromUtf8(dbl_to_str(title_dbl)));
    return FluxQtBridge::instance().registerObject(dock);
}

static void flux_qt_dock_set_widget(double dock_h, double widget_h) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (dock && w) dock->setWidget(w);
}

static void flux_qt_dock_set_features(double dock_h, double features) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    if (dock) dock->setFeatures(static_cast<QDockWidget::DockWidgetFeatures>(static_cast<int>(features)));
}

static void flux_qt_dock_set_title(double dock_h, double title_dbl) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    if (dock) dock->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
}

static void flux_qt_dock_set_visible(double dock_h, double visible) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    if (dock) dock->setVisible(visible != 0.0);
}

static double flux_qt_dock_is_visible(double dock_h) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    return dock && dock->isVisible() ? 1.0 : 0.0;
}

static double flux_qt_dock_is_floating(double dock_h) {
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    return dock && dock->isFloating() ? 1.0 : 0.0;
}

// ── QMainWindow dock management ──
static void flux_qt_mainwindow_add_dock(double mw_h, double dock_h, double area) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(mw_h));
    auto* dock = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(dock_h));
    if (!mw || !dock) return;
    Qt::DockWidgetArea a;
    int ia = static_cast<int>(area);
    if (ia == 1) a = Qt::RightDockWidgetArea;
    else if (ia == 2) a = Qt::TopDockWidgetArea;
    else if (ia == 3) a = Qt::BottomDockWidgetArea;
    else a = Qt::LeftDockWidgetArea;
    mw->addDockWidget(a, dock);
}

static void flux_qt_mainwindow_tabify_docks(double mw_h, double a_h, double b_h) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(mw_h));
    auto* a = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(a_h));
    auto* b = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(b_h));
    if (mw && a && b) mw->tabifyDockWidget(a, b);
}

static void flux_qt_mainwindow_split_dock(double mw_h, double first_h, double second_h, double orient) {
    auto* mw = qobject_cast<QMainWindow*>(FluxQtBridge::instance().resolveHandle(mw_h));
    auto* first = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(first_h));
    auto* second = qobject_cast<QDockWidget*>(FluxQtBridge::instance().resolveHandle(second_h));
    if (mw && first && second)
        mw->splitDockWidget(first, second, static_cast<int>(orient) == 0 ? Qt::Horizontal : Qt::Vertical);
}

// ── Signals ──
static void flux_qt_dock_on_visibility_changed(double dock_h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(dock_h, "visibilityChanged(bool)", dbl_to_str(callback_dbl));
}

static void flux_qt_dock_on_dock_location_changed(double dock_h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(dock_h, "dockLocationChanged(Qt::DockWidgetArea)", dbl_to_str(callback_dbl));
}

static void flux_qt_dock_on_top_level_changed(double dock_h, double callback_dbl) {
    FluxQtBridge::instance().connectSignalByName(dock_h, "topLevelChanged(bool)", dbl_to_str(callback_dbl));
}

// ===========================================================================
// Properties — numeric
// ===========================================================================
static double flux_qt_get_property(double handle, double name_dbl) {
    return get_numeric_prop(handle, dbl_to_str(name_dbl));
}

static double flux_qt_set_property(double handle, double name_dbl, double value) {
    set_numeric_prop(handle, dbl_to_str(name_dbl), value);
    return 0.0;
}

// ===========================================================================
// Properties — string  (new: set/get text properties on any widget)
// ===========================================================================
static double flux_qt_set_str_prop(double handle, double name_dbl, double value_dbl) {
    auto* obj = FluxQtBridge::instance().resolveHandle(handle);
    if (!obj) return 0.0;
    obj->setProperty(dbl_to_str(name_dbl), QString::fromUtf8(dbl_to_str(value_dbl)));
    return 0.0;
}

static double flux_qt_get_str_prop(double handle, double name_dbl) {
    auto* obj = FluxQtBridge::instance().resolveHandle(handle);
    if (!obj) return 0.0;
    auto val = obj->property(dbl_to_str(name_dbl));
    auto s = val.toString().toUtf8();
    return ptr_to_double(strdup(s.constData()));
}

// Convenience: set text (label text, button text, etc.)
static double flux_qt_set_text(double handle, double text_dbl) {
    return flux_qt_set_str_prop(handle, ptr_to_double("text"), text_dbl);
}

static double flux_qt_get_text(double handle) {
    return flux_qt_get_str_prop(handle, ptr_to_double("text"));
}

// Convenience: set window title
static double flux_qt_set_window_title(double handle, double title_dbl) {
    return flux_qt_set_str_prop(handle, ptr_to_double("windowTitle"), title_dbl);
}

// ===========================================================================
// Styling
// ===========================================================================
static double flux_qt_set_stylesheet(double handle, double css_dbl) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle));
    if (w) w->setStyleSheet(QString::fromUtf8(dbl_to_str(css_dbl)));
    return 0.0;
}

// ===========================================================================
// Widget control
// ===========================================================================
static double flux_qt_set_visible(double handle, double visible) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle));
    if (w) w->setVisible(visible != 0.0);
    return 0.0;
}

static double flux_qt_set_tooltip(double handle, double text_dbl) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(handle));
    if (w) w->setToolTip(QString::fromUtf8(dbl_to_str(text_dbl)));
    return 0.0;
}

// ===========================================================================
// App control
// ===========================================================================
static void flux_qt_app_quit() {
    QApplication::quit();
}

// ===========================================================================
// QCalendarWidget
// ===========================================================================
static double flux_qt_create_calendar(double parent_h) {
    auto* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(parent_h));
    auto* cal = new QCalendarWidget(parent);
    return FluxQtBridge::instance().registerObject(cal);
}

static double flux_qt_calendar_selected_date(double h) {
    auto* cal = qobject_cast<QCalendarWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (cal) {
        QString s = cal->selectedDate().toString("yyyy-MM-dd");
        return ptr_to_double(strdup(s.toUtf8().constData()));
    }
    return ptr_to_double(strdup(""));
}

// ===========================================================================
// QMdiArea
// ===========================================================================
static double flux_qt_create_mdiarea(double parent_h) {
    auto* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(parent_h));
    auto* mdi = new QMdiArea(parent);
    return FluxQtBridge::instance().registerObject(mdi);
}

static double flux_qt_mdi_add_subwindow(double mdi_h, double widget_h) {
    auto* mdi = qobject_cast<QMdiArea*>(FluxQtBridge::instance().resolveHandle(mdi_h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (mdi && w) {
        auto* sub = mdi->addSubWindow(w);
        sub->setAttribute(Qt::WA_DeleteOnClose);
        sub->show();
        return ptr_to_double(sub);
    }
    return 0.0;
}

static double flux_qt_mdi_cascade(double mdi_h) {
    auto* mdi = qobject_cast<QMdiArea*>(FluxQtBridge::instance().resolveHandle(mdi_h));
    if (mdi) mdi->cascadeSubWindows();
    return 0.0;
}

static double flux_qt_mdi_tile(double mdi_h) {
    auto* mdi = qobject_cast<QMdiArea*>(FluxQtBridge::instance().resolveHandle(mdi_h));
    if (mdi) mdi->tileSubWindows();
    return 0.0;
}

static double flux_qt_mdi_subwindow_set_title(double sw_h, double title_dbl) {
    auto* sub = reinterpret_cast<QMdiSubWindow*>(double_to_ptr(sw_h));
    if (sub) sub->setWindowTitle(QString::fromUtf8(dbl_to_str(title_dbl)));
    return 0.0;
}

static double flux_qt_mdi_close_all(double mdi_h) {
    auto* mdi = qobject_cast<QMdiArea*>(FluxQtBridge::instance().resolveHandle(mdi_h));
    if (mdi) mdi->closeAllSubWindows();
    return 0.0;
}

// ===========================================================================
// QProgressDialog
// ===========================================================================
static double flux_qt_create_progress_dialog(double label_dbl, double cancel, double min, double max) {
    QString label = QString::fromUtf8(dbl_to_str(label_dbl));
    auto* dlg = new QProgressDialog(
        label,
        cancel != 0.0 ? QString::fromUtf8("Cancel") : QString(),
        static_cast<int>(min),
        static_cast<int>(max)
    );
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(true);
    dlg->show();
    return FluxQtBridge::instance().registerObject(dlg);
}

static double flux_qt_progress_set_value(double h, double val) {
    auto* dlg = qobject_cast<QProgressDialog*>(FluxQtBridge::instance().resolveHandle(h));
    if (dlg) dlg->setValue(static_cast<int>(val));
    return 0.0;
}

static double flux_qt_progress_set_label(double h, double label_dbl) {
    auto* dlg = qobject_cast<QProgressDialog*>(FluxQtBridge::instance().resolveHandle(h));
    if (dlg) dlg->setLabelText(QString::fromUtf8(dbl_to_str(label_dbl)));
    return 0.0;
}

static double flux_qt_progress_was_canceled(double h) {
    auto* dlg = qobject_cast<QProgressDialog*>(FluxQtBridge::instance().resolveHandle(h));
    if (dlg) return dlg->wasCanceled() ? 1.0 : 0.0;
    return 0.0;
}

// ===========================================================================
// QShortcut
// ===========================================================================
static double flux_qt_create_shortcut(double parent_h, double key_dbl, double callback_dbl) {
    auto* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(parent_h));
    if (!parent) return 0.0;
    auto* sc = new QShortcut(QKeySequence(QString::fromUtf8(dbl_to_str(key_dbl))), parent);
    auto h = FluxQtBridge::instance().registerObject(sc);
    FluxQtBridge::instance().connectSignalByName(h, "activated()", dbl_to_str(callback_dbl));
    return h;
}

// ===========================================================================
// QSystemTrayIcon
// ===========================================================================
static double flux_qt_create_tray_icon(double icon_dbl, double tooltip_dbl) {
    auto* tray = new QSystemTrayIcon();
    tray->setIcon(QIcon(QString::fromUtf8(dbl_to_str(icon_dbl))));
    tray->setToolTip(QString::fromUtf8(dbl_to_str(tooltip_dbl)));
    auto h = FluxQtBridge::instance().registerObject(tray);
    return h;
}

static double flux_qt_tray_set_visible(double h, double visible) {
    auto* tray = qobject_cast<QSystemTrayIcon*>(FluxQtBridge::instance().resolveHandle(h));
    if (tray) tray->setVisible(visible != 0.0);
    return 0.0;
}

static double flux_qt_tray_show_message(double h, double title_dbl, double msg_dbl, double timeout) {
    auto* tray = qobject_cast<QSystemTrayIcon*>(FluxQtBridge::instance().resolveHandle(h));
    if (tray) tray->showMessage(
        QString::fromUtf8(dbl_to_str(title_dbl)),
        QString::fromUtf8(dbl_to_str(msg_dbl)),
        QSystemTrayIcon::Information,
        static_cast<int>(timeout));
    return 0.0;
}

static double flux_qt_tray_set_context_menu(double h, double menu_h) {
    auto* tray = qobject_cast<QSystemTrayIcon*>(FluxQtBridge::instance().resolveHandle(h));
    auto* menu = qobject_cast<QMenu*>(FluxQtBridge::instance().resolveHandle(menu_h));
    if (tray && menu) tray->setContextMenu(menu);
    return 0.0;
}

static double flux_qt_tray_on_activated(double h, double callback_dbl) {
    auto* tray = qobject_cast<QSystemTrayIcon*>(FluxQtBridge::instance().resolveHandle(h));
    if (!tray) return 0.0;
    std::string cb = dbl_to_str(callback_dbl);
    QObject::connect(tray, &QSystemTrayIcon::activated, [cb](QSystemTrayIcon::ActivationReason) {
        extern void callFluxFunction(const char* name);
        callFluxFunction(cb.c_str());
    });
    return 0.0;
}

static double flux_qt_app_set_quit_on_last_closed(double enabled) {
    QApplication::setQuitOnLastWindowClosed(enabled != 0.0);
    return 0.0;
}

static double flux_qt_set_app_icon(double path_dbl) {
    QIcon icon(QString::fromUtf8(dbl_to_str(path_dbl)));
    if (!icon.isNull())
        QApplication::setWindowIcon(icon);
    return 0.0;
}

// ===========================================================================
// Drag-and-drop
// ===========================================================================
static double flux_qt_enable_drops(double widget_h, double callback_dbl) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (!w) return 0.0;
    w->setAcceptDrops(true);
    new DropFilter(w, dbl_to_str(callback_dbl));
    return 0.0;
}

static double flux_qt_last_dropped_file() {
    return ptr_to_double(strdup(g_lastDroppedFile.c_str()));
}

// ===========================================================================
// FluxScript eval
// ===========================================================================
static double flux_qt_eval(double source_dbl) {
    auto& jit = Flux::JITEngine::instance();
    std::string err;
    if (!jit.executeString(dbl_to_str(source_dbl), &err))
        return ptr_to_double(strdup(("Error: " + err).c_str()));
    return ptr_to_double(strdup("OK"));
}

// ===========================================================================
// Signal binding (by FluxScript function name)
// ===========================================================================
static void flux_qt_on_click_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "clicked()", dbl_to_str(name_dbl));
}
static void flux_qt_on_value_changed_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "valueChanged(int)", dbl_to_str(name_dbl));
}
static void flux_qt_on_current_index_changed_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "currentIndexChanged(int)", dbl_to_str(name_dbl));
}
static void flux_qt_on_toggled_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "toggled(bool)", dbl_to_str(name_dbl));
}
static void flux_qt_on_text_changed_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "textChanged(QString)", dbl_to_str(name_dbl));
}
static void flux_qt_on_current_row_changed_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "currentRowChanged(int)", dbl_to_str(name_dbl));
}
static void flux_qt_on_selection_changed_by_name(double h, double name_dbl) {
    FluxQtBridge::instance().connectSignalByName(h, "selectionChanged()", dbl_to_str(name_dbl));
}

} // extern "C"

// ===========================================================================
// Forward declare
// ===========================================================================
extern void callFluxFunction(const char* name);

void registerFluxQtSymbols(Flux::JITEngine& jit) {
    struct { const char* name; void* ptr; } const fns[] = {
        {"flux_qt_create_button",          (void*)&flux_qt_create_button},
        {"flux_qt_create_label",           (void*)&flux_qt_create_label},
        {"flux_qt_create_slider",          (void*)&flux_qt_create_slider},
        {"flux_qt_create_dial",            (void*)&flux_qt_create_dial},
        {"flux_qt_create_lcd",             (void*)&flux_qt_create_lcd},
        {"flux_qt_create_spinbox",         (void*)&flux_qt_create_spinbox},
        {"flux_qt_create_progressbar",     (void*)&flux_qt_create_progressbar},
        {"flux_qt_create_checkbox",        (void*)&flux_qt_create_checkbox},
        {"flux_qt_create_lineedit",        (void*)&flux_qt_create_lineedit},
        {"flux_qt_create_textedit",        (void*)&flux_qt_create_textedit},
        {"flux_qt_create_combobox",        (void*)&flux_qt_create_combobox},
        {"flux_qt_create_tableview",       (void*)&flux_qt_create_tableview},
        {"flux_qt_create_tabwidget",       (void*)&flux_qt_create_tabwidget},
        {"flux_qt_create_groupbox",        (void*)&flux_qt_create_groupbox},
        {"flux_qt_create_radiobutton",     (void*)&flux_qt_create_radiobutton},
        {"flux_qt_create_toolbutton",      (void*)&flux_qt_create_toolbutton},
        {"flux_qt_toolbutton_set_arrow_type",        (void*)&flux_qt_toolbutton_set_arrow_type},
        {"flux_qt_toolbutton_set_popup_mode",        (void*)&flux_qt_toolbutton_set_popup_mode},
        {"flux_qt_toolbutton_set_toolbutton_style",  (void*)&flux_qt_toolbutton_set_toolbutton_style},
        {"flux_qt_toolbutton_set_auto_raise",        (void*)&flux_qt_toolbutton_set_auto_raise},
        {"flux_qt_toolbutton_set_menu",              (void*)&flux_qt_toolbutton_set_menu},
        {"flux_qt_toolbutton_show_menu",             (void*)&flux_qt_toolbutton_show_menu},
        {"flux_qt_create_commandlinkbutton",         (void*)&flux_qt_create_commandlinkbutton},
        {"flux_qt_commandlinkbutton_set_description",(void*)&flux_qt_commandlinkbutton_set_description},
        {"flux_qt_set_abstractbutton_icon",          (void*)&flux_qt_set_abstractbutton_icon},
        {"flux_qt_create_splitter",        (void*)&flux_qt_create_splitter},
        {"flux_qt_create_listwidget",      (void*)&flux_qt_create_listwidget},
        {"flux_qt_create_stackedwidget",   (void*)&flux_qt_create_stackedwidget},
        {"flux_qt_create_widget",          (void*)&flux_qt_create_widget},
        {"flux_qt_create_window",          (void*)&flux_qt_create_window},
        {"flux_qt_create_mainwindow",      (void*)&flux_qt_create_mainwindow},
        {"flux_qt_get_window",             (void*)&flux_qt_get_window},
        {"flux_qt_create_scrollarea",      (void*)&flux_qt_create_scrollarea},
        {"flux_qt_create_layout",          (void*)&flux_qt_create_layout},
        {"flux_qt_set_layout",             (void*)&flux_qt_set_layout},
        {"flux_qt_add_widget",             (void*)&flux_qt_add_widget},
        {"flux_qt_layout_add_widget",      (void*)&flux_qt_layout_add_widget},
        {"flux_qt_grid_add_widget",        (void*)&flux_qt_grid_add_widget},
        {"flux_qt_set_window_size",        (void*)&flux_qt_set_window_size},
        {"flux_qt_layout_set_margins",     (void*)&flux_qt_layout_set_margins},
        {"flux_qt_layout_set_spacing",     (void*)&flux_qt_layout_set_spacing},
        {"flux_qt_layout_set_stretch",     (void*)&flux_qt_layout_set_stretch},
        {"flux_qt_msg_box",                (void*)&flux_qt_msg_box},
        {"flux_qt_lcd_display",            (void*)&flux_qt_lcd_display},
        {"flux_qt_combo_add_item",         (void*)&flux_qt_combo_add_item},
        {"flux_qt_combo_clear",            (void*)&flux_qt_combo_clear},
        {"flux_qt_combo_set_current_index",(void*)&flux_qt_combo_set_current_index},
        {"flux_qt_table_row_count",        (void*)&flux_qt_table_row_count},
        {"flux_qt_table_col_count",        (void*)&flux_qt_table_col_count},
        {"flux_qt_table_set_value",        (void*)&flux_qt_table_set_value},
        {"flux_qt_table_set_item",         (void*)&flux_qt_table_set_item},
        {"flux_qt_table_set_header",       (void*)&flux_qt_table_set_header},
        {"flux_qt_create_timer",           (void*)&flux_qt_create_timer},
        {"flux_qt_timer_start",            (void*)&flux_qt_timer_start},
        {"flux_qt_timer_stop",             (void*)&flux_qt_timer_stop},
        {"flux_qt_tab_add",                (void*)&flux_qt_tab_add},
        {"flux_qt_tab_set_current",        (void*)&flux_qt_tab_set_current},
        {"flux_qt_groupbox_set_layout",    (void*)&flux_qt_groupbox_set_layout},
        {"flux_qt_splitter_add",           (void*)&flux_qt_splitter_add},
        {"flux_qt_list_add_item",          (void*)&flux_qt_list_add_item},
        {"flux_qt_list_current_row",       (void*)&flux_qt_list_current_row},
        {"flux_qt_list_set_current_row",   (void*)&flux_qt_list_set_current_row},
        {"flux_qt_list_clear",             (void*)&flux_qt_list_clear},
        {"flux_qt_list_count",             (void*)&flux_qt_list_count},
        {"flux_qt_stacked_add",            (void*)&flux_qt_stacked_add},
        {"flux_qt_stacked_set_current",    (void*)&flux_qt_stacked_set_current},
        {"flux_qt_stacked_count",          (void*)&flux_qt_stacked_count},
        {"flux_qt_get_menubar",            (void*)&flux_qt_get_menubar},
        {"flux_qt_create_menu",            (void*)&flux_qt_create_menu},
        {"flux_qt_set_central_widget",     (void*)&flux_qt_set_central_widget},
        {"flux_qt_set_statusbar_text",     (void*)&flux_qt_set_statusbar_text},
        {"flux_qt_menu_add_menu",          (void*)&flux_qt_menu_add_menu},
        {"flux_qt_menu_add_action",        (void*)&flux_qt_menu_add_action},
        {"flux_qt_open_file_dialog",       (void*)&flux_qt_open_file_dialog},
        {"flux_qt_save_file_dialog",       (void*)&flux_qt_save_file_dialog},
        {"flux_qt_input_dialog",           (void*)&flux_qt_input_dialog},
        {"flux_qt_color_dialog",           (void*)&flux_qt_color_dialog},
        {"flux_qt_color_r",                (void*)&flux_qt_color_r},
        {"flux_qt_color_g",                (void*)&flux_qt_color_g},
        {"flux_qt_color_b",                (void*)&flux_qt_color_b},
        {"flux_qt_font_dialog",            (void*)&flux_qt_font_dialog},
        {"flux_qt_font_size",              (void*)&flux_qt_font_size},
        {"flux_qt_font_family",            (void*)&flux_qt_font_family},
        {"flux_qt_scroll_set_widget",      (void*)&flux_qt_scroll_set_widget},
        {"flux_qt_create_svgwidget",       (void*)&flux_qt_create_svgwidget},
        {"flux_qt_svgwidget_load",         (void*)&flux_qt_svgwidget_load},
        {"flux_qt_svgwidget_set_size",     (void*)&flux_qt_svgwidget_set_size},
        {"flux_qt_label_load_image",       (void*)&flux_qt_label_load_image},
        {"flux_qt_label_set_pixmap",       (void*)&flux_qt_label_set_pixmap},
        {"flux_qt_set_window_icon",        (void*)&flux_qt_set_window_icon},
        {"flux_qt_set_button_icon",        (void*)&flux_qt_set_button_icon},
        {"flux_qt_set_action_icon",        (void*)&flux_qt_set_action_icon},
        {"flux_qt_create_toolbar",         (void*)&flux_qt_create_toolbar},
        {"flux_qt_toolbar_add_action",     (void*)&flux_qt_toolbar_add_action},
        {"flux_qt_toolbar_add_separator",  (void*)&flux_qt_toolbar_add_separator},
        {"flux_qt_create_chartview",       (void*)&flux_qt_create_chartview},
        {"flux_qt_chart_set_title",        (void*)&flux_qt_chart_set_title},
        {"flux_qt_chart_set_animation",    (void*)&flux_qt_chart_set_animation},
        {"flux_qt_chart_set_theme",        (void*)&flux_qt_chart_set_theme},
        {"flux_qt_chart_set_background",   (void*)&flux_qt_chart_set_background},
        {"flux_qt_chart_set_drop_shadow",  (void*)&flux_qt_chart_set_drop_shadow},
        {"flux_qt_chart_set_margins",      (void*)&flux_qt_chart_set_margins},
        {"flux_qt_chart_legend_show",      (void*)&flux_qt_chart_legend_show},
        {"flux_qt_chart_legend_align",     (void*)&flux_qt_chart_legend_align},
        {"flux_qt_chart_add_line_series",  (void*)&flux_qt_chart_add_line_series},
        {"flux_qt_chart_add_spline_series",(void*)&flux_qt_chart_add_spline_series},
        {"flux_qt_chart_add_scatter_series",(void*)&flux_qt_chart_add_scatter_series},
        {"flux_qt_chart_add_area_series",  (void*)&flux_qt_chart_add_area_series},
        {"flux_qt_xy_series_append",       (void*)&flux_qt_xy_series_append},
        {"flux_qt_series_append_data",     (void*)&flux_qt_series_append_data},
        {"flux_qt_xy_series_set_pen",      (void*)&flux_qt_xy_series_set_pen},
        {"flux_qt_xy_series_count",        (void*)&flux_qt_xy_series_count},
        {"flux_qt_xy_series_clear",        (void*)&flux_qt_xy_series_clear},
        {"flux_qt_xy_series_scroll",       (void*)&flux_qt_xy_series_scroll},
        {"flux_qt_progress_set_range",     (void*)&flux_qt_progress_set_range},
        {"flux_qt_line_series_append",     (void*)&flux_qt_xy_series_append},
        {"flux_qt_line_series_set_pen",    (void*)&flux_qt_xy_series_set_pen},
        {"flux_qt_scatter_series_set_marker_size",(void*)&flux_qt_scatter_series_set_marker_size},
        {"flux_qt_chart_add_bar_series",   (void*)&flux_qt_chart_add_bar_series},
        {"flux_qt_chart_add_stacked_bar_series",(void*)&flux_qt_chart_add_stacked_bar_series},
        {"flux_qt_create_bar_set",         (void*)&flux_qt_create_bar_set},
        {"flux_qt_bar_set_append",         (void*)&flux_qt_bar_set_append},
        {"flux_qt_bar_set_remove",         (void*)&flux_qt_bar_set_remove},
        {"flux_qt_bar_set_set_color",      (void*)&flux_qt_bar_set_set_color},
        {"flux_qt_bar_series_append_set",  (void*)&flux_qt_bar_series_append_set},
        {"flux_qt_chart_add_hbar_series",  (void*)&flux_qt_chart_add_hbar_series},
        {"flux_qt_chart_add_pie_series",   (void*)&flux_qt_chart_add_pie_series},
        {"flux_qt_pie_series_append",      (void*)&flux_qt_pie_series_append},
        {"flux_qt_pie_slice_set_color",    (void*)&flux_qt_pie_slice_set_color},
        {"flux_qt_pie_slice_set_label_visible",(void*)&flux_qt_pie_slice_set_label_visible},
        {"flux_qt_pie_slice_set_exploded", (void*)&flux_qt_pie_slice_set_exploded},
        {"flux_qt_pie_series_set_hole_size",(void*)&flux_qt_pie_series_set_hole_size},
        {"flux_qt_pie_series_set_labels_position",(void*)&flux_qt_pie_series_set_labels_position},
        {"flux_qt_chart_create_default_axes",(void*)&flux_qt_chart_create_default_axes},
        {"flux_qt_chart_add_axis",         (void*)&flux_qt_chart_add_axis},
        {"flux_qt_axis_set_range",         (void*)&flux_qt_axis_set_range},
        {"flux_qt_axis_set_min",           (void*)&flux_qt_axis_set_min},
        {"flux_qt_axis_set_max",           (void*)&flux_qt_axis_set_max},
        {"flux_qt_axis_set_title",         (void*)&flux_qt_axis_set_title},
        {"flux_qt_axis_set_label_format",  (void*)&flux_qt_axis_set_label_format},
        {"flux_qt_axis_set_grid_visible",  (void*)&flux_qt_axis_set_grid_visible},
        {"flux_qt_axis_set_labels_visible",(void*)&flux_qt_axis_set_labels_visible},
        {"flux_qt_axis_set_line_visible",  (void*)&flux_qt_axis_set_line_visible},
        {"flux_qt_axis_set_tick_count",    (void*)&flux_qt_axis_set_tick_count},
        {"flux_qt_series_attach_axis",     (void*)&flux_qt_series_attach_axis},
        {"flux_qt_chart_add_series",       (void*)&flux_qt_chart_add_series},
        {"flux_qt_chart_save_png",         (void*)&flux_qt_chart_save_png},
        {"flux_qt_create_treewidget",      (void*)&flux_qt_create_treewidget},
        {"flux_qt_tree_set_header",        (void*)&flux_qt_tree_set_header},
        {"flux_qt_tree_set_column_count",  (void*)&flux_qt_tree_set_column_count},
        {"flux_qt_tree_set_column_width",  (void*)&flux_qt_tree_set_column_width},
        {"flux_qt_tree_resize_to_contents",(void*)&flux_qt_tree_resize_to_contents},
        {"flux_qt_tree_top_level_count",   (void*)&flux_qt_tree_top_level_count},
        {"flux_qt_tree_top_level_item",    (void*)&flux_qt_tree_top_level_item},
        {"flux_qt_tree_item_child_count",  (void*)&flux_qt_tree_item_child_count},
        {"flux_qt_tree_item_child",        (void*)&flux_qt_tree_item_child},
        {"flux_qt_tree_item_parent",       (void*)&flux_qt_tree_item_parent},
        {"flux_qt_tree_item_row",          (void*)&flux_qt_tree_item_row},
        {"flux_qt_tree_invisible_root",    (void*)&flux_qt_tree_invisible_root},
        {"flux_qt_tree_add_item",          (void*)&flux_qt_tree_add_item},
        {"flux_qt_tree_add_item_col",      (void*)&flux_qt_tree_add_item_col},
        {"flux_qt_tree_add_child",         (void*)&flux_qt_tree_add_child},
        {"flux_qt_tree_item_set_text",     (void*)&flux_qt_tree_item_set_text},
        {"flux_qt_tree_item_text",         (void*)&flux_qt_tree_item_text},
        {"flux_qt_tree_item_set_icon",     (void*)&flux_qt_tree_item_set_icon},
        {"flux_qt_tree_item_set_checked",  (void*)&flux_qt_tree_item_set_checked},
        {"flux_qt_tree_item_checked",      (void*)&flux_qt_tree_item_checked},
        {"flux_qt_tree_item_set_expanded", (void*)&flux_qt_tree_item_set_expanded},
        {"flux_qt_tree_item_set_tooltip",  (void*)&flux_qt_tree_item_set_tooltip},
        {"flux_qt_tree_item_set_foreground",(void*)&flux_qt_tree_item_set_foreground},
        {"flux_qt_tree_item_set_background",(void*)&flux_qt_tree_item_set_background},
        {"flux_qt_tree_current_item",      (void*)&flux_qt_tree_current_item},
        {"flux_qt_tree_set_current_item",  (void*)&flux_qt_tree_set_current_item},
        {"flux_qt_tree_set_selection_mode",(void*)&flux_qt_tree_set_selection_mode},
        {"flux_qt_tree_set_header_visible",(void*)&flux_qt_tree_set_header_visible},
        {"flux_qt_tree_set_alternating",   (void*)&flux_qt_tree_set_alternating},
        {"flux_qt_tree_set_animated",      (void*)&flux_qt_tree_set_animated},
        {"flux_qt_tree_set_indentation",   (void*)&flux_qt_tree_set_indentation},
        {"flux_qt_tree_clear",             (void*)&flux_qt_tree_clear},
        {"flux_qt_tree_sort",              (void*)&flux_qt_tree_sort},
        {"flux_qt_tree_remove_item",       (void*)&flux_qt_tree_remove_item},
        {"flux_qt_tree_on_item_clicked",   (void*)&flux_qt_tree_on_item_clicked},
        {"flux_qt_tree_on_current_item_changed",(void*)&flux_qt_tree_on_current_item_changed},
        {"flux_qt_tree_on_item_expanded",  (void*)&flux_qt_tree_on_item_expanded},
        {"flux_qt_tree_on_item_collapsed", (void*)&flux_qt_tree_on_item_collapsed},
        {"flux_qt_tree_on_item_activated", (void*)&flux_qt_tree_on_item_activated},
        {"flux_qt_create_dockwidget",      (void*)&flux_qt_create_dockwidget},
        {"flux_qt_dock_set_widget",        (void*)&flux_qt_dock_set_widget},
        {"flux_qt_dock_set_features",      (void*)&flux_qt_dock_set_features},
        {"flux_qt_dock_set_title",         (void*)&flux_qt_dock_set_title},
        {"flux_qt_dock_set_visible",       (void*)&flux_qt_dock_set_visible},
        {"flux_qt_dock_is_visible",        (void*)&flux_qt_dock_is_visible},
        {"flux_qt_dock_is_floating",       (void*)&flux_qt_dock_is_floating},
        {"flux_qt_mainwindow_add_dock",    (void*)&flux_qt_mainwindow_add_dock},
        {"flux_qt_mainwindow_tabify_docks",(void*)&flux_qt_mainwindow_tabify_docks},
        {"flux_qt_mainwindow_split_dock",  (void*)&flux_qt_mainwindow_split_dock},
        {"flux_qt_dock_on_visibility_changed",(void*)&flux_qt_dock_on_visibility_changed},
        {"flux_qt_dock_on_dock_location_changed",(void*)&flux_qt_dock_on_dock_location_changed},
        {"flux_qt_dock_on_top_level_changed",(void*)&flux_qt_dock_on_top_level_changed},
        {"flux_qt_get_property",           (void*)&flux_qt_get_property},
        {"flux_qt_set_property",           (void*)&flux_qt_set_property},
        {"flux_qt_get_str_prop",           (void*)&flux_qt_get_str_prop},
        {"flux_qt_set_str_prop",           (void*)&flux_qt_set_str_prop},
        {"flux_qt_get_text",               (void*)&flux_qt_get_text},
        {"flux_qt_set_text",               (void*)&flux_qt_set_text},
        {"flux_qt_set_window_title",       (void*)&flux_qt_set_window_title},
        {"flux_qt_set_stylesheet",         (void*)&flux_qt_set_stylesheet},
        {"flux_qt_set_visible",            (void*)&flux_qt_set_visible},
        {"flux_qt_set_tooltip",            (void*)&flux_qt_set_tooltip},
        {"flux_qt_app_quit",               (void*)&flux_qt_app_quit},
        {"flux_qt_create_calendar",        (void*)&flux_qt_create_calendar},
        {"flux_qt_calendar_selected_date", (void*)&flux_qt_calendar_selected_date},
        {"flux_qt_create_mdiarea",         (void*)&flux_qt_create_mdiarea},
        {"flux_qt_mdi_add_subwindow",      (void*)&flux_qt_mdi_add_subwindow},
        {"flux_qt_mdi_cascade",            (void*)&flux_qt_mdi_cascade},
        {"flux_qt_mdi_tile",               (void*)&flux_qt_mdi_tile},
        {"flux_qt_mdi_subwindow_set_title",(void*)&flux_qt_mdi_subwindow_set_title},
        {"flux_qt_mdi_close_all",          (void*)&flux_qt_mdi_close_all},
        {"flux_qt_create_progress_dialog", (void*)&flux_qt_create_progress_dialog},
        {"flux_qt_progress_set_value",     (void*)&flux_qt_progress_set_value},
        {"flux_qt_progress_set_label",     (void*)&flux_qt_progress_set_label},
        {"flux_qt_progress_was_canceled",  (void*)&flux_qt_progress_was_canceled},
        {"flux_qt_on_click_by_name",       (void*)&flux_qt_on_click_by_name},
        {"flux_qt_on_value_changed_by_name",(void*)&flux_qt_on_value_changed_by_name},
        {"flux_qt_on_current_index_changed_by_name",(void*)&flux_qt_on_current_index_changed_by_name},
        {"flux_qt_on_toggled_by_name",     (void*)&flux_qt_on_toggled_by_name},
        {"flux_qt_on_text_changed_by_name",(void*)&flux_qt_on_text_changed_by_name},
        {"flux_qt_on_current_row_changed_by_name",(void*)&flux_qt_on_current_row_changed_by_name},
        {"flux_qt_on_selection_changed_by_name",(void*)&flux_qt_on_selection_changed_by_name},
        {"flux_qt_create_shortcut",        (void*)&flux_qt_create_shortcut},
        {"flux_qt_create_tray_icon",       (void*)&flux_qt_create_tray_icon},
        {"flux_qt_tray_set_visible",       (void*)&flux_qt_tray_set_visible},
        {"flux_qt_tray_show_message",      (void*)&flux_qt_tray_show_message},
        {"flux_qt_tray_set_context_menu",  (void*)&flux_qt_tray_set_context_menu},
        {"flux_qt_tray_on_activated",      (void*)&flux_qt_tray_on_activated},
        {"flux_qt_app_set_quit_on_last_closed",(void*)&flux_qt_app_set_quit_on_last_closed},
        {"flux_qt_set_app_icon",           (void*)&flux_qt_set_app_icon},
        {"flux_qt_put_pixel",              (void*)&flux_qt_put_pixel},
        {"flux_qt_render_image",           (void*)&flux_qt_render_image},
        {"flux_qt_ray_pixel_count",        (void*)&flux_qt_ray_pixel_count},
        {"flux_qt_enable_drops",           (void*)&flux_qt_enable_drops},
        {"flux_qt_last_dropped_file",      (void*)&flux_qt_last_dropped_file},
        {"flux_qt_eval",                   (void*)&flux_qt_eval},
    };
    for (auto& f : fns)
        jit.registerFunction(f.name, f.ptr);
}
