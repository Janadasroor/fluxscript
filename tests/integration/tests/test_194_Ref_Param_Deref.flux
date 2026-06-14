def sum_refs(a: &Double, b: &Double) -> Double {
    *a + *b
}
def main() -> Double {
    var x = 1.0;
    var y = 2.0;
    let s = sum_refs(&x, &y);
    assert(s == 3.0, "ref deref sum wrong");
    1.0
}
main()
