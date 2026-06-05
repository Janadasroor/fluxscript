# FluxScript Desktop — QMainWindow with menus, dialogs, status bar, splitter panels
# Showcases: QMainWindow, menu bar, file dialogs, input dialog, status bar

def K_WIN()     1.0
def K_MB()      2.0
def K_SPLIT()   3.0
def K_LIST()    4.0
def K_STACK()   5.0
def K_LCD()     6.0
def K_SLIDER()  7.0
def K_DIAL()    8.0
def K_PBAR0()   9.0
def K_PBAR1()   10.0

def on_open() {
  let f = flux_qt_open_file_dialog("Open File", "All (*)")
  if (f != 0.0) {
    flux_qt_set_statusbar_text(flux_get_var(K_WIN()), f)
  } else {}
  0.0
}

def on_save() {
  let f = flux_qt_save_file_dialog("Save As", "All (*)")
  if (f != 0.0) {
    flux_qt_set_statusbar_text(flux_get_var(K_WIN()), f)
  } else {}
  0.0
}

def on_rename() {
  let name = flux_qt_input_dialog("Rename", "New name:")
  if (name != 0.0) {
    flux_qt_set_statusbar_text(flux_get_var(K_WIN()), name)
  } else {}
  0.0
}

def on_about() {
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "FluxScript Desktop v1.0")
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

def on_nav() {
  let row = flux_qt_get_property(flux_get_var(K_LIST()), "currentRow")
  flux_qt_stacked_set_current(flux_get_var(K_STACK()), row)
  if (row == 0.0) { flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Dashboard") }
  elif (row == 1.0) { flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Sensors") }
  else { flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "About") }
  0.0
}

def on_slider() {
  let v = flux_qt_get_property(flux_get_var(K_SLIDER()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR0()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD()), v)
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Slider moved")
  0.0
}

def on_dial() {
  let v = flux_qt_get_property(flux_get_var(K_DIAL()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR1()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD()), v)
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Dial turned")
  0.0
}

# ── Main window ──
flux_set_var(K_WIN(), flux_qt_create_mainwindow("FluxScript Desktop"))
flux_qt_set_window_size(flux_get_var(K_WIN()), 720, 480)

flux_set_var(K_MB(), flux_qt_get_menubar(flux_get_var(K_WIN())))
let file_menu = flux_qt_menu_add_menu(flux_get_var(K_MB()), "&File")
flux_qt_menu_add_action(file_menu, "&Open...", "on_open")
flux_qt_menu_add_action(file_menu, "&Save As...", "on_save")
flux_qt_menu_add_action(file_menu, "&Rename...", "on_rename")
flux_qt_menu_add_action(file_menu, "E&xit", "on_quit")

let help_menu = flux_qt_menu_add_menu(flux_get_var(K_MB()), "&Help")
flux_qt_menu_add_action(help_menu, "&About", "on_about")

flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Ready")

# ── Splitter: sidebar + content ──
flux_set_var(K_SPLIT(), flux_qt_create_splitter(0.0))
flux_qt_set_central_widget(flux_get_var(K_WIN()), flux_get_var(K_SPLIT()))

# Sidebar
let nav_gb = flux_qt_create_groupbox("Navigation")
let nav_lay = flux_qt_create_layout("vbox")
flux_qt_groupbox_set_layout(nav_gb, nav_lay)
flux_set_var(K_LIST(), flux_qt_create_listwidget())
flux_qt_list_add_item(flux_get_var(K_LIST()), "Dashboard")
flux_qt_list_add_item(flux_get_var(K_LIST()), "Sensors")
flux_qt_list_add_item(flux_get_var(K_LIST()), "About")
flux_qt_layout_add_widget(nav_lay, flux_get_var(K_LIST()))
flux_qt_splitter_add(flux_get_var(K_SPLIT()), nav_gb)

# Stacked content
flux_set_var(K_STACK(), flux_qt_create_stackedwidget())
flux_qt_splitter_add(flux_get_var(K_SPLIT()), flux_get_var(K_STACK()))

# ── Page 0: Dashboard ──
let p0 = flux_qt_create_widget()
let p0l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p0, p0l)
flux_qt_layout_set_margins(p0l, 12.0, 12.0, 12.0, 12.0)

let p0h = flux_qt_create_label(" Dashboard")
flux_qt_set_property(p0h, "alignment", 32.0)
flux_qt_set_stylesheet(p0h, "font-size:18px; font-weight:bold; color:#26a;")
flux_qt_layout_add_widget(p0l, p0h)
flux_qt_layout_add_widget(p0l, flux_qt_create_label("Level:"))
flux_set_var(K_SLIDER(), flux_qt_create_slider(0.0))
flux_qt_layout_add_widget(p0l, flux_get_var(K_SLIDER()))
flux_set_var(K_PBAR0(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR0()), "QProgressBar::chunk { background:#26a; }")
flux_qt_layout_add_widget(p0l, flux_get_var(K_PBAR0()))
flux_set_var(K_LCD(), flux_qt_create_lcd())
flux_qt_layout_add_widget(p0l, flux_get_var(K_LCD()))
flux_qt_stacked_add(flux_get_var(K_STACK()), p0)

# ── Page 1: Sensors ──
let p1 = flux_qt_create_widget()
let p1l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p1, p1l)
flux_qt_layout_set_margins(p1l, 12.0, 12.0, 12.0, 12.0)

let p1h = flux_qt_create_label(" Sensors")
flux_qt_set_property(p1h, "alignment", 32.0)
flux_qt_set_stylesheet(p1h, "font-size:18px; font-weight:bold; color:#2a6;")
flux_qt_layout_add_widget(p1l, p1h)
flux_qt_layout_add_widget(p1l, flux_qt_create_label("Rotary:"))
flux_set_var(K_DIAL(), flux_qt_create_dial())
flux_qt_layout_add_widget(p1l, flux_get_var(K_DIAL()))
flux_set_var(K_PBAR1(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR1()), "QProgressBar::chunk { background:#2a6; }")
flux_qt_layout_add_widget(p1l, flux_get_var(K_PBAR1()))
flux_qt_stacked_add(flux_get_var(K_STACK()), p1)

# ── Page 2: About ──
let p2 = flux_qt_create_widget()
let p2l = flux_qt_create_layout("vbox")
flux_qt_set_layout(p2, p2l)
flux_qt_layout_set_margins(p2l, 20.0, 20.0, 20.0, 20.0)

let p2h = flux_qt_create_label(" About FluxScript Desktop")
flux_qt_set_property(p2h, "alignment", 32.0)
flux_qt_set_stylesheet(p2h, "font-size:18px; font-weight:bold; color:#888;")
flux_qt_layout_add_widget(p2l, p2h)
let p2b = flux_qt_create_label("FluxScript Qt Framework\n\nDesktop app infrastructure:\n- QMainWindow with menus\n- File/input dialogs\n- Status bar\n- Splitter panels\n- Stacked navigation\n- Tooltips & stylesheets")
flux_qt_layout_add_widget(p2l, p2b)
flux_qt_stacked_add(flux_get_var(K_STACK()), p2)

# ── Init ──
flux_qt_lcd_display(flux_get_var(K_LCD()), 0.0)
flux_qt_list_set_current_row(flux_get_var(K_LIST()), 0.0)
flux_qt_stacked_set_current(flux_get_var(K_STACK()), 0.0)

# ── Signals ──
flux_qt_on_current_row_changed_by_name(flux_get_var(K_LIST()), "on_nav")
flux_qt_on_value_changed_by_name(flux_get_var(K_SLIDER()), "on_slider")
flux_qt_on_value_changed_by_name(flux_get_var(K_DIAL()), "on_dial")
