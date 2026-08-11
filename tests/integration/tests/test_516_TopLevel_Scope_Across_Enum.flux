// Regression: top-level statements around an enum declaration share scope.
// Bug: each top-level segment around an enum became its own anon_expr, so the
// `scale` binding declared before the enum was invisible in later statements.
scale = 6.0
enum Color {
    Red
    Green
    Blue
}
var c = Color.Red
var result = 0.0
match c {
    Color.Red => { result = 1.0 }
    Color.Green => { result = 2.0 }
    _ => { result = 3.0 }
}
var total = result * scale
assert(total == 6.0, "top-level scope across enum failed")
1.0
