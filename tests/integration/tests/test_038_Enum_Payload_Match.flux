enum Option { Some { value: Double }, None }
let five = Option.Some(5.0)
match five {
    Option.Some(v) -> v,
    default -> -1.0
}
