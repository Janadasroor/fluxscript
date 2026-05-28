async def helper() -> Double { await 1.0; 5.0 }
async def user() -> Double {
    let factor = 2.0;
    let base = await helper();
    factor * base
}

def run_check() -> Double {
    let r = user();
    assert(abs(r - 10.0) < 0.0001, "let before await");
    1.0
}
run_check()
