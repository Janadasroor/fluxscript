def a(x: Double) -> Double {
    if x <= 0.0 then 0.0 else b(x - 1.0) + 1.0
}
def b(x: Double) -> Double {
    if x <= 0.0 then 0.0 else a(x - 1.0) + 1.0
}
def main() -> Double {
    assert(a(0.0) == 0.0, "a(0) wrong");
    assert(a(5.0) == 5.0, "a(5) wrong");
    assert(b(0.0) == 0.0, "b(0) wrong");
    assert(b(3.0) == 3.0, "b(3) wrong");
    1.0
}
main()
