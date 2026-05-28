enum Option { Some { value: Double }, None }
let five = Option.None
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
