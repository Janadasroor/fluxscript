def main() -> Double {
    var caught = 0.0;
    var value = 0.0;
    try {
        value = 42.0;
    } catch e {
        caught = 1.0;
    };
    assert(caught == 0.0, "catch should not trigger");
    assert(value == 42.0, "value should be set");
    1.0
}
main()
