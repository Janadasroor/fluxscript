struct Foo<'a> { x: &'a Double }
struct Bar<'a, 'b> { first: &'a Double, second: &'b Double }
def lifetime_basic() -> Double {
    let val = 42.0
    let r = &val
    assert(*r == 42.0, "lifetime basic deref")
    let foo = Foo { x: r }
    assert(*foo.x == 42.0, "lifetime struct field")
    1.0
}
def lifetime_multiple() -> Double {
    let a = 10.0
    let b = 20.0
    let bar = Bar { first: &a, second: &b }
    assert(*bar.first == 10.0, "bar first")
    assert(*bar.second == 20.0, "bar second")
    assert(*bar.first + *bar.second == 30.0, "bar sum")
    1.0
}
def lifetime_in_loop() -> Double {
    var total = 0.0
    var i = 0.0
    while i < 50.0 do {
        let val = i * 2.0
        let r = &val
        total = total + *r
        i = i + 1.0
    }
    assert(total == 2450.0, "lifetime loop wrong")
    1.0
}
def main() -> Double {
    assert(lifetime_basic() == 1.0, "lifetime_basic failed")
    assert(lifetime_multiple() == 1.0, "lifetime_multiple failed")
    assert(lifetime_in_loop() == 1.0, "lifetime_in_loop failed")
    1.0
}
main()
