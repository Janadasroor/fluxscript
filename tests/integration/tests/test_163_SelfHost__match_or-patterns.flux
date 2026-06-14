enum MyEnum { Val1 { value: Double }, Val2 { value: Double }, Val3 }
def test_match(x: MyEnum) -> Double {
    match x {
        MyEnum.Val1(v) | MyEnum.Val2(v) -> v,
        MyEnum.Val3 -> 100.0
    }
}
def main() -> Double {
    let r1 = test_match(MyEnum.Val1(42.0));
    let r2 = test_match(MyEnum.Val2(55.0));
    let r3 = test_match(MyEnum.Val3);
    if (r1 == 42.0 && r2 == 55.0 && r3 == 100.0) { 1.0 } else { 0.0 }
}
