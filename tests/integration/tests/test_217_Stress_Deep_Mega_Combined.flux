struct Pair[T, U] { first: T, second: U }
def id[T](x: T) -> T { x }
enum Shape { Circle(Double), Rect { w: Double, h: Double }, Nothing }
def classify(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 2.0 * r,
        Shape.Rect(w, h) -> w + h,
        default -> 0.0
    }
}
def main() -> Double {
    assert(classify(Shape.Circle(3.0)) == 6.0, "circle wrong");
    assert(classify(Shape.Rect(4.0, 5.0)) == 9.0, "rect wrong");
    assert(classify(Shape.Nothing) == 0.0, "nothing wrong");
    let p1 = Pair[Double, Double] { first: 1.0, second: 2.0 };
    let p2 = Pair[Double, Double] { first: 3.0, second: 4.0 };
    let p3 = Pair[Pair[Double, Double], Pair[Double, Double]] { first: p1, second: p2 };
    assert(p3.first.first + p3.first.second + p3.second.first + p3.second.second == 10.0, "nested pair sum wrong");
    assert(id[Double](42.0) == 42.0, "id double wrong");
    let opt_some = Option.Some(100.0);
    let sv = match opt_some { Option.Some(x) -> x, Option.None -> 0.0 };
    let opt_none = Option.None;
    let nv = match opt_none { Option.Some(x) -> x, Option.None -> -1.0 };
    assert(sv == 100.0, "some match wrong");
    assert(nv == -1.0, "none match wrong");
    var accum = 0.0;
    var wi = 0.0;
    while wi < 4.0 do {
        accum = accum + classify(Shape.Rect(wi, wi + 1.0));
        wi = wi + 1.0;
    };
    assert(accum == 16.0, "while shape accum wrong");
    var total = 0.0;
    for i in 0, 3 do {
        let lp = Pair[Double, Double] { first: i, second: i * 2.0 };
        total = total + lp.first + lp.second;
        var jtotal = 0.0;
        for j in 0, 2 do {
            let inner = Pair[Double, Double] { first: j, second: j * 3.0 };
            jtotal = jtotal + inner.first + inner.second;
        };
        total = total + jtotal;
    };
    assert(total == 21.0, "nested for sum wrong");
    assert(id[Double](99.0) + classify(Shape.Circle(5.0)) == 109.0, "combined id+classify wrong");
    let pair_nest = Pair[Pair[Double, Double], Double] { first: Pair[Double, Double]{first:7.0,second:8.0}, second: 9.0 };
    assert(pair_nest.first.first + pair_nest.first.second + pair_nest.second == 24.0, "inline nested pair wrong");
    1.0
}
main()
