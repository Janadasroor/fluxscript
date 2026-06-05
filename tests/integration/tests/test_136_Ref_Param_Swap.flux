def swap(a: &mut Double, b: &mut Double) {
    let tmp = *a;
    *a = *b;
    *b = tmp;
}
def main() -> Double {
    var x = 1.0;
    var y = 2.0;
    swap(&mut x, &mut y);
    assert(x == 2.0, "swap x should be 2");
    assert(y == 1.0, "swap y should be 1");
    1.0
}
main()
