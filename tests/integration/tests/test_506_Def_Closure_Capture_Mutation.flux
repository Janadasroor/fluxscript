// Test: captured variable is mutated after def definition, closure sees current value
def test() -> Double {
    var k = 1.0
    def add(x: Double) -> Double {
        x + k
    }
    var r1 = add(2.0)  // k = 1.0 => 2 + 1 = 3
    k = 10.0
    var r2 = add(3.0)  // k = 10.0 => 3 + 10 = 13
    r1 + r2
}
assert(test() == 16.0, "Mutation def capture failed")
1.0
