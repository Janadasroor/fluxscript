async def inner(x: Double) -> Double {
    await x;
    x
}
async def middle(x: Double) -> Double {
    let a = await inner(x);
    a + 1.0
}
async def multi_await(x: Double) -> Double {
    let a = await inner(x);
    let b = await inner(x + 1.0);
    a + b + 2.0
}
def main() -> Double {
    let r = middle(0.0);
    assert(r == 1.0, "async chain wrong");
    let r2 = multi_await(0.0);
    assert(r2 == 3.0, "multi-await wrong");
    1.0
}
main()
