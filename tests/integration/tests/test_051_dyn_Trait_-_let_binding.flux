trait Draw {
    def draw(self) -> Double
}

struct Circle { r: Double }
struct Square { s: Double }

impl Draw for Circle {
    def draw(self) -> Double { 3.14159 * self.r * self.r }
}
impl Draw for Square {
    def draw(self) -> Double { self.s * self.s }
}

let c = Circle { r: 2.0 };
let s = Square { s: 3.0 };
let dc: dyn Draw = c;
let ds: dyn Draw = s;
let result = dc.draw() + ds.draw();
assert(abs(result - 21.56636) < 0.001, "dyn Draw area sum wrong");
result
