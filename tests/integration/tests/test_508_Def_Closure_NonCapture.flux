// Test: nested def that does NOT capture anything (no closure overhead)
def test() -> Double {
    def identity(x: Double) -> Double {
        x
    }
    identity(42.0)
}
assert(test() == 42.0, "Non-capture def failed")
1.0
