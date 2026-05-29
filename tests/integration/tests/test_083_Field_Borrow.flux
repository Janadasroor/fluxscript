// Test: &f.x borrows f, blocks assignment to f
struct Foo { x: Double }

def id(x: Double) -> Double { x }

def main() -> Double {
    let f = Foo { x: 10.0 }
    let r = &f.x
    f = Foo { x: 20.0 }
    id(r)
}
main()
