trait Container {
    type Item;
    def get(self, index: Double) -> Item
}

struct MyBox {
    value: Double
}

impl Container for MyBox {
    type Item = Double;
    def get(self, index: Double) -> Double {
        self.value
    }
}

def test() -> Double {
    let b = MyBox { value: 42.0 };
    let v = b.get(0.0);
    assert(abs(v - 42.0) < 0.001, "associated type basic failed");
    1.0
}
test()
