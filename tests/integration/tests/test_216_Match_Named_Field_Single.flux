// Named-field match pattern: single-field enum payload using braced syntax
// e.g. Option.Some { value: v } => v + 1.0

enum Option {
    Some { value: Double },
    None {}
}

def unwrap_add(opt: Option) -> Double {
    match opt {
        Option.Some { value: v } => v + 1.0,
        Option.None {} => 0.0
    }
}

def main() -> Double {
    let a = Option.Some { value: 41.0 };
    let b = Option.None {};
    let r1 = unwrap_add(a);
    let r2 = unwrap_add(b);
    assert(r1 == 42.0, "named_single_some");
    assert(r2 == 0.0, "named_single_none");
    1.0
}
main()
