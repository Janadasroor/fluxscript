enum Container { Box { item: Double, label: Double }, Can { radius: Double, height: Double }, Tag(Double), Empty }
def volume(c: Container) -> Double {
    match c {
        Container.Box(w, h) -> w * h,
        Container.Can(r, h) -> r * r * 3.14159 * h,
        Container.Tag(v) -> v,
        Container.Empty -> 0.0
    }
}
def classify_vol(v: Double) -> Double {
    if v > 50.0 then 1.0 else if v == 0.0 then 0.0 else -1.0
}
def main() -> Double {
    let b = Container.Box(3.0, 4.0);
    assert(volume(b) == 12.0, "box vol wrong");
    assert(volume(Container.Empty) == 0.0, "empty vol wrong");
    assert(volume(Container.Tag(42.0)) == 42.0, "tag vol wrong");
    let can = Container.Can(2.0, 5.0);
    let cv = volume(can);
    assert(cv > 62.83 && cv < 62.84, "can vol wrong");
    assert(classify_vol(volume(Container.Box(10.0, 10.0))) == 1.0, "classify large box wrong");
    assert(classify_vol(volume(Container.Empty)) == 0.0, "classify empty wrong");
    assert(classify_vol(volume(Container.Tag(3.0))) == -1.0, "classify small tag wrong");
    var sum = 0.0;
    for i in 0, 4 do {
        let tag = Container.Tag(i * 10.0);
        sum = sum + volume(tag);
        var wsum = 0.0;
        var j = 0.0;
        while j < 3.0 do {
            let b2 = Container.Box(j, j + 1.0);
            wsum = wsum + volume(b2);
            j = j + 1.0;
        };
        sum = sum + wsum;
    };
    assert(sum == 92.0, "for while enum sum wrong");
    1.0
}
main()
