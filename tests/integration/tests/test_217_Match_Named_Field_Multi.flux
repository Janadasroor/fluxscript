// Named-field match pattern: multi-field enum payload, binding by name in different order

enum Point2D {
    Cartesian { x: Double, y: Double },
    Polar { r: Double, theta: Double }
}

def distance(pt: Point2D) -> Double {
    match pt {
        Point2D.Cartesian { x: px, y: py } -> px * px + py * py,
        Point2D.Polar { r: radius, theta: angle } -> radius
    }
}

def main() -> Double {
    let c = Point2D.Cartesian { x: 3.0, y: 4.0 };
    let p = Point2D.Polar { r: 5.0, theta: 0.0 };
    let d1 = distance(c);
    let d2 = distance(p);
    assert(d1 == 25.0, "named_multi_cartesian");
    assert(d2 == 5.0, "named_multi_polar");
    1.0
}
main()
