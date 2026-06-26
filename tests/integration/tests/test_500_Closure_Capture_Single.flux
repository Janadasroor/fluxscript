// Test: single variable capture in closure
def test() -> Double {
    let x = 5.0
    let f = fn(y) -> x + y
    f(10.0)
}

assert(test() == 15.0, "Single capture failed")
1.0
