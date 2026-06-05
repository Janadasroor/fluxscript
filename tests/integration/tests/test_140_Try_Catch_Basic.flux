def main() -> Double {
    var caught = 0.0;
    try {
        throw "error";
    } catch e {
        caught = 1.0;
    };
    assert(caught == 1.0, "catch should have been triggered");
    1.0
}
main()
