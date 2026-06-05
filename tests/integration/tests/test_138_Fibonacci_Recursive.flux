def fib(n: Double) -> Double {
    if n <= 1.0 then n else fib(n - 1.0) + fib(n - 2.0)
}
def main() -> Double {
    assert(fib(0.0) == 0.0, "fib(0) should be 0");
    assert(fib(1.0) == 1.0, "fib(1) should be 1");
    assert(fib(10.0) == 55.0, "fib(10) should be 55");
    assert(fib(20.0) == 6765.0, "fib(20) should be 6765");
    1.0
}
main()
