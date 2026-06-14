enum MyEnum { Val1 { value: Double }, Val2 { value: Double }, Val3 }
def test_match(x: MyEnum) -> Double {
    match x {
        MyEnum.Val1(v) | MyEnum.Val2(v) -> v,
        MyEnum.Val3 -> 100.0
    }
}
def main() -> Double {
    assert(test_match(MyEnum.Val1(42.0)) == 42.0, "or arm 1 failed");
    assert(test_match(MyEnum.Val2(55.0)) == 55.0, "or arm 2 failed");
    assert(test_match(MyEnum.Val3) == 100.0, "val3 failed");
    1.0
}
main()
