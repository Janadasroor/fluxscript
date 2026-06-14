struct Pair[T, U] { first: T, second: U }
enum Opt { Some(Pair[Double, Double]), None }
def pick(opt: Opt) -> Double {
    match opt {
        Opt.Some(p) -> p.first + p.second,
        Opt.None -> -1.0
    }
}
def main() -> Double {
    let a = Opt.Some(Pair[Double, Double] { first: 3.0, second: 4.0 });
    let b = Opt.None;
    assert(pick(a) == 7.0, "generic struct match payload sum wrong");
    assert(pick(b) == -1.0, "generic struct match none wrong");
    1.0
}
main()
