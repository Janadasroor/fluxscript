def main() -> Double {
    let x = 1.0
    let x2 = x + 1.0
    let x3 = x2 + 1.0
    assert(x3 == 3.0, "shadow chain")
    let a = 10.0
    let b = {
        let a2 = a * 2.0
        let b2 = a2 + 5.0
        a2 + b2
    }
    assert(b == 45.0, "let-in block")
    let result = {
        let tmp1 = 3.0 * 4.0
        let tmp2 = tmp1 + 1.0
        let tmp3 = tmp2 * 2.0
        tmp3
    }
    assert(result == 26.0, "nested let-in")
    let nested = {
        let outer = 10.0
        let inner = {
            let deep = outer * 3.0
            deep + 1.0
        }
        inner + outer
    }
    assert(nested == 41.0, "nested blocks")
    var acc = 0.0
    var i = 0.0
    while i < 100.0 do {
        let val = {
            let base = i
            let modified = base * 2.0 + 1.0
            modified
        }
        acc = acc + val
        i = i + 1.0
    }
    assert(acc == 10000.0, "let-in loop wrong")
    1.0
}
main()
