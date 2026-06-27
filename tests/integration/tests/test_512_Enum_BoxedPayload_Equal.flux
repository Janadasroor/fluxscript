// Test: boxed (heap-allocated) enum payload deep equality comparison
// Struct payload > 16 bytes triggers boxing
struct Big {
    a: Double
    b: Double
    c: Double
}

enum Boxed {
    Small(x: Double)
    BigVal(p: Big)
}

// Two identical Big structs as boxed payloads — should compare equal
let b1 = Big { a: 1.0, b: 2.0, c: 3.0 }
let b2 = Big { a: 1.0, b: 2.0, c: 3.0 }

let e1 = Boxed.BigVal(b1)
let e2 = Boxed.BigVal(b2)
let e3 = Boxed.BigVal(Big { a: 4.0, b: 5.0, c: 6.0 })

assert(e1 == e2, "Boxed enum BigVal deep equal should be true")
assert(e1 != e3, "Boxed enum BigVal deep not-equal should be true")

// Small (non-boxed) variant comparison should still work
let s1 = Boxed.Small(42.0)
let s2 = Boxed.Small(42.0)
let s3 = Boxed.Small(99.0)

assert(s1 == s2, "Non-boxed enum Small equal should be true")
assert(s1 != s3, "Non-boxed enum Small not-equal should be true")

// Cross-variant comparison should be false
assert(e1 != s1, "Cross-variant enum comparison should be false")
assert(e2 != s2, "Cross-variant enum comparison should be false")

1.0
