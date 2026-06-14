trait Greeter {
    def greet(self) -> Double
}

struct Hello { val: Double }
struct Goodbye { }

impl Greeter for Hello {
    def greet(self) -> Double { self.val }
}
impl Greeter for Goodbye {
    def greet(self) -> Double { 0.0 }
}

def process(g: dyn Greeter) -> Double {
    g.greet()
}

let a = Hello { val: 5.0 };
let b = Goodbye {};
let r1 = process(a);
let r2 = process(b);
assert(abs(r1 - 5.0) < 0.001, "Hello greet via dyn param wrong");
assert(abs(r2 - 0.0) < 0.001, "Goodbye greet via dyn param wrong");
r1 + r2
