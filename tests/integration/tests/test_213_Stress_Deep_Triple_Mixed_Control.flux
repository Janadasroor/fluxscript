struct Pair[T, U] { first: T, second: U }
struct Triple[A, B, C] { a: A, b: B, c: C }
def id[T](x: T) -> T { x }
def main() -> Double {
    let p1 = Pair[Double, Double] { first: 1.0, second: 2.0 };
    let p2 = Pair[Double, Double] { first: 3.0, second: 4.0 };
    let triple = Triple[Pair[Double, Double], Pair[Double, Double], Double] { a: p1, b: p2, c: 5.0 };
    let tsum = triple.a.first + triple.a.second + triple.b.first + triple.b.second + triple.c;
    assert(tsum == 15.0, "triple pair sum wrong");
    let v = id[Double](42.0);
    assert(v == 42.0, "id double wrong");
    var acc = 0.0;
    for i in 0, 5 do {
        if i == 0.0 then
            acc = acc + 1.0
        else if i == 1.0 then
            acc = acc + 10.0
        else if i == 2.0 then
            acc = acc + 100.0
        else
            acc = acc + 1000.0;
    };
    assert(acc == 2111.0, "for if accum wrong");
    var w = 0.0;
    var wi = 0.0;
    while wi < 4.0 do {
        w = w + wi * 2.0;
        wi = wi + 1.0;
    };
    assert(w == 12.0, "while accum wrong");
    let triple2 = Triple[Double, Double, Double] { a: 10.0, b: 20.0, c: 30.0 };
    assert(triple2.a + triple2.b + triple2.c == 60.0, "triple scalar sum wrong");
    let inner = Pair[Double, Double] { first: 100.0, second: 200.0 };
    let outer = Pair[Pair[Double, Double], Double] { first: inner, second: 300.0 };
    assert(outer.first.first + outer.first.second + outer.second == 600.0, "pair chain sum wrong");
    let triple3 = Triple[Double, Triple[Double, Double, Double], Double] {
        a: 5.0, b: triple2, c: 7.0
    };
    assert(triple3.a + triple3.b.a + triple3.b.b + triple3.b.c + triple3.c == 72.0, "nested triple chain wrong");
    1.0
}
main()
