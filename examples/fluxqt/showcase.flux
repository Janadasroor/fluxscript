# FluxScript Qt Framework Showcase
# Demonstrates: stylesheets, dial, string props, themes, live text binding

def K_WIN()      1.0
def K_LAYOUT()   2.0
def K_DIAL()     3.0
def K_LCD_DIAL() 4.0
def K_PBAR()     5.0
def K_LABEL_OUT() 6.0
def K_EDIT()     7.0
def K_BTN_QUIT() 8.0

def on_dial_live() {
  let dv = flux_qt_get_property(flux_get_var(K_DIAL()), "value")
  flux_qt_lcd_display(flux_get_var(K_LCD_DIAL()), dv)
  flux_qt_set_property(flux_get_var(K_PBAR()), "value", dv)
  0.0
}

def on_edit_changed() {
  let txt = flux_qt_get_text(flux_get_var(K_EDIT()))
  flux_qt_set_text(flux_get_var(K_LABEL_OUT()), txt)
  flux_qt_set_window_title(flux_get_var(K_WIN()), txt)
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

flux_set_var(K_WIN(), flux_qt_create_window("FluxScript Showcase"))
flux_qt_set_window_size(flux_get_var(K_WIN()), 520, 440)
flux_set_var(K_LAYOUT(), flux_qt_create_layout("vbox"))
flux_qt_set_layout(flux_get_var(K_WIN()), flux_get_var(K_LAYOUT()))
let ly = flux_get_var(K_LAYOUT())

let hdr = flux_qt_create_label("  FluxScript Qt Framework")
flux_qt_set_property(hdr, "alignment", 32.0)
flux_qt_set_stylesheet(hdr, "font-size:20px; font-weight:bold; color:#fff; background:#26a; padding:10px; border-radius:6px;")
flux_qt_layout_add_widget(ly, hdr)

flux_qt_layout_add_widget(ly, flux_qt_create_label("Rotary Dial:"))
flux_set_var(K_DIAL(), flux_qt_create_dial())
flux_qt_layout_add_widget(ly, flux_get_var(K_DIAL()))
flux_set_var(K_LCD_DIAL(), flux_qt_create_lcd())
flux_qt_layout_add_widget(ly, flux_get_var(K_LCD_DIAL()))

flux_qt_layout_add_widget(ly, flux_qt_create_label("Progress:"))
flux_set_var(K_PBAR(), flux_qt_create_progressbar())
flux_qt_set_stylesheet(flux_get_var(K_PBAR()), "QProgressBar { border:1px solid #888; border-radius:4px; text-align:center; } QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #26a, stop:1 #2a6); border-radius:4px; }")
flux_qt_layout_add_widget(ly, flux_get_var(K_PBAR()))

flux_qt_layout_add_widget(ly, flux_qt_create_label("Live Text Binding:"))
flux_set_var(K_EDIT(), flux_qt_create_lineedit("type here"))
flux_qt_layout_add_widget(ly, flux_get_var(K_EDIT()))

flux_set_var(K_LABEL_OUT(), flux_qt_create_label("type here"))
flux_qt_set_stylesheet(flux_get_var(K_LABEL_OUT()), "font-size:16px; color:#26a; font-weight:bold; padding:4px;")
flux_qt_layout_add_widget(ly, flux_get_var(K_LABEL_OUT()))

flux_set_var(K_BTN_QUIT(), flux_qt_create_button("Quit"))
flux_qt_set_stylesheet(flux_get_var(K_BTN_QUIT()), "font-size:14px; color:#fff; background:#c33; padding:6px; border-radius:4px;")
flux_qt_layout_add_widget(ly, flux_get_var(K_BTN_QUIT()))

flux_qt_lcd_display(flux_get_var(K_LCD_DIAL()), 0.0)

flux_qt_on_value_changed_by_name(flux_get_var(K_DIAL()), "on_dial_live")
flux_qt_on_text_changed_by_name(flux_get_var(K_EDIT()), "on_edit_changed")
flux_qt_on_click_by_name(flux_get_var(K_BTN_QUIT()), "on_quit")
