struct Point { x: Double, y: Double }
impl Point
    def scale(self, f: Double) -> Double
        self.x * f
    end
end
def test() {
    let p = Point { x: 3.0, y: 4.0 }
    p.scale(2.0)
}
test()
