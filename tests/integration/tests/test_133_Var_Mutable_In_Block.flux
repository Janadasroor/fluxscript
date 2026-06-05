def main() -> Double {
    var x = 1.0;
    {
        x = 2.0;
    };
    assert(x == 2.0, "var mutated inside block should persist");
    1.0
}
main()
