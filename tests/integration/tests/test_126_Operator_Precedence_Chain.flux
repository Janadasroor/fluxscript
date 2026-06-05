def main() -> Double {
    let r = 2.0 + 3.0 * 4.0 - 6.0 / 2.0;
    assert(r == 11.0, "precedence 2+3*4-6/2 should be 11");
    let r2 = 10.0 / 2.0 + 3.0 * 2.0 - 1.0;
    assert(r2 == 10.0, "precedence 10/2+3*2-1 should be 10");
    let r3 = 100.0 / 4.0 / 5.0;
    assert(r3 == 5.0, "left assoc 100/4/5 should be 5");
    1.0
}
main()
