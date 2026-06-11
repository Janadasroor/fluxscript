def identity[T](x: T) -> T { x }
def sum_generic(x: Double) -> Double {
    let a = identity[Double](x);
    let b = identity[Double](x + 1.0);
    let c = identity[Double](x + 2.0);
    a + b + c
}
def deep_generic(x: Double) -> Double {
    let a = identity[Double](x);
    let b = identity[Double](a + 1.0);
    let c = identity[Double](b + 2.0);
    let d = identity[Double](c + 3.0);
    let e = identity[Double](d + 4.0);
    e
}
def main() -> Double {
    assert(identity[Double](42.0) == 42.0, "generic id wrong");
    let r = sum_generic(1.0);
    assert(r == 6.0, "sum generic wrong");
    let r2 = deep_generic(0.0);
    assert(r2 == 10.0, "deep generic wrong");
    1.0
}
main()
