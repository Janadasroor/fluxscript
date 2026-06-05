# FluxScript Infra Demo
# Demonstrates: QShortcut, QSystemTrayIcon

# ── Global keys ──────────────────────────────────────────────────────────────

def K_WIN()  1.0
def K_TRAY() 2.0

# ── Callbacks ────────────────────────────────────────────────────────────────

def on_shortcut_hide() {
  flux_qt_set_visible(flux_get_var(K_WIN()), 0.0)
  flux_qt_tray_show_message(flux_get_var(K_TRAY()),
    "FluxScript Demo", "Minimized to tray. Click tray icon to restore.", 2000)
  0.0
}

def on_shortcut_quit() {
  flux_qt_app_quit()
  0.0
}

def on_tray_show() {
  flux_qt_set_visible(flux_get_var(K_WIN()), 1.0)
  0.0
}

def on_tray_hide() {
  flux_qt_set_visible(flux_get_var(K_WIN()), 0.0)
  0.0
}

def on_tray_quit() {
  flux_qt_app_quit()
  0.0
}

def on_tray_click() {
  flux_qt_set_visible(flux_get_var(K_WIN()), 1.0)
  0.0
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  flux_qt_set_app_icon("flux.svg")
  flux_qt_app_set_quit_on_last_closed(0.0)

  let win = flux_qt_create_mainwindow("FluxScript Infra Demo")
  flux_set_var(K_WIN(), win)
  flux_qt_set_window_size(win, 500, 360)
  flux_qt_set_window_icon(win, "flux.svg")

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)

  let info = flux_qt_create_label(
    "Keyboard Shortcuts:\n  Ctrl+H   Hide to tray\n  Ctrl+Q   Quit\n\nTray Icon: right-click for menu\nClose window = hide (not quit)")
  flux_qt_layout_add_widget(lay, info)

  # ── Keyboard shortcuts ──
  flux_qt_create_shortcut(win, "Ctrl+H", "on_shortcut_hide")
  flux_qt_create_shortcut(win, "Ctrl+Q", "on_shortcut_quit")

  # ── System tray icon ──
  let tray = flux_qt_create_tray_icon("flux.svg", "FluxScript Demo")
  flux_set_var(K_TRAY(), tray)

  let tray_menu = flux_qt_create_menu("Tray")
  flux_qt_menu_add_action(tray_menu, "Show", "on_tray_show")
  flux_qt_menu_add_action(tray_menu, "Hide", "on_tray_hide")
  flux_qt_menu_add_action(tray_menu, "───", "")
  flux_qt_menu_add_action(tray_menu, "Quit", "on_tray_quit")

  flux_qt_tray_set_context_menu(tray, tray_menu)
  flux_qt_tray_set_visible(tray, 1.0)
  flux_qt_tray_on_activated(tray, "on_tray_click")

  0.0
}

main()
