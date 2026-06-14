def main() -> Double {
    var i = 0.0;
    var s = 0.0;
    while i < 10.0 do {
        s = s + i;
        i = i + 1.0;
    }
    assert(s == 45.0, "while sum wrong");
    assert(i == 10.0, "while count wrong");
    1.0
}
main()
