def fact(n: Double) -> Double {
    if n <= 1.0 then 1.0 else n * fact(n - 1.0)
}
def fib(n: Double) -> Double {
    if n <= 1.0 then n else fib(n - 1.0) + fib(n - 2.0)
}
def ack(m: Double, n: Double) -> Double {
    if m == 0.0 then n + 1.0
    else if n == 0.0 then ack(m - 1.0, 1.0)
    else ack(m - 1.0, ack(m, n - 1.0))
}
def main() -> Double {
    assert(fact(10.0) == 3628800.0, "fact(10) wrong");
    assert(fact(20.0) == 2.432902e18, "fact(20) wrong");
    assert(fib(10.0) == 55.0, "fib(10) wrong");
    assert(fib(20.0) == 6765.0, "fib(20) wrong");
    assert(ack(3.0, 4.0) == 125.0, "ack(3,4) wrong");
    1.0
}
main()
