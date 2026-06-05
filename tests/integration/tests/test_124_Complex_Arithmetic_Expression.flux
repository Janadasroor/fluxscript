def main() -> Double {
    let r = (1.0 + 2.0) * (3.0 + 4.0) / (5.0 - 1.0) - 2.0;
    assert(abs(r - 3.25) < 0.001, "complex arith wrong");
    let r2 = ((1.0 + 2.0 * 3.0) - 4.0 / 2.0) * 5.0;
    assert(r2 == 25.0, "complex arith 2 wrong");
    1.0
}
main()
