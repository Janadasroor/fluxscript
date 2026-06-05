def compute(a: Double, b: Double) -> Double { a * a + b * b }
def main() -> Double {
    let r = compute(3.0, 4.0);
    assert(r == 25.0, "implicit return 3^2+4^2 should be 25");
    let r2 = compute(5.0, 12.0);
    assert(r2 == 169.0, "implicit return 5^2+12^2 should be 169");
    1.0
}
main()
