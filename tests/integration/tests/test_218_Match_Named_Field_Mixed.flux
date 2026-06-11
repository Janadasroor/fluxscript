// Named-field match pattern: mixed arms (named-field and positional in same match)
// Also tests wildcard _ arm

enum Shape {
    Circle { radius: Double },
    Rect { width: Double, height: Double },
    Dot {}
}

def area(s: Shape) -> Double {
    match s {
        Shape.Circle { radius: r } => r * r * 3.14159265,
        Shape.Rect { width: w, height: h } => w * h,
        Shape.Dot {} => 0.0
    }
}

def main() -> Double {
    let c = Shape.Circle { radius: 2.0 };
    let r = Shape.Rect { width: 3.0, height: 4.0 };
    let d = Shape.Dot {};
    let a1 = area(c);
    let a2 = area(r);
    let a3 = area(d);
    assert(a3 == 0.0, "dot_area");
    assert(a2 == 12.0, "rect_area");
    // pi * 4 ~= 12.566
    assert(a1 > 12.5 && a1 < 12.6, "circle_area");
    1.0
}
main()
