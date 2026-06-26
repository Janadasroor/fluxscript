enum Option {
    Some { value: Double },
    None
}

def unwrap_or_val(x: Option, fallback: Double) -> Double {
    match x {
        Option.Some(p) => p + 1.0,
        Option.None => fallback
    }
}

def test() -> Double {
    let some_val = Option.Some { value: 10.0 }
    let none_val = Option.None
    unwrap_or_val(some_val, 0.0) + unwrap_or_val(none_val, 5.0)
}

assert(test() == 16.0, "Fat arrow match syntax failed")
1.0
