struct Movable ~Copy {
    x: Double
}

def main() -> Double {
    let a = Movable { x: 1.0 }
    let b = a
    let c = a
    0.0
}
main()
