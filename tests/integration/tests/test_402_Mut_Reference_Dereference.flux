// Test: &mut reference dereference assignment
def mutate(x: &mut Double) {
    x = 10.0
}
def test() -> Double {
    let v = 5.0
    mutate(&mut v)
    return v
}

assert(test() == 10.0, "Mut reference dereference failed")
1.0
