struct Box[T] { val: T }
struct Pair[A, B] { left: A, right: B }
def main() -> Double {
    let b1 = Box[Double] { val: 1.0 }
    let b2 = Box[Box[Double]] { val: Box[Double] { val: 2.0 } }
    let b3 = Box[Box[Box[Double]]] { val: Box[Box[Double]] { val: Box[Double] { val: 3.0 } } }
    assert(b1.val == 1.0, "b1 wrong")
    assert(b2.val.val == 2.0, "b2 wrong")
    assert(b3.val.val.val == 3.0, "b3 wrong")
    let p1 = Pair[Double, Double] { left: 10.0, right: 20.0 }
    let p2 = Pair[Pair[Double, Double], Double] { left: p1, right: 30.0 }
    assert(p2.left.left == 10.0, "p2 deep left wrong")
    assert(p2.left.right == 20.0, "p2 deep mid wrong")
    assert(p2.right == 30.0, "p2 right wrong")
    let nested = Pair[Pair[Double, Double], Pair[Double, Double]] {
        left: Pair[Double, Double] { left: 1.0, right: 2.0 },
        right: Pair[Double, Double] { left: 3.0, right: 4.0 }
    }
    assert(nested.left.left + nested.left.right + nested.right.left + nested.right.right == 10.0, "nested sum")
    1.0
}
main()
