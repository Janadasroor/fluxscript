struct Pair { x: Double, y: Double }
impl Pair {
    def sum(self: Pair) -> Double { self.x + self.y }
}
def main() -> Double {
    let p: Pair = Pair { x: 3.0, y: 4.0 };
    p.sum()
}
