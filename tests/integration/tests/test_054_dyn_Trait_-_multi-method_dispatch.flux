trait Shape {
    def area(self) -> Double
    def perimeter(self) -> Double
}

struct Rect { w: Double, h: Double }
struct Circle2 { r: Double }

impl Shape for Rect {
    def area(self) -> Double { self.w * self.h }
    def perimeter(self) -> Double { 2.0 * (self.w + self.h) }
}
impl Shape for Circle2 {
    def area(self) -> Double { 3.14159 * self.r * self.r }
    def perimeter(self) -> Double { 2.0 * 3.14159 * self.r }
}

def describe(s: dyn Shape) -> Double {
    s.area() + s.perimeter()
}

let r = Rect { w: 3.0, h: 4.0 };
let c = Circle2 { r: 2.0 };
let rd = describe(r);
let cd = describe(c);
assert(abs(rd - 26.0) < 0.001, "Rect describe wrong");
assert(abs(cd - 25.13272) < 0.001, "Circle describe wrong");
rd + cd
