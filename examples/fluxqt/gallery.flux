# FluxScript Gallery — SVG, icons, toolbar, window icon showcase

def K_WIN()      1.0
def K_TOOLBAR()  2.0
def K_SPLIT()    3.0
def K_LIST()     4.0
def K_SVG()      5.0

def on_pick_star() {
  flux_qt_svgwidget_load(flux_get_var(K_SVG()), "star.svg")
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Star")
  0.0
}

def on_pick_gear() {
  flux_qt_svgwidget_load(flux_get_var(K_SVG()), "gear.svg")
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Gear")
  0.0
}

def on_pick_logo() {
  flux_qt_svgwidget_load(flux_get_var(K_SVG()), "flux.svg")
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "FluxScript logo")
  0.0
}

def on_quit() {
  flux_qt_app_quit()
  0.0
}

def on_nav() {
  let row = flux_qt_get_property(flux_get_var(K_LIST()), "currentRow")
  if (row == 0.0) { flux_qt_svgwidget_load(flux_get_var(K_SVG()), "star.svg") }
  elif (row == 1.0) { flux_qt_svgwidget_load(flux_get_var(K_SVG()), "gear.svg") }
  else { flux_qt_svgwidget_load(flux_get_var(K_SVG()), "flux.svg") }
  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Navigated")
  0.0
}

def main() {
  # ── Main window ──
  flux_set_var(K_WIN(), flux_qt_create_mainwindow("FluxScript Gallery"))
  flux_qt_set_window_size(flux_get_var(K_WIN()), 640, 480)
  flux_qt_set_window_icon(flux_get_var(K_WIN()), "flux.svg")

  # ── Menu ──
  let mb = flux_qt_get_menubar(flux_get_var(K_WIN()))
  flux_qt_menu_add_action(flux_qt_menu_add_menu(mb, "&File"), "E&xit", "on_quit")
  flux_qt_menu_add_action(flux_qt_menu_add_menu(mb, "&Help"), "&About", "on_quit")

  # ── Toolbar ──
  flux_set_var(K_TOOLBAR(), flux_qt_create_toolbar(flux_get_var(K_WIN()), "Main"))
  let a_s = flux_qt_toolbar_add_action(flux_get_var(K_TOOLBAR()), "Star", "on_pick_star")
  flux_qt_set_action_icon(a_s, "star.svg")
  let a_g = flux_qt_toolbar_add_action(flux_get_var(K_TOOLBAR()), "Gear", "on_pick_gear")
  flux_qt_set_action_icon(a_g, "gear.svg")
  flux_qt_toolbar_add_separator(flux_get_var(K_TOOLBAR()))
  let a_l = flux_qt_toolbar_add_action(flux_get_var(K_TOOLBAR()), "Logo", "on_pick_logo")
  flux_qt_set_action_icon(a_l, "flux.svg")

  flux_qt_set_statusbar_text(flux_get_var(K_WIN()), "Ready")

  # ── Splitter: list + SVG viewer ──
  flux_set_var(K_SPLIT(), flux_qt_create_splitter(0.0))
  flux_qt_set_central_widget(flux_get_var(K_WIN()), flux_get_var(K_SPLIT()))

  let lg = flux_qt_create_groupbox("Icons")
  let ll = flux_qt_create_layout("vbox")
  flux_qt_groupbox_set_layout(lg, ll)
  flux_set_var(K_LIST(), flux_qt_create_listwidget())
  flux_qt_list_add_item(flux_get_var(K_LIST()), "Star")
  flux_qt_list_add_item(flux_get_var(K_LIST()), "Gear")
  flux_qt_list_add_item(flux_get_var(K_LIST()), "Logo")
  flux_qt_layout_add_widget(ll, flux_get_var(K_LIST()))
  flux_qt_splitter_add(flux_get_var(K_SPLIT()), lg)

  let sg = flux_qt_create_groupbox("Preview")
  let sl = flux_qt_create_layout("vbox")
  flux_qt_groupbox_set_layout(sg, sl)
  flux_set_var(K_SVG(), flux_qt_create_svgwidget("star.svg"))
  flux_qt_svgwidget_set_size(flux_get_var(K_SVG()), 280.0, 280.0)
  flux_qt_layout_add_widget(sl, flux_get_var(K_SVG()))
  flux_qt_splitter_add(flux_get_var(K_SPLIT()), sg)

  # ── Init ──
  flux_qt_list_set_current_row(flux_get_var(K_LIST()), 0.0)

  # ── Signals ──
  flux_qt_on_current_row_changed_by_name(flux_get_var(K_LIST()), "on_nav")
  0.0
}

main()