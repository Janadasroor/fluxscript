def main() -> Double {
    let a = 5.0
    let b = 10.0
    assert(a == 5.0, "a wrong")
    assert(b == 10.0, "b wrong")
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        total = total + i
        i = i + 1.0
    }
    assert(total == 1225.0, "loop sum wrong")
    1.0
}
main()
