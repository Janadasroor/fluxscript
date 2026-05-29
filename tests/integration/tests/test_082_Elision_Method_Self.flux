// Test: Method returning &T borrows from self, blocks assignment to self
struct Foo { x: Double }

impl Foo {
    def get_x(self: &Foo) -> &Double { self.x }
}

def id(x: Double) -> Double { x }

def main() -> Double {
    let f = Foo { x: 10.0 }
    let r = f.get_x()
    f = Foo { x: 20.0 }
    id(r)
}
main()
