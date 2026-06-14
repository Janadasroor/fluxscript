enum Option { Some(value: Double), None }
def main() -> Double {
    var x = Option.Some(10.0);
    x = Option.Some(42.0);
    match x {
        Option.Some(v) -> v,
        Option.None -> 0.0
    }
}
