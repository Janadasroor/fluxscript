# FluxScript Smart Control Panel v3
# Full framework demo: dial, text/edit, stylesheet, string props, timer speed, peak tracker

def K_WIN()       1.0
def K_LAYOUT()    2.0
def K_SLIDER()    3.0
def K_DIAL()      4.0
def K_PBAR()      5.0
def K_PBAR_ANI()  6.0
def K_LCD_SL()    7.0
def K_LCD_TICK()  8.0
def K_LCD_DIAL()  9.0
def K_COUNTER()   10.0
def K_PROGRESS()  11.0
def K_RUNNING()   12.0
def K_TMR()       13.0
def K_BTN_TOG()   14.0
def K_LCD_RUN()   15.0
def K_SPIN()      16.0
def K_LCD_SPIN()  17.0
def K_CHK_AUTO()  18.0
def K_BTN_RST()   19.0
def K_BTN_QUIT()  20.0
def K_AUTO_MODE() 21.0
def K_LCD_PEAK()  22.0
def K_PEAK_VAL()  23.0
def K_LABEL_STATUS() 24.0
def K_EDIT()      25.0

def on_reset() {
  flux_set_var(K_COUNTER(), 0.0)
  flux_set_var(K_PROGRESS(), 0.0)
  flux_set_var(K_PEAK_VAL(), 0.0)
  flux_qt_lcd_display(flux_get_var(K_LCD_TICK()), 0.0)
  flux_qt_set_property(flux_get_var(K_PBAR_ANI()), "value", 0.0)
  flux_qt_lcd_display(flux_get_var(K_LCD_PEAK()), 0.0)
  0.0
}

def on_tog() {
  let r = flux_get_var(K_RUNNING())
  let rn = if (r == 0.0) { 1.0 } else { 0.0 }
  flux_set_var(K_RUNNING(), rn)
  flux_qt_lcd_display(flux_get_var(K_LCD_RUN()), rn)
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

def on_slider_live() {
  let sv = flux_qt_get_property(flux_get_var(K_SLIDER()), "value")
  flux_qt_set_property(flux_get_var(K_PBAR()), "value", sv)
  flux_qt_lcd_display(flux_get_var(K_LCD_SL()), sv)
  0.0
}

def on_dial_live() {
  let dv = flux_qt_get_property(flux_get_var(K_DIAL()), "value")
  flux_qt_lcd_display(flux_get_var(K_LCD_DIAL()), dv)
  0.0
}

def on_spin_live() {
  let sv = flux_qt_get_property(flux_get_var(K_SPIN()), "value")
  flux_qt_lcd_display(flux_get_var(K_LCD_SPIN()), sv)
  flux_qt_set_property(flux_get_var(K_TMR()), "interval", sv)
  0.0
}

def on_auto_toggled() {
  let chk = flux_qt_get_property(flux_get_var(K_CHK_AUTO()), "checked")
  flux_set_var(K_AUTO_MODE(), chk)
  0.0
}

def on_tick() {
  let run = flux_get_var(K_RUNNING())
  if (run == 0.0) { 0.0 } else {
    let p = flux_get_var(K_PROGRESS())
    let step = if (flux_get_var(K_AUTO_MODE()) == 0.0) { 2.0 } else { 6.0 }
    let p_raw = p + step
    let pn = if (p_raw > 100.0) { 0.0 } else { p_raw }
    flux_set_var(K_PROGRESS(), pn)
    flux_qt_set_property(flux_get_var(K_PBAR_ANI()), "value", pn)

    let peak = flux_get_var(K_PEAK_VAL())
    if (pn > peak) {
      flux_set_var(K_PEAK_VAL(), pn)
      flux_qt_lcd_display(flux_get_var(K_LCD_PEAK()), pn)
    } else {}

    let cnt = flux_get_var(K_COUNTER())
    let cnt_new = cnt + 1.0
    flux_set_var(K_COUNTER(), cnt_new)
    flux_qt_lcd_display(flux_get_var(K_LCD_TICK()), cnt_new)
    0.0
  }
}

# ── Build UI ──
flux_set_var(K_WIN(), flux_qt_create_window("Smart Control Panel v3"))
flux_qt_set_window_size(flux_get_var(K_WIN()), 700, 520)
flux_set_var(K_LAYOUT(), flux_qt_create_layout("grid"))
flux_qt_set_layout(flux_get_var(K_WIN()), flux_get_var(K_LAYOUT()))
let ly = flux_get_var(K_LAYOUT())

let hdr = flux_qt_create_label("  Smart Control Panel v3")
flux_qt_set_property(hdr, "alignment", 32.0)
flux_qt_set_stylesheet(hdr, "font-size:18px; font-weight:bold; color:#2a6; padding:6px;")
flux_qt_grid_add_widget(ly, hdr, 0, 0, 1, 3)

flux_qt_grid_add_widget(ly, flux_qt_create_label("Level:"), 1, 0, 1, 1)
flux_qt_grid_add_widget(ly, flux_qt_create_label("Dial:"), 1, 1, 1, 1)
flux_qt_grid_add_widget(ly, flux_qt_create_label("Timer (ms):"), 1, 2, 1, 1)

flux_set_var(K_SLIDER(), flux_qt_create_slider(0.0))
flux_qt_grid_add_widget(ly, flux_get_var(K_SLIDER()), 2, 0, 1, 1)
flux_set_var(K_DIAL(), flux_qt_create_dial())
flux_qt_grid_add_widget(ly, flux_get_var(K_DIAL()), 2, 1, 1, 1)
flux_set_var(K_SPIN(), flux_qt_create_spinbox())
flux_qt_set_property(flux_get_var(K_SPIN()), "value", 100.0)
flux_qt_set_property(flux_get_var(K_SPIN()), "singleStep", 50.0)
flux_qt_grid_add_widget(ly, flux_get_var(K_SPIN()), 2, 2, 1, 1)

flux_set_var(K_LCD_SL(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_SL()), 3, 0, 1, 1)
flux_set_var(K_LCD_DIAL(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_DIAL()), 3, 1, 1, 1)
flux_set_var(K_LCD_SPIN(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_SPIN()), 3, 2, 1, 1)

flux_qt_grid_add_widget(ly, flux_qt_create_label("Animated Progress:"), 4, 0, 1, 3)
flux_set_var(K_PBAR_ANI(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR_ANI()), "QProgressBar::chunk { background: #2a6; }")
flux_qt_grid_add_widget(ly, flux_get_var(K_PBAR_ANI()), 5, 0, 1, 3)

flux_qt_grid_add_widget(ly, flux_qt_create_label("Ticks:"), 6, 0, 1, 1)
flux_qt_grid_add_widget(ly, flux_qt_create_label("Peak:"), 6, 1, 1, 1)
flux_qt_grid_add_widget(ly, flux_qt_create_label("Status"), 6, 2, 1, 1)
flux_set_var(K_LCD_TICK(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_TICK()), 7, 0, 1, 1)
flux_set_var(K_LCD_PEAK(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_PEAK()), 7, 1, 1, 1)
flux_set_var(K_LABEL_STATUS(), flux_qt_create_label("ok"))
flux_qt_grid_add_widget(ly, flux_get_var(K_LABEL_STATUS()), 7, 2, 1, 1)

flux_set_var(K_CHK_AUTO(), flux_qt_create_checkbox("Fast Mode"))
flux_qt_grid_add_widget(ly, flux_get_var(K_CHK_AUTO()), 8, 0, 1, 1)
flux_qt_grid_add_widget(ly, flux_qt_create_label("Running:"), 8, 1, 1, 1)
flux_set_var(K_LCD_RUN(), flux_qt_create_lcd())
flux_qt_grid_add_widget(ly, flux_get_var(K_LCD_RUN()), 8, 2, 1, 1)

flux_set_var(K_EDIT(), flux_qt_create_lineedit(""))
flux_qt_grid_add_widget(ly, flux_get_var(K_EDIT()), 9, 0, 1, 3)

flux_set_var(K_BTN_TOG(), flux_qt_create_button("Pause / Resume"))
flux_qt_grid_add_widget(ly, flux_get_var(K_BTN_TOG()), 10, 0, 1, 1)
flux_set_var(K_BTN_RST(), flux_qt_create_button("Reset"))
flux_qt_grid_add_widget(ly, flux_get_var(K_BTN_RST()), 10, 1, 1, 1)
flux_set_var(K_BTN_QUIT(), flux_qt_create_button("Quit"))
flux_qt_grid_add_widget(ly, flux_get_var(K_BTN_QUIT()), 10, 2, 1, 1)

# Init display
flux_qt_lcd_display(flux_get_var(K_LCD_SL()), 0.0)
flux_qt_lcd_display(flux_get_var(K_LCD_DIAL()), 0.0)
flux_qt_lcd_display(flux_get_var(K_LCD_SPIN()), 100.0)
flux_qt_lcd_display(flux_get_var(K_LCD_RUN()), 1.0)
flux_qt_lcd_display(flux_get_var(K_LCD_TICK()), 0.0)
flux_qt_lcd_display(flux_get_var(K_LCD_PEAK()), 0.0)

flux_set_var(K_COUNTER(), 0.0)
flux_set_var(K_PROGRESS(), 0.0)
flux_set_var(K_PEAK_VAL(), 0.0)
flux_set_var(K_RUNNING(), 1.0)
flux_set_var(K_AUTO_MODE(), 0.0)

# Wire signals
flux_qt_on_click_by_name(flux_get_var(K_BTN_TOG()), "on_tog")
flux_qt_on_click_by_name(flux_get_var(K_BTN_RST()), "on_reset")
flux_qt_on_click_by_name(flux_get_var(K_BTN_QUIT()), "on_quit")
flux_qt_on_value_changed_by_name(flux_get_var(K_SLIDER()), "on_slider_live")
flux_qt_on_value_changed_by_name(flux_get_var(K_DIAL()), "on_dial_live")
flux_qt_on_value_changed_by_name(flux_get_var(K_SPIN()), "on_spin_live")
flux_qt_on_toggled_by_name(flux_get_var(K_CHK_AUTO()), "on_auto_toggled")

# Start timer
flux_set_var(K_TMR(), flux_qt_create_timer(100, "on_tick"))
flux_qt_timer_start(flux_get_var(K_TMR()))
