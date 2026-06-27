// Test: nested def captures multiple variables from enclosing scope
def test() -> Double {
    var a = 2.0
    var b = 3.0
    def mul(x: Double) -> Double {
        x * a + b
    }
    mul(5.0)
}
assert(test() == 13.0, "Multi def capture failed")  // 5*2 + 3 = 13
1.0
