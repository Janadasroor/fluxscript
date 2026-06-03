# FluxScript Browser — splitter + sidebar + stacked pages demo
# Showcases: QSplitter, QListWidget, QStackedWidget, tooltips, layout stretch

def K_SPLIT()  1.0
def K_LIST()   2.0
def K_STACK()  3.0
def K_SLIDER() 7.0
def K_DIAL()   8.0
def K_PBAR0()  10.0
def K_PBAR1()  11.0
def K_LCD0()   12.0
def K_LCD1()   13.0

def on_nav() {
  let row = flux_qt_get_property(flux_get_var(K_LIST()), "currentRow")
  flux_qt_stacked_set_current(flux_get_var(K_STACK()), row)
  0.0
}

def on_slider() {
  let v = flux_qt_get_property(flux_get_var(K_SLIDER()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR0()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD0()), v)
  0.0
}

def on_dial() {
  let v = flux_qt_get_property(flux_get_var(K_DIAL()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR1()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD1()), v)
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

# ── Window with splitter ──
let win = flux_qt_create_window("FluxScript Browser")
flux_qt_set_window_size(win, 640, 480)

flux_set_var(K_SPLIT(), flux_qt_create_splitter(0.0))
flux_qt_add_widget(win, flux_get_var(K_SPLIT()))

# ── Left: sidebar ──
let nav_gb = flux_qt_create_groupbox("Navigation")
let nav_lay = flux_qt_create_layout("vbox")
flux_qt_groupbox_set_layout(nav_gb, nav_lay)

flux_set_var(K_LIST(), flux_qt_create_listwidget())
flux_qt_list_add_item(flux_get_var(K_LIST()), "Dashboard")
flux_qt_list_add_item(flux_get_var(K_LIST()), "Sensors")
flux_qt_list_add_item(flux_get_var(K_LIST()), "About")
flux_qt_set_tooltip(flux_get_var(K_LIST()), "Select a page")
flux_qt_layout_add_widget(nav_lay, flux_get_var(K_LIST()))
flux_qt_layout_set_stretch(nav_lay, flux_get_var(K_LIST()), 1.0)

flux_qt_splitter_add(flux_get_var(K_SPLIT()), nav_gb)
flux_qt_set_tooltip(nav_gb, "Navigation sidebar")

# ── Right: stacked pages ──
flux_set_var(K_STACK(), flux_qt_create_stackedwidget())
flux_qt_splitter_add(flux_get_var(K_SPLIT()), flux_get_var(K_STACK()))

# ── Page 0: Dashboard ──
let p0 = flux_qt_create_widget()
let p0l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p0, p0l)
flux_qt_layout_set_margins(p0l, 12.0, 12.0, 12.0, 12.0)
flux_qt_layout_set_spacing(p0l, 8.0)

let p0h = flux_qt_create_label(" Dashboard")
flux_qt_set_property(p0h, "alignment", 32.0)
flux_qt_set_stylesheet(p0h, "font-size:16px; font-weight:bold; color:#26a;")
flux_qt_layout_add_widget(p0l, p0h)

flux_qt_layout_add_widget(p0l, flux_qt_create_label("Level Control:"))
flux_set_var(K_SLIDER(), flux_qt_create_slider(0.0))
flux_qt_set_tooltip(flux_get_var(K_SLIDER()), "Drag to adjust level")
flux_qt_layout_add_widget(p0l, flux_get_var(K_SLIDER()))

flux_set_var(K_PBAR0(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR0()), "QProgressBar::chunk { background:#26a; }")
flux_qt_layout_add_widget(p0l, flux_get_var(K_PBAR0()))

flux_set_var(K_LCD0(), flux_qt_create_lcd())
flux_qt_layout_add_widget(p0l, flux_get_var(K_LCD0()))

let qbtn = flux_qt_create_button("Quit")
flux_qt_set_stylesheet(qbtn, "font-size:14px; color:#fff; background:#c33; padding:4px; border-radius:3px;")
flux_qt_set_tooltip(qbtn, "Exit the application")
flux_qt_layout_add_widget(p0l, qbtn)

flux_qt_stacked_add(flux_get_var(K_STACK()), p0)

# ── Page 1: Sensors ──
let p1 = flux_qt_create_widget()
let p1l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p1, p1l)
flux_qt_layout_set_margins(p1l, 12.0, 12.0, 12.0, 12.0)
flux_qt_layout_set_spacing(p1l, 8.0)

let p1h = flux_qt_create_label(" Sensors")
flux_qt_set_property(p1h, "alignment", 32.0)
flux_qt_set_stylesheet(p1h, "font-size:16px; font-weight:bold; color:#2a6;")
flux_qt_layout_add_widget(p1l, p1h)

flux_qt_layout_add_widget(p1l, flux_qt_create_label("Rotary Sensor:"))
flux_set_var(K_DIAL(), flux_qt_create_dial())
flux_qt_set_tooltip(flux_get_var(K_DIAL()), "Rotate to adjust value")
flux_qt_layout_add_widget(p1l, flux_get_var(K_DIAL()))

flux_set_var(K_PBAR1(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR1()), "QProgressBar::chunk { background:#2a6; }")
flux_qt_layout_add_widget(p1l, flux_get_var(K_PBAR1()))

flux_set_var(K_LCD1(), flux_qt_create_lcd())
flux_qt_layout_add_widget(p1l, flux_get_var(K_LCD1()))

flux_qt_stacked_add(flux_get_var(K_STACK()), p1)

# ── Page 2: About ──
let p2 = flux_qt_create_widget()
let p2l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p2, p2l)
flux_qt_layout_set_margins(p2l, 20.0, 20.0, 20.0, 20.0)
flux_qt_layout_set_spacing(p2l, 10.0)

let p2h = flux_qt_create_label(" About FluxScript")
flux_qt_set_property(p2h, "alignment", 32.0)
flux_qt_set_stylesheet(p2h, "font-size:18px; font-weight:bold; color:#888;")
flux_qt_layout_add_widget(p2l, p2h)

let p2b = flux_qt_create_label("FluxScript Qt Bridge  v3\n\nA framework for building\nQt GUIs with FluxScript.\n\nWidgets: splitter, tabs, dial,\nslider, radio, lists, groups,\nstacked panels, stylesheets,\ntooltips, and more.")
flux_qt_layout_add_widget(p2l, p2b)

flux_qt_stacked_add(flux_get_var(K_STACK()), p2)

# ── Init ──
flux_qt_lcd_display(flux_get_var(K_LCD0()), 0.0)
flux_qt_lcd_display(flux_get_var(K_LCD1()), 0.0)
flux_qt_list_set_current_row(flux_get_var(K_LIST()), 0.0)
flux_qt_stacked_set_current(flux_get_var(K_STACK()), 0.0)

# ── Signals ──
flux_qt_on_current_row_changed_by_name(flux_get_var(K_LIST()), "on_nav")
flux_qt_on_value_changed_by_name(flux_get_var(K_SLIDER()), "on_slider")
flux_qt_on_value_changed_by_name(flux_get_var(K_DIAL()), "on_dial")
flux_qt_on_click_by_name(qbtn, "on_quit")
