// Test: &f.x rejected when f has mutable borrow active
struct Foo { x: Double }

def id(x: Double) -> Double { x }

def main() -> Double {
    let f = Foo { x: 10.0 }
    let r1 = &f.x
    let r2 = &mut f.x
    id(r1)
}
main()
