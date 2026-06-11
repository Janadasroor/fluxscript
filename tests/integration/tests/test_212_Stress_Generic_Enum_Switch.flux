def classify(v: Option[Double]) -> Double {
    match v {
        Option.Some(x) -> x,
        Option.None -> -1.0
    }
}
def main() -> Double {
    let some = Option.Some(21.0);
    let r1 = classify(some);
    assert(r1 == 21.0, "classify some");
    let none = Option.None;
    let r2 = classify(none);
    assert(r2 == -1.0, "classify none");
    let v = match Option.Some(100.0) {
        Option.Some(p) -> p,
        Option.None -> 0.0
    };
    assert(v == 100.0, "inline match");
    1.0
}
main()
