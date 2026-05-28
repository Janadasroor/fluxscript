def identity[T](x: T) -> T {
    return x
}

struct Pair[T] {
    first: T,
    second: T
}

def verify_generics() -> Double {
    let explicit_val = identity[Double](4.2);
    assert(explicit_val == 4.2, "Explicit generic function wrong");

    let implicit_val = identity(3.14);
    assert(implicit_val == 3.14, "Implicit generic function wrong");

    let pair = Pair[Double] { first: 10.0, second: 20.0 };
    assert(pair.first == 10.0, "Generic struct first field wrong");
    assert(pair.second == 20.0, "Generic struct second field wrong");

    1.0
}

verify_generics()
