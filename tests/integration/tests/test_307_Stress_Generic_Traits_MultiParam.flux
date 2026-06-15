def main() -> Double {
    let a = 10.0
    let b = 20.0
    assert(a == 10.0, "a wrong")
    assert(b == 20.0, "b wrong")
    assert(a + b == 30.0, "sum wrong")
    var total = 0.0
    var i = 0.0
    while i < 100.0 do {
        total = total + i
        i = i + 1.0
    }
    assert(total == 4950.0, "loop sum wrong")
    1.0
}
main()
