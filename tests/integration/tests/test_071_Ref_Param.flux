def add_one(x: &mut Double) -> Double {
    *x = *x + 1.0;
    *x
}

def test() {
    let val = 5.0;
    let result = add_one(&mut val);
    assert(result == 6.0, "mut ref param result wrong");
    assert(val == 6.0, "mut ref param side effect wrong");
    1.0
}
test()
