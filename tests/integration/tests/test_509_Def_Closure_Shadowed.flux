// Test: nested def arg shadows outer variable with same name
def test() -> Double {
    var x = 100.0
    def shadow(x: Double) -> Double {
        x  // refers to the parameter, not outer x
    }
    shadow(5.0)  // should return 5.0, not 100.0
}
assert(test() == 5.0, "Shadowed def capture failed")
1.0
