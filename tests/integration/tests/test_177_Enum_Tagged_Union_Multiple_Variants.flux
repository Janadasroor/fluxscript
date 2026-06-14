enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Nothing }
def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        default -> 0.0
    }
}
def main() -> Double {
    let c = Shape.Circle(2.0);
    let r = Shape.Rect(3.0, 4.0);
    let n = Shape.Nothing;
    assert(abs(area(c) - 12.56636) < 0.001, "circle area wrong");
    assert(area(r) == 12.0, "rect area wrong");
    assert(area(n) == 0.0, "nothing area wrong");
    1.0
}
main()
