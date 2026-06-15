enum Maybe3 {
    One { a: Double },
    Two { a: Double, b: Double },
    Three { a: Double, b: Double, c: Double }
}
def maybe3_sum(m: Maybe3) -> Double {
    match m {
        Maybe3.One(a) -> a,
        Maybe3.Two(a, b) -> a + b,
        Maybe3.Three(a, b, c) -> a + b + c
    }
}
def main() -> Double {
    let m1 = Maybe3.One { a: 1.0 }
    let m2 = Maybe3.Two { a: 2.0, b: 3.0 }
    let m3 = Maybe3.Three { a: 4.0, b: 5.0, c: 6.0 }
    assert(maybe3_sum(m1) == 1.0, "maybe3 one")
    assert(maybe3_sum(m2) == 5.0, "maybe3 two")
    assert(maybe3_sum(m3) == 15.0, "maybe3 three")
    1.0
}
main()
