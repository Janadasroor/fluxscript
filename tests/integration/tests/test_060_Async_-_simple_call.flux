async def make_val() -> Double { await 1.0; 7.0 }
def check() -> Double {
    let r = make_val();
    assert(abs(r - 7.0) < 0.0001, "simple call");
    1.0
}
check()
