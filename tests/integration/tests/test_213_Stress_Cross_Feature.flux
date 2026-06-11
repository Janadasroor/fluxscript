struct Pair[T, U] { first: T, second: U }
def identity[T](x: T) -> T { x }
def map_option(opt: Option[Double]) -> Double {
    match opt {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
def main() -> Double {
    let some1 = Option.Some(100.0);
    assert(map_option(some1) == 100.0, "map some");
    let none = Option.None;
    assert(map_option(none) == 0.0, "map none");
    let p = Pair[Double, Double] { first: identity[Double](3.0), second: 4.0 };
    assert(p.first + p.second == 7.0, "generic pair wrong");
    let v = identity[Double](42.0);
    assert(v == 42.0, "identity wrong");
    1.0
}
main()
