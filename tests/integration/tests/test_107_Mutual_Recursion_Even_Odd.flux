def is_even(n: Double) -> Double {
    if n == 0.0 then 1.0 else is_odd(n - 1.0)
}
def is_odd(n: Double) -> Double {
    if n == 0.0 then 0.0 else is_even(n - 1.0)
}
def main() -> Double {
    assert(is_even(0.0) == 1.0, "even(0) should be true");
    assert(is_even(2.0) == 1.0, "even(2) should be true");
    assert(is_even(3.0) == 0.0, "even(3) should be false");
    assert(is_odd(1.0) == 1.0, "odd(1) should be true");
    assert(is_odd(4.0) == 0.0, "odd(4) should be false");
    1.0
}
main()
