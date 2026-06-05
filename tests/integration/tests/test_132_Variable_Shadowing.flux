def main() -> Double {
    let x = 1.0;
    let r1 = x;
    let x = 2.0;
    let r2 = x;
    {
        let x = 3.0;
        let r3 = x;
        assert(r3 == 3.0, "inner shadow should be 3");
    };
    assert(r1 == 1.0, "original x should still be 1");
    assert(r2 == 2.0, "shadow x should be 2");
    x = 4.0;
    assert(x == 4.0, "reassigned shadow should be 4");
    1.0
}
main()
