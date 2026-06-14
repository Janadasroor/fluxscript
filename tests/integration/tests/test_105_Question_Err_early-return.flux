enum Result { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Result { Result.Ok { value: v } }
def make_err(v: Double) -> Result { Result.Err { msg: v } }
def div_with(a: Double, b: Double) -> Result {
    if (b == 0.0) { make_err(99.0) } else { make_ok(a / b) }
}
def chain_err() -> Result {
    let a = div_with(10.0, 0.0)?;
    div_with(a, 5.0)
}
def extract_msg(r: Result) -> Double {
    match r {
        Result.Ok(v) -> v,
        Result.Err(m) -> m
    }
}
extract_msg(chain_err())
