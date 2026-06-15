struct Point { x: Double, y: Double, z: Double }
struct Segment { start: Point, end: Point }
struct Triangle { a: Point, b: Point, c: Point }
def dist_sq(a: Point, b: Point) -> Double {
    let dx = b.x - a.x
    let dy = b.y - a.y
    let dz = b.z - a.z
    dx * dx + dy * dy + dz * dz
}
def midpoint(s: Segment) -> Point {
    Point {
        x: (s.start.x + s.end.x) * 0.5,
        y: (s.start.y + s.end.y) * 0.5,
        z: (s.start.z + s.end.z) * 0.5
    }
}
def shift_point(p: Point, dx: Double, dy: Double, dz: Double) -> Point {
    Point { x: p.x + dx, y: p.y + dy, z: p.z + dz }
}
def scale_point(p: Point, s: Double) -> Point {
    Point { x: p.x * s, y: p.y * s, z: p.z * s }
}
def main() -> Double {
    let p1 = Point { x: 0.0, y: 0.0, z: 0.0 }
    let p2 = Point { x: 3.0, y: 4.0, z: 0.0 }
    let p3 = Point { x: 6.0, y: 0.0, z: 0.0 }
    assert(dist_sq(p1, p2) == 25.0, "dist wrong")
    let seg = Segment { start: p1, end: p2 }
    let mid = midpoint(seg)
    assert(mid.x == 1.5, "mid.x wrong")
    assert(mid.y == 2.0, "mid.y wrong")
    assert(mid.z == 0.0, "mid.z wrong")
    let shifted = shift_point(p1, 1.0, 2.0, 3.0)
    assert(shifted.x == 1.0, "shift.x wrong")
    assert(shifted.y == 2.0, "shift.y wrong")
    assert(shifted.z == 3.0, "shift.z wrong")
    let scaled = scale_point(p2, 2.0)
    assert(scaled.x == 6.0, "scale.x wrong")
    assert(scaled.y == 8.0, "scale.y wrong")
    assert(scaled.z == 0.0, "scale.z wrong")
    var total = 0.0
    var i = 0.0
    while i < 100.0 do {
        let pp = Point { x: i, y: i * 2.0, z: i * 3.0 }
        total = total + pp.x + pp.y + pp.z
        i = i + 1.0
    }
    assert(total == 29700.0, "struct loop sum wrong")
    1.0
}
main()
