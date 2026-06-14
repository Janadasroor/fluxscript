def main() -> Double {
    let x = 42.0;
    let r = &x;
    assert(*r == 42.0, "basic borrow/deref failed");
    *r
}
