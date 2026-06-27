// Test: boxed enum payload edge cases — comparison only
struct Big {
    a: Double
    b: Double
    c: Double
}

// Only boxed variants (all > 16 bytes)
enum AllBoxed {
    V1(p: Big)
    V2(p: Big)
}

let b1 = Big { a: 1.0, b: 2.0, c: 3.0 }
let b2 = Big { a: 1.0, b: 2.0, c: 3.0 }
let b3 = Big { a: 9.0, b: 8.0, c: 7.0 }

// Same variant, same values
assert(AllBoxed.V1(b1) == AllBoxed.V1(b2), "AllBoxed same variant same value should be equal")

// Same variant, different values
assert(AllBoxed.V1(b1) != AllBoxed.V1(b3), "AllBoxed same variant different value should not be equal")

// Different variants, same values
assert(AllBoxed.V1(b1) != AllBoxed.V2(b1), "AllBoxed different variant should not be equal")

// Different variants, different values
assert(AllBoxed.V1(b1) != AllBoxed.V2(b3), "AllBoxed different variant different values should not be equal")

// Single boxed variant
enum SingleBoxed {
    Wrap(p: Big)
}

let w1 = SingleBoxed.Wrap(Big { a: 10.0, b: 20.0, c: 30.0 })
let w2 = SingleBoxed.Wrap(Big { a: 10.0, b: 20.0, c: 30.0 })
let w3 = SingleBoxed.Wrap(Big { a: 1.0, b: 2.0, c: 3.0 })

assert(w1 == w2, "SingleBoxed same values should be equal")
assert(w1 != w3, "SingleBoxed different values should not be equal")

// Compare within condition
let cond = if w1 == w2 { 1.0 } else { 0.0 }
assert(cond == 1.0, "Boxed compare in if condition should work")

// Non-equal in if condition
let cond2 = if w1 != w3 { 1.0 } else { 0.0 }
assert(cond2 == 1.0, "Boxed != in if condition should work")

1.0
