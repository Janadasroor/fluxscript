def main() -> Double {
    var s = 0.0;
    for i in 0, 3 do
        for j in 0, 3 do
            s = s + i * j;
    assert(s == 36.0, "nested for sum wrong");
    1.0
}
main()
