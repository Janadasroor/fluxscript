async def source_a() -> Double { await 0.5; 10.0 }
async def source_b() -> Double { await 1.0; 20.0 }
async def source_c() -> Double { await 1.5; 30.0 }

async def combiner() -> Double {
    source_a() + source_b() + source_c()
}

def run_check() -> Double {
    let r = combiner();
    assert(abs(r - 60.0) < 0.0001, "multi async calls");
    1.0
}
run_check()
