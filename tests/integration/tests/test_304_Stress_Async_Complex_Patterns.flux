async def delayed_add(a: Double, b: Double) -> Double {
    await 0.5
    a + b
}
async def delayed_mul(a: Double, b: Double) -> Double {
    await 0.5
    a * b
}
async def chain3(x: Double) -> Double {
    let a = await delayed_add(x, 1.0)
    let b = await delayed_add(a, 2.0)
    let c = await delayed_add(b, 3.0)
    c
}
async def parallel_sum(a: Double, b: Double, c: Double) -> Double {
    let x = await delayed_add(a, 0.0)
    let y = await delayed_add(b, 0.0)
    let z = await delayed_add(c, 0.0)
    x + y + z
}
async def nested_async(depth: Double, val: Double) -> Double {
    if depth <= 0.0 then val
    else {
        let half = await delayed_add(val, depth)
        nested_async(depth - 1.0, half)
    }
}
def main() -> Double {
    let r1 = chain3(10.0)
    assert(r1 == 16.0, "chain3 wrong")
    let r2 = chain3(0.0)
    assert(r2 == 6.0, "chain3 zero wrong")
    let r3 = parallel_sum(10.0, 20.0, 30.0)
    assert(r3 == 60.0, "parallel_sum wrong")
    let r4 = nested_async(5.0, 0.0)
    assert(r4 == 15.0, "nested_async wrong")
    let r5 = nested_async(10.0, 1.0)
    assert(r5 == 56.0, "nested_async deep wrong")
    1.0
}
main()
