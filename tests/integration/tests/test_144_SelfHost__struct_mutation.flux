struct Point { x: Double, y: Double }
def main() -> Double {
    var p = Point { x: 1.0, y: 2.0 };
    p.x = 42.0;
    p.x
}
