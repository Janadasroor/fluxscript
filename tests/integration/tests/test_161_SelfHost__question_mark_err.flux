enum Res { Ok { value: Double }, Err { msg: Double } }
def make_ok(v: Double) -> Res { Res.Ok { value: v } }
def make_err(v: Double) -> Res { Res.Err { msg: v } }
def step1() -> Res { make_ok(3.0) }
def step2(x: Double) -> Res {
    if (x > 5.0) { make_ok(x + 1.0) } else { make_err(99.0) }
}
def main() -> Double {
    let a = step1()?;
    let b = step2(a)?;
    b
}
