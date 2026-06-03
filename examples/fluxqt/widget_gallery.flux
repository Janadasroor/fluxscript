# FluxScript Widget Gallery
# Demonstrates: QCalendarWidget, QMdiArea, QProgressDialog

# ── Global keys ──────────────────────────────────────────────────────────────

def K_CAL()    1.0
def K_DATE()   2.0
def K_MDI()    3.0
def K_MDI_N()  4.0
def K_PROG()   5.0
def K_PROG_V() 6.0

# ── Callbacks ────────────────────────────────────────────────────────────────

def on_date_changed() {
  let d = flux_qt_calendar_selected_date(flux_get_var(K_CAL()))
  flux_qt_set_text(flux_get_var(K_DATE()), d)
  0.0
}

def on_new_mdi() {
  let n = flux_get_var(K_MDI_N())
  let ed = flux_qt_create_textedit()
  flux_qt_set_text(ed, "Document ")
  flux_qt_mdi_add_subwindow(flux_get_var(K_MDI()), ed)
  flux_set_var(K_MDI_N(), n + 1.0)
  0.0
}

def on_cascade() {
  flux_qt_mdi_cascade(flux_get_var(K_MDI()))
  0.0
}

def on_tile() {
  flux_qt_mdi_tile(flux_get_var(K_MDI()))
  0.0
}

def on_close_all() {
  flux_qt_mdi_close_all(flux_get_var(K_MDI()))
  0.0
}

def on_start_progress() {
  let dlg = flux_qt_create_progress_dialog("Working...", 1.0, 0.0, 100.0)
  flux_set_var(K_PROG(), dlg)
  flux_set_var(K_PROG_V(), 0.0)

  let tm = flux_qt_create_timer(80.0, "on_progress_tick")
  flux_qt_timer_start(tm)
  0.0
}

def on_progress_tick() {
  let v = flux_get_var(K_PROG_V())
  let dlg = flux_get_var(K_PROG())
  if (flux_qt_progress_was_canceled(dlg) != 0.0) {
    flux_qt_set_text(flux_get_var(K_DATE()), "Canceled")
    0.0
  } else {
    flux_qt_progress_set_value(dlg, v)
    if (v >= 100.0) {
      flux_qt_progress_set_label(dlg, "Done!")
      flux_qt_set_text(flux_get_var(K_DATE()), "Complete")
    } else {
      flux_set_var(K_PROG_V(), v + 5.0)
    }
    0.0
  }
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  flux_qt_set_app_icon("flux.svg")

  let win = flux_qt_create_mainwindow("FluxScript Widget Gallery (hot-reloaded)")
  flux_qt_set_window_size(win, 820, 580)
  flux_qt_set_stylesheet(win, "QMainWindow { background-color: #2b2b2b; } QTabWidget::pane { background-color: #3c3c3c; } QLabel { color: #e0e0e0; } QPushButton { background-color: #505050; color: #ffffff; border: 1px solid #606060; padding: 4px; } QPushButton:hover { background-color: #606060; } QCalendarWidget { background-color: #3c3c3c; color: #e0e0e0; }")

  let tabs = flux_qt_create_tabwidget()
  flux_qt_set_central_widget(win, tabs)

  # ── Tab 1: QCalendarWidget ──
  let cal_page = flux_qt_create_widget()
  let cal_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(cal_page, cal_lay)

  let cal = flux_qt_create_calendar(0.0)
  flux_set_var(K_CAL(), cal)
  flux_qt_layout_add_widget(cal_lay, cal)

  let date_label = flux_qt_create_label("Select a date")
  flux_set_var(K_DATE(), date_label)
  flux_qt_layout_add_widget(cal_lay, date_label)

  flux_qt_on_selection_changed_by_name(cal, "on_date_changed")

  flux_qt_tab_add(tabs, cal_page, "Calendar")

  # ── Tab 2: QMdiArea ──
  let mdi_page = flux_qt_create_widget()
  let mdi_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(mdi_page, mdi_lay)

  let mdi = flux_qt_create_mdiarea(0.0)
  flux_set_var(K_MDI(), mdi)
  flux_set_var(K_MDI_N(), 1.0)
  flux_qt_layout_add_widget(mdi_lay, mdi)

  let mdi_btns = flux_qt_create_layout("hbox")
  let btn_new = flux_qt_create_button("New Doc")
  let btn_cas = flux_qt_create_button("Cascade")
  let btn_til = flux_qt_create_button("Tile")
  let btn_cls = flux_qt_create_button("Close All")

  flux_qt_layout_add_widget(mdi_btns, btn_new)
  flux_qt_layout_add_widget(mdi_btns, btn_cas)
  flux_qt_layout_add_widget(mdi_btns, btn_til)
  flux_qt_layout_add_widget(mdi_btns, btn_cls)
  flux_qt_layout_add_widget(mdi_lay, mdi_btns)

  flux_qt_on_click_by_name(btn_new, "on_new_mdi")
  flux_qt_on_click_by_name(btn_cas, "on_cascade")
  flux_qt_on_click_by_name(btn_til, "on_tile")
  flux_qt_on_click_by_name(btn_cls, "on_close_all")

  flux_qt_tab_add(tabs, mdi_page, "MDI")

  # ── Tab 3: QProgressDialog ──
  let prog_page = flux_qt_create_widget()
  let prog_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(prog_page, prog_lay)

  let prog_info = flux_qt_create_label(
    "Click the button to start a simulated task\nwith a progress dialog and timer.")
  flux_qt_layout_add_widget(prog_lay, prog_info)

  let prog_btn = flux_qt_create_button("Start Progress")
  flux_qt_on_click_by_name(prog_btn, "on_start_progress")
  flux_qt_layout_add_widget(prog_lay, prog_btn)

  let spacer = flux_qt_create_widget()
  flux_qt_layout_add_widget(prog_lay, spacer)

  flux_qt_tab_add(tabs, prog_page, "Progress")

  0.0
}

main()
