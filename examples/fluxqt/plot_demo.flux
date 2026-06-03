# Improved Plot Demo — Python-like 2×2 subplots with styled charts
# Demonstrates: grid layout, axis labels, color cycling, dark theme, save-to-PNG

# ── Color palette (matplotlib-like) ──────────────────────────────────────────

def palette(i) {
  if (i == 0.0) { "31,119,180" }
  else if (i == 1.0) { "255,127,14" }
  else if (i == 2.0) { "44,160,44" }
  else if (i == 3.0) { "148,103,189" }
  else { "140,86,75" }
}

# ── Plot library helpers ─────────────────────────────────────────────────────

def draw_plot(cv, data, label) {
  let s = flux_qt_chart_add_line_series(cv, label)
  flux_qt_series_append_data(s, data)
  s
}

def draw_spline(cv, data, label) {
  let s = flux_qt_chart_add_spline_series(cv, label)
  flux_qt_series_append_data(s, data)
  s
}

def draw_scatter(cv, data, label) {
  let s = flux_qt_chart_add_scatter_series(cv, label)
  flux_qt_series_append_data(s, data)
  flux_qt_scatter_series_set_marker_size(s, 8.0)
  s
}

def draw_area(cv, data, label) {
  let s = flux_qt_chart_add_area_series(cv, label)
  flux_qt_series_append_data(s, data)
  s
}

def draw_bar(cv, label) {
  flux_qt_chart_add_bar_series(cv, label)
}

def draw_pie(cv, label) {
  flux_qt_chart_add_pie_series(cv, label)
}

def add_bar_set(series, label) {
  let bs = flux_qt_create_bar_set(label)
  flux_qt_bar_series_append_set(series, bs)
  bs
}

def add_pie_slice(pie, label, value) {
  flux_qt_pie_series_append(pie, label, value)
}

# ── Chart styling helpers ────────────────────────────────────────────────────

def style_chart(cv) {
  flux_qt_chart_set_animation(cv, 1.0)
  flux_qt_chart_set_drop_shadow(cv, 1.0)
  flux_qt_chart_set_margins(cv, 5, 5, 5, 5)
  flux_qt_chart_set_background(cv, 30, 30, 30)
  flux_qt_chart_legend_show(cv, 1.0)
}

def set_chart_title(cv, t) {
  flux_qt_chart_set_title(cv, t)
}

# ── Build individual charts ──────────────────────────────────────────────────

def build_plot_1(cv) {
  set_chart_title(cv, "Sine Waves")
  style_chart(cv)
  let s1 = draw_spline(cv, "0,0;0.5,0.48;1,0.84;1.5,1.0;2,0.91;2.5,0.6;3,0.14;3.5,-0.35;4,-0.76;4.5,-0.98;5,-0.96;5.5,-0.71;6,-0.28;6.5,0.22;7,0.66;7.5,0.94;8,0.99;8.5,0.8;9,0.41;9.5,-0.08;10,-0.54;10.5,-0.88;11,-0.99;11.5,-0.88;12,-0.54", "sin(x)")
  let s2 = draw_spline(cv, "0,0;0.5,0.96;1,1.0;1.5,0.14;2,-0.76;2.5,-0.96;3,-0.28;3.5,0.76;4,1.0;4.5,0.28;5,-0.76;5.5,-1.0;6,-0.28;6.5,0.76;7,1.0;7.5,0.28;8,-0.76;8.5,-1.0;9,-0.28;9.5,0.76;10,1.0;10.5,0.28;11,-0.76;11.5,-1.0;12,-0.28", "sin(2x)")
  let xa = flux_qt_chart_add_axis(cv, 1.0)
  let ya = flux_qt_chart_add_axis(cv, 2.0)
  flux_qt_axis_set_title(xa, "Time (s)")
  flux_qt_axis_set_title(ya, "Amplitude")
  flux_qt_axis_set_grid_visible(xa, 1.0)
  flux_qt_axis_set_grid_visible(ya, 1.0)
  flux_qt_axis_set_range(xa, 0.0, 12.0)
  flux_qt_axis_set_range(ya, -1.2, 1.2)
  flux_qt_series_attach_axis(s1, xa)
  flux_qt_series_attach_axis(s1, ya)
  flux_qt_series_attach_axis(s2, xa)
  flux_qt_series_attach_axis(s2, ya)
}

def build_plot_2(cv) {
  set_chart_title(cv, "Quadratic")
  style_chart(cv)
  let s1 = draw_plot(cv, "-5,25;-4,16;-3,9;-2,4;-1,1;0,0;1,1;2,4;3,9;4,16;5,25", "x²")
  let s2 = draw_scatter(cv, "-5,25;-4,16;-3,9;-2,4;-1,1;0,0;1,1;2,4;3,9;4,16;5,25", "samples")
  let xa = flux_qt_chart_add_axis(cv, 1.0)
  let ya = flux_qt_chart_add_axis(cv, 2.0)
  flux_qt_axis_set_title(xa, "X")
  flux_qt_axis_set_title(ya, "Y")
  flux_qt_axis_set_grid_visible(xa, 1.0)
  flux_qt_axis_set_grid_visible(ya, 1.0)
  flux_qt_series_attach_axis(s1, xa)
  flux_qt_series_attach_axis(s1, ya)
  flux_qt_series_attach_axis(s2, xa)
  flux_qt_series_attach_axis(s2, ya)
}

def build_plot_3(cv) {
  set_chart_title(cv, "Revenue by Quarter")
  style_chart(cv)
  let s1 = draw_bar(cv, "2024")
  let s2 = draw_bar(cv, "2025")
  let b1 = add_bar_set(s1, "Q1")
  let b2 = add_bar_set(s1, "Q2")
  let b3 = add_bar_set(s2, "Q3")
  let b4 = add_bar_set(s2, "Q4")
  flux_qt_bar_set_set_color(b1, 31, 119, 180)
  flux_qt_bar_set_set_color(b2, 255, 127, 14)
  flux_qt_bar_set_set_color(b3, 44, 160, 44)
  flux_qt_bar_set_set_color(b4, 148, 103, 189)
  flux_qt_bar_set_append(b1, 30)
  flux_qt_bar_set_append(b1, 45)
  flux_qt_bar_set_append(b1, 38)
  flux_qt_bar_set_append(b2, 55)
  flux_qt_bar_set_append(b2, 62)
  flux_qt_bar_set_append(b2, 48)
  flux_qt_bar_set_append(b3, 70)
  flux_qt_bar_set_append(b3, 85)
  flux_qt_bar_set_append(b3, 66)
  flux_qt_bar_set_append(b4, 22)
  flux_qt_bar_set_append(b4, 28)
  flux_qt_bar_set_append(b4, 35)
  let xa = flux_qt_chart_add_axis(cv, 1.0)
  let ya = flux_qt_chart_add_axis(cv, 2.0)
  flux_qt_axis_set_title(xa, "Category")
  flux_qt_axis_set_title(ya, "Revenue")
  flux_qt_axis_set_grid_visible(ya, 1.0)
}

def build_plot_4(cv) {
  set_chart_title(cv, "Market Share")
  style_chart(cv)
  let p = draw_pie(cv, "Share")
  flux_qt_pie_series_set_hole_size(p, 0.4)
  let s1 = add_pie_slice(p, "Chromium", 65.2)
  let s2 = add_pie_slice(p, "Firefox", 18.3)
  let s3 = add_pie_slice(p, "Safari", 10.5)
  let s4 = add_pie_slice(p, "Other", 6.0)
  flux_qt_pie_slice_set_color(s1, 31, 119, 180)
  flux_qt_pie_slice_set_color(s2, 255, 127, 14)
  flux_qt_pie_slice_set_color(s3, 44, 160, 44)
  flux_qt_pie_slice_set_color(s4, 148, 103, 189)
  flux_qt_pie_slice_set_label_visible(s1, 1.0)
  flux_qt_pie_slice_set_label_visible(s2, 1.0)
  flux_qt_pie_slice_set_label_visible(s3, 1.0)
  flux_qt_pie_slice_set_label_visible(s4, 1.0)
  flux_qt_pie_slice_set_exploded(s1, 1.0)
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  let win = flux_qt_get_window()
  flux_qt_set_window_title(win, "Plot Demo — Matplotlib-style")
  flux_qt_set_window_size(win, 1200, 800)

  # Toolbar with Save button
  let toolbar = flux_qt_create_widget()
  let tb_lay = flux_qt_create_layout("hbox")
  flux_qt_set_layout(toolbar, tb_lay)
  let btn_save = flux_qt_create_button("Save Chart as PNG")
  flux_qt_layout_add_widget(tb_lay, btn_save)

  let central = flux_qt_create_widget()
  let main_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, main_lay)
  flux_qt_set_central_widget(win, central)
  flux_qt_layout_add_widget(main_lay, toolbar)

  # 2×2 grid of charts
  let gpanel = flux_qt_create_widget()
  let glay = flux_qt_create_layout("grid")
  flux_qt_set_layout(gpanel, glay)
  flux_qt_layout_add_widget(main_lay, gpanel)

  # Chart views
  let cv1 = flux_qt_create_chartview()
  let cv2 = flux_qt_create_chartview()
  let cv3 = flux_qt_create_chartview()
  let cv4 = flux_qt_create_chartview()

  flux_qt_grid_add_widget(glay, cv1, 0, 0, 1, 1)
  flux_qt_grid_add_widget(glay, cv2, 0, 1, 1, 1)
  flux_qt_grid_add_widget(glay, cv3, 1, 0, 1, 1)
  flux_qt_grid_add_widget(glay, cv4, 1, 1, 1, 1)

  build_plot_1(cv1)
  build_plot_2(cv2)
  build_plot_3(cv3)
  build_plot_4(cv4)

  # Save button saves the first chart (cv1) — change index to save others
  flux_set_var(100.0, cv1)
  flux_qt_on_click_by_name(btn_save, "on_save")

  0.0
}

main()
