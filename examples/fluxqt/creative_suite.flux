# FluxScript Creative Suite
# A multi-tab GUI demo showcasing fluxqt widgets, QGraphicsView 2D canvas,
# timer-driven animation, and core FluxScript features.

# ── Global variable keys ──
def K_WIN()     1.0
def K_TABS()    2.0
def K_SLIDER()  3.0
def K_DIAL()    4.0
def K_PBAR()    5.0
def K_LCD()     6.0
def K_LABEL()   7.0
def K_CANVAS()  8.0
def K_SCENE()   9.0
def K_CAL()     10.0
def K_DATELBL() 11.0
def K_TIMER()   12.0
def K_ANGLE()   13.0
def K_ROT_LBL() 14.0
def K_INFO_TXT() 15.0

# ── Callbacks ──

def on_slider_changed() {
    h = flux_get_var(K_SLIDER())
    v = flux_qt_get_property(h, "value")
    flux_qt_set_property(flux_get_var(K_PBAR()), "value", v)
    flux_qt_lcd_display(flux_get_var(K_LCD()), v)
    flux_qt_set_text(flux_get_var(K_LABEL()), "Value: " + flux_double_to_string(v))
}

def on_dial_changed() {
    h = flux_get_var(K_DIAL())
    v = flux_qt_get_property(h, "value")
    flux_qt_set_property(flux_get_var(K_PBAR()), "value", v + 100.0)
    flux_qt_lcd_display(flux_get_var(K_LCD()), v)
    flux_qt_set_text(flux_get_var(K_LABEL()), "Dial: " + flux_double_to_string(v))
}

def on_canvas_tick() {
    angle = flux_get_var(K_ANGLE())
    angle = angle + 2.0
    if (angle >= 360.0) { angle = 0.0 }
    flux_set_var(K_ANGLE(), angle)

    scene = flux_get_var(K_SCENE())
    flux_qt_graphics_scene_clear(scene)

    cx = 200.0
    cy = 150.0
    r = 80.0
    rad = angle * 3.14159 / 180.0
    px = cx + r * cos(rad)
    py = cy + r * sin(rad)

    flux_qt_graphics_scene_add_ellipse(scene, cx - r, cy - r, r * 2.0, r * 2.0)
    flux_qt_graphics_scene_add_line(scene, cx, cy, px, py)
    flux_qt_graphics_scene_add_text(scene, "rotating arm", 10.0, 10.0)
    flux_qt_graphics_scene_add_rect(scene, px - 5.0, py - 5.0, 10.0, 10.0)

    flux_qt_set_text(flux_get_var(K_ROT_LBL()), "Angle: " + flux_double_to_string(angle) + " deg")
}

def on_date_changed() {
    d = flux_qt_calendar_selected_date(flux_get_var(K_CAL()))
    flux_qt_set_text(flux_get_var(K_DATELBL()), "Selected: " + d)
}

def on_quit() {
    flux_qt_app_quit()
}

def on_show_about() {
    flux_qt_msg_box("About FluxScript Creative Suite",
        "FluxScript Creative Suite v1.0\n\n"
        + "Core features:\n"
        + "  enum, match, class, generics [T]\n"
        + "  elif chains, impl methods\n"
        + "  struct params, async/await\n\n"
        + "GUI features:\n"
        + "  QMainWindow, tabs, menus, toolbar\n"
        + "  QGraphicsView 2D canvas\n"
        + "  Timers, signals, LCD, progress bars\n"
        + "  Calendar, sliders, dials\n"
        + "  Stylesheets, status bar")
}

# ── Tab 1: Playground — interactive widgets ──

def build_playground(tabs) {
    page = flux_qt_create_widget()
    layout = flux_qt_create_layout("vbox")
    flux_qt_set_layout(page, layout)

    flux_qt_layout_add_widget(layout, flux_qt_create_label("=== Widget Playground ==="))

    sld = flux_qt_create_slider(0.0)
    flux_qt_set_property(sld, "minimum", 0.0)
    flux_qt_set_property(sld, "maximum", 100.0)
    flux_qt_set_property(sld, "value", 50.0)
    flux_set_var(K_SLIDER(), sld)
    flux_qt_layout_add_widget(layout, sld)

    d = flux_qt_create_dial()
    flux_qt_set_property(d, "minimum", 0.0)
    flux_qt_set_property(d, "maximum", 200.0)
    flux_qt_set_property(d, "value", 0.0)
    flux_set_var(K_DIAL(), d)
    flux_qt_layout_add_widget(layout, d)

    pbar = flux_qt_create_progressbar()
    flux_qt_set_property(pbar, "value", 50.0)
    flux_set_var(K_PBAR(), pbar)
    flux_qt_layout_add_widget(layout, pbar)

    lcd = flux_qt_create_lcd()
    flux_qt_lcd_display(lcd, 50.0)
    flux_set_var(K_LCD(), lcd)
    flux_qt_layout_add_widget(layout, lcd)

    label = flux_qt_create_label("Move the slider or dial")
    flux_set_var(K_LABEL(), label)
    flux_qt_layout_add_widget(layout, label)

    flux_qt_on_value_changed_by_name(sld, "on_slider_changed")
    flux_qt_on_value_changed_by_name(d, "on_dial_changed")

    flux_qt_tab_add(tabs, page, "Playground")
}

# ── Tab 2: Canvas — QGraphicsView 2D drawing ──

def build_canvas(tabs) {
    page = flux_qt_create_widget()
    layout = flux_qt_create_layout("vbox")
    flux_qt_set_layout(page, layout)

    rot_lbl = flux_qt_create_label("Angle: 0 deg")
    flux_set_var(K_ROT_LBL(), rot_lbl)
    flux_qt_layout_add_widget(layout, rot_lbl)

    scene = flux_qt_create_graphics_scene()
    flux_set_var(K_SCENE(), scene)

    flux_qt_graphics_scene_set_scene_rect(scene, -10.0, -10.0, 420.0, 320.0)

    flux_qt_graphics_scene_add_rect(scene, 10.0, 10.0, 100.0, 80.0)
    flux_qt_graphics_scene_add_ellipse(scene, 130.0, 10.0, 80.0, 80.0)
    flux_qt_graphics_scene_add_line(scene, 230.0, 50.0, 330.0, 50.0)

    txt_h = flux_qt_graphics_scene_add_text(scene, "FluxScript 2D", 10.0, 110.0)
    flux_qt_graphics_item_set_pos(txt_h, 10.0, 110.0)

    view = flux_qt_create_graphics_view(scene)
    flux_set_var(K_CANVAS(), view)
    flux_qt_layout_add_widget(layout, view)

    flux_qt_tab_add(tabs, page, "Canvas")
}

# ── Tab 3: Calendar ──

def build_calendar(tabs) {
    page = flux_qt_create_widget()
    layout = flux_qt_create_layout("vbox")
    flux_qt_set_layout(page, layout)

    cal = flux_qt_create_calendar(0.0)
    flux_set_var(K_CAL(), cal)
    flux_qt_layout_add_widget(layout, cal)

    date_lbl = flux_qt_create_label("Select a date")
    flux_set_var(K_DATELBL(), date_lbl)
    flux_qt_layout_add_widget(layout, date_lbl)

    flux_qt_on_selection_changed_by_name(cal, "on_date_changed")

    flux_qt_tab_add(tabs, page, "Calendar")
}

# ── Tab 4: Info — system info ──

def build_info(tabs) {
    page = flux_qt_create_widget()
    layout = flux_qt_create_layout("vbox")
    flux_qt_set_layout(page, layout)

    flux_qt_layout_add_widget(layout, flux_qt_create_label("=== System Info ==="))

    info_txt = flux_qt_create_textedit()
    info_txt.readOnly = 1.0
    flux_set_var(K_INFO_TXT(), info_txt)
    flux_qt_layout_add_widget(layout, info_txt)

    flux_qt_set_text(info_txt,
        "FluxScript Creative Suite\n\n"
        + "Language Features:\n"
        + "  Enums with payloads\n"
        + "  Match expressions\n"
        + "  Classes with methods\n"
        + "  Generic functions [T]\n"
        + "  Structs with impl\n"
        + "  Async/await\n"
        + "  Try/catch/throw\n\n"
        + "GUI Features:\n"
        + "  Widgets: buttons, labels, sliders, dials\n"
        + "  LCD display, progress bars\n"
        + "  Main window with menus and toolbar\n"
        + "  Tab widget, calendar\n"
        + "  2D canvas (QGraphicsView)\n"
        + "  Timers, status bar\n"
        + "  File/input dialogs\n"
        + "  Stylesheets for theming")

    flux_qt_tab_add(tabs, page, "Info")
}

# ── Timer — auto-starts to animate the canvas ──

def on_timer_tick() {
    on_canvas_tick()
}

# ── Main entry point ──

def main() {
    win = flux_qt_create_mainwindow("FluxScript Creative Suite")
    flux_set_var(K_WIN(), win)
    flux_qt_set_window_size(win, 640, 520)
    flux_qt_set_stylesheet(win,
        "QMainWindow { background-color: #1e1e2e; }"
        + " QTabWidget::pane { background-color: #2b2b3d; }"
        + " QLabel { color: #cdd6f4; font-size: 13px; }"
        + " QPushButton { background-color: #45475a; color: #cdd6f4;"
        + "   border: 1px solid #585b70; padding: 6px 16px; border-radius: 4px; }"
        + " QPushButton:hover { background-color: #585b70; }"
        + " QLCDNumber { color: #a6e3a1; }"
        + " QProgressBar { background-color: #313244; border: 1px solid #45475a;"
        + "   border-radius: 3px; text-align: center; }"
        + " QProgressBar::chunk { background-color: #89b4fa; border-radius: 3px; }"
        + " QSlider::groove:horizontal { height: 6px; background: #313244;"
        + "   border-radius: 3px; }"
        + " QSlider::handle:horizontal { background: #89b4fa; width: 18px;"
        + "   margin: -6px 0; border-radius: 9px; }"
        + " QTextEdit { background-color: #181825; color: #cdd6f4;"
        + "   border: 1px solid #45475a; font-family: monospace; }")

    # Menu bar
    mb = flux_qt_get_menubar(win)
    file_menu = flux_qt_menu_add_menu(mb, "&File")
    flux_qt_menu_add_action(file_menu, "&Quit", "on_quit")
    help_menu = flux_qt_menu_add_menu(mb, "&Help")
    flux_qt_menu_add_action(help_menu, "&About", "on_show_about")

    flux_qt_set_statusbar_text(win, "Ready")

    # Tab widget as central
    tabs = flux_qt_create_tabwidget()
    flux_set_var(K_TABS(), tabs)
    flux_qt_set_central_widget(win, tabs)

    build_playground(tabs)
    build_canvas(tabs)
    build_calendar(tabs)
    build_info(tabs)

    # Animation timer
    flux_set_var(K_ANGLE(), 0.0)
    tmr = flux_qt_create_timer(50.0, "on_timer_tick")
    flux_set_var(K_TIMER(), tmr)
    flux_qt_timer_start(tmr)
}

main()
