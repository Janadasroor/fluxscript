def main() -> Double {
    var x = 1.0;
    var result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 200.0, "switch matching 1.0 wrong");
    x = 99.0;
    result = switch (x) {
        0.0 => 100.0,
        1.0 => 200.0,
        ~ => 300.0
    };
    assert(result == 300.0, "switch default wrong");
    1.0
}
main()
