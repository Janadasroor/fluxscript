struct Point { x: Double, y: Double }
struct Line { start: Point, end: Point }
def modify_point(p: Point, dx: Double, dy: Double) -> Point {
    Point { x: p.x + dx, y: p.y + dy }
}
def line_length_squared(l: Line) -> Double {
    let dx = l.end.x - l.start.x
    let dy = l.end.y - l.start.y
    dx * dx + dy * dy
}
def main() -> Double {
    let p = Point { x: 1.0, y: 2.0 }
    let p2 = modify_point(p, 3.0, 4.0)
    assert(p2.x == 4.0, "modify x wrong")
    assert(p2.y == 6.0, "modify y wrong")
    let l = Line { start: Point { x: 0.0, y: 0.0 }, end: Point { x: 3.0, y: 4.0 } }
    let ls = line_length_squared(l)
    assert(ls == 25.0, "length_sq wrong")
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        let pt = Point { x: i, y: i * 2.0 }
        let pt2 = modify_point(pt, 1.0, 1.0)
        total = total + pt2.x + pt2.y
        i = i + 1.0
    }
    assert(total == 3775.0, "struct loop wrong")
    1.0
}
main()
