enum Opt ~Copy { Some { val: Double }, None }
let x = Opt.Some { val: 42.0 };
match x {
    Opt.Some(v) -> v,
    Opt.None -> 0.0
}
