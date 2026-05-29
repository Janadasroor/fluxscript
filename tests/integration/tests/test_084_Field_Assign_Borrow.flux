// Test: f.x = val rejected while f is borrowed
struct Foo { x: Double }

def id(x: Double) -> Double { x }

def main() -> Double {
    let f = Foo { x: 10.0 }
    let r = &f.x
    f.x = 20.0
    id(r)
}
main()
