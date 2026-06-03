# Live Data Dashboard — real-time charts + gauges
# Demonstrates: timer-driven data, scrolling series, LCD, progress, dial

# ── Global keys ──────────────────────────────────────────────────────────────

def K_SINE()  10.0
def K_WALK()  11.0
def K_BAR()   12.0
def K_TIME()  13.0
def K_LCD()   14.0
def K_PROG()  15.0
def K_DIAL()  16.0

# ── Constants ────────────────────────────────────────────────────────────────

def MAX_PTS() 60.0
def DT() 0.08

# ── Random walk state ────────────────────────────────────────────────────────

def K_WALK_VAL() 17.0

# ── Bar data state ───────────────────────────────────────────────────────────

def K_BAR_TICK() 18.0

# ── Callbacks ────────────────────────────────────────────────────────────────

def on_tick() {
  let t = flux_get_var(K_TIME())

  # ── Sine wave ──
  let y = sin(t * 3.0)
      flux_qt_xy_series_scroll(flux_get_var(K_SINE()), t, y, MAX_PTS())

  # ── Random walk ──
  let walk = flux_get_var(K_WALK_VAL())
  let step = rand_uniform(-0.3, 0.3)
  walk = walk + step
  if (walk > 5.0) { walk = 5.0 }
  if (walk < -5.0) { walk = -5.0 }
  flux_set_var(K_WALK_VAL(), walk)
  flux_qt_xy_series_scroll(flux_get_var(K_WALK()), t, walk, MAX_PTS())

  # ── Bar race ──
  let bt = flux_get_var(K_BAR_TICK())
  if (bt > 9.0) {
    flux_qt_bar_set_remove(flux_get_var(K_BAR()), 0)
  }
  flux_qt_bar_set_append(flux_get_var(K_BAR()), rand_uniform(0.0, 100.0))
  bt = bt + 1.0
  if (bt > 20.0) { bt = 20.0 }
  flux_set_var(K_BAR_TICK(), bt)

  # ── Gauges ──
  flux_qt_lcd_display(flux_get_var(K_LCD()), y)
  flux_qt_progress_set_value(flux_get_var(K_PROG()), (y + 1.2) * 40.0)
  let dial_val = (walk + 5.0) * 10.0
  flux_qt_set_property(flux_get_var(K_DIAL()), "value", dial_val)

  # Advance time
  t = t + DT()
  flux_set_var(K_TIME(), t)

  0.0
}

def on_start() {
  flux_qt_timer_start(flux_get_var(99.0))
  0.0
}

# ── Chart builders ───────────────────────────────────────────────────────────

def make_chart(parent_lay) {
  let cv = flux_qt_create_chartview()
  flux_qt_chart_set_animation(cv, 0.0)
  flux_qt_chart_set_drop_shadow(cv, 1.0)
  flux_qt_chart_set_margins(cv, 3, 3, 3, 3)
  flux_qt_chart_set_background(cv, 20, 20, 25)
  flux_qt_layout_add_widget(parent_lay, cv)
  cv
}

# ── Main ─────────────────────────────────────────────────────────────────────

def main() {
  let win = flux_qt_get_window()
  flux_qt_set_window_title(win, "Live Data Dashboard")
  flux_qt_set_window_size(win, 1100, 750)

  let central = flux_qt_create_widget()
  let main_lay = flux_qt_create_layout("vbox")
  flux_qt_set_layout(central, main_lay)
  flux_qt_set_central_widget(win, central)

  # ── Gauge row ──
  let gauge_row = flux_qt_create_widget()
  let g_lay = flux_qt_create_layout("hbox")
  flux_qt_set_layout(gauge_row, g_lay)
  flux_qt_layout_add_widget(main_lay, gauge_row)

  let lcd = flux_qt_create_lcd()
  flux_set_var(K_LCD(), lcd)
  let prog = flux_qt_create_progressbar()
  flux_qt_progress_set_range(prog, 0, 100)
  flux_set_var(K_PROG(), prog)
  let dial = flux_qt_create_dial()
  flux_set_var(K_DIAL(), dial)

  flux_qt_layout_add_widget(g_lay, flux_qt_create_label("Value:"))
  flux_qt_layout_add_widget(g_lay, lcd)
  flux_qt_layout_add_widget(g_lay, flux_qt_create_label("Level:"))
  flux_qt_layout_add_widget(g_lay, prog)
  flux_qt_layout_add_widget(g_lay, flux_qt_create_label("Dial:"))
  flux_qt_layout_add_widget(g_lay, dial)

  # ── Chart row (3 columns) ──
  let chart_row = flux_qt_create_widget()
  let c_lay = flux_qt_create_layout("hbox")
  flux_qt_set_layout(chart_row, c_lay)
  flux_qt_layout_add_widget(main_lay, chart_row)

  # Sine wave chart
  let cv1 = make_chart(c_lay)
  flux_qt_chart_set_title(cv1, "Sine Wave")
  let s1 = flux_qt_chart_add_line_series(cv1, "sin(3t)")
  flux_qt_xy_series_set_pen(s1, 0, 200, 255, 2.0)
  flux_qt_chart_legend_show(cv1, 0.0)
  let xa1 = flux_qt_chart_add_axis(cv1, 1.0)
  let ya1 = flux_qt_chart_add_axis(cv1, 2.0)
  flux_qt_axis_set_range(xa1, -1.0, 10.0)
  flux_qt_axis_set_range(ya1, -2.0, 2.0)
  flux_qt_series_attach_axis(s1, xa1)
  flux_qt_series_attach_axis(s1, ya1)
  flux_set_var(K_SINE(), s1)

  # Random walk chart
  let cv2 = make_chart(c_lay)
  flux_qt_chart_set_title(cv2, "Random Walk")
  let s2 = flux_qt_chart_add_line_series(cv2, "walk")
  flux_qt_xy_series_set_pen(s2, 255, 180, 50, 2.0)
  flux_qt_chart_legend_show(cv2, 0.0)
  let xa2 = flux_qt_chart_add_axis(cv2, 1.0)
  let ya2 = flux_qt_chart_add_axis(cv2, 2.0)
  flux_qt_axis_set_range(xa2, -1.0, 10.0)
  flux_qt_axis_set_range(ya2, -6.0, 6.0)
  flux_qt_series_attach_axis(s2, xa2)
  flux_qt_series_attach_axis(s2, ya2)
  flux_set_var(K_WALK(), s2)
  flux_set_var(K_WALK_VAL(), 0.0)

  # Bar race chart
  let cv3 = make_chart(c_lay)
  flux_qt_chart_set_title(cv3, "Bar Race")
  let s3 = flux_qt_chart_add_bar_series(cv3, "random")
  flux_qt_chart_legend_show(cv3, 0.0)
  let bs = flux_qt_create_bar_set("data")
  flux_qt_bar_set_set_color(bs, 120, 200, 80)
  flux_qt_bar_series_append_set(s3, bs)
  flux_set_var(K_BAR(), bs)
  flux_set_var(K_BAR_TICK(), 0.0)

  # ── Start button ──
  let btn_lay = flux_qt_create_layout("hbox")
  let btn = flux_qt_create_button("Start Live Data")
  flux_qt_layout_add_widget(btn_lay, btn)
  flux_qt_layout_add_widget(main_lay, btn_lay)

  # ── Timer ──
  let tmr = flux_qt_create_timer(DT() * 1000.0, "on_tick")
  flux_set_var(99.0, tmr)

  flux_qt_on_click_by_name(btn, "on_start")

  0.0
}

main()
