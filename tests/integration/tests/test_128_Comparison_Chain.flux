def main() -> Double {
    assert(1.0 < 2.0 == true, "1<2 should be true");
    assert(2.0 < 2.0 == false, "2<2 should be false");
    assert(2.0 <= 2.0 == true, "2<=2 should be true");
    assert(3.0 > 2.0 == true, "3>2 should be true");
    assert(3.0 >= 3.0 == true, "3>=3 should be true");
    assert(1.0 != 2.0 == true, "1!=2 should be true");
    assert(1.0 == 1.0 == true, "1==1 should be true");
    assert(1.0 == 2.0 == false, "1==2 should be false");
    1.0
}
main()
