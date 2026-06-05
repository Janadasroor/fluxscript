# FluxScript Charts Demo — all series types

def fill_sine(h) {
  let i = -6.28
  while (i <= 6.28) {
    flux_qt_xy_series_append(h, i, sin(i) * 50.0 + 50.0)
    i = i + 0.05
  }
  0.0
}

def fill_noisy(h) {
  let i = 0.0
  while (i <= 100.0) {
    flux_qt_xy_series_append(h, i, sin(i * 0.3) * 30.0 + 40.0)
    i = i + 1.0
  }
  0.0
}

def main() {
  let win = flux_qt_create_mainwindow("FluxScript Qt Charts")
  flux_qt_set_window_size(win, 840, 580)

  let tab = s o()
  flux_qt_set_central_widget(win, tab)

  # ── 1: Line vs Spline ──
  let t1 = flux_qt_create_widget()
  let l1 = flux_qt_create_layout("vbox")
  flux_qt_set_layout(t1, l1)

  let c1 = flux_qt_create_chartview()
  flux_qt_chart_set_title(c1, "Line vs Spline")
  flux_qt_chart_set_animation(c1, 1.0)
  flux_qt_chart_set_margins(c1, 5, 5, 5, 5)

  let raw = flux_qt_chart_add_line_series(c1, "Line")
  flux_qt_xy_series_set_pen(raw, 200.0, 60.0, 60.0, 2.0)
  fill_sine(raw)

  let smooth = flux_qt_chart_add_spline_series(c1, "Spline")
  flux_qt_xy_series_set_pen(smooth, 60.0, 160.0, 230.0, 2.5)
  fill_sine(smooth)

  flux_qt_chart_create_default_axes(c1)
  flux_qt_layout_add_widget(l1, c1)
  flux_qt_tab_add(tab, t1, "Line / Spline")

  # ── 2: Scatter + Area ──
  let t2 = flux_qt_create_widget()
  let l2 = flux_qt_create_layout("vbox")
  flux_qt_set_layout(t2, l2)

  let c2 = flux_qt_create_chartview()
  flux_qt_chart_set_title(c2, "Scatter + Area Fill")
  flux_qt_chart_set_animation(c2, 1.0)

  let scatter = flux_qt_chart_add_scatter_series(c2, "Points")
  flux_qt_scatter_series_set_marker_size(scatter, 8.0)
  fill_noisy(scatter)

  let upper = flux_qt_chart_add_area_series(c2, "Envelope")
  flux_qt_xy_series_set_pen(upper, 60.0, 180.0, 100.0, 0.0)
  fill_sine(upper)

  flux_qt_chart_create_default_axes(c2)
  flux_qt_layout_add_widget(l2, c2)
  flux_qt_tab_add(tab, t2, "Scatter / Area")

  # ── 3: Stacked Bar ──
  let t3 = flux_qt_create_widget()
  let l3 = flux_qt_create_layout("vbox")
  flux_qt_set_layout(t3, l3)

  let c3 = flux_qt_create_chartview()
  flux_qt_chart_set_title(c3, "Stacked Bar")
  flux_qt_chart_set_animation(c3, 1.0)

  let stacked = flux_qt_chart_add_stacked_bar_series(c3, "Products")

  let sa = flux_qt_create_bar_set("Alpha")
  flux_qt_bar_set_append(sa, 30.0)
  flux_qt_bar_set_append(sa, 45.0)
  flux_qt_bar_set_append(sa, 25.0)
  flux_qt_bar_set_append(sa, 50.0)
  flux_qt_bar_set_set_color(sa, 60.0, 160.0, 230.0)
  flux_qt_bar_series_append_set(stacked, sa)

  let sb = flux_qt_create_bar_set("Beta")
  flux_qt_bar_set_append(sb, 20.0)
  flux_qt_bar_set_append(sb, 30.0)
  flux_qt_bar_set_append(sb, 40.0)
  flux_qt_bar_set_append(sb, 25.0)
  flux_qt_bar_set_set_color(sb, 230.0, 120.0, 50.0)
  flux_qt_bar_series_append_set(stacked, sb)

  let sc = flux_qt_create_bar_set("Gamma")
  flux_qt_bar_set_append(sc, 15.0)
  flux_qt_bar_set_append(sc, 10.0)
  flux_qt_bar_set_append(sc, 20.0)
  flux_qt_bar_set_append(sc, 15.0)
  flux_qt_bar_set_set_color(sc, 100.0, 200.0, 80.0)
  flux_qt_bar_series_append_set(stacked, sc)

  flux_qt_chart_create_default_axes(c3)
  flux_qt_layout_add_widget(l3, c3)
  flux_qt_tab_add(tab, t3, "Stacked Bar")

  # ── 4: Horizontal Bar ──
  let t4 = flux_qt_create_widget()
  let l4 = flux_qt_create_layout("vbox")
  flux_qt_set_layout(t4, l4)

  let c4 = flux_qt_create_chartview()
  flux_qt_chart_set_title(c4, "Horizontal Bar")
  flux_qt_chart_set_background(c4, 245.0, 248.0, 250.0)
  flux_qt_chart_set_drop_shadow(c4, 0.0)

  let hbars = flux_qt_chart_add_hbar_series(c4, "Revenue")

  let h1 = flux_qt_create_bar_set("Q1")
  flux_qt_bar_set_append(h1, 85.0)
  flux_qt_bar_set_append(h1, 92.0)
  flux_qt_bar_set_set_color(h1, 60.0, 160.0, 230.0)
  flux_qt_bar_series_append_set(hbars, h1)

  let h2 = flux_qt_create_bar_set("Q2")
  flux_qt_bar_set_append(h2, 110.0)
  flux_qt_bar_set_append(h2, 105.0)
  flux_qt_bar_set_set_color(h2, 230.0, 120.0, 50.0)
  flux_qt_bar_series_append_set(hbars, h2)

  let h3 = flux_qt_create_bar_set("Q3")
  flux_qt_bar_set_append(h3, 75.0)
  flux_qt_bar_set_append(h3, 95.0)
  flux_qt_bar_set_set_color(h3, 100.0, 200.0, 80.0)
  flux_qt_bar_series_append_set(hbars, h3)

  flux_qt_chart_create_default_axes(c4)
  flux_qt_layout_add_widget(l4, c4)
  flux_qt_tab_add(tab, t4, "H-Bar")

  # ── 5: Pie (donut) ──
  let t5 = flux_qt_create_widget()
  let l5 = flux_qt_create_layout("vbox")
  flux_qt_set_layout(t5, l5)

  let c5 = flux_qt_create_chartview()
  flux_qt_chart_set_title(c5, "Donut")
  flux_qt_chart_set_animation(c5, 1.0)
  flux_qt_chart_set_theme(c5, 2.0)
  flux_qt_chart_legend_show(c5, 1.0)
  flux_qt_chart_legend_align(c5, 1.0)

  let pie = flux_qt_chart_add_pie_series(c5, "Share")
  flux_qt_pie_series_set_hole_size(pie, 0.40)
  flux_qt_pie_series_set_labels_position(pie, 0.0)

  let p1 = flux_qt_pie_series_append(pie, "Widgets", 35.0)
  flux_qt_pie_slice_set_color(p1, 60.0, 160.0, 230.0)
  flux_qt_pie_slice_set_label_visible(p1, 1.0)

  let p2 = flux_qt_pie_series_append(pie, "Gadgets", 28.0)
  flux_qt_pie_slice_set_color(p2, 230.0, 120.0, 50.0)
  flux_qt_pie_slice_set_label_visible(p2, 1.0)

  let p3 = flux_qt_pie_series_append(pie, "Doohickeys", 22.0)
  flux_qt_pie_slice_set_color(p3, 100.0, 200.0, 80.0)
  flux_qt_pie_slice_set_label_visible(p3, 1.0)
  flux_qt_pie_slice_set_exploded(p3, 1.0)

  let p4 = flux_qt_pie_series_append(pie, "Thingamajigs", 15.0)
  flux_qt_pie_slice_set_color(p4, 200.0, 130.0, 200.0)
  flux_qt_pie_slice_set_label_visible(p4, 1.0)

  flux_qt_layout_add_widget(l5, c5)
  flux_qt_tab_add(tab, t5, "Pie")

  flux_qt_tab_set_current(tab, 0.0)
  0.0
}

main()