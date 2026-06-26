def classify(x: Double) -> Double {
    if x > 0.0 then 1.0 else if x < 0.0 then -1.0 else 0.0
}

def main() -> Double {
    assert(classify(5.0) == 1.0, "chained positive");
    assert(classify(-3.0) == -1.0, "chained negative");
    assert(classify(0.0) == 0.0, "chained zero");
    1.0
}
main()
