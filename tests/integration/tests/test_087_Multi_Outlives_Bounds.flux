struct Foo<'a, 'b: 'a, 'c: 'b> {
    a: &'a Double,
    b: &'b Double,
    c: &'c Double
}
def test() -> Double { 1.0 }
test()
