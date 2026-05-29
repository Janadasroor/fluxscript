struct Point { x: Double, y: Double }

def swap_y(a: &mut Point, b: &mut Point) {
    let tmp = a.y;
    a.y = b.y;
    b.y = tmp;
    1.0
}

def test() {
    let p1 = Point { x: 1.0, y: 10.0 };
    let p2 = Point { x: 2.0, y: 20.0 };

    let r = &p1.x;
    assert(*r == 1.0, "immut struct field borrow failed");

    let ry = &mut p1.y;
    *ry = 100.0;
    assert(p1.y == 100.0, "mut struct field assign failed");

    swap_y(&mut p1, &mut p2);
    assert(p1.y == 20.0, "swap through ref failed");
    assert(p2.y == 100.0, "swap through ref 2 failed");

    let a = &p1.x;
    let b = &p2.x;
    assert(*a + *b == 3.0, "multi struct field borrow failed");

    1.0
}
test()
