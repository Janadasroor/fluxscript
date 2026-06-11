def main() -> Double {
    var s = 0.0;
    for i in 0, 10000 do
        s = s + 1.0;
    assert(s == 10000.0, "loop count wrong");
    var t = 1.0;
    for j in 1, 20 do
        t = t * j;
    assert(t == 121645100408832000.0, "loop product wrong");
    1.0
}
main()
