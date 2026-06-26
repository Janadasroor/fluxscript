// Test: inline lambda as function argument
def apply(f, x) -> Double {
    f(x)
}
def compose(f, g, x) -> Double {
    f(g(x))
}
def test() -> Double {
    let r1 = apply(fn(x) -> x * 2.0, 5.0)
    let r2 = apply(fn(x) -> x + 1.0, 5.0)
    let r3 = compose(fn(x) -> x * 2.0, fn(x) -> x + 1.0, 5.0)
    return r1 + r2 + r3
}

assert(test() == 28.0, "Inline fn as argument failed")
1.0
