enum Opt { Some { value: Double }, None }
def unwrap_or_default(x: Opt, def_val: Double) -> Double {
    match x {
        Opt.Some(v) -> v,
        default -> def_val
    }
}
def main() -> Double {
    let a = Opt.Some(42.0);
    let b = Opt.None;
    assert(unwrap_or_default(a, 0.0) == 42.0, "unwrap some wrong");
    assert(unwrap_or_default(b, 99.0) == 99.0, "unwrap none wrong");
    1.0
}
main()
