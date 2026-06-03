# FluxScript Drop Demo — drag-and-drop .flux files
# Demonstrates: file drop event, QShortcut

# ── Global keys ──────────────────────────────────────────────────────────────

def K_WIN()   1.0
def K_LABEL() 2.0
def K_EDIT()  3.0

# ── Callbacks ────────────────────────────────────────────────────────────────

def on_file_dropped() {
  let path = flux_qt_last_dropped_file()
  flux_qt_set_text(flux_get_var(K_LABEL()), path)
  flux_qt_set_text(flux_get_var(K_EDIT()), "Reloading...")
  0.0
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  flux_qt_set_app_icon("flux.svg")

  let win = flux_qt_create_mainwindow("FluxScript Drop Demo")
  flux_set_var(K_WIN(), win)
  flux_qt_set_window_size(win, 520, 380)
  flux_qt_set_window_icon(win, "flux.svg")

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)

  let info = flux_qt_create_label("Drag and drop a .flux file here")
  flux_set_var(K_LABEL(), info)
  flux_qt_layout_add_widget(lay, info)

  let editor = flux_qt_create_textedit()
  flux_set_var(K_EDIT(), editor)
  flux_qt_set_text(editor, "// Drop a .flux file to load it\n")
  flux_qt_layout_add_widget(lay, editor)

  # Enable drops on central widget
  flux_qt_enable_drops(central, "on_file_dropped")

  # Shortcut info
  let hint = flux_qt_create_label("💡 Drop any .flux file anywhere in the window")
  flux_qt_layout_add_widget(lay, hint)

  0.0
}

main()
