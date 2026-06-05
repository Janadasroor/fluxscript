async def fetch_data() -> Double {
    await 0.5;
    42.0
}

async def process_data(val: Double) -> Double {
    let data = await fetch_data();
    data + val
}

def verify_async() -> Double {
    let result = process_data(1.0);
    assert(abs(result - 43.0) < 0.0001, "async result");
    1.0
}

verify_async()
