// Test: non-capturing lambda bound with let (was crashing due to NamedTypes pollution)
def apply(f, x) -> Double {
    f(x)
}
def test() -> Double {
    let f = fn(x) -> x + 1.0
    apply(f, 5.0)
}

assert(test() == 6.0, "Non-capturing let lambda failed")
1.0
