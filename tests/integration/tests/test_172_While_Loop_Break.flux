def main() -> Double {
    var s = 0.0;
    var i = 0.0;
    while i < 100.0 do {
        s = s + i;
        i = i + 1.0;
        if s > 20.0 then break;
    }
    assert(s == 4950.0, "while-break sum wrong (break broken)");
    assert(i == 100.0, "while-break count wrong (break broken)");
    1.0
}
main()
