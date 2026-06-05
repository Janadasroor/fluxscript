async def identity(x: Double) -> Double { await x; x }

def run_check() -> Double {
    let r = identity(99.0);
    assert(abs(r - 99.0) < 0.0001, "identity");
    1.0
}
run_check()
