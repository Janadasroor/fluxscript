def test() {
    let x = 5.0;
    let r = &mut x;
    *r = 10.0;
    assert(x == 10.0, "mut borrow assign failed");
    1.0
}
test()
