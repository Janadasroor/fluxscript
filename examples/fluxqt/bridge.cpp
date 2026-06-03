#include "bridge.h"
#include <flux/jit_engine.h>
#include <QApplication>
#include <QBoxLayout>
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
#include <QMetaProperty>
#include <QProgressBar>
#include <QPushButton>
#include <QAbstractButton>
#include <QRadioButton>
#include <QToolButton>
#include <QCommandLinkButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QScrollArea>
#include <QStatusBar>
#include <QSvgWidget>
#include <QToolBar>
#include <cstring>
#include <iostream>

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

// ===========================================================================
// Property helpers
// ===========================================================================
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

static double flux_qt_create_scrollarea() {
    auto* sa = new QScrollArea;
    return FluxQtBridge::instance().registerObject(sa);
}

// ===========================================================================
// Layout
// ===========================================================================
static double flux_qt_create_layout(double type_dbl) {
    const char* type = dbl_to_str(type_dbl);
    if (std::strcmp(type, "vbox") == 0)
        return ptr_to_double(new QVBoxLayout);
    if (std::strcmp(type, "grid") == 0)
        return ptr_to_double(new QGridLayout);
    return ptr_to_double(new QHBoxLayout);
}

static void flux_qt_set_layout(double container_h, double layout_h) {
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(container_h));
    auto* lay = static_cast<QLayout*>(double_to_ptr(layout_h));
    if (w && lay) w->setLayout(lay);
}

static void flux_qt_add_widget(double container, double widget) {
    auto* parent = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(container));
    auto* child = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget));
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
    auto* lay = static_cast<QLayout*>(double_to_ptr(layout_h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (lay && w) lay->addWidget(w);
}

static void flux_qt_grid_add_widget(
    double layout_h, double widget_h,
    double row, double col, double rowSpan, double colSpan) {
    auto* grid = static_cast<QGridLayout*>(double_to_ptr(layout_h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
    if (grid && w)
        grid->addWidget(w, static_cast<int>(row), static_cast<int>(col),
                        static_cast<int>(rowSpan), static_cast<int>(colSpan));
}

static void flux_qt_set_window_size(double h, double w, double hh) {
    auto* widget = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(h));
    if (widget) widget->resize(static_cast<int>(w), static_cast<int>(hh));
}

static void flux_qt_layout_set_margins(double layout_h, double l, double t, double r, double b) {
    auto* lay = static_cast<QLayout*>(double_to_ptr(layout_h));
    if (lay) lay->setContentsMargins(static_cast<int>(l), static_cast<int>(t), static_cast<int>(r), static_cast<int>(b));
}

static void flux_qt_layout_set_spacing(double layout_h, double spacing) {
    auto* lay = static_cast<QLayout*>(double_to_ptr(layout_h));
    if (lay) lay->setSpacing(static_cast<int>(spacing));
}

static void flux_qt_layout_set_stretch(double layout_h, double widget_h, double stretch) {
    auto* box = static_cast<QBoxLayout*>(double_to_ptr(layout_h));
    auto* w = qobject_cast<QWidget*>(FluxQtBridge::instance().resolveHandle(widget_h));
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

} // extern "C"

// ===========================================================================
// Forward declare
// ===========================================================================
extern void callFluxFunction(const char* name);

void registerFluxQtSymbols(Flux::JITEngine& jit) {
    // Widget creation
    jit.registerFunction("flux_qt_create_window",     (void*)&flux_qt_create_window);
    jit.registerFunction("flux_qt_create_button",     (void*)&flux_qt_create_button);
    jit.registerFunction("flux_qt_create_label",      (void*)&flux_qt_create_label);
    jit.registerFunction("flux_qt_create_slider",     (void*)&flux_qt_create_slider);
    jit.registerFunction("flux_qt_create_dial",       (void*)&flux_qt_create_dial);
    jit.registerFunction("flux_qt_create_lcd",        (void*)&flux_qt_create_lcd);
    jit.registerFunction("flux_qt_create_combobox",   (void*)&flux_qt_create_combobox);
    jit.registerFunction("flux_qt_create_spinbox",    (void*)&flux_qt_create_spinbox);
    jit.registerFunction("flux_qt_create_progressbar",(void*)&flux_qt_create_progressbar);
    jit.registerFunction("flux_qt_create_checkbox",   (void*)&flux_qt_create_checkbox);
    jit.registerFunction("flux_qt_create_lineedit",   (void*)&flux_qt_create_lineedit);
    jit.registerFunction("flux_qt_create_textedit",   (void*)&flux_qt_create_textedit);
    jit.registerFunction("flux_qt_create_tableview",  (void*)&flux_qt_create_tableview);
    jit.registerFunction("flux_qt_create_tabwidget",  (void*)&flux_qt_create_tabwidget);
    jit.registerFunction("flux_qt_create_groupbox",   (void*)&flux_qt_create_groupbox);
    jit.registerFunction("flux_qt_create_radiobutton",(void*)&flux_qt_create_radiobutton);
    jit.registerFunction("flux_qt_create_toolbutton", (void*)&flux_qt_create_toolbutton);
    jit.registerFunction("flux_qt_toolbutton_set_arrow_type", (void*)&flux_qt_toolbutton_set_arrow_type);
    jit.registerFunction("flux_qt_toolbutton_set_popup_mode", (void*)&flux_qt_toolbutton_set_popup_mode);
    jit.registerFunction("flux_qt_toolbutton_set_toolbutton_style", (void*)&flux_qt_toolbutton_set_toolbutton_style);
    jit.registerFunction("flux_qt_toolbutton_set_auto_raise", (void*)&flux_qt_toolbutton_set_auto_raise);
    jit.registerFunction("flux_qt_toolbutton_set_menu", (void*)&flux_qt_toolbutton_set_menu);
    jit.registerFunction("flux_qt_toolbutton_show_menu", (void*)&flux_qt_toolbutton_show_menu);
    jit.registerFunction("flux_qt_create_commandlinkbutton", (void*)&flux_qt_create_commandlinkbutton);
    jit.registerFunction("flux_qt_commandlinkbutton_set_description", (void*)&flux_qt_commandlinkbutton_set_description);
    jit.registerFunction("flux_qt_set_abstractbutton_icon", (void*)&flux_qt_set_abstractbutton_icon);
    jit.registerFunction("flux_qt_create_splitter",   (void*)&flux_qt_create_splitter);
    jit.registerFunction("flux_qt_create_listwidget",  (void*)&flux_qt_create_listwidget);
    jit.registerFunction("flux_qt_create_stackedwidget",(void*)&flux_qt_create_stackedwidget);
    jit.registerFunction("flux_qt_create_widget",      (void*)&flux_qt_create_widget);
    jit.registerFunction("flux_qt_create_mainwindow",  (void*)&flux_qt_create_mainwindow);
    jit.registerFunction("flux_qt_create_scrollarea",  (void*)&flux_qt_create_scrollarea);

    // Layout
    jit.registerFunction("flux_qt_create_layout",    (void*)&flux_qt_create_layout);
    jit.registerFunction("flux_qt_set_layout",       (void*)&flux_qt_set_layout);
    jit.registerFunction("flux_qt_add_widget",       (void*)&flux_qt_add_widget);
    jit.registerFunction("flux_qt_layout_add_widget",(void*)&flux_qt_layout_add_widget);
    jit.registerFunction("flux_qt_grid_add_widget",  (void*)&flux_qt_grid_add_widget);
    jit.registerFunction("flux_qt_set_window_size",  (void*)&flux_qt_set_window_size);
    jit.registerFunction("flux_qt_layout_set_margins",(void*)&flux_qt_layout_set_margins);
    jit.registerFunction("flux_qt_layout_set_spacing",(void*)&flux_qt_layout_set_spacing);
    jit.registerFunction("flux_qt_layout_set_stretch",(void*)&flux_qt_layout_set_stretch);

    // Misc widgets
    jit.registerFunction("flux_qt_msg_box",           (void*)&flux_qt_msg_box);
    jit.registerFunction("flux_qt_lcd_display",       (void*)&flux_qt_lcd_display);
    jit.registerFunction("flux_qt_combo_add_item",    (void*)&flux_qt_combo_add_item);
    jit.registerFunction("flux_qt_combo_clear",       (void*)&flux_qt_combo_clear);
    jit.registerFunction("flux_qt_combo_set_current_index", (void*)&flux_qt_combo_set_current_index);
    jit.registerFunction("flux_qt_table_row_count",   (void*)&flux_qt_table_row_count);
    jit.registerFunction("flux_qt_table_col_count",   (void*)&flux_qt_table_col_count);
    jit.registerFunction("flux_qt_table_set_value",   (void*)&flux_qt_table_set_value);
    jit.registerFunction("flux_qt_table_set_item",    (void*)&flux_qt_table_set_item);
    jit.registerFunction("flux_qt_table_set_header",  (void*)&flux_qt_table_set_header);
    jit.registerFunction("flux_qt_create_timer",      (void*)&flux_qt_create_timer);
    jit.registerFunction("flux_qt_timer_start",       (void*)&flux_qt_timer_start);
    jit.registerFunction("flux_qt_timer_stop",        (void*)&flux_qt_timer_stop);
    jit.registerFunction("flux_qt_tab_add",           (void*)&flux_qt_tab_add);
    jit.registerFunction("flux_qt_tab_set_current",   (void*)&flux_qt_tab_set_current);
    jit.registerFunction("flux_qt_groupbox_set_layout",(void*)&flux_qt_groupbox_set_layout);
    jit.registerFunction("flux_qt_splitter_add",       (void*)&flux_qt_splitter_add);
    jit.registerFunction("flux_qt_list_add_item",      (void*)&flux_qt_list_add_item);
    jit.registerFunction("flux_qt_list_current_row",   (void*)&flux_qt_list_current_row);
    jit.registerFunction("flux_qt_list_set_current_row",(void*)&flux_qt_list_set_current_row);
    jit.registerFunction("flux_qt_list_clear",         (void*)&flux_qt_list_clear);
    jit.registerFunction("flux_qt_list_count",         (void*)&flux_qt_list_count);
    jit.registerFunction("flux_qt_stacked_add",        (void*)&flux_qt_stacked_add);
    jit.registerFunction("flux_qt_stacked_set_current",(void*)&flux_qt_stacked_set_current);
    jit.registerFunction("flux_qt_stacked_count",      (void*)&flux_qt_stacked_count);
    jit.registerFunction("flux_qt_get_menubar",        (void*)&flux_qt_get_menubar);
    jit.registerFunction("flux_qt_create_menu",         (void*)&flux_qt_create_menu);
    jit.registerFunction("flux_qt_set_central_widget", (void*)&flux_qt_set_central_widget);
    jit.registerFunction("flux_qt_set_statusbar_text", (void*)&flux_qt_set_statusbar_text);
    jit.registerFunction("flux_qt_menu_add_menu",      (void*)&flux_qt_menu_add_menu);
    jit.registerFunction("flux_qt_menu_add_action",    (void*)&flux_qt_menu_add_action);
    jit.registerFunction("flux_qt_open_file_dialog",   (void*)&flux_qt_open_file_dialog);
    jit.registerFunction("flux_qt_save_file_dialog",   (void*)&flux_qt_save_file_dialog);
    jit.registerFunction("flux_qt_input_dialog",       (void*)&flux_qt_input_dialog);
    jit.registerFunction("flux_qt_scroll_set_widget",  (void*)&flux_qt_scroll_set_widget);
    jit.registerFunction("flux_qt_create_svgwidget",   (void*)&flux_qt_create_svgwidget);
    jit.registerFunction("flux_qt_svgwidget_load",     (void*)&flux_qt_svgwidget_load);
    jit.registerFunction("flux_qt_svgwidget_set_size", (void*)&flux_qt_svgwidget_set_size);
    jit.registerFunction("flux_qt_label_load_image",   (void*)&flux_qt_label_load_image);
    jit.registerFunction("flux_qt_label_set_pixmap",   (void*)&flux_qt_label_set_pixmap);
    jit.registerFunction("flux_qt_set_window_icon",    (void*)&flux_qt_set_window_icon);
    jit.registerFunction("flux_qt_set_button_icon",    (void*)&flux_qt_set_button_icon);
    jit.registerFunction("flux_qt_set_action_icon",    (void*)&flux_qt_set_action_icon);
    jit.registerFunction("flux_qt_create_toolbar",     (void*)&flux_qt_create_toolbar);
    jit.registerFunction("flux_qt_toolbar_add_action", (void*)&flux_qt_toolbar_add_action);
    jit.registerFunction("flux_qt_toolbar_add_separator",(void*)&flux_qt_toolbar_add_separator);

    // Numeric properties
    jit.registerFunction("flux_qt_get_property",     (void*)&flux_qt_get_property);
    jit.registerFunction("flux_qt_set_property",     (void*)&flux_qt_set_property);

    // String properties (new)
    jit.registerFunction("flux_qt_get_str_prop",     (void*)&flux_qt_get_str_prop);
    jit.registerFunction("flux_qt_set_str_prop",     (void*)&flux_qt_set_str_prop);
    jit.registerFunction("flux_qt_get_text",         (void*)&flux_qt_get_text);
    jit.registerFunction("flux_qt_set_text",         (void*)&flux_qt_set_text);
    jit.registerFunction("flux_qt_set_window_title", (void*)&flux_qt_set_window_title);

    // Styling / widget control
    jit.registerFunction("flux_qt_set_stylesheet",   (void*)&flux_qt_set_stylesheet);
    jit.registerFunction("flux_qt_set_visible",      (void*)&flux_qt_set_visible);
    jit.registerFunction("flux_qt_set_tooltip",      (void*)&flux_qt_set_tooltip);

    // App control
    jit.registerFunction("flux_qt_app_quit",         (void*)&flux_qt_app_quit);

    // Signal bindings
    jit.registerFunction("flux_qt_on_click_by_name", (void*)&flux_qt_on_click_by_name);
    jit.registerFunction("flux_qt_on_value_changed_by_name", (void*)&flux_qt_on_value_changed_by_name);
    jit.registerFunction("flux_qt_on_current_index_changed_by_name", (void*)&flux_qt_on_current_index_changed_by_name);
    jit.registerFunction("flux_qt_on_toggled_by_name", (void*)&flux_qt_on_toggled_by_name);
    jit.registerFunction("flux_qt_on_text_changed_by_name", (void*)&flux_qt_on_text_changed_by_name);
    jit.registerFunction("flux_qt_on_current_row_changed_by_name", (void*)&flux_qt_on_current_row_changed_by_name);
}
