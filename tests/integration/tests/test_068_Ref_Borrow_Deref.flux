def test() {
    let x = 5.0;
    let r = &x;
    assert(*r == 5.0, "basic borrow/deref failed");
    1.0
}
test()
