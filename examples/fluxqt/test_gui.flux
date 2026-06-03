# Simple FluxQt test - creates a window with a button
# No enums/structs to avoid known pattern-matching issues

def on_clicked()
  flux_qt_msg_box("Hello", "Button was clicked from FluxScript!")
  0.0

let win = flux_qt_create_window("FluxScript Qt Test")
let btn = flux_qt_create_button("Click me!")
flux_qt_on_click_by_name(btn, "on_clicked")
flux_qt_add_widget(win, btn)
