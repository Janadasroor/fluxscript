def identity[T](x: T) -> T { x }
struct Pair[T, U] { first: T, second: U }
def triple_identity(x: Double) -> Double {
    identity[Double](identity[Double](identity[Double](x)))
}
def nested_pair(x: Double) -> Double {
    let p1 = Pair[Double, Double] { first: x, second: x + 1.0 };
    let p2 = Pair[Pair[Double, Double], Double] { first: p1, second: x + 2.0 };
    p2.first.first + p2.first.second + p2.second
}
def main() -> Double {
    assert(identity[Double](42.0) == 42.0, "id wrong");
    assert(triple_identity(5.0) == 5.0, "triple id wrong");
    assert(nested_pair(10.0) == 33.0, "nested pair wrong");
    1.0
}
main()
