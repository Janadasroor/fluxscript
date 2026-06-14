def fact(n: Double) -> Double {
    if n <= 1.0 then 1.0 else n * fact(n - 1.0)
}
def main() -> Double {
    assert(fact(0.0) == 1.0, "fact(0) wrong");
    assert(fact(1.0) == 1.0, "fact(1) wrong");
    assert(fact(5.0) == 120.0, "fact(5) wrong");
    assert(fact(10.0) == 3628800.0, "fact(10) wrong");
    1.0
}
main()
