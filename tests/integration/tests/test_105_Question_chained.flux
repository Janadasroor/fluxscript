enum Result { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Result { Result.Ok { value: v } }
def make_err(v: Double) -> Result { Result.Err { msg: v } }
def step1() -> Result { make_ok(7.0) }
def step2(x: Double) -> Result {
    if (x > 5.0) { make_ok(x + 1.0) } else { make_err(1.0) }
}
def step3(x: Double) -> Result {
    if (x > 8.0) { make_ok(x * 2.0) } else { make_err(2.0) }
}
def pipeline() -> Result {
    let a = step1()?;
    let b = step2(a)?;
    step3(b)
}
match pipeline() {
    Result.Ok(v) -> v,
    default -> -1.0
}
