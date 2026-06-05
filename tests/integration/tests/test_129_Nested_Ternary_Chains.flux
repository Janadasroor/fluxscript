def main() -> Double {
    let x = 5.0;
    let r = if x < 0.0 then -1.0
            else if x == 0.0 then 0.0
            else if x < 10.0 then 1.0
            else if x < 100.0 then 2.0
            else 3.0;
    assert(r == 1.0, "nested ternary chain wrong");
    let x2 = -5.0;
    let r2 = if x2 < 0.0 then -1.0 else if x2 == 0.0 then 0.0 else 1.0;
    assert(r2 == -1.0, "neg ternary chain wrong");
    1.0
}
main()
