// Regression: top-level statements around a struct declaration share scope.
// Bug: each top-level segment around a struct became its own anon_expr, so the
// mutable `count` declared before the struct was invisible in later statements.
var count = 2.0
struct Point { x: Double, y: Double }
var p = Point { x: 3.0, y: 4.0 }
count = count + p.x
var result = count * 10.0
assert(result == 50.0, "top-level scope across struct failed")
1.0
