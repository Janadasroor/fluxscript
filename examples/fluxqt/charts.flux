# FluxScript Charts Demo — line, bar, and pie charts side by side

def K_TAB()     1.0

def main() {
  let win = flux_qt_create_mainwindow("FluxScript Qt Charts")
  flux_qt_set_window_size(win, 780, 540)

  let tab = flux_qt_create_tabwidget()
  flux_qt_set_central_widget(win, tab)

  # ═══════════════════════════════════════════════════════════
  # Line chart
  # ═══════════════════════════════════════════════════════════
  let line_tab = flux_qt_create_widget()
  let line_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(line_tab, line_lay)

  let line_cv = flux_qt_create_chartview()
  flux_qt_chart_set_title(line_cv, "Sine Wave")
  flux_qt_chart_set_animation(line_cv, 1.0)
  flux_qt_chart_legend_show(line_cv, 0.0)

  let sine = flux_qt_chart_add_line_series(line_cv, "sin(x)")
  flux_qt_line_series_set_pen(sine, 0.0, 180.0, 100.0, 2.0)

  let i = -6.28
  while (i <= 6.28) {
    flux_qt_line_series_append(sine, i, sin(i) * 50.0 + 50.0)
    i = i + 0.05
  }

  flux_qt_chart_create_default_axes(line_cv)
  flux_qt_layout_add_widget(line_lay, line_cv)

  flux_qt_tab_add(tab, line_tab, "Line")

  # ═══════════════════════════════════════════════════════════
  # Bar chart
  # ═══════════════════════════════════════════════════════════
  let bar_tab = flux_qt_create_widget()
  let bar_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(bar_tab, bar_lay)

  let bar_cv = flux_qt_create_chartview()
  flux_qt_chart_set_title(bar_cv, "Quarterly Sales")
  flux_qt_chart_set_animation(bar_cv, 1.0)

  let bars = flux_qt_chart_add_bar_series(bar_cv, "Sales")

  let q1 = flux_qt_create_bar_set("Q1")
  flux_qt_bar_set_append(q1, 120.0)
  flux_qt_bar_set_append(q1, 135.0)
  flux_qt_bar_set_append(q1, 100.0)
  flux_qt_bar_set_set_color(q1, 60.0, 160.0, 230.0)
  flux_qt_bar_series_append_set(bars, q1)

  let q2 = flux_qt_create_bar_set("Q2")
  flux_qt_bar_set_append(q2, 145.0)
  flux_qt_bar_set_append(q2, 160.0)
  flux_qt_bar_set_append(q2, 130.0)
  flux_qt_bar_set_set_color(q2, 230.0, 120.0, 50.0)
  flux_qt_bar_series_append_set(bars, q2)

  let q3 = flux_qt_create_bar_set("Q3")
  flux_qt_bar_set_append(q3, 90.0)
  flux_qt_bar_set_append(q3, 110.0)
  flux_qt_bar_set_append(q3, 95.0)
  flux_qt_bar_set_set_color(q3, 100.0, 200.0, 80.0)
  flux_qt_bar_series_append_set(bars, q3)

  flux_qt_chart_create_default_axes(bar_cv)
  flux_qt_layout_add_widget(bar_lay, bar_cv)

  flux_qt_tab_add(tab, bar_tab, "Bar")

  # ═══════════════════════════════════════════════════════════
  # Pie chart
  # ═══════════════════════════════════════════════════════════
  let pie_tab = flux_qt_create_widget()
  let pie_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(pie_tab, pie_lay)

  let pie_cv = flux_qt_create_chartview()
  flux_qt_chart_set_title(pie_cv, "Market Share")
  flux_qt_chart_set_animation(pie_cv, 1.0)
  flux_qt_chart_legend_show(pie_cv, 1.0)
  flux_qt_chart_legend_align(pie_cv, 1.0)

  let pie = flux_qt_chart_add_pie_series(pie_cv, "Share")
  flux_qt_pie_series_set_hole_size(pie, 0.35)

  let s1 = flux_qt_pie_series_append(pie, "Widgets", 35.0)
  flux_qt_pie_slice_set_color(s1, 60.0, 160.0, 230.0)
  flux_qt_pie_slice_set_label_visible(s1, 1.0)

  let s2 = flux_qt_pie_series_append(pie, "Gadgets", 28.0)
  flux_qt_pie_slice_set_color(s2, 230.0, 120.0, 50.0)
  flux_qt_pie_slice_set_label_visible(s2, 1.0)

  let s3 = flux_qt_pie_series_append(pie, "Doohickeys", 22.0)
  flux_qt_pie_slice_set_color(s3, 100.0, 200.0, 80.0)
  flux_qt_pie_slice_set_label_visible(s3, 1.0)
  flux_qt_pie_slice_set_exploded(s3, 1.0)

  let s4 = flux_qt_pie_series_append(pie, "Thingamajigs", 15.0)
  flux_qt_pie_slice_set_color(s4, 200.0, 130.0, 200.0)
  flux_qt_pie_slice_set_label_visible(s4, 1.0)

  flux_qt_layout_add_widget(pie_lay, pie_cv)

  flux_qt_tab_add(tab, pie_tab, "Pie")

  # Set initial tab
  flux_qt_tab_set_current(tab, 0.0)
  0.0
}

main()