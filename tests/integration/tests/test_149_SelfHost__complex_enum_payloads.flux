enum Shape { Circle(r: Double), Rect(w: Double, h: Double), Empty }
def main() -> Double {
    let s = Shape.Rect { w: 3.0, h: 4.0 };
    match s {
        Shape.Circle(r) -> r,
        Shape.Rect(pl) -> pl.w * pl.h,
        Shape.Empty -> 0.0
    }
}
