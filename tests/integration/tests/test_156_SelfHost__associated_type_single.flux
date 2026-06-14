trait Container {
    type Item;
    def get(self: Container) -> Double;
}
struct MyBox { val: Double }
impl Container for MyBox {
    type Item = Double;
    def get(self: MyBox) -> Double { self.val }
}
def main() -> Double {
    0.0
}
