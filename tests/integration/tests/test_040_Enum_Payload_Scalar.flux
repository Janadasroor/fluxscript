enum Wrap { Value { x: Double }, Empty }
let w = Wrap.Value(3.14)
match w {
    Wrap.Value(v) -> v,
    default -> -1.0
}
