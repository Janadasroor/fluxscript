struct Pair[T, U] { first: T, second: U }
struct Triple[A, B, C] { a: A, b: B, c: C }
def level4(x: Double) -> Double {
    let l1 = Pair[Double, Double] { first: x, second: x + 1.0 };
    let l2 = Pair[Pair[Double, Double], Double] { first: l1, second: x + 2.0 };
    let l3 = Pair[Pair[Pair[Double, Double], Double], Double] { first: l2, second: x + 3.0 };
    let l4 = Pair[Pair[Pair[Pair[Double, Double], Double], Double], Double] { first: l3, second: x + 4.0 };
    l4.first.first.first.first + l4.first.first.first.second +
    l4.first.first.second + l4.first.second + l4.second
}
def mixed_nesting(x: Double) -> Double {
    let p = Pair[Double, Double] { first: x, second: x + 10.0 };
    let t = Triple[Double, Pair[Double, Double], Double] { a: x, b: p, c: x + 20.0 };
    let outer = Pair[Triple[Double, Pair[Double, Double], Double], Double] { first: t, second: x + 30.0 };
    outer.first.a + outer.first.b.first + outer.first.b.second + outer.first.c + outer.second
}
def main() -> Double {
    assert(level4(10.0) == 60.0, "level4 wrong");
    assert(mixed_nesting(1.0) == 74.0, "mixed nesting wrong");
    1.0
}
main()
