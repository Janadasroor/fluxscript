// Test: nested def captures single variable from enclosing scope
def test() -> Double {
    var k = 3.0
    def scale(x: Double) -> Double {
        x * k
    }
    scale(5.0)
}
assert(test() == 15.0, "Single def capture failed")
1.0
