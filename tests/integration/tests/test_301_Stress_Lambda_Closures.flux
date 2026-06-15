def apply_twice(f, x) -> Double {
    f(f(x))
}
def compose(f, g, x) -> Double {
    f(g(x))
}
def double_it(x) -> Double { x * 2.0 }
def add_one(x) -> Double { x + 1.0 }
def square(x) -> Double { x * x }
def main() -> Double {
    let r1 = apply_twice(double_it, 3.0)
    assert(r1 == 12.0, "apply_twice double wrong")
    let r2 = apply_twice(add_one, 5.0)
    assert(r2 == 7.0, "apply_twice add_one wrong")
    let r3 = compose(double_it, add_one, 5.0)
    assert(r3 == 12.0, "compose wrong")
    let r4 = compose(add_one, double_it, 3.0)
    assert(r4 == 7.0, "compose rev wrong")
    let r5 = compose(square, add_one, 2.0)
    assert(r5 == 9.0, "compose square+add wrong")
    let r6 = apply_twice(square, 3.0)
    assert(r6 == 81.0, "apply_twice square wrong")
    1.0
}
main()
