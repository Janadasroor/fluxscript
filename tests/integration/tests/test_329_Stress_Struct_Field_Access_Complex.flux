struct Vec2 { x: Double, y: Double }
struct AABB { min: Vec2, max: Vec2 }
struct Circle { center: Vec2, radius: Double }
struct ShapeUnion { circle: Circle, rect: AABB, kind: Double }
def aabb_area(bb: AABB) -> Double {
    let w = bb.max.x - bb.min.x
    let h = bb.max.y - bb.min.y
    w * h
}
def circle_area(c: Circle) -> Double {
    3.14159 * c.radius * c.radius
}
def point_in_aabb(p: Vec2, bb: AABB) -> Double {
    if p.x >= bb.min.x && p.x <= bb.max.x && p.y >= bb.min.y && p.y <= bb.max.y then 1.0
    else 0.0
}
def aabb_center(bb: AABB) -> Vec2 {
    Vec2 { x: (bb.min.x + bb.max.x) * 0.5, y: (bb.min.y + bb.max.y) * 0.5 }
}
def main() -> Double {
    let bb = AABB { min: Vec2 { x: 0.0, y: 0.0 }, max: Vec2 { x: 10.0, y: 5.0 } }
    assert(aabb_area(bb) == 50.0, "aabb area")
    let center = aabb_center(bb)
    assert(center.x == 5.0, "aabb center x")
    assert(center.y == 2.5, "aabb center y")
    let c = Circle { center: Vec2 { x: 5.0, y: 5.0 }, radius: 3.0 }
    assert(abs(circle_area(c) - 28.27431) < 0.01, "circle area")
    assert(point_in_aabb(Vec2 { x: 5.0, y: 2.5 }, bb) == 1.0, "point in")
    assert(point_in_aabb(Vec2 { x: 15.0, y: 2.5 }, bb) == 0.0, "point out")
    assert(point_in_aabb(bb.min, bb) == 1.0, "point min")
    assert(point_in_aabb(bb.max, bb) == 1.0, "point max")
    let su = ShapeUnion { circle: c, rect: bb, kind: 1.0 }
    assert(su.kind == 1.0, "union kind")
    assert(su.circle.radius == 3.0, "union circle radius")
    assert(aabb_area(su.rect) == 50.0, "union rect area")
    var total = 0.0
    var i = 0.0
    while i < 20.0 do {
        let test_bb = AABB {
            min: Vec2 { x: 0.0, y: 0.0 },
            max: Vec2 { x: i + 1.0, y: i + 1.0 }
        }
        total = total + aabb_area(test_bb)
        i = i + 1.0
    }
    assert(total == 2870.0, "aabb loop wrong")
    let nested = AABB {
        min: Vec2 { x: AABB { min: Vec2 { x: 1.0, y: 2.0 }, max: Vec2 { x: 3.0, y: 4.0 } }.min.x, y: 0.0 },
        max: Vec2 { x: 10.0, y: 10.0 }
    }
    assert(nested.min.x == 1.0, "nested struct access")
    1.0
}
main()
