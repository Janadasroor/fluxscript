async def sum3(a: Double, b: Double, c: Double) -> Double { await 1.0; a + b + c }

def run_check() -> Double {
    let r = sum3(10.0, 20.0, 30.0);
    assert(abs(r - 60.0) < 0.0001, "multi-param");
    1.0
}
run_check()
