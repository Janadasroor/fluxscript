# FluxScript Tree Demo — QTreeWidget with columns, icons, checkboxes, hierarchy

def K_WIN()  1.0
def K_TREE() 2.0
def K_SEL()  3.0

def on_item_changed() {
  let cur = flux_qt_tree_current_item(flux_get_var(K_TREE()))
  if (cur == 0.0) {
    flux_qt_set_text(flux_get_var(K_SEL()), "(none)")
  } else {
    flux_qt_set_text(flux_get_var(K_SEL()), flux_qt_tree_item_text(cur, 0.0))
  }
  0.0
}

def on_clear() {
  flux_qt_tree_clear(flux_get_var(K_TREE()))
  flux_qt_set_text(flux_get_var(K_SEL()), "Cleared")
  0.0
}

def on_expand_all() {
  let i = 0.0
  while (i < 100.0) {
    let ci = flux_qt_tree_current_item(flux_get_var(K_TREE()))
    if (ci == 0.0) { i = 100.0 } else {
      flux_qt_tree_item_set_expanded(ci, 1.0)
      flux_qt_set_text(flux_get_var(K_SEL()), "Expanded")
      i = i + 1.0
    }
  }
  0.0
}

def main() {
  flux_set_var(K_WIN(), flux_qt_create_mainwindow("FluxScript TreeWidget"))
  flux_qt_set_window_size(flux_get_var(K_WIN()), 520, 420)

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(flux_get_var(K_WIN()), central)

  # ── Tree ──
  flux_set_var(K_TREE(), flux_qt_create_treewidget())
  flux_qt_tree_set_column_count(flux_get_var(K_TREE()), 3.0)
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 0.0, "Item")
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 1.0, "Type")
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 2.0, "Status")

  # Root items with children
  let docs = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Documents")
  flux_qt_tree_item_set_text(docs, 1.0, "Folder")
  flux_qt_tree_item_set_icon(docs, 0.0, "star.svg")
  let r1 = flux_qt_tree_add_child(docs, "report.txt")
  flux_qt_tree_item_set_text(r1, 1.0, "File")
  flux_qt_tree_item_set_text(r1, 2.0, "Draft")
  let r2 = flux_qt_tree_add_child(docs, "budget.xlsx")
  flux_qt_tree_item_set_text(r2, 1.0, "File")
  flux_qt_tree_item_set_text(r2, 2.0, "Final")
  flux_qt_tree_item_set_checked(r2, 0.0, 1.0)

  let pics = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Pictures")
  flux_qt_tree_item_set_text(pics, 1.0, "Folder")
  flux_qt_tree_item_set_icon(pics, 0.0, "gear.svg")
  let p1 = flux_qt_tree_add_child(pics, "photo.jpg")
  flux_qt_tree_item_set_text(p1, 1.0, "Image")
  let p2 = flux_qt_tree_add_child(pics, "screenshot.png")
  flux_qt_tree_item_set_text(p2, 1.0, "Image")
  flux_qt_tree_item_set_checked(p2, 0.0, 1.0)
  let p3 = flux_qt_tree_add_child(pics, "diagram.svg")
  flux_qt_tree_item_set_text(p3, 1.0, "Vector")
  flux_qt_tree_item_set_icon(p3, 0.0, "flux.svg")

  let apps = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Applications")
  flux_qt_tree_item_set_text(apps, 1.0, "Folder")
  let a1 = flux_qt_tree_add_child(apps, "text_editor")
  flux_qt_tree_item_set_text(a1, 1.0, "App")
  flux_qt_tree_item_set_text(a1, 2.0, "Installed")
  let a2 = flux_qt_tree_add_child(apps, "image_viewer")
  flux_qt_tree_item_set_text(a2, 1.0, "App")
  flux_qt_tree_item_set_text(a2, 2.0, "Installed")

  # Expand all
  flux_qt_tree_item_set_expanded(docs, 1.0)
  flux_qt_tree_item_set_expanded(pics, 1.0)

  flux_qt_layout_add_widget(lay, flux_get_var(K_TREE()))

  # ── Controls ──
  flux_set_var(K_SEL(), flux_qt_create_label("(none)"))
  flux_qt_layout_add_widget(lay, flux_get_var(K_SEL()))

  let btn_lay = flux_qt_create_layout("hbox")
  let cb = flux_qt_create_button("Clear")
  flux_qt_layout_add_widget(btn_lay, cb)
  let eb = flux_qt_create_button("Expand All")
  flux_qt_layout_add_widget(btn_lay, eb)
  flux_qt_layout_add_widget(lay, btn_lay)

  # ── Signals ──
  flux_qt_tree_on_current_item_changed(flux_get_var(K_TREE()), "on_item_changed")
  flux_qt_on_click_by_name(cb, "on_clear")
  flux_qt_on_click_by_name(eb, "on_expand_all")

  flux_qt_tree_set_current_item(flux_get_var(K_TREE()), docs)
  0.0
}

main()