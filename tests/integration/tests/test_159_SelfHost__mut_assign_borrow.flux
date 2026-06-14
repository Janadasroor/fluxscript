def main() -> Double {
    var x = 5.0;
    let r = &mut x;
    *r = 10.0;
    assert(x == 10.0, "mutable borrow assignment failed");
    x
}
