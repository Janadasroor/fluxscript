# FluxScript Dock Demo — Modern Rewrite
# Demonstrates: enum, struct, impl, match, method calls

# ── Enums ────────────────────────────────────────────────────────────────────

enum DockId {
  Toolbox,
  Files,
  Output,
  Log
}

# ── Structs ──────────────────────────────────────────────────────────────────

struct DockLayout {
  toolbox: Double,
  files: Double,
  output: Double,
  log: Double
}

# ── Impl (methods on DockLayout) ─────────────────────────────────────────────

impl DockLayout {
  def handle(self, id: DockId) -> Double {
    match id {
      DockId.Toolbox -> self.toolbox,
      DockId.Files -> self.files,
      DockId.Output -> self.output,
      DockId.Log -> self.log,
      default -> 0.0
    }
  }

  def toggle(self, id: DockId) -> Double {
    let h = self.handle(id)
    let v = flux_qt_dock_is_visible(h)
    flux_qt_dock_set_visible(h, 1.0 - v)
    0.0
  }

}

# ── Global state keys ────────────────────────────────────────────────────────

def K_TB() 1.0
def K_FI() 2.0
def K_OU() 3.0
def K_LO() 4.0

# ── Callbacks (registered with Qt signals) ──────────────────────────────────

def on_toggle_toolbox() {
  let lay = DockLayout {
    toolbox: flux_get_var(K_TB()),
    files: flux_get_var(K_FI()),
    output: flux_get_var(K_OU()),
    log: flux_get_var(K_LO())
  }
  lay.toggle(DockId.Toolbox)
  0.0
}

def on_toggle_files() {
  let lay = DockLayout {
    toolbox: flux_get_var(K_TB()),
    files: flux_get_var(K_FI()),
    output: flux_get_var(K_OU()),
    log: flux_get_var(K_LO())
  }
  lay.toggle(DockId.Files)
  0.0
}

def on_toggle_output() {
  let lay = DockLayout {
    toolbox: flux_get_var(K_TB()),
    files: flux_get_var(K_FI()),
    output: flux_get_var(K_OU()),
    log: flux_get_var(K_LO())
  }
  lay.toggle(DockId.Output)
  0.0
}

def on_toggle_log() {
  let lay = DockLayout {
    toolbox: flux_get_var(K_TB()),
    files: flux_get_var(K_FI()),
    output: flux_get_var(K_OU()),
    log: flux_get_var(K_LO())
  }
  lay.toggle(DockId.Log)
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  let win = flux_qt_create_mainwindow("FluxScript Dock Demo (Modern)")
  flux_qt_set_window_size(win, 800, 500)

  # ── Central editor ──
  let editor = flux_qt_create_textedit()
  flux_qt_set_text(editor,
    "// FluxScript Editor — Modern Dock Demo\n// Uses: enum, struct, impl, match, method calls\n")
  flux_qt_set_central_widget(win, editor)

  # ── Dock: Toolbox (left) ──
  flux_set_var(K_TB(), flux_qt_create_dockwidget("Toolbox"))
  flux_qt_dock_set_features(flux_get_var(K_TB()), 7.0)
  let tools = flux_qt_create_widget()
  let tl = flux_qt_create_layout("vbox")
  flux_qt_set_layout(tools, tl)
  flux_qt_layout_add_widget(tl, flux_qt_create_button("New"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Open"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Save"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Run"))
  flux_qt_dock_set_widget(flux_get_var(K_TB()), tools)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_TB()), 0.0)

  # ── Dock: File Explorer (right) ──
  flux_set_var(K_FI(), flux_qt_create_dockwidget("Files"))
  flux_qt_dock_set_features(flux_get_var(K_FI()), 7.0)
  let tree = flux_qt_create_treewidget()
  flux_qt_tree_set_column_count(tree, 1.0)
  let src = flux_qt_tree_add_item(tree, "src")
  flux_qt_tree_add_child(src, "main.flux")
  flux_qt_tree_add_child(src, "utils.flux")
  let lib = flux_qt_tree_add_item(tree, "lib")
  flux_qt_tree_add_child(lib, "core.flux")
  flux_qt_tree_add_child(lib, "io.flux")
  flux_qt_tree_item_set_expanded(src, 1.0)
  flux_qt_tree_item_set_expanded(lib, 1.0)
  flux_qt_dock_set_widget(flux_get_var(K_FI()), tree)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_FI()), 1.0)

  # ── Dock: Output (bottom) ──
  flux_set_var(K_OU(), flux_qt_create_dockwidget("Output"))
  flux_qt_dock_set_features(flux_get_var(K_OU()), 7.0)
  let out_text = flux_qt_create_textedit()
  flux_qt_set_text(out_text, "> Build ready\n")
  flux_qt_dock_set_widget(flux_get_var(K_OU()), out_text)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_OU()), 3.0)

  # ── Dock: Log (tabified with Output) ──
  flux_set_var(K_LO(), flux_qt_create_dockwidget("Log"))
  flux_qt_dock_set_features(flux_get_var(K_LO()), 7.0)
  let log_text = flux_qt_create_textedit()
  flux_qt_set_text(log_text, "[2026-06-03 10:00] Started\n")
  flux_qt_dock_set_widget(flux_get_var(K_LO()), log_text)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_LO()), 3.0)
  flux_qt_mainwindow_tabify_docks(win,
    flux_get_var(K_OU()), flux_get_var(K_LO()))

  # ── Menu ──
  let mb = flux_qt_get_menubar(win)
  let view = flux_qt_menu_add_menu(mb, "&View")
  flux_qt_menu_add_action(view, "Toggle &Toolbox", "on_toggle_toolbox")
  flux_qt_menu_add_action(view, "Toggle &Files", "on_toggle_files")
  flux_qt_menu_add_action(view, "Toggle &Output", "on_toggle_output")
  flux_qt_menu_add_action(view, "Toggle &Log", "on_toggle_log")
  let sep = flux_qt_menu_add_menu(mb, "&File")
  flux_qt_menu_add_action(sep, "E&xit", "on_quit")

  0.0
}

main()
