struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    assert(p.x == 1.0, "struct field x wrong");
    assert(p.y == 2.0, "struct field y wrong");
    1.0
}
main()
