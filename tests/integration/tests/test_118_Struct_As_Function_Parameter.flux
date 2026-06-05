struct Pair { a: Double, b: Double }
def sum_pair(p: Pair) -> Double { p.a + p.b }
def main() -> Double {
    let p = Pair { a: 10.0, b: 20.0 };
    let r = sum_pair(p);
    assert(r == 30.0, "struct param sum wrong");
    1.0
}
main()
