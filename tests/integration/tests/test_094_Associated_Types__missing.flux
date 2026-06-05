trait Container {
    type Item;
    def get(self, index: Double) -> Item
}
struct MyBox { value: Double }
impl Container for MyBox {
    def get(self, index: Double) -> Double { self.value }
}
