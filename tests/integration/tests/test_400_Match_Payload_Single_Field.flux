// Test: single-field enum payload binds to field value (not whole struct)
enum Option {
    Some { value: Double },
    None
}

def test() -> Double {
    let x = Option.Some { value: 42.0 }
    match x {
        Option.Some(p) -> p + 1.0
        Option.None -> 0.0
    }
}

assert(test() == 43.0, "Match payload single field binding failed")
1.0
