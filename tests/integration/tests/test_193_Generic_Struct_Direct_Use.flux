struct Box[T] { val: T }
def main() -> Double {
    let b = Box[Double] { val: 42.0 };
    assert(b.val == 42.0, "generic struct field wrong");
    1.0
}
main()
