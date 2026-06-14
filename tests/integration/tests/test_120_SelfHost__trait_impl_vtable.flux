trait Math {
    def add(a: Double, b: Double) -> Double;
    def mul(a: Double, b: Double) -> Double;
}
struct Calc { }
impl Math for Calc {
    def add(a: Double, b: Double) -> Double { a + b }
    def mul(a: Double, b: Double) -> Double { a * b }
}
def main() -> Double { 0.0 }
