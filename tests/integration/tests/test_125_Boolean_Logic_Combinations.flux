def main() -> Double {
    let a = true;
    let b = false;
    let c = true;
    assert(a && b == false, "true && false should be false");
    assert(a || b == true, "true || false should be true");
    assert(!a == false, "!true should be false");
    assert(!b == true, "!false should be true");
    assert((a || b) && c == true, "(true||false)&&true should be true");
    assert((a && b) || (b && c) || (a && c) == true, "complex bool wrong");
    assert(!(a && b) == true, "not(false) should be true");
    1.0
}
main()
