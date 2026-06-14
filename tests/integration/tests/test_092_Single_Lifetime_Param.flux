struct Foo<'a> { x: &'a Double }
impl<'a> Foo<'a>
    def get(self) -> &'a Double { self.x }
end
def test() -> Double { 1.0 }
test()
