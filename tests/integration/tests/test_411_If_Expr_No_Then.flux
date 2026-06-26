def main() -> Double {
    let a = if 1.0 == 1.0 42.0 else 0.0;
    assert(a == 42.0, "if-no-then true failed");

    let b = if 1.0 == 0.0 42.0 else 99.0;
    assert(b == 99.0, "if-no-then false failed");

    let c = if 2.0 > 1.0 10.0 else 20.0;
    assert(c == 10.0, "if-gt no-then failed");
    1.0
}
main()
