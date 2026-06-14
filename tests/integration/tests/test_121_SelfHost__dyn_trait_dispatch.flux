trait Doubler {
    def double_it(self) -> Double;
}
impl Doubler for Double {
    def double_it(self) -> Double { self * 2.0 }
}
def test(x: Double) -> Double {
    let d: dyn Doubler = x;
    d.double_it()
}
def main() -> Double { test(21.0) }
