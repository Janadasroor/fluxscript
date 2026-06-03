# FluxScript Tree Demo — QTreeWidget with hierarchy, columns, icons, colors

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

def on_add() {
  flux_qt_tree_add_item(flux_get_var(K_TREE()), "New Item")
  0.0
}

def on_remove() {
  let cur = flux_qt_tree_current_item(flux_get_var(K_TREE()))
  if (cur != 0.0) {
    flux_qt_tree_remove_item(flux_get_var(K_TREE()), cur)
  }
  0.0
}

def on_count() {
  let n = flux_qt_tree_top_level_count(flux_get_var(K_TREE()))
  flux_qt_set_text(flux_get_var(K_SEL()), "Items: ")
  0.0
}

def main() {
  flux_set_var(K_WIN(), flux_qt_create_mainwindow("FluxScript TreeWidget"))
  flux_qt_set_window_size(flux_get_var(K_WIN()), 560, 480)

  let central = flux_qt_create_widget()
  let lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, lay)
  flux_qt_set_central_widget(flux_get_var(K_WIN()), central)

  # ── Tree with 3 columns ──
  flux_set_var(K_TREE(), flux_qt_create_treewidget())
  flux_qt_tree_set_column_count(flux_get_var(K_TREE()), 3.0)
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 0.0, "Name")
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 1.0, "Type")
  flux_qt_tree_set_header(flux_get_var(K_TREE()), 2.0, "Size")
  flux_qt_tree_set_column_width(flux_get_var(K_TREE()), 0.0, 180.0)
  flux_qt_tree_set_column_width(flux_get_var(K_TREE()), 1.0, 100.0)
  flux_qt_tree_set_column_width(flux_get_var(K_TREE()), 2.0, 80.0)
  flux_qt_tree_set_alternating(flux_get_var(K_TREE()), 1.0)
  flux_qt_tree_set_animated(flux_get_var(K_TREE()), 1.0)
  flux_qt_tree_set_indentation(flux_get_var(K_TREE()), 20.0)

  # ── Build tree hierarchy ──
  let docs = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Documents")
  flux_qt_tree_item_set_text(docs, 1.0, "Folder")
  flux_qt_tree_item_set_icon(docs, 0.0, "star.svg")
  flux_qt_tree_item_set_tooltip(docs, 0.0, "My documents folder")
  flux_qt_tree_item_set_foreground(docs, 0.0, 0.0, 80.0, 180.0)

  let r1 = flux_qt_tree_add_child(docs, "report.txt")
  flux_qt_tree_item_set_text(r1, 1.0, "Text")
  flux_qt_tree_item_set_text(r1, 2.0, "24 KB")

  let r2 = flux_qt_tree_add_child(docs, "budget.xlsx")
  flux_qt_tree_item_set_text(r2, 1.0, "Sheet")
  flux_qt_tree_item_set_text(r2, 2.0, "156 KB")
  flux_qt_tree_item_set_checked(r2, 0.0, 1.0)

  let r3 = flux_qt_tree_add_child(docs, "notes.md")
  flux_qt_tree_item_set_text(r3, 1.0, "Markdown")
  flux_qt_tree_item_set_text(r3, 2.0, "8 KB")

  let pics = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Pictures")
  flux_qt_tree_item_set_text(pics, 1.0, "Folder")
  flux_qt_tree_item_set_icon(pics, 0.0, "gear.svg")
  flux_qt_tree_item_set_foreground(pics, 0.0, 0.0, 120.0, 60.0)

  let p1 = flux_qt_tree_add_child(pics, "photo.jpg")
  flux_qt_tree_item_set_text(p1, 1.0, "Image")
  flux_qt_tree_item_set_text(p1, 2.0, "2.1 MB")

  let p2 = flux_qt_tree_add_child(pics, "screenshot.png")
  flux_qt_tree_item_set_text(p2, 1.0, "Image")
  flux_qt_tree_item_set_text(p2, 2.0, "840 KB")
  flux_qt_tree_item_set_checked(p2, 0.0, 1.0)

  let p3 = flux_qt_tree_add_child(pics, "diagram.svg")
  flux_qt_tree_item_set_text(p3, 1.0, "Vector")
  flux_qt_tree_item_set_text(p3, 2.0, "12 KB")
  flux_qt_tree_item_set_icon(p3, 0.0, "flux.svg")

  let apps = flux_qt_tree_add_item(flux_get_var(K_TREE()), "Applications")
  flux_qt_tree_item_set_text(apps, 1.0, "Folder")
  flux_qt_tree_item_set_foreground(apps, 0.0, 160.0, 60.0, 0.0)

  let a1 = flux_qt_tree_add_child(apps, "Text Editor")
  flux_qt_tree_item_set_text(a1, 1.0, "App")
  flux_qt_tree_item_set_text(a1, 2.0, "4.2 MB")
  flux_qt_tree_item_set_background(a1, 0.0, 230.0, 240.0, 255.0)

  let a2 = flux_qt_tree_add_child(apps, "Image Viewer")
  flux_qt_tree_item_set_text(a2, 1.0, "App")
  flux_qt_tree_item_set_text(a2, 2.0, "3.8 MB")

  let a3 = flux_qt_tree_add_child(apps, "Terminal")
  flux_qt_tree_item_set_text(a3, 1.0, "App")
  flux_qt_tree_item_set_text(a3, 2.0, "1.1 MB")
  flux_qt_tree_item_set_background(a3, 0.0, 240.0, 240.0, 240.0)

  # Expand all
  flux_qt_tree_item_set_expanded(docs, 1.0)
  flux_qt_tree_item_set_expanded(pics, 1.0)
  flux_qt_tree_item_set_expanded(apps, 1.0)

  flux_qt_layout_add_widget(lay, flux_get_var(K_TREE()))

  # ── Selection info ──
  flux_set_var(K_SEL(), flux_qt_create_label("Select an item"))
  flux_qt_layout_add_widget(lay, flux_get_var(K_SEL()))

  # ── Buttons ──
  let btn_lay = flux_qt_create_layout("hbox")
  let add_btn = flux_qt_create_button("Add Item")
  flux_qt_layout_add_widget(btn_lay, add_btn)
  let rm_btn = flux_qt_create_button("Remove")
  flux_qt_layout_add_widget(btn_lay, rm_btn)
  let clr_btn = flux_qt_create_button("Clear")
  flux_qt_layout_add_widget(btn_lay, clr_btn)
  flux_qt_layout_add_widget(lay, btn_lay)

  # ── Signals ──
  flux_qt_tree_on_current_item_changed(flux_get_var(K_TREE()), "on_item_changed")
  flux_qt_on_click_by_name(add_btn, "on_add")
  flux_qt_on_click_by_name(rm_btn, "on_remove")
  flux_qt_on_click_by_name(clr_btn, "on_clear")

  0.0
}

main()