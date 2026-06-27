// Test: multi-level closure capture with mutation at middle level
// middle captures x from outer by value, modifies its local copy,
// then inner sees the modified value via its own capture.
def outer() -> Double {
    var x = 10.0
    def middle() -> Double {
        def inner() -> Double {
            x
        }
        x = x + 5.0
        inner()
    }
    middle()
}
assert(outer() == 15.0, "Multi-level def mutation in middle failed")
1.0
