// Test: Multi-ref-field struct — each field borrows its own target
struct Pair { r1: &Double, r2: &Double }

def test() {
    let a = 10.0
    let b = 20.0
    let p = Pair { r1: &a, r2: &b }
    let x = p.r1
    let y = p.r2
    assert(*x == 10.0, "r1 should be 10.0")
    assert(*y == 20.0, "r2 should be 20.0")
    1.0
}
test()
