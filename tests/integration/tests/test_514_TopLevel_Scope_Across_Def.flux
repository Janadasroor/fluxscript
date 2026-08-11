// Regression: top-level statements around a def must share one scope.
// Bug: compileParser() emitted a separate __anon_expr_N per top-level segment,
// so `x` declared before the def was invisible to statements after it.
x = 5.0
def double_it(v) -> Double {
    v * 2.0
}
y = x + double_it(3.0)
z = sin(0.0) + sqrt(16.0)
result = y * 100.0 + z
assert(result == 1104.0, "top-level scope across def failed")
1.0
