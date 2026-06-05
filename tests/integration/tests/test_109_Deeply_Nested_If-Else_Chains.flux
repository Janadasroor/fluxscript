def classify(x: Double) -> Double {
    if x < 0.0 then
        if x < -10.0 then
            if x < -100.0 then -3.0 else -2.0
        else -1.0
    else
        if x == 0.0 then 0.0
        else if x < 10.0 then 1.0
        else if x < 100.0 then 2.0
        else 3.0
}
def main() -> Double {
    assert(classify(-200.0) == -3.0, "very negative wrong");
    assert(classify(-50.0) == -2.0, "moderate negative wrong");
    assert(classify(-5.0) == -1.0, "slight negative wrong");
    assert(classify(0.0) == 0.0, "zero wrong");
    assert(classify(5.0) == 1.0, "small positive wrong");
    assert(classify(50.0) == 2.0, "medium positive wrong");
    assert(classify(500.0) == 3.0, "large positive wrong");
    1.0
}
main()
