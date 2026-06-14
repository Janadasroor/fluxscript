enum Opt { Some { value: Double }, None }
def add_one(x: Opt) -> Double {
    match x {
        Opt.Some(v) -> { v + 1.0 },
        default -> 0.0
    }
}
def main() -> Double {
    assert(add_one(Opt.Some(41.0)) == 42.0, "match var add wrong");
    assert(add_one(Opt.None) == 0.0, "match var none wrong");
    1.0
}
main()
