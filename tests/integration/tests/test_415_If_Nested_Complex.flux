def fizzbuzz(x: Double) -> Double {
    if x % 15.0 == 0.0 then 0.0
    else if x % 3.0 == 0.0 then 3.0
    else if x % 5.0 == 0.0 then 5.0
    else x
}

def main() -> Double {
    assert(fizzbuzz(15.0) == 0.0, "fizzbuzz 15");
    assert(fizzbuzz(9.0) == 3.0, "fizzbuzz 9");
    assert(fizzbuzz(10.0) == 5.0, "fizzbuzz 10");
    assert(fizzbuzz(7.0) == 7.0, "fizzbuzz 7");
    assert(fizzbuzz(30.0) == 0.0, "fizzbuzz 30");
    assert(fizzbuzz(2.0) == 2.0, "fizzbuzz 2");
    1.0
}
main()
