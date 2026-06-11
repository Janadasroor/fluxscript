struct BigPayload { a: Double, b: Double, c: Double, d: Double, e: Double, f: Double, g: Double, h: Double, i: Double, j: Double }
struct Pair[T, U] { first: T, second: U }
def make_big(v: Double) -> BigPayload {
    BigPayload { a: v, b: v+1.0, c: v+2.0, d: v+3.0, e: v+4.0,
                 f: v+5.0, g: v+6.0, h: v+7.0, i: v+8.0, j: v+9.0 }
}
def sum_big(b: BigPayload) -> Double {
    b.a + b.b + b.c + b.d + b.e + b.f + b.g + b.h + b.i + b.j
}
def main() -> Double {
    let big1 = make_big(10.0);
    let s1 = sum_big(big1);
    assert(s1 == 145.0, "big sum wrong");
    let big2 = make_big(100.0);
    assert(sum_big(big2) == 1045.0, "big sum 2 wrong");
    let p = Pair[Double, Double] { first: 1.0, second: 2.0 };
    assert(p.first + p.second == 3.0, "pair wrong");
    var result = 0.0;
    for i in 0, 5 do
        result = result + i;
    assert(result == 10.0, "loop wrong");
    let some = Option[Double].Some(42.0);
    let v = match some {
        Option.Some(x) -> x,
        Option.None -> 0.0
    };
    assert(v == 42.0, "option match");
    1.0
}
main()
