def f1(x: Double) -> Double { x * 2.0 }
def f2(x: Double) -> Double { x + 1.0 }
def composite(x: Double) -> Double { f2(f1(x)) }
def main() -> Double {
    assert(composite(5.0) == 11.0, "composite 5*2+1 should be 11");
    assert(composite(0.0) == 1.0, "composite 0*2+1 should be 1");
    1.0
}
main()
