def main() -> Double {
    let r = 0.0;
    if (1.0 == 1.0) {
        r = 42.0
    };
    assert(r == 42.0, "stmt no else true");

    r = 0.0;
    if (1.0 == 0.0) {
        r = 42.0
    };
    assert(r == 0.0, "stmt no else false");

    let s = if (1.0 == 0.0) {
        7.0
    } else {
        99.0
    };
    assert(s == 99.0, "stmt with else false");

    let t = if (1.0 == 1.0) {
        42.0
    } else {
        99.0
    };
    assert(t == 42.0, "stmt with else true");
    1.0
}
main()
