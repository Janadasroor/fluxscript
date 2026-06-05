async def maybe_await(flag: Double) -> Double {
    if flag > 0.5 {
        await 0.5;
        100.0
    } else {
        200.0
    }
}

def run_check() -> Double {
    let r1 = maybe_await(1.0);
    assert(abs(r1 - 100.0) < 0.0001, "cond await true");
    let r2 = maybe_await(0.0);
    assert(abs(r2 - 200.0) < 0.0001, "cond await false");
    1.0
}
run_check()
