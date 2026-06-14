struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    assert(p.x == 1.0, "init x wrong");
    assert(p.y == 2.0, "init y wrong");
    p.x = 10.0;
    p.y = p.x * 2.0;
    assert(p.x == 10.0, "mut x wrong");
    assert(p.y == 20.0, "mut y wrong");
    1.0
}
main()
