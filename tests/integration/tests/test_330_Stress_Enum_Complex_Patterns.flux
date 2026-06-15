enum Shape { Circle { radius: Double }, Rect { w: Double, h: Double }, Dot }
def area(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 3.14159 * r * r,
        Shape.Rect(w, h) -> w * h,
        Shape.Dot -> 0.0
    }
}
def perimeter(s: Shape) -> Double {
    match s {
        Shape.Circle(r) -> 2.0 * 3.14159 * r,
        Shape.Rect(w, h) -> 2.0 * (w + h),
        Shape.Dot -> 0.0
    }
}
def classify_size(s: Shape) -> Double {
    let a = area(s)
    if a > 100.0 then 3.0
    else if a > 10.0 then 2.0
    else if a > 0.0 then 1.0
    else 0.0
}
def main() -> Double {
    let c = Shape.Circle { radius: 5.0 }
    assert(abs(area(c) - 78.53975) < 0.01, "circle area")
    assert(abs(perimeter(c) - 31.4159) < 0.01, "circle perimeter")
    let r = Shape.Rect { w: 4.0, h: 5.0 }
    assert(area(r) == 20.0, "rect area")
    assert(perimeter(r) == 18.0, "rect perimeter")
    let d = Shape.Dot
    assert(area(d) == 0.0, "dot area")
    assert(perimeter(d) == 0.0, "dot perimeter")
    assert(classify_size(c) == 2.0, "circle classify")
    assert(classify_size(d) == 0.0, "dot classify")
    var total_area = 0.0
    var i = 0.0
    while i < 20.0 do {
        let s = Shape.Rect { w: i + 1.0, h: i + 1.0 }
        total_area = total_area + area(s)
        i = i + 1.0
    }
    assert(total_area == 2870.0, "shape loop wrong")
    let big = Shape.Rect { w: 100.0, h: 100.0 }
    assert(classify_size(big) == 3.0, "big classify")
    let tiny = Shape.Circle { radius: 0.1 }
    assert(classify_size(tiny) == 1.0, "tiny classify")
    1.0
}
main()
