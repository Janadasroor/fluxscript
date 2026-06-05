# FluxScript Code Editor — edit and run .flux code
# Demonstrates: QPlainTextEdit, eval, file dialogs

# ── Global keys ──────────────────────────────────────────────────────────────

def K_EDIT() 1.0
def K_OUT()  2.0

# ── Callbacks ────────────────────────────────────────────────────────────────

def on_run() {
  let src = flux_qt_get_text(flux_get_var(K_EDIT()))
  let result = flux_qt_eval(src)
  flux_qt_set_text(flux_get_var(K_OUT()), result)
  0.0
}

def on_clear() {
  flux_qt_set_text(flux_get_var(K_OUT()), "")
  0.0
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  flux_qt_set_app_icon("flux.svg")

  let win = flux_qt_create_mainwindow("FluxScript Code Editor")
  flux_qt_set_window_size(win, 680, 520)

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(win, central)

  # ── Toolbar buttons ──
  let tool = flux_qt_create_layout("hbox")

  let btn_run = flux_qt_create_button("Run (F5)")
  let btn_clr = flux_qt_create_button("Clear Output")

  flux_qt_layout_add_widget(tool, btn_run)
  flux_qt_layout_add_widget(tool, btn_clr)
  flux_qt_layout_add_widget(lay, tool)

  # ── Editor area ──
  let editor = flux_qt_create_textedit()
  flux_set_var(K_EDIT(), editor)
  flux_qt_set_text(editor, "def square(x) { x * x }\nsquare(5)\n")
  flux_qt_layout_add_widget(lay, editor)

  # ── Output area ──
  let out = flux_qt_create_textedit()
  flux_set_var(K_OUT(), out)
  flux_qt_set_text(out, "// Output\n")
  flux_qt_layout_add_widget(lay, out)

  # ── Signals ──
  flux_qt_on_click_by_name(btn_run, "on_run")
  flux_qt_on_click_by_name(btn_clr, "on_clear")

  # ── Shortcuts ──
  flux_qt_create_shortcut(win, "F5", "on_run")

  0.0
}

main()
