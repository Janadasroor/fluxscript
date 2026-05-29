enum Movable ~Copy {
    A(Double),
    B
}

def main() -> Double {
    let a = Movable.A(42.0)
    let b = a
    match b {
        Movable.A(v) -> assert(v == 42.0)
        default -> assert(false)
    }
    0.0
}
main()
