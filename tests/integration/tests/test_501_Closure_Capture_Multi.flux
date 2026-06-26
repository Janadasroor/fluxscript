// Test: multiple variable captures in closure
def test() -> Double {
    let x = 10.0
    let y = 20.0
    let f = fn(z) -> x + y + z
    f(30.0)
}

assert(test() == 60.0, "Multi capture failed")
1.0
