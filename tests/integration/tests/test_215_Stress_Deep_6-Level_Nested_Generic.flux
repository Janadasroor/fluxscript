struct Pair[T, U] { first: T, second: U }
def id[T](x: T) -> T { x }
def main() -> Double {
    let l1 = Pair[Double, Double] { first: 10.0, second: 11.0 };
    let l2 = Pair[Pair[Double, Double], Double] { first: l1, second: 12.0 };
    let l3 = Pair[Pair[Pair[Double, Double], Double], Double] { first: l2, second: 13.0 };
    let l4 = Pair[Pair[Pair[Pair[Double, Double], Double], Double], Double] { first: l3, second: 14.0 };
    let l5 = Pair[Pair[Pair[Pair[Pair[Double, Double], Double], Double], Double], Double] { first: l4, second: 15.0 };
    let l6 = Pair[Pair[Pair[Pair[Pair[Pair[Double, Double], Double], Double], Double], Double], Double] { first: l5, second: 16.0 };
    let r = l6.first.first.first.first.first.first + l6.first.first.first.first.first.second +
        l6.first.first.first.first.second + l6.first.first.first.second +
        l6.first.first.second + l6.first.second + l6.second;
    assert(r == 91.0, "six level sum wrong");
    let v = id[Double](100.0);
    assert(v == 100.0, "id double wrong");
    let deep_base = l6.first.first.first.first.first.first;
    let deep_second = l6.first.first.first.first.first.second;
    assert(deep_base == 10.0, "deep first wrong");
    assert(deep_second == 11.0, "deep second wrong");
    var acc = 0.0;
    var idx = 0.0;
    while idx < 3.0 do {
        let p = Pair[Double, Double] { first: idx, second: idx * 2.0 };
        let pp = Pair[Pair[Double, Double], Double] { first: p, second: idx * 3.0 };
        let ppp = Pair[Pair[Pair[Double, Double], Double], Double] { first: pp, second: idx * 4.0 };
        acc = acc + ppp.first.first.first + ppp.first.first.second + ppp.first.second + ppp.second;
        idx = idx + 1.0;
    };
    assert(acc == 30.0, "while nested construct sum wrong");
    let l3b = Pair[Pair[Pair[Double, Double], Double], Double] { first: l2, second: 99.0 };
    let r2 = l3b.first.first.first + l3b.first.first.second + l3b.first.second + l3b.second;
    assert(r2 == 132.0, "reuse l1-l2 in new l3 wrong");
    1.0
}
main()
