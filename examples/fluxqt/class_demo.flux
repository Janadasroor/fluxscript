# FluxScript Class Demo — Multi-Counter Dashboard
# Demonstrates: class fields, methods, field mutation via self,
# multiple instances, method dispatch, UI layout

class Counter {
    val_key: Double
    lcd: Double

    def get_val(self) -> Double {
        flux_get_var(self.val_key)
    }

    def set_val(self, v: Double) {
        flux_set_var(self.val_key, v)
        flux_qt_lcd_display(self.lcd, v)
    }

    def increment(self) {
        self.set_val(self.get_val() + 1.0)
    }

    def decrement(self) {
        self.set_val(self.get_val() - 1.0)
    }

    def reset(self) {
        self.set_val(0.0)
    }
}

# Keys: 100+ = LCD handles, 200+ = counter values

# ── Main ─────────────────────────────────────────────────────────────────

def main() {
    flux_qt_set_app_icon("flux.svg")

    let win = flux_qt_create_mainwindow("FluxScript Class Demo")
    flux_qt_set_window_size(win, 560, 320)
    flux_qt_set_stylesheet(win, "QMainWindow { background-color: #1e1e2e; } QLabel { color: #cdd6f4; font-size: 14px; font-weight: bold; } QPushButton { background-color: #45475a; color: #cdd6f4; border: 1px solid #585b70; border-radius: 4px; padding: 6px 16px; font-size: 13px; } QPushButton:hover { background-color: #585b70; } QLCDNumber { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; min-height: 30px; }")

    let central = flux_qt_create_widget()
    flux_qt_set_central_widget(win, central)

    let root = flux_qt_create_layout("vbox")
    flux_qt_set_layout(central, root)

    let hdr = flux_qt_create_label("Class-based Multi-Counter Demo")
    flux_qt_layout_add_widget(root, hdr)

    # ── Counter A ──
    let row_a = flux_qt_create_widget()
    let lay_a = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_a, lay_a)
    flux_qt_layout_add_widget(root, row_a)

    let lbl_a = flux_qt_create_label("A")
    flux_qt_layout_add_widget(lay_a, lbl_a)

    let lcd_a = flux_qt_create_lcd()
    flux_qt_lcd_display(lcd_a, 0.0)
    flux_qt_layout_add_widget(lay_a, lcd_a)

    let inc_a = flux_qt_create_button("+1")
    flux_qt_layout_add_widget(lay_a, inc_a)
    flux_qt_on_click_by_name(inc_a, "on_inc_a")

    let dec_a = flux_qt_create_button("-1")
    flux_qt_layout_add_widget(lay_a, dec_a)
    flux_qt_on_click_by_name(dec_a, "on_dec_a")

    let rst_a = flux_qt_create_button("Reset")
    flux_qt_layout_add_widget(lay_a, rst_a)
    flux_qt_on_click_by_name(rst_a, "on_rst_a")

    # ── Counter B ──
    let row_b = flux_qt_create_widget()
    let lay_b = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_b, lay_b)
    flux_qt_layout_add_widget(root, row_b)

    let lbl_b = flux_qt_create_label("B")
    flux_qt_layout_add_widget(lay_b, lbl_b)

    let lcd_b = flux_qt_create_lcd()
    flux_qt_lcd_display(lcd_b, 5.0)
    flux_qt_layout_add_widget(lay_b, lcd_b)

    let inc_b = flux_qt_create_button("+1")
    flux_qt_layout_add_widget(lay_b, inc_b)
    flux_qt_on_click_by_name(inc_b, "on_inc_b")

    let dec_b = flux_qt_create_button("-1")
    flux_qt_layout_add_widget(lay_b, dec_b)
    flux_qt_on_click_by_name(dec_b, "on_dec_b")

    let rst_b = flux_qt_create_button("Reset")
    flux_qt_layout_add_widget(lay_b, rst_b)
    flux_qt_on_click_by_name(rst_b, "on_rst_b")

    # ── Counter C ──
    let row_c = flux_qt_create_widget()
    let lay_c = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_c, lay_c)
    flux_qt_layout_add_widget(root, row_c)

    let lbl_c = flux_qt_create_label("C")
    flux_qt_layout_add_widget(lay_c, lbl_c)

    let lcd_c = flux_qt_create_lcd()
    flux_qt_lcd_display(lcd_c, 10.0)
    flux_qt_layout_add_widget(lay_c, lcd_c)

    let inc_c = flux_qt_create_button("+1")
    flux_qt_layout_add_widget(lay_c, inc_c)
    flux_qt_on_click_by_name(inc_c, "on_inc_c")

    let dec_c = flux_qt_create_button("-1")
    flux_qt_layout_add_widget(lay_c, dec_c)
    flux_qt_on_click_by_name(dec_c, "on_dec_c")

    let rst_c = flux_qt_create_button("Reset")
    flux_qt_layout_add_widget(lay_c, rst_c)
    flux_qt_on_click_by_name(rst_c, "on_rst_c")

    # Store LCD handles and initial values
    flux_set_var(100.0, lcd_a)
    flux_set_var(110.0, lcd_b)
    flux_set_var(120.0, lcd_c)

    flux_set_var(200.0, 0.0)
    flux_set_var(210.0, 5.0)
    flux_set_var(220.0, 10.0)

    # ── Verify class methods programmatically ──
    let test = Counter { val_key: 200.0, lcd: lcd_a }

    test.increment()
    assert(flux_get_var(200.0) == 1.0, "increment failed")

    test.increment()
    assert(flux_get_var(200.0) == 2.0, "increment #2 failed")

    test.decrement()
    assert(flux_get_var(200.0) == 1.0, "decrement failed")

    test.reset()
    assert(flux_get_var(200.0) == 0.0, "reset failed")

    0.0
}

# ── Callbacks ────────────────────────────────────────────────────────────

def on_inc_a() {
    let c = Counter { val_key: 200.0, lcd: flux_get_var(100.0) }
    c.increment()
    0.0
}

def on_dec_a() {
    let c = Counter { val_key: 200.0, lcd: flux_get_var(100.0) }
    c.decrement()
    0.0
}

def on_rst_a() {
    let c = Counter { val_key: 200.0, lcd: flux_get_var(100.0) }
    c.reset()
    0.0
}

def on_inc_b() {
    let c = Counter { val_key: 210.0, lcd: flux_get_var(110.0) }
    c.increment()
    0.0
}

def on_dec_b() {
    let c = Counter { val_key: 210.0, lcd: flux_get_var(110.0) }
    c.decrement()
    0.0
}

def on_rst_b() {
    let c = Counter { val_key: 210.0, lcd: flux_get_var(110.0) }
    c.reset()
    0.0
}

def on_inc_c() {
    let c = Counter { val_key: 220.0, lcd: flux_get_var(120.0) }
    c.increment()
    0.0
}

def on_dec_c() {
    let c = Counter { val_key: 220.0, lcd: flux_get_var(120.0) }
    c.decrement()
    0.0
}

def on_rst_c() {
    let c = Counter { val_key: 220.0, lcd: flux_get_var(120.0) }
    c.reset()
    0.0
}

main()
