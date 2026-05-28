struct Vec2 { x: Double, y: Double }
def test() {
    let v = Vec2 { x: 10.0, y: 20.0 };
    assert(v.x == 10.0, "x wrong");
    assert(v.y == 20.0, "y wrong");
    1.0
}
test()
