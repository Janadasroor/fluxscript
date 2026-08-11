// Regression: multiple top-level segments (mimics dashboard_demo.flux pattern
// where widgets were built, defs declared, then bindings wired up). All top-level
// statements must share one scope and the single anon_expr must execute all of
// them in order, so later segments see bindings from earlier segments.
r_slider = 1.0
dash = 10.0

def update_resistor(v) -> Double {
    v * 2.0
}

run_btn = 20.0
result = run_btn + dash + update_resistor(r_slider)
assert(result == 32.0, "multi-segment top-level scope failed")
1.0
