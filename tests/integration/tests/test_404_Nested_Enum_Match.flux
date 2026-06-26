enum Inner {
    A { x: Double },
    B
}

enum Outer {
    Wrap { inner: Inner },
    Empty
}

def test() -> Double {
    let o = Outer.Wrap { inner: Inner.A { x: 5.0 } }
    match o {
        Outer.Wrap(w) -> match w {
            Inner.A(v) -> v
            Inner.B -> 0.0
        }
        Outer.Empty -> -1.0
    }
}

assert(test() == 5.0, "Nested enum match failed")
1.0
