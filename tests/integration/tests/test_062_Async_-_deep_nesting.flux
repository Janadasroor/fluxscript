async def level3() -> Double { await 0.5; 3.0 }
async def level2() -> Double { let v = await level3(); v + 2.0 }
async def level1() -> Double { let v = await level2(); v + 1.0 }

def run_test() -> Double {
    let r = level1();
    assert(abs(r - 6.0) < 0.0001, "deep nested");
    1.0
}
run_test()
