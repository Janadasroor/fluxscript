struct Triple[A, B, C] { a: A, b: B, c: C }
struct Pair[T, U] { first: T, second: U }
def identity[T](x: T) -> T { x }
def test_triple(x: Double) -> Double {
    let t = Triple[Double, Double, Double] { a: x, b: x + 1.0, c: x + 2.0 };
    t.a + t.b + t.c
}
def deep_nest(x: Double) -> Double {
    let p1 = Pair[Double, Double] { first: x, second: x + 1.0 };
    let p2 = Pair[Pair[Double, Double], Double] { first: p1, second: x + 2.0 };
    let p3 = Pair[Pair[Pair[Double, Double], Double], Double] { first: p2, second: x + 3.0 };
    let a = p3.first.first.first;
    let b = p3.first.first.second;
    let c = p3.first.second;
    let d = p3.second;
    a + b + c + d
}
def main() -> Double {
    assert(identity[Double](42.0) == 42.0, "identity wrong");
    assert(test_triple(5.0) == 18.0, "triple sum wrong");
    assert(deep_nest(10.0) == 46.0, "deep nest wrong");
    1.0
}
main()
