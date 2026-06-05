async def inner() -> Double { await 0.5; 5.0 }
async def outer() -> Double { await inner() + 3.0 }

def run_check() -> Double {
    let r = outer();
    assert(abs(r - 8.0) < 0.0001, "nested await expr");
    1.0
}
run_check()
