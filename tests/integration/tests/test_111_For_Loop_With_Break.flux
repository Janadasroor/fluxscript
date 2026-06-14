def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    for i in 1, 10 do {
        s = s + i;
        if i >= 5.0 then break;
    }
    assert(s == 45.0, "for-break sum wrong");
    1.0
}
main()
