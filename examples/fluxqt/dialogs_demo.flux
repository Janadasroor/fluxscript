# FluxScript Dialogs Demo — QColorDialog + QFontDialog

def K_WIN()  1.0
def K_LAB()  2.0

def on_color() {
  let ok = flux_qt_color_dialog("Pick a color")
  if (ok != 0.0) {
    flux_qt_msg_box("Color", "Picked")
  } else {
    flux_qt_msg_box("Color", "Cancelled")
  }
  0.0
}

def on_font() {
  let ok = flux_qt_font_dialog("Pick a font")
  if (ok != 0.0) {
    let s = flux_qt_font_size()
    flux_qt_msg_box("Font", "Picked")
  } else {
    flux_qt_msg_box("Font", "Cancelled")
  }
  0.0
}

def main() {
  flux_set_var(K_WIN(), flux_qt_create_mainwindow("Dialogs Demo"))
  flux_qt_set_window_size(flux_get_var(K_WIN()), 380, 200)

  let w = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(w, lay)
  flux_qt_set_central_widget(flux_get_var(K_WIN()), w)

  flux_qt_layout_add_widget(lay, flux_qt_create_label("Choose a color or font:"))

  let cb = flux_qt_create_button("Pick Color")
  flux_qt_layout_add_widget(lay, cb)

  let fb = flux_qt_create_button("Pick Font")
  flux_qt_layout_add_widget(lay, fb)

  flux_qt_on_click_by_name(cb, "on_color")
  flux_qt_on_click_by_name(fb, "on_font")

  0.0
}

main()