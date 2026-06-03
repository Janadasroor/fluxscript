# FluxScript Dock Demo — QDockWidget panels

def K_TOOLS()  1.0
def K_FILES()  2.0
def K_OUT()    3.0
def K_LOG()    4.0

def on_toggle_tools() {
  let v = flux_qt_dock_is_visible(flux_get_var(K_TOOLS()))
  flux_qt_dock_set_visible(flux_get_var(K_TOOLS()), 1.0 - v)
  0.0
}

def on_toggle_files() {
  let v = flux_qt_dock_is_visible(flux_get_var(K_FILES()))
  flux_qt_dock_set_visible(flux_get_var(K_FILES()), 1.0 - v)
  0.0
}

def on_toggle_output() {
  let v = flux_qt_dock_is_visible(flux_get_var(K_OUT()))
  flux_qt_dock_set_visible(flux_get_var(K_OUT()), 1.0 - v)
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

def main() {
  let win = flux_qt_create_mainwindow("FluxScript Dock Demo")
  flux_qt_set_window_size(win, 800, 500)

  # ── Central editor ──
  let editor = flux_qt_create_textedit()
  flux_qt_set_text(editor, "// FluxScript Editor\n\n")
  flux_qt_set_central_widget(win, editor)

  # ── Dock: Toolbox (left) ──
  flux_set_var(K_TOOLS(), flux_qt_create_dockwidget("Toolbox"))
  flux_qt_dock_set_features(flux_get_var(K_TOOLS()), 7.0)
  let tools = flux_qt_create_widget()
  let tl = flux_qt_create_layout("vbox")
  flux_qt_set_layout(tools, tl)
  flux_qt_layout_add_widget(tl, flux_qt_create_button("New"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Open"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Save"))
  flux_qt_layout_add_widget(tl, flux_qt_create_button("Run"))
  flux_qt_dock_set_widget(flux_get_var(K_TOOLS()), tools)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_TOOLS()), 0.0)

  # ── Dock: File Explorer (right) ──
  flux_set_var(K_FILES(), flux_qt_create_dockwidget("Files"))
  flux_qt_dock_set_features(flux_get_var(K_FILES()), 7.0)
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
  flux_qt_dock_set_widget(flux_get_var(K_FILES()), tree)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_FILES()), 1.0)

  # ── Dock: Output (bottom) ──
  flux_set_var(K_OUT(), flux_qt_create_dockwidget("Output"))
  flux_qt_dock_set_features(flux_get_var(K_OUT()), 7.0)
  let out_text = flux_qt_create_textedit()
  flux_qt_set_text(out_text, "> Build ready\n")
  flux_qt_dock_set_widget(flux_get_var(K_OUT()), out_text)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_OUT()), 3.0)

  # ── Dock: Log (tabified with Output) ──
  flux_set_var(K_LOG(), flux_qt_create_dockwidget("Log"))
  flux_qt_dock_set_features(flux_get_var(K_LOG()), 7.0)
  let log_text = flux_qt_create_textedit()
  flux_qt_set_text(log_text, "[2026-06-03 10:00] Started\n")
  flux_qt_dock_set_widget(flux_get_var(K_LOG()), log_text)
  flux_qt_mainwindow_add_dock(win, flux_get_var(K_LOG()), 3.0)
  flux_qt_mainwindow_tabify_docks(win, flux_get_var(K_OUT()), flux_get_var(K_LOG()))

  # ── Menu ──
  let mb = flux_qt_get_menubar(win)
  let view = flux_qt_menu_add_menu(mb, "&View")
  flux_qt_menu_add_action(view, "Toggle &Toolbox", "on_toggle_tools")
  flux_qt_menu_add_action(view, "Toggle &Files", "on_toggle_files")
  flux_qt_menu_add_action(view, "Toggle &Output", "on_toggle_output")
  let sep = flux_qt_menu_add_menu(mb, "&File")
  flux_qt_menu_add_action(sep, "E&xit", "on_quit")

  0.0
}

main()