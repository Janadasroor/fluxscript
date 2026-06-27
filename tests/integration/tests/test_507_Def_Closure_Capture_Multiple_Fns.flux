// Test: multiple nested defs capturing different combinations of variables
def test() -> Double {
    var x = 1.0
    var y = 2.0
    def add_x(a: Double) -> Double {
        a + x
    }
    def add_y(b: Double) -> Double {
        b + y
    }
    def add_both(c: Double) -> Double {
        c + x + y
    }
    var r1 = add_x(10.0)     // 10 + 1 = 11
    var r2 = add_y(20.0)     // 20 + 2 = 22
    var r3 = add_both(30.0)  // 30 + 1 + 2 = 33
    r1 + r2 + r3
}
assert(test() == 66.0, "Multiple fns def capture failed")
1.0
