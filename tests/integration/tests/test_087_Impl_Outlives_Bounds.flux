struct Foo<'a, 'b> {
    x: &'a Double,
    y: &'b Double
}
impl<'a, 'b: 'a> Foo<'a, 'b>
    def get_x(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
