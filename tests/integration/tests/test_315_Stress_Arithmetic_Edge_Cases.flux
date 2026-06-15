def main() -> Double {
    let a = 1.0 + 2.0 * 3.0
    assert(a == 7.0, "precedence 1")
    let b = (1.0 + 2.0) * 3.0
    assert(b == 9.0, "precedence 2")
    let c = 2.0 + 3.0 * 4.0 + 5.0
    assert(c == 19.0, "precedence 3")
    let d = (2.0 + 3.0) * (4.0 + 5.0)
    assert(d == 45.0, "precedence 4")
    let e = 100.0 / 4.0 / 5.0
    assert(e == 5.0, "left assoc div")
    let f = 2.0 * 3.0 + 4.0 * 5.0
    assert(f == 26.0, "mixed mul add")
    let g = 10.0 - 3.0 - 2.0
    assert(g == 5.0, "left assoc sub")
    let h = -(3.0 + 4.0)
    assert(h == -7.0, "neg paren")
    let i = -(-5.0)
    assert(i == 5.0, "double neg")
    assert(2.0 % 3.0 == 2.0, "mod 1")
    assert(10.0 % 3.0 == 1.0, "mod 2")
    assert(6.0 % 3.0 == 0.0, "mod 3")
    assert(1.0 / 3.0 < 0.334, "float div")
    assert(1.0 / 3.0 > 0.333, "float div 2")
    var acc = 0.0
    var i = 0.0
    while i < 1000.0 do {
        acc = acc + 1.0 / (i + 1.0)
        i = i + 1.0
    }
    assert(abs(acc - 7.48547086055034) < 0.0001, "harmonic sum wrong")
    let big = 1.0e18
    let small = 1.0e-18
    let product = big * small
    assert(abs(product - 1.0) < 0.001, "big*small wrong")
    var factorial = 1.0
    var j = 1.0
    while j <= 20.0 do {
        factorial = factorial * j
        j = j + 1.0
    }
    assert(abs(factorial - 2432902008176640000.0) < 1.0e8, "20! wrong")
    1.0
}
main()
