// Test: multiple closures capturing same variable
def test() -> Double {
    let x = 3.0
    let f1 = fn(y) -> x * y
    let f2 = fn(z) -> x + z
    f1(5.0) + f2(7.0)
}

assert(test() == 25.0, "Multiple closures failed")
1.0
