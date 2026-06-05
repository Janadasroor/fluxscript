# Minimal placeholder demo — uses persistent window, no blink on hot reload
def main() {
  let win = flux_qt_get_window()
  flux_qt_set_window_title(win, "Placeholder")

  flux_qt_set_window_size(win, 300, 200)

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)

  flux_qt_layout_add_widget(lay, flux_qt_create_label("Smooth hot reload works!"))
  flux_qt_layout_add_widget(lay, flux_qt_create_label("Second line added"))

  0.0
}
main()
