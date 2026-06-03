# FluxScript Trait Demo — Shape Area Calculator
# Demonstrates: trait declaration, impl Trait for Type,
# generic trait bounds [T: Trait], dyn Trait dispatch

trait Shape {
    def area(self) -> Double
    def name(self) -> String
}

struct Circle {
    radius: Double
}

impl Shape for Circle {
    def area(self) -> Double {
        3.14159 * self.radius * self.radius
    }
    def name(self) -> String {
        "Circle"
    }
}

struct Rect {
    width: Double
    height: Double
}

impl Shape for Rect {
    def area(self) -> Double {
        self.width * self.height
    }
    def name(self) -> String {
        "Rectangle"
    }
}

struct Square {
    side: Double
}

impl Shape for Square {
    def area(self) -> Double {
        self.side * self.side
    }
    def name(self) -> String {
        "Square"
    }
}

# Generic static dispatch (monomorphized at compile time)
def describe_area[T: Shape](s: T) -> String {
    s.name() + " area = " + flux_double_to_string(s.area())
}

# Dyn dispatch functions (fat pointer {data_ptr, vtable_ptr})
def dyn_area(s: dyn Shape) -> Double {
    s.area()
}

def dyn_name(s: dyn Shape) -> String {
    s.name()
}

def dyn_describe(s: dyn Shape) -> String {
    dyn_name(s) + " area = " + flux_double_to_string(dyn_area(s))
}

# Key helpers
def K_RES_A()  10.0
def K_RES_B()  20.0
def K_RES_C()  30.0
def K_RES_D()  40.0

# ── Main ─────────────────────────────────────────────────────────────────

def main() {
    flux_qt_set_app_icon("flux.svg")

    let win = flux_qt_create_mainwindow("FluxScript Trait Demo")
    flux_qt_set_window_size(win, 620, 400)
    flux_qt_set_stylesheet(win, "QMainWindow { background-color: #1e1e2e; } QLabel { color: #cdd6f4; font-size: 13px; } QLabel.title { font-size: 18px; font-weight: bold; color: #cba6f7; } QLabel.section { font-size: 14px; font-weight: bold; color: #89b4fa; } QLabel.dynsec { font-size: 14px; font-weight: bold; color: #a6e3a1; } QLabel.result { background-color: #181825; border: 1px solid #313244; border-radius: 4px; padding: 4px 8px; } QPushButton { background-color: #45475a; color: #cdd6f4; border: 1px solid #585b70; border-radius: 4px; padding: 6px 16px; font-size: 13px; } QPushButton:hover { background-color: #585b70; } QDoubleSpinBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 3px; padding: 3px; }")

    let central = flux_qt_create_widget()
    flux_qt_set_central_widget(win, central)
    let root = flux_qt_create_layout("vbox")
    flux_qt_set_layout(central, root)

    let hdr = flux_qt_create_label("Trait-based Shape Area Calculator")
    flux_qt_set_stylesheet(hdr, "font-size: 18px; font-weight: bold; color: #cba6f7; margin: 6px;")
    flux_qt_layout_add_widget(root, hdr)

    # ── Static dispatch (generic trait bound) ──
    let sec1 = flux_qt_create_label("Static Dispatch — [T: Shape] generic bound")
    flux_qt_set_stylesheet(sec1, "font-size: 14px; font-weight: bold; color: #89b4fa; margin: 4px;")
    flux_qt_layout_add_widget(root, sec1)

    # Circle row: spinner + Compute + result
    let row_c = flux_qt_create_widget()
    let lay_c = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_c, lay_c)
    flux_qt_layout_add_widget(root, row_c)

    let lbl_c = flux_qt_create_label("Circle radius:")
    flux_qt_layout_add_widget(lay_c, lbl_c)

    let spin_r = flux_qt_create_spinbox()
    flux_qt_set_property(spin_r, "minimum", 0.0)
    flux_qt_set_property(spin_r, "maximum", 100.0)
    flux_qt_set_property(spin_r, "value", 5.0)
    flux_qt_set_property(spin_r, "singleStep", 0.5)
    flux_qt_set_property(spin_r, "decimals", 2)
    flux_qt_layout_add_widget(lay_c, spin_r)

    let btn_c = flux_qt_create_button("describe_area[Circle]")
    flux_qt_layout_add_widget(lay_c, btn_c)
    flux_qt_on_click_by_name(btn_c, "on_circle")

    let res_c = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_c, res_c)
    flux_set_var(K_RES_A(), res_c)

    # Rectangle row: two spinners + Compute + result
    let row_r = flux_qt_create_widget()
    let lay_r = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_r, lay_r)
    flux_qt_layout_add_widget(root, row_r)

    let lbl_r = flux_qt_create_label("Rect W:")
    flux_qt_layout_add_widget(lay_r, lbl_r)

    let spin_w = flux_qt_create_spinbox()
    flux_qt_set_property(spin_w, "minimum", 0.0)
    flux_qt_set_property(spin_w, "maximum", 100.0)
    flux_qt_set_property(spin_w, "value", 3.0)
    flux_qt_set_property(spin_w, "singleStep", 0.5)
    flux_qt_set_property(spin_w, "decimals", 2)
    flux_qt_layout_add_widget(lay_r, spin_w)

    let lbl_h = flux_qt_create_label("H:")
    flux_qt_layout_add_widget(lay_r, lbl_h)

    let spin_h = flux_qt_create_spinbox()
    flux_qt_set_property(spin_h, "minimum", 0.0)
    flux_qt_set_property(spin_h, "maximum", 100.0)
    flux_qt_set_property(spin_h, "value", 4.0)
    flux_qt_set_property(spin_h, "singleStep", 0.5)
    flux_qt_set_property(spin_h, "decimals", 2)
    flux_qt_layout_add_widget(lay_r, spin_h)

    let btn_r = flux_qt_create_button("describe_area[Rect]")
    flux_qt_layout_add_widget(lay_r, btn_r)
    flux_qt_on_click_by_name(btn_r, "on_rect")

    let res_r = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_r, res_r)
    flux_set_var(K_RES_B(), res_r)

    # ── Dyn dispatch section ──
    let sec2 = flux_qt_create_label("Dynamic Dispatch — dyn Shape (fat pointer)")
    flux_qt_set_stylesheet(sec2, "font-size: 14px; font-weight: bold; color: #a6e3a1; margin: 4px;")
    flux_qt_layout_add_widget(root, sec2)

    # Each shape gets its own button + result (no shared dyn variable across branches)
    let row_dc = flux_qt_create_widget()
    let lay_dc = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_dc, lay_dc)
    flux_qt_layout_add_widget(root, row_dc)

    let btn_dc = flux_qt_create_button("dyn_describe(Circle)")
    flux_qt_layout_add_widget(lay_dc, btn_dc)
    flux_qt_on_click_by_name(btn_dc, "on_dyn_circle")

    let res_dc = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_dc, res_dc)
    flux_set_var(K_RES_C(), res_dc)

    let row_dr = flux_qt_create_widget()
    let lay_dr = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_dr, lay_dr)
    flux_qt_layout_add_widget(root, row_dr)

    let btn_dr = flux_qt_create_button("dyn_describe(Rect)")
    flux_qt_layout_add_widget(lay_dr, btn_dr)
    flux_qt_on_click_by_name(btn_dr, "on_dyn_rect")

    let res_dr = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_dr, res_dr)
    flux_set_var(K_RES_D(), res_dr)

    # Store spinbox handles
    flux_set_var(1.0, spin_r)
    flux_set_var(2.0, spin_w)
    flux_set_var(3.0, spin_h)

    0.0
}

# ── Static dispatch callbacks ────────────────────────────────────────────

def on_circle() {
    let r = flux_qt_get_property(flux_get_var(1.0), "value")
    let c = Circle { radius: r }
    let txt = describe_area[Circle](c)
    flux_qt_set_text(flux_get_var(K_RES_A()), txt)
    0.0
}

def on_rect() {
    let w = flux_qt_get_property(flux_get_var(2.0), "value")
    let h = flux_qt_get_property(flux_get_var(3.0), "value")
    let r = Rect { width: w, height: h }
    let txt = describe_area[Rect](r)
    flux_qt_set_text(flux_get_var(K_RES_B()), txt)
    0.0
}

# ── Dynamic dispatch callbacks (each creates its own dyn object) ────────

def on_dyn_circle() {
    let r = flux_qt_get_property(flux_get_var(1.0), "value")
    let s: dyn Shape = Circle { radius: r }
    let txt = dyn_describe(s)
    flux_qt_set_text(flux_get_var(K_RES_C()), txt)
    0.0
}

def on_dyn_rect() {
    let w = flux_qt_get_property(flux_get_var(2.0), "value")
    let h = flux_qt_get_property(flux_get_var(3.0), "value")
    let s: dyn Shape = Rect { width: w, height: h }
    let txt = dyn_describe(s)
    flux_qt_set_text(flux_get_var(K_RES_D()), txt)
    0.0
}

main()
