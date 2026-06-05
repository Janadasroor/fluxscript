struct Inner { val: Double }
struct Outer { inner: Inner, scale: Double }
def main() -> Double {
    let inner = Inner { val: 5.0 };
    let outer = Outer { inner: inner, scale: 2.0 };
    assert(outer.inner.val == 5.0, "nested struct inner field wrong");
    assert(outer.scale == 2.0, "nested struct scale wrong");
    let result = outer.inner.val * outer.scale;
    assert(result == 10.0, "nested struct computed value wrong");
    1.0
}
main()
