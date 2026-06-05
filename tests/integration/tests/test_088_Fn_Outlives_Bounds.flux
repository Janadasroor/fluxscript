def foo<'a, 'b: 'a>(x: &'a Double) -> &'b Double { x }
def test() -> Double { 1.0 }
test()
