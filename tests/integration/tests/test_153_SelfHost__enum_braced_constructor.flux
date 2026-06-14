enum Option { Some { value: Double }, None }
def main() -> Double {
    let x = Option.Some { value: 42.0 };
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
