// Test: multi-level closure capture — def inside def captures from grandparent
def outer() -> Double {
    var k = 42.0
    def middle() -> Double {
        def inner() -> Double {
            k
        }
        inner()
    }
    middle()
}
assert(outer() == 42.0, "Multi-level def capture failed")
1.0
