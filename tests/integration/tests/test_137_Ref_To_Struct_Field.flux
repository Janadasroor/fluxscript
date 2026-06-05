struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    let px = &mut p.x;
    *px = 10.0;
    assert(p.x == 10.0, "ref to struct field mutate wrong");
    assert(p.y == 2.0, "other field unchanged");
    1.0
}
main()
