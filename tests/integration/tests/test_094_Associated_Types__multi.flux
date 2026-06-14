trait Pair {
    type First;
    type Second;
    def first(self) -> First
    def second(self) -> Second
}

struct TwoValues {
    a: Double,
    b: Double
}

impl Pair for TwoValues {
    type First = Double;
    type Second = Double;
    def first(self) -> Double { self.a }
    def second(self) -> Double { self.b }
}

def test() -> Double {
    let t = TwoValues { a: 10.0, b: 20.0 };
    let f = t.first();
    let s = t.second();
    assert(abs(f - 10.0) < 0.001, "assoc type first failed");
    assert(abs(s - 20.0) < 0.001, "assoc type second failed");
    1.0
}
test()
