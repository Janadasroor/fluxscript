# FluxScript Qt Dashboard — tabbed GUI framework demo
# Showcases: QTabWidget, QGroupBox, QRadioButton, layout spacing,
#            stylesheets, dial, live string binding, error logging

def K_TAB()       1.0
def K_TAB1_LAY()  2.0
def K_TAB2_LAY()  3.0
def K_TAB3_LAY()  4.0
def K_SLIDER()    5.0
def K_DIAL()      6.0
def K_PBAR()      7.0
def K_PBAR2()     8.0
def K_LCD_VAL()   9.0
def K_SPIN()      10.0
def K_CHK()       11.0
def K_EDIT()      12.0
def K_LABEL_OUT() 13.0

def on_slider() {
  let v = flux_qt_get_property(flux_get_var(K_SLIDER()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD_VAL()), v)
  0.0
}

def on_dial() {
  let v = flux_qt_get_property(flux_get_var(K_DIAL()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR2()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD_VAL()), v)
  0.0
}

def on_spin() {
  let v = flux_qt_get_property(flux_get_var(K_SPIN()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR()), "value", v)
  flux_qt_set_property(flux_get_var(K_PBAR2()), "value", v)
  flux_qt_lcd_display(flux_get_var(K_LCD_VAL()), v)
  0.0
}

def on_edit() {
  let t = flux_qt_get_text(flux_get_var(K_EDIT()))
  flux_qt_set_text(flux_get_var(K_LABEL_OUT()), t)
  flux_qt_set_window_title(flux_get_var(K_TAB()), t)
  0.0
}

flux_set_var(K_TAB(), flux_qt_create_tabwidget())
flux_qt_set_stylesheet(flux_get_var(K_TAB()), "QTabWidget::pane { border:2px solid #ccc; border-radius:4px; } QTabBar::tab { padding:6px 14px; } QTabBar::tab:selected { background:#26a; color:#fff; }")

# ── Tab 1: Controls ──
let g1 = flux_qt_create_groupbox("Analog Controls")
flux_set_var(K_TAB1_LAY(), flux_qt_create_layout("vbox"))
flux_qt_groupbox_set_layout(g1, flux_get_var(K_TAB1_LAY()))
flux_qt_layout_set_margins(flux_get_var(K_TAB1_LAY()), 10.0, 10.0, 10.0, 10.0)
flux_qt_layout_set_spacing(flux_get_var(K_TAB1_LAY()), 8.0)

flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_qt_create_label("Slider:"))
flux_set_var(K_SLIDER(), flux_qt_create_slider(0.0))
flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_get_var(K_SLIDER()))
flux_set_var(K_PBAR(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR()), "QProgressBar::chunk { background: #26a; }")
flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_get_var(K_PBAR()))

flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_qt_create_label("Dial:"))
flux_set_var(K_DIAL(), flux_qt_create_dial())
flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_get_var(K_DIAL()))
flux_set_var(K_PBAR2(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR2()), "QProgressBar::chunk { background: #2a6; }")
flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_get_var(K_PBAR2()))

flux_set_var(K_LCD_VAL(), flux_qt_create_lcd())
flux_qt_layout_add_widget(flux_get_var(K_TAB1_LAY()), flux_get_var(K_LCD_VAL()))

flux_qt_tab_add(flux_get_var(K_TAB()), g1, "Controls")

# ── Tab 2: Settings ──
let g2 = flux_qt_create_groupbox("Preferences")
flux_set_var(K_TAB2_LAY(), flux_qt_create_layout("vbox"))
flux_qt_groupbox_set_layout(g2, flux_get_var(K_TAB2_LAY()))
flux_qt_layout_set_margins(flux_get_var(K_TAB2_LAY()), 10.0, 10.0, 10.0, 10.0)
flux_qt_layout_set_spacing(flux_get_var(K_TAB2_LAY()), 8.0)

flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_qt_create_label("Timer Speed:"))
flux_set_var(K_SPIN(), flux_qt_create_spinbox())
flux_qt_set_property(flux_get_var(K_SPIN()), "value", 100.0)
flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_get_var(K_SPIN()))

flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_qt_create_label("Options:"))
flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_qt_create_radiobutton("Option A"))
flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_qt_create_radiobutton("Option B"))
flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_qt_create_radiobutton("Option C"))

flux_set_var(K_CHK(), flux_qt_create_checkbox("Enable Notifications"))
flux_qt_layout_add_widget(flux_get_var(K_TAB2_LAY()), flux_get_var(K_CHK()))

flux_qt_tab_add(flux_get_var(K_TAB()), g2, "Settings")

# ── Tab 3: Live Text ──
let g3 = flux_qt_create_groupbox("Live String Binding")
flux_set_var(K_TAB3_LAY(), flux_qt_create_layout("vbox"))
flux_qt_groupbox_set_layout(g3, flux_get_var(K_TAB3_LAY()))
flux_qt_layout_set_margins(flux_get_var(K_TAB3_LAY()), 10.0, 10.0, 10.0, 10.0)
flux_qt_layout_set_spacing(flux_get_var(K_TAB3_LAY()), 8.0)

flux_qt_layout_add_widget(flux_get_var(K_TAB3_LAY()), flux_qt_create_label("Type something:"))
flux_set_var(K_EDIT(), flux_qt_create_lineedit("hello flux"))
flux_qt_layout_add_widget(flux_get_var(K_TAB3_LAY()), flux_get_var(K_EDIT()))

flux_qt_layout_add_widget(flux_get_var(K_TAB3_LAY()), flux_qt_create_label("Live preview:"))
flux_set_var(K_LABEL_OUT(), flux_qt_create_label("hello flux"))
flux_qt_set_stylesheet(flux_get_var(K_LABEL_OUT()), "font-size:18px; font-weight:bold; color:#26a;")
flux_qt_layout_add_widget(flux_get_var(K_TAB3_LAY()), flux_get_var(K_LABEL_OUT()))

flux_qt_tab_add(flux_get_var(K_TAB()), g3, "Text")

# ── Wire window ──
let win = flux_qt_create_window("Dashboard")
flux_qt_set_window_size(win, 480, 500)
flux_qt_add_widget(win, flux_get_var(K_TAB()))

# Init display
flux_qt_lcd_display(flux_get_var(K_LCD_VAL()), 0.0)

# Wire signals
flux_qt_on_value_changed_by_name(flux_get_var(K_SLIDER()), "on_slider")
flux_qt_on_value_changed_by_name(flux_get_var(K_DIAL()), "on_dial")
flux_qt_on_value_changed_by_name(flux_get_var(K_SPIN()), "on_spin")
flux_qt_on_text_changed_by_name(flux_get_var(K_EDIT()), "on_edit")
