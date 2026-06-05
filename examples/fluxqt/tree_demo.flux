# FluxScript Tree Demo — Modern Rewrite
# Demonstrates: enum, struct, impl, match, method calls

# ── Enums ────────────────────────────────────────────────────────────────────

enum FileType {
  Folder,
  Text,
  Sheet,
  Markdown,
  Image,
  Vector,
  App
}

# ── Structs ──────────────────────────────────────────────────────────────────

struct TreeRef {
  handle: Double
}

# ── Helpers ──────────────────────────────────────────────────────────────────

def type_label(t: FileType) -> Double {
  match t {
    FileType.Folder -> "Folder",
    FileType.Text -> "Text",
    FileType.Sheet -> "Sheet",
    FileType.Markdown -> "Markdown",
    FileType.Image -> "Image",
    FileType.Vector -> "Vector",
    FileType.App -> "App",
    default -> "?"
  }
}

def icon_path(t: FileType) -> Double {
  match t {
    FileType.Folder -> "gear.svg",
    FileType.Image -> "flux.svg",
    default -> ""
  }
}

# ── Impl (methods on TreeRef) ────────────────────────────────────────────────

impl TreeRef {
  def add_item(self, name: Double, kind: FileType, size: Double) -> Double {
    let h = flux_qt_tree_add_item(self.handle, name)
    flux_qt_tree_item_set_text(h, 1.0, type_label(kind))
    flux_qt_tree_item_set_text(h, 2.0, size)
    h
  }

  def add_child(self, parent: Double, name: Double, kind: FileType, size: Double) -> Double {
    let h = flux_qt_tree_add_child(parent, name)
    flux_qt_tree_item_set_text(h, 1.0, type_label(kind))
    flux_qt_tree_item_set_text(h, 2.0, size)
    h
  }
}

# ── Global keys ──────────────────────────────────────────────────────────────

def K_WIN()  1.0
def K_TREE() 2.0
def K_SEL()  3.0

# ── Callbacks ────────────────────────────────────────────────────────────────

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
  let tr = TreeRef { handle: flux_get_var(K_TREE()) }
  tr.add_item("New Item", FileType.Text, "-")
  0.0
}

def on_remove() {
  let cur = flux_qt_tree_current_item(flux_get_var(K_TREE()))
  if (cur != 0.0) {
    flux_qt_tree_remove_item(flux_get_var(K_TREE()), cur)
  }
  0.0
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  flux_set_var(K_WIN(), flux_qt_create_mainwindow("FluxScript TreeWidget (Modern)"))
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

  let tr = TreeRef { handle: flux_get_var(K_TREE()) }

  # ── Build tree hierarchy ──
  let docs = tr.add_item("Documents", FileType.Folder, "-")
  flux_qt_tree_item_set_icon(docs, 0.0, "star.svg")
  flux_qt_tree_item_set_tooltip(docs, 0.0, "My documents folder")
  flux_qt_tree_item_set_foreground(docs, 0.0, 0.0, 80.0, 180.0)

  let r1 = tr.add_child(docs, "report.txt", FileType.Text, "24 KB")

  let r2 = tr.add_child(docs, "budget.xlsx", FileType.Sheet, "156 KB")
  flux_qt_tree_item_set_checked(r2, 0.0, 1.0)

  let r3 = tr.add_child(docs, "notes.md", FileType.Markdown, "8 KB")

  let pics = tr.add_item("Pictures", FileType.Folder, "-")
  flux_qt_tree_item_set_icon(pics, 0.0, "gear.svg")
  flux_qt_tree_item_set_foreground(pics, 0.0, 0.0, 120.0, 60.0)

  let p1 = tr.add_child(pics, "photo.jpg", FileType.Image, "2.1 MB")

  let p2 = tr.add_child(pics, "screenshot.png", FileType.Image, "840 KB")
  flux_qt_tree_item_set_checked(p2, 0.0, 1.0)

  let p3 = tr.add_child(pics, "diagram.svg", FileType.Vector, "12 KB")
  flux_qt_tree_item_set_icon(p3, 0.0, "flux.svg")

  let apps = tr.add_item("Applications", FileType.Folder, "-")
  flux_qt_tree_item_set_foreground(apps, 0.0, 160.0, 60.0, 0.0)

  let a1 = tr.add_child(apps, "Text Editor", FileType.App, "4.2 MB")
  flux_qt_tree_item_set_background(a1, 0.0, 230.0, 240.0, 255.0)

  let a2 = tr.add_child(apps, "Image Viewer", FileType.App, "3.8 MB")

  let a3 = tr.add_child(apps, "Terminal", FileType.App, "1.1 MB")
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
