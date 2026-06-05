# FluxScript Fix Showcase — demonstrates all bug fixes & features
#
# Fixes demonstrated:
#   1. String interpolation sub-lexer fix  (parser.cpp:239)
#   2. Struct return type inference fix    (ParseDefinition + compiler_instance.cpp)
#   3. Class desugaring with field mutation
#   4. Trait + generic bound + dyn dispatch
#
# Features: String API, Class, Trait, dyn Trait, generics

# ═══════════════════════════════════════════════════════════════════════════
# 1. STRUCT RETURN TYPE INFERENCE
#    def fn(params) ReturnType { field: val, ... } — no -> annotation needed
# ═══════════════════════════════════════════════════════════════════════════

struct Vec3 { x: Double, y: Double, z: Double }

def vec3_make(x, y, z) Vec3 { x: x, y: y, z: z }

def vec3_add(a: Vec3, b: Vec3) -> Vec3 {
    Vec3 { x: a.x + b.x, y: a.y + b.y, z: a.z + b.z }
}

def vec3_len(v: Vec3) -> Double {
    sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
}

# ═══════════════════════════════════════════════════════════════════════════
# 2. STRING API + INTERPOLATION
#    (tests sub-lexer fix — "${expr}" inside strings)
# ═══════════════════════════════════════════════════════════════════════════

def greet(name: String) -> String {
    "Hello, ${name}! (len=${name.len()})"
}

def describe_vec3(v: Vec3) -> String {
    "Vec3(${flux_double_to_string(v.x)}, ${flux_double_to_string(v.y)}, ${flux_double_to_string(v.z)}) len=${flux_double_to_string(vec3_len(v))}"
}

# ═══════════════════════════════════════════════════════════════════════════
# 3. CLASS
# ═══════════════════════════════════════════════════════════════════════════

class ClickCounter {
    val: Double

    def inc(self) { self.val = self.val + 1.0 }
    def dec(self) { self.val = self.val - 1.0 }
    def reset(self) -> Double { self.val = 0.0; self.val }
}

# ═══════════════════════════════════════════════════════════════════════════
# 4. TRAIT + GENERIC BOUND + DYN DISPATCH
# ═══════════════════════════════════════════════════════════════════════════

trait Volume {
    def volume(self) -> Double
    def label(self) -> String
}

struct Sphere { radius: Double }

impl Volume for Sphere {
    def volume(self) -> Double { 4.18879 * self.radius * self.radius * self.radius }
    def label(self) -> String { "Sphere(r=${flux_double_to_string(self.radius)})" }
}

struct Box3 { w: Double, h: Double, d: Double }

impl Volume for Box3 {
    def volume(self) -> Double { self.w * self.h * self.d }
    def label(self) -> String { "Box(${flux_double_to_string(self.w)}x${flux_double_to_string(self.h)}x${flux_double_to_string(self.d)})" }
}

# Generic static dispatch
def report_volume[T: Volume](shape: T) -> String {
    shape.label() + " vol=" + flux_double_to_string(shape.volume())
}

# Dyn dispatch
def dyn_report(s: dyn Volume) -> String {
    s.label() + " vol=" + flux_double_to_string(s.volume())
}

# ═══════════════════════════════════════════════════════════════════════════
# QT UI — Key constants
# ═══════════════════════════════════════════════════════════════════════════

def K_RES_GREET()   100.0
def K_RES_VEC3()    110.0
def K_CLASS_VAL()   120.0
def K_CLASS_LCD()   130.0
def K_RES_TRAIT()   140.0
def K_EDIT()        10.0
def K_SPIN_R()      20.0
def K_SPIN_W()      21.0
def K_SPIN_H()      22.0
def K_SPIN_D()      23.0

# ═══════════════════════════════════════════════════════════════════════════
# CALLBACKS
# ═══════════════════════════════════════════════════════════════════════════

def on_greet() {
    let name = flux_qt_get_property(flux_get_var(K_EDIT()), "text")
    flux_qt_set_text(flux_get_var(K_RES_GREET()), greet(name))
    0.0
}

def on_vec3_test() {
    let a = vec3_make(3.0, 4.0, 0.0)
    let b = vec3_make(1.0, 2.0, 3.0)
    let c = vec3_add(a, b)
    flux_qt_set_text(flux_get_var(K_RES_VEC3()), describe_vec3(c))
    0.0
}

def on_class_inc() {
    let v = flux_get_var(K_CLASS_VAL())
    let c = ClickCounter { val: v }
    c.inc()
    flux_set_var(K_CLASS_VAL(), c.val)
    flux_qt_lcd_display(flux_get_var(K_CLASS_LCD()), c.val)
    0.0
}

def on_class_dec() {
    let v = flux_get_var(K_CLASS_VAL())
    let c = ClickCounter { val: v }
    c.dec()
    flux_set_var(K_CLASS_VAL(), c.val)
    flux_qt_lcd_display(flux_get_var(K_CLASS_LCD()), c.val)
    0.0
}

def on_class_reset() {
    let c = ClickCounter { val: 0.0 }
    c.reset()
    flux_set_var(K_CLASS_VAL(), c.val)
    flux_qt_lcd_display(flux_get_var(K_CLASS_LCD()), c.val)
    0.0
}

def on_trait_static() {
    let r = flux_qt_get_property(flux_get_var(K_SPIN_R()), "value")
    let w = flux_qt_get_property(flux_get_var(K_SPIN_W()), "value")
    let h = flux_qt_get_property(flux_get_var(K_SPIN_H()), "value")
    let d = flux_qt_get_property(flux_get_var(K_SPIN_D()), "value")
    let sphere = Sphere { radius: r }
    let box3 = Box3 { w: w, h: h, d: d }
    let txt = report_volume[Sphere](sphere) + "\n" + report_volume[Box3](box3)
    flux_qt_set_text(flux_get_var(K_RES_TRAIT()), txt)
    0.0
}

def on_trait_dyn() {
    let r = flux_qt_get_property(flux_get_var(K_SPIN_R()), "value")
    let w = flux_qt_get_property(flux_get_var(K_SPIN_W()), "value")
    let h = flux_qt_get_property(flux_get_var(K_SPIN_H()), "value")
    let d = flux_qt_get_property(flux_get_var(K_SPIN_D()), "value")
    let sphere: dyn Volume = Sphere { radius: r }
    let box3: dyn Volume = Box3 { w: w, h: h, d: d }
    let txt = dyn_report(sphere) + "\n" + dyn_report(box3)
    flux_qt_set_text(flux_get_var(K_RES_TRAIT()), txt)
    0.0
}

# ═══════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════

def main() {
    flux_qt_set_app_icon("flux.svg")

    let win = flux_qt_create_mainwindow("FluxScript Fix Showcase")
    flux_qt_set_window_size(win, 720, 520)
    flux_qt_set_stylesheet(win, "QMainWindow { background-color: #1e1e2e; } QLabel { color: #cdd6f4; font-size: 13px; } QLabel.hdr { font-size: 18px; font-weight: bold; color: #cba6f7; } QLabel.sec { font-size: 14px; font-weight: bold; color: #f9e2af; } QLabel.res { background-color: #181825; border: 1px solid #313244; border-radius: 4px; padding: 4px 8px; } QPushButton { background-color: #45475a; color: #cdd6f4; border: 1px solid #585b70; border-radius: 4px; padding: 5px 12px; font-size: 12px; } QPushButton:hover { background-color: #585b70; } QDoubleSpinBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 3px; padding: 2px; } QLineEdit { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 3px; padding: 3px; } QLCDNumber { background-color: #11111b; border: 1px solid #313244; border-radius: 4px; }")

    let central = flux_qt_create_widget()
    flux_qt_set_central_widget(win, central)
    let root = flux_qt_create_layout("vbox")
    flux_qt_set_layout(central, root)

    let hdr = flux_qt_create_label("FluxScript Fix Showcase — all fixes & features")
    flux_qt_set_stylesheet(hdr, "font-size: 18px; font-weight: bold; color: #cba6f7; margin: 6px;")
    flux_qt_layout_add_widget(root, hdr)

    # ── Section 1: String Interpolation ──
    let s1 = flux_qt_create_label("1. String Interpolation + API (sub-lexer fix)")
    flux_qt_set_stylesheet(s1, "font-size: 14px; font-weight: bold; color: #f38ba8; margin: 4px;")
    flux_qt_layout_add_widget(root, s1)

    let row_s1 = flux_qt_create_widget()
    let lay_s1 = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_s1, lay_s1)
    flux_qt_layout_add_widget(root, row_s1)

    let edit = flux_qt_create_lineedit("World")
    flux_qt_layout_add_widget(lay_s1, edit)
    flux_set_var(K_EDIT(), edit)

    let btn_hello = flux_qt_create_button("Hello via interpolation")
    flux_qt_layout_add_widget(lay_s1, btn_hello)
    flux_qt_on_click_by_name(btn_hello, "on_greet")

    let res_s1 = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_s1, res_s1)
    flux_set_var(K_RES_GREET(), res_s1)

    let btn_v3 = flux_qt_create_button("vec3 (inferred return)")
    flux_qt_layout_add_widget(lay_s1, btn_v3)
    flux_qt_on_click_by_name(btn_v3, "on_vec3_test")

    let res_v3 = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_s1, res_v3)
    flux_set_var(K_RES_VEC3(), res_v3)

    # ── Section 2: Class ──
    let s2 = flux_qt_create_label("2. Class — ClickCounter with inc/dec/reset")
    flux_qt_set_stylesheet(s2, "font-size: 14px; font-weight: bold; color: #a6e3a1; margin: 4px;")
    flux_qt_layout_add_widget(root, s2)

    let row_c = flux_qt_create_widget()
    let lay_c = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_c, lay_c)
    flux_qt_layout_add_widget(root, row_c)

    let lcd_c = flux_qt_create_lcd()
    flux_qt_lcd_display(lcd_c, 0.0)
    flux_qt_layout_add_widget(lay_c, lcd_c)
    flux_set_var(K_CLASS_LCD(), lcd_c)

    let btn_inc = flux_qt_create_button("inc")
    flux_qt_layout_add_widget(lay_c, btn_inc)
    flux_qt_on_click_by_name(btn_inc, "on_class_inc")

    let btn_dec = flux_qt_create_button("dec")
    flux_qt_layout_add_widget(lay_c, btn_dec)
    flux_qt_on_click_by_name(btn_dec, "on_class_dec")

    let btn_rst = flux_qt_create_button("reset")
    flux_qt_layout_add_widget(lay_c, btn_rst)
    flux_qt_on_click_by_name(btn_rst, "on_class_reset")

    # ── Section 3: Trait ──
    let s3 = flux_qt_create_label("3. Trait — Volume trait (static [T: V] + dyn dispatch)")
    flux_qt_set_stylesheet(s3, "font-size: 14px; font-weight: bold; color: #89b4fa; margin: 4px;")
    flux_qt_layout_add_widget(root, s3)

    let row_t = flux_qt_create_widget()
    let lay_t = flux_qt_create_layout("hbox")
    flux_qt_set_layout(row_t, lay_t)
    flux_qt_layout_add_widget(root, row_t)

    let spin_r = flux_qt_create_spinbox()
    flux_qt_set_property(spin_r, "minimum", 0.0)
    flux_qt_set_property(spin_r, "maximum", 20.0)
    flux_qt_set_property(spin_r, "value", 3.0)
    flux_qt_set_property(spin_r, "singleStep", 0.5)
    flux_qt_set_property(spin_r, "decimals", 2)
    flux_qt_layout_add_widget(lay_t, spin_r)
    flux_set_var(K_SPIN_R(), spin_r)

    let spin_w = flux_qt_create_spinbox()
    flux_qt_set_property(spin_w, "minimum", 0.0)
    flux_qt_set_property(spin_w, "maximum", 20.0)
    flux_qt_set_property(spin_w, "value", 2.0)
    flux_qt_set_property(spin_w, "singleStep", 0.5)
    flux_qt_set_property(spin_w, "decimals", 2)
    flux_qt_layout_add_widget(lay_t, spin_w)
    flux_set_var(K_SPIN_W(), spin_w)

    let spin_h = flux_qt_create_spinbox()
    flux_qt_set_property(spin_h, "minimum", 0.0)
    flux_qt_set_property(spin_h, "maximum", 20.0)
    flux_qt_set_property(spin_h, "value", 4.0)
    flux_qt_set_property(spin_h, "singleStep", 0.5)
    flux_qt_set_property(spin_h, "decimals", 2)
    flux_qt_layout_add_widget(lay_t, spin_h)
    flux_set_var(K_SPIN_H(), spin_h)

    let spin_d = flux_qt_create_spinbox()
    flux_qt_set_property(spin_d, "minimum", 0.0)
    flux_qt_set_property(spin_d, "maximum", 20.0)
    flux_qt_set_property(spin_d, "value", 5.0)
    flux_qt_set_property(spin_d, "singleStep", 0.5)
    flux_qt_set_property(spin_d, "decimals", 2)
    flux_qt_layout_add_widget(lay_t, spin_d)
    flux_set_var(K_SPIN_D(), spin_d)

    let btn_ts = flux_qt_create_button("[T: Volume] static")
    flux_qt_layout_add_widget(lay_t, btn_ts)
    flux_qt_on_click_by_name(btn_ts, "on_trait_static")

    let btn_td = flux_qt_create_button("dyn Volume dispatch")
    flux_qt_layout_add_widget(lay_t, btn_td)
    flux_qt_on_click_by_name(btn_td, "on_trait_dyn")

    let res_t = flux_qt_create_label("---")
    flux_qt_layout_add_widget(lay_t, res_t)
    flux_set_var(K_RES_TRAIT(), res_t)

    0.0
}

main()
