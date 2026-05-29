def test() {
    let x = 5.0;
    let a = &x;
    let b = &x;
    assert(*a + *b == 10.0, "multiple borrow failed");
    1.0
}
test()
