async def leaf(x: Double) -> Double {
    let a = await x;
    a + 1.0
}
async def stage2(x: Double) -> Double {
    let a = await leaf(x);
    let b = await leaf(a);
    a + b
}
async def stage3(x: Double) -> Double {
    let a = await stage2(x);
    let b = if a > 10.0 then a * 2.0 else a / 2.0;
    b
}
async def top(x: Double) -> Double {
    let a = await stage3(x);
    var sum = 0.0;
    for i in 0, 3 do {
        sum = sum + i + a;
    };
    let b = await leaf(a);
    sum + b + 1.0
}
def main() -> Double {
    assert(top(2.0) == 19.0, "top(2) wrong");
    assert(top(5.0) == 109.0, "top(5) wrong");
    1.0
}
main()
