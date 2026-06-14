struct Box { val: Double }
impl Box {
    def scale(self: Box, f: Double) -> Double { self.val * f }
}
def main() -> Double {
    let b: Box = Box { val: 3.0 };
    5.0 |> b.scale
}
