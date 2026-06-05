def id[T](x: T) -> T { x }
def main() -> Double {
    assert(id[Double](42.0) == 42.0, "generic id double wrong");
    let s = id[Double](3.14);
    assert(abs(s - 3.14) < 0.001, "generic id pi wrong");
    1.0
}
main()
