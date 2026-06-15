def risky_add(a: Double, b: Double) -> Double {
    if a < 0.0 then throw("neg a")
    else if b < 0.0 then throw("neg b")
    else a + b
}
def risky_div(a: Double, b: Double) -> Double {
    if b == 0.0 then throw("div by zero")
    else a / b
}
def main() -> Double {
    var caught_count = 0.0
    try { risky_add(-1.0, 5.0) } catch(e) {
        caught_count = caught_count + 1.0
    }
    try { risky_add(5.0, -1.0) } catch(e) {
        caught_count = caught_count + 1.0
    }
    assert(caught_count == 2.0, "catch count wrong")
    let r1 = try { risky_add(3.0, 4.0) } catch(e) { 0.0 }
    assert(r1 == 7.0, "try ok wrong")
    let r2 = try { risky_add(-1.0, 4.0) } catch(e) { -1.0 }
    assert(r2 == -1.0, "try catch wrong")
    let r3 = try { risky_div(10.0, 2.0) } catch(e) { 0.0 }
    assert(r3 == 5.0, "div ok wrong")
    let r4 = try { risky_div(10.0, 0.0) } catch(e) { -999.0 }
    assert(r4 == -999.0, "div catch wrong")
    var sum = 0.0
    var i = 0.0
    while i < 20.0 do {
        let val = try { risky_add(i, i) } catch(e) { -1.0 }
        sum = sum + val
        i = i + 1.0
    }
    assert(sum == 380.0, "try-catch loop wrong")
    1.0
}
main()
