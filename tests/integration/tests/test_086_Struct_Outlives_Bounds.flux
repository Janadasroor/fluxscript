struct Foo<'a, 'b: 'a> {
    x: &'a Double,
    y: &'b Double
}
def test() -> Double { 1.0 }
test()
