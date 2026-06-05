class Box { val: Double }
impl Box {
    def apply(self, f: Double) -> Double { self.val * f }
}
let b = Box { val: 3.0 };
5.0 |> b.apply
