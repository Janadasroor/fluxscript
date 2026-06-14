struct Triple { a: Double, b: Double, c: Double }
enum Container { Item { value: Triple }, Empty }
def main() -> Double {
    let t = Triple { a: 10.0, b: 20.0, c: 30.0 }
    let c = Container.Item { value: t }
    let m = match c { Container.Item(v) -> v.a + v.b + v.c, Container.Empty -> -1.0 }
    let v = c.value
    let f = v.a + v.b + v.c
    assert(m == 60.0, "match boxed enum wrong")
    assert(f == 60.0, "field boxed enum wrong")
    1.0
}
main()
