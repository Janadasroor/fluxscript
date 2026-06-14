trait Pair {
    type First;
    type Second;
    def first(self: Pair) -> Double;
    def second(self: Pair) -> Double;
}
struct TwoVals { a: Double, b: Double }
impl Pair for TwoVals {
    type First = Double;
    type Second = Double;
    def first(self: TwoVals) -> Double { self.a }
    def second(self: TwoVals) -> Double { self.b }
}
def main() -> Double {
    0.0
}
