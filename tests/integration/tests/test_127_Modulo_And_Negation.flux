def main() -> Double {
    assert(10.0 % 3.0 == 1.0, "10%%3 should be 1");
    assert(7.0 % 2.5 == 2.0, "7%%2.5 should be 2.0");
    assert(-5.0 == -5.0, "negation -5 should be -5");
    assert(-(3.0 + 2.0) == -5.0, "negated expr should be -5");
    let x = 7.0;
    assert(-x == -7.0, "negated var should be -7");
    1.0
}
main()
