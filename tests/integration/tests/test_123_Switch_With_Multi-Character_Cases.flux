def main() -> Double {
    let result = switch (10.0) {
        5.0 => 1.0,
        10.0 => 2.0,
        15.0 => 3.0,
        ~ => 0.0
    };
    assert(result == 2.0, "switch multi-char case wrong");
    1.0
}
main()
