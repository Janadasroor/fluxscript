struct Movable ~Copy {
    x: Double,
    y: Double
}

def main() -> Double {
    let a = Movable { x: 1.0, y: 2.0 }
    let b = a
    assert(b.x == 1.0)
    assert(b.y == 2.0)
    0.0
}
main()
