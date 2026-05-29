struct Movable ~Copy {
    x: Double
}

def take(m: Movable) -> Double {
    m.x
}

def main() -> Double {
    let a = Movable { x: 42.0 }
    let result = take(a)
    assert(result == 42.0)
    0.0
}
main()
