def main() -> Double {
    var caught = 0.0;
    var finalized = 0.0;
    try {
        throw "err";
    } catch e {
        caught = 1.0;
    } finally {
        finalized = 1.0;
    };
    assert(caught == 1.0, "catch should trigger");
    assert(finalized == 1.0, "finally should trigger");
    1.0
}
main()
